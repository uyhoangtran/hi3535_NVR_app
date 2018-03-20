/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : sample_pciv_slave.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2009/09/22
  Description   : this sample of pciv in PCI device
  History       :
  1.Date        : 2009/09/22
    Author      : Hi3520MPP
    Modification: Created file
  2.Date        : 2010/02/12
    Author      : Hi3520MPP
    Modification: 将消息端口的打开操作放到最开始的初始化过程中
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <math.h>

#include "hi_debug.h"
#include "hi_comm_pciv.h"
#include "mpi_pciv.h"
#include "pciv_msg.h"
#include "pciv_trans.h"
#include "sample_pciv_comm.h"
#include "sample_common.h"
#include "loadbmp.h"


#define PCIV_FRMNUM_ONCEDMA 5

typedef struct hiSAMPLE_PCIV_CTX_S
{
    VDEC_CHN VdChn;
    pthread_t pid;
    HI_BOOL bThreadStart;
    HI_CHAR aszFileName[64];
} SAMPLE_PCIV_CTX_S;

static SAMPLE_PCIV_CTX_S g_astSamplePciv[PCIV_MAX_CHN_NUM];
static SAMPLE_PCIV_VENC_CTX_S g_stSamplePcivVenc = {0};
static SAMPLE_PCIV_VDEC_CTX_S g_astSamplePcivVdec[VDEC_MAX_CHN_NUM] = {{0}};

static HI_S32 g_s32PciLocalId  = -1;
static HI_U32 g_u32PfAhbBase   = 0;
pthread_t   VdecSlaveThread[2*VDEC_MAX_CHN_NUM];
pthread_t   VdecThread[2*VDEC_MAX_CHN_NUM];
static HI_BOOL bExit[VDEC_MAX_CHN_NUM] = {HI_FALSE};


HI_S32 SamplePcivStopVdec(VDEC_CHN VdChn)
{
    SAMPLE_PCIV_CTX_S *pstCtx;

    HI_MPI_VDEC_StopRecvStream(VdChn);
    HI_MPI_VDEC_DestroyChn(VdChn);

    pstCtx = &g_astSamplePciv[VdChn];
    if (pstCtx->bThreadStart == HI_TRUE)
    {
        printf("start pthread_join vdec %d \n", VdChn);
        pstCtx->bThreadStart = HI_FALSE;
        pthread_join(pstCtx->pid, 0);
    }

    printf("vdec chn %d destroyed ok\n", VdChn);

    return HI_SUCCESS;
}

HI_S32 SamplePcivStartVdec(VDEC_CHN VdChn, VDEC_CHN_ATTR_S *pstVdecChnAttr)
{
    HI_S32 s32ret;
        
    s32ret = HI_MPI_VDEC_CreateChn(VdChn, pstVdecChnAttr);  
    if (HI_SUCCESS != s32ret)
    {
        printf("HI_MPI_VDEC_CreateChn %d failed, errno 0x%x \n", VdChn, s32ret);
        return s32ret;
    }

    s32ret = HI_MPI_VDEC_StartRecvStream(VdChn);
    if (HI_SUCCESS != s32ret)
    {
        printf("HI_MPI_VDEC_StartRecvStream %d failed, errno 0x%x \n", VdChn, s32ret);
        return s32ret;
    }
    printf("create vdec chn %d ok\n", VdChn);

    return HI_SUCCESS;
}

HI_S32 SamplePcivEchoMsg(HI_S32 s32RetVal, HI_S32 s32EchoMsgLen, SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;

    pMsg->stMsgHead.u32Target  = 0; /* To host */
    pMsg->stMsgHead.s32RetVal  = s32RetVal;
    pMsg->stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_ECHO;
    pMsg->stMsgHead.u32MsgLen  = s32EchoMsgLen + sizeof(SAMPLE_PCIV_MSGHEAD_S);
    s32Ret = PCIV_SendMsg(0, PCIV_MSGPORT_COMM_CMD, pMsg);
    HI_ASSERT(s32Ret != HI_FAILURE);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveInitWinVb(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_WINVB_S *pstWinVbArgs = (SAMPLE_PCIV_MSG_WINVB_S*)pMsg->cMsgBody;

    /* create buffer pool in PCI Window */
    s32Ret = HI_MPI_PCIV_WinVbDestroy();
    s32Ret = HI_MPI_PCIV_WinVbCreate(&pstWinVbArgs->stPciWinVbCfg);
    PCIV_CHECK_ERR(s32Ret);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveMalloc(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret = HI_SUCCESS, i;
    HI_U32 au32PhyAddr[PCIV_MAX_BUF_NUM];
    PCIV_PCIVCMD_MALLOC_S *pstMallocArgs = (PCIV_PCIVCMD_MALLOC_S *)pMsg->cMsgBody;

    /* in slave chip, this func will alloc a buffer from Window MMZ */
    s32Ret = HI_MPI_PCIV_Malloc(pstMallocArgs->u32BlkSize, pstMallocArgs->u32BlkCount, au32PhyAddr);
    HI_ASSERT(!s32Ret);

    /* Attation: return the offset from PCI shm_phys_addr */
    g_u32PfAhbBase = 0x9f800000;
    for(i=0; i<pstMallocArgs->u32BlkCount; i++)
    {
        pstMallocArgs->u32PhyAddr[i] = au32PhyAddr[i] - g_u32PfAhbBase;
        printf("func:%s, phyaddr:0x%x = 0x%x - 0x%x \n",
            __FUNCTION__, pstMallocArgs->u32PhyAddr[i], au32PhyAddr[i], g_u32PfAhbBase);
    }

    return HI_SUCCESS;
}

void* SamplePcivVdStreamThread(void* arg)
{
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = (SAMPLE_PCIV_VDEC_CTX_S*)arg;
    HI_VOID *pReceiver = pstVdecCtx->pTransHandle;
    PCIV_STREAM_HEAD_S *pStrmHead = NULL;
    HI_U8 *pu8Addr;
    HI_U32 u32Len;
    VDEC_STREAM_S stStream;
    HI_CHAR aszFileName[64] = {0};
    HI_S32 s32WriteLen = 0;
    static FILE *pFile[VENC_MAX_CHN_NUM] = {NULL};
    
    while (pstVdecCtx->bThreadStart)
    {
        /* get data from pciv stream receiver */
        if (PCIV_Trans_GetData(pReceiver, &pu8Addr, &u32Len))
        {
            usleep(10000);
            continue;
        }

        pStrmHead = (PCIV_STREAM_HEAD_S *)pu8Addr;
        HI_ASSERT(PCIV_STREAM_MAGIC == pStrmHead->u32Magic);
    #if 0
        if (0 == pstVdecCtx->VdecChn)
        {
            printf("Func: %s, Line: %d, u32Len: 0x%x, u32StreamDataLen: 0x%x, u32DMADataLen: 0x%x, head: %d, chn: %d.\n", 
                __FUNCTION__, __LINE__, u32Len, pStrmHead->u32StreamDataLen, pStrmHead->u32DMADataLen, sizeof(PCIV_STREAM_HEAD_S), pstVdecCtx->VdecChn);
        }
    #endif          
        HI_ASSERT(u32Len >= pStrmHead->u32DMADataLen + sizeof(PCIV_STREAM_HEAD_S));

        /* send the data to video decoder */
        stStream.pu8Addr = pu8Addr + sizeof(PCIV_STREAM_HEAD_S);
        stStream.u64PTS = 0;
        stStream.u32Len = pStrmHead->u32StreamDataLen;
        stStream.bEndOfStream = HI_FALSE;
        //printf("Func: %s, Line: %d, u32Len: 0x%x, \n", __FUNCTION__, __LINE__, stStream.u32Len);
        if (0 == stStream.u32Len)
        {
            usleep(10000);
            continue;
        }

        /* save stream data to file */
        if (NULL == pFile[pstVdecCtx->VdecChn])
        {
            sprintf(aszFileName, "slave_vdec_chn%d.h264", pstVdecCtx->VdecChn);
            pFile[pstVdecCtx->VdecChn] = fopen(aszFileName, "wb");
            HI_ASSERT(pFile[pstVdecCtx->VdecChn]);
        }
        s32WriteLen = fwrite(pu8Addr + sizeof(PCIV_STREAM_HEAD_S), pStrmHead->u32StreamDataLen, 1, pFile[pstVdecCtx->VdecChn]);
        //HI_ASSERT(1 == s32WriteLen);
        
        while (HI_TRUE == pstVdecCtx->bThreadStart && (HI_MPI_VDEC_SendStream(pstVdecCtx->VdecChn, &stStream, HI_IO_NOBLOCK)))
        {
            usleep(10000);
        }

        //memset(pu8Addr, 0, u32Len);
        /* release data to pciv stream receiver */
        PCIV_Trans_ReleaseData(pReceiver, pu8Addr, u32Len);
    }

    pstVdecCtx->bThreadStart = HI_FALSE;
    return NULL;
}


HI_S32 SamplePciv_SlaveStartVdecStream(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    PCIV_TRANS_ATTR_S *pstTransAttr = (PCIV_TRANS_ATTR_S*)pMsg->cMsgBody;
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = &g_astSamplePcivVdec[pstTransAttr->s32ChnId];

    /* msg port should have open when SamplePciv_SlaveInitPort() */
    HI_ASSERT(pstTransAttr->s32MsgPortWrite == pstVdecCtx->s32MsgPortWrite);
    HI_ASSERT(pstTransAttr->s32MsgPortRead  == pstVdecCtx->s32MsgPortRead);

    /* init vdec stream receiver */
    pstTransAttr->u32PhyAddr += g_u32PfAhbBase;/* NOTE:phyaddr in msg is a offset */
    s32Ret = PCIV_Trans_InitReceiver(pstTransAttr, &pstVdecCtx->pTransHandle);
    PCIV_CHECK_ERR(s32Ret);

    pstVdecCtx->bThreadStart = HI_TRUE;
    pstVdecCtx->VdecChn = pstTransAttr->s32ChnId;
    /* create thread to get stream coming from host chip, and send stream to decoder */
    pthread_create(&pstVdecCtx->pid, NULL, SamplePcivVdStreamThread, pstVdecCtx);

    printf("init vdec:%d stream receiver in slave chip ok!\n", pstTransAttr->s32ChnId);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStopVdecStream(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    PCIV_TRANS_ATTR_S *pstInitCmd = (PCIV_TRANS_ATTR_S*)pMsg->cMsgBody;
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = &g_astSamplePcivVdec[pstInitCmd->s32ChnId];

    /* exit thread*/
    if (HI_TRUE == pstVdecCtx->bThreadStart)
    {
        pstVdecCtx->bThreadStart = HI_FALSE;
        pthread_join(pstVdecCtx->pid, 0);
    }

    /* eixt vdec stream receiver */
    s32Ret = PCIV_Trans_DeInitReceiver(pstVdecCtx->pTransHandle);
    PCIV_CHECK_ERR(s32Ret);

    printf("exit vdec:%d stream receiver in slave chip ok!\n", pstVdecCtx->VdecChn);
    return HI_SUCCESS;
}

HI_S32 SamplePcivLoadRgnBmp(const char *filename, BITMAP_S *pstBitmap, HI_BOOL bFil, HI_U32 u16FilColor)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    if(GetBmpInfo(filename,&bmpFileHeader,&bmpInfo) < 0)
    {
		printf("GetBmpInfo err!\n");
        return HI_FAILURE;
    }

    Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;
    
    pstBitmap->pData = malloc(2*(bmpInfo.bmiHeader.biWidth)*(bmpInfo.bmiHeader.biHeight));
	
    if(NULL == pstBitmap->pData)
    {
        printf("malloc osd memroy err!\n");        
        return HI_FAILURE;
    }
    CreateSurfaceByBitMap(filename,&Surface,(HI_U8*)(pstBitmap->pData));
	
    pstBitmap->u32Width = Surface.u16Width;
    pstBitmap->u32Height = Surface.u16Height;
    pstBitmap->enPixelFormat = PIXEL_FORMAT_RGB_1555;

    int i,j;
    HI_U16 *pu16Temp;
    pu16Temp = (HI_U16*)pstBitmap->pData;
    
    if (bFil)
    {
        for (i=0; i<pstBitmap->u32Height; i++)
        {
            for (j=0; j<pstBitmap->u32Width; j++)
            {
                if (u16FilColor == *pu16Temp)
                {
                    *pu16Temp &= 0x7FFF;
                }

                pu16Temp++;
            }
        }

    }
        
    return HI_SUCCESS;
}


HI_S32 SamplePcivChnCreateRegion(PCIV_CHN PcivChn)
{
    HI_S32 s32Ret;
    MPP_CHN_S stChn;
    RGN_ATTR_S stRgnAttr;
    RGN_CHN_ATTR_S stChnAttr;
    BITMAP_S stBitmap;
    
    /*创建区域*/
    stRgnAttr.enType = OVERLAYEX_RGN;
    stRgnAttr.unAttr.stOverlayEx.enPixelFmt = PIXEL_FORMAT_RGB_1555;
    stRgnAttr.unAttr.stOverlayEx.stSize.u32Width  = 128;
    stRgnAttr.unAttr.stOverlayEx.stSize.u32Height = 128;
    stRgnAttr.unAttr.stOverlayEx.u32BgColor = 0xfc;
    
    s32Ret = HI_MPI_RGN_Create(PcivChn, &stRgnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("region of pciv chn %d create fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }
    
    /*将区域显示到通道中*/
    stChn.enModId = HI_ID_PCIV;
    stChn.s32DevId = 0;
    stChn.s32ChnId = PcivChn;
    
    stChnAttr.bShow = HI_TRUE;
    stChnAttr.enType = OVERLAYEX_RGN;
    stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32X = 128;
    stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32Y = 128;
    stChnAttr.unChnAttr.stOverlayExChn.u32BgAlpha   = 128;
    stChnAttr.unChnAttr.stOverlayExChn.u32FgAlpha   = 128;
    stChnAttr.unChnAttr.stOverlayExChn.u32Layer     = 0;

    /*添加位图*/
    SamplePcivLoadRgnBmp("mm2.bmp", &stBitmap, HI_FALSE, 0);
    
    s32Ret = HI_MPI_RGN_SetBitMap(PcivChn, &stBitmap);
    if (s32Ret != HI_SUCCESS)
    {
        printf("region set bitmap to  pciv chn %d fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }
    free(stBitmap.pData);
    
    s32Ret = HI_MPI_RGN_AttachToChn(PcivChn, &stChn, &stChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("region attach to  pciv chn %d fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }
       
    return HI_SUCCESS;
}

HI_S32 SamplePcivChnDestroyRegion(PCIV_CHN PcivChn)
{
    HI_S32 s32Ret;
    MPP_CHN_S stChn;
    stChn.enModId = HI_ID_PCIV;
    stChn.s32DevId = 0;
    stChn.s32ChnId = PcivChn;
    /* 区域解绑定 */
    s32Ret = HI_MPI_RGN_DetachFromChn(PcivChn, &stChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("region attach to  pciv chn %d fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }

    /* 销毁区域 */
    s32Ret = HI_MPI_RGN_Destroy(PcivChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("destroy  pciv chn %d region fail. value=0x%x.", PcivChn, s32Ret);
        return s32Ret;
    }       

    return HI_SUCCESS;
}

HI_S32 VDEC_SendEos(VDEC_CHN Vdchn)
{
    return HI_SUCCESS;
}


/******************************************************************************
 * function : send stream to vdec
 ******************************************************************************/
HI_VOID* SamplePcivSlaveStartVdecSendStreamThread(void* p)
{
    VDEC_STREAM_S stStream;
    VDEC_SENDPARAM_S *pstSendParam;
    char sFileName[50], sFilePostfix[20] = ".h264";
    FILE* fp = NULL;
    HI_S32 s32Ret;
    HI_S32 s32BlockMode = HI_IO_BLOCK;
    struct timeval stTime,*ptv; 
    HI_U8 *pu8Buf;
    HI_S32 s32LeftBytes,i;
    HI_BOOL bTimeFlag=HI_TRUE;
    HI_U64 pts= 1000;
    HI_S32 s32IntervalTime = 1;

    HI_U32 u32StartCode[4] = {0x41010000, 0x67010000, 0x01010000, 0x61010000};
    HI_U16 u16JpegStartCode = 0xD9FF;

    s32LeftBytes = 0;

    pstSendParam = (VDEC_SENDPARAM_S *)p;

    /*open the stream file*/
    sprintf(sFileName, "1080P%s", sFilePostfix);
    fp = fopen(sFileName, "r");
    if (HI_NULL == fp)
    {
        printf("open file %s err\n", sFileName);
        return NULL;
    }
    //printf("open file [%s] ok!\n", sFileName);

    if(0 != pstSendParam->s32MinBufSize)
    {
        pu8Buf=malloc(pstSendParam->s32MinBufSize);
        if(NULL == pu8Buf)
        {
            printf("can't alloc %d in send stream thread:%d\n",pstSendParam->s32MinBufSize,pstSendParam->VdChn);
            fclose(fp);
            return (HI_VOID *)(HI_FAILURE);
        }
    }
    else
    {
        printf("none buffer to operate in send stream thread:%d\n",pstSendParam->VdChn);
        return (HI_VOID *)(HI_FAILURE);
    }
    ptv=(struct timeval *)&stStream.u64PTS;

    while (HI_FALSE == bExit[pstSendParam->VdChn])
    {
        if(gettimeofday(&stTime, NULL))
        {
            if(bTimeFlag)
                printf("can't get time for pts in send stream thread %d\n",pstSendParam->VdChn);
            bTimeFlag=HI_FALSE;
        }
        stStream.u64PTS= 0;//((HI_U64)(stTime.tv_sec)<<32)|((HI_U64)stTime.tv_usec);
        stStream.bEndOfStream = HI_FALSE;
        stStream.bEndOfFrame  = HI_FALSE;
        stStream.pu8Addr=pu8Buf;
        stStream.u32Len=fread(pu8Buf+s32LeftBytes,1,pstSendParam->s32MinBufSize-s32LeftBytes,fp);
        // SAMPLE_PRT("bufsize:%d,readlen:%d,left:%d\n",pstVdecThreadParam->s32MinBufSize,stStream.u32Len,s32LeftBytes);
        s32LeftBytes=stStream.u32Len+s32LeftBytes;

        if((pstSendParam->enVideoMode==VIDEO_MODE_FRAME)&&(pstSendParam->enPayload== PT_H264))
        {
            HI_U8 *pFramePtr;
            HI_U32 u32StreamVal;
            HI_BOOL bFindStartCode = HI_FALSE;
            pFramePtr=pu8Buf+4;
            for(i=0;i<(s32LeftBytes-4);i++)
            {
                u32StreamVal=(pFramePtr[0]);
                u32StreamVal=u32StreamVal|((HI_U32)pFramePtr[1]<<8);
                u32StreamVal=u32StreamVal|((HI_U32)pFramePtr[2]<<16);
                u32StreamVal=u32StreamVal|((HI_U32)pFramePtr[3]<<24);
                if(  (u32StreamVal==u32StartCode[1])||
                        (u32StreamVal==u32StartCode[0])||
                        (u32StreamVal==u32StartCode[2])||
                        (u32StreamVal==u32StartCode[3]))
                {
                    bFindStartCode = HI_TRUE;
                    break;
                }
                pFramePtr++;
            }
            if (HI_FALSE == bFindStartCode)
            {
                printf("\033[0;31mALERT!!!,the search buffer is not big enough for one frame!!!%d\033[0;39m\n",
                        pstSendParam->VdChn);
            }
            i=i+4;
            stStream.u32Len=i;
            s32LeftBytes=s32LeftBytes-i;
        }
        else if((pstSendParam->enVideoMode==VIDEO_MODE_FRAME)&&((pstSendParam->enPayload== PT_JPEG)
                    ||(pstSendParam->enPayload == PT_MJPEG)))
        {
            HI_U8 *pFramePtr;
            HI_U16 u16StreamVal;
            HI_BOOL bFindStartCode = HI_FALSE;
            pFramePtr=pu8Buf; 
            for(i=0;i<(s32LeftBytes-1);i++)
            {
                u16StreamVal=(pFramePtr[0]);
                u16StreamVal=u16StreamVal|((HI_U16)pFramePtr[1]<<8);
                if(  (u16StreamVal == u16JpegStartCode))
                {
                    bFindStartCode = HI_TRUE;
                    break;
                }
                pFramePtr++;
            }
            if (HI_FALSE == bFindStartCode)
            {
                printf("\033[0;31mALERT!!!,the search buffer is not big enough for one frame!!!%d\033[0;39m\n",
                        pstSendParam->VdChn);
            }
            i=i+2;
            stStream.u32Len=i;
            s32LeftBytes=s32LeftBytes-i;
        }
        else // stream mode 
        {
            stStream.u32Len=s32LeftBytes;
            s32LeftBytes=0;
        }

        pts+=40000;
        stStream.u64PTS = pts;
        s32Ret=HI_MPI_VDEC_SendStream(pstSendParam->VdChn, &stStream, s32BlockMode);
        if (HI_SUCCESS != s32Ret)
        {
            printf("failret:%x\n",s32Ret);
            sleep(s32IntervalTime);
        }
        if(s32BlockMode==HI_IO_NOBLOCK && s32Ret==HI_FAILURE)
        {
            sleep(s32IntervalTime);
        }
        else if(s32BlockMode==HI_IO_BLOCK && s32Ret==HI_FAILURE)
        {
            printf("can't send stream in send stream thread %d\n",pstSendParam->VdChn);
            sleep(s32IntervalTime);
        }
        if(pstSendParam->enVideoMode==VIDEO_MODE_FRAME && s32Ret==HI_SUCCESS)
        {
            memcpy(pu8Buf,pu8Buf+stStream.u32Len,s32LeftBytes);
        }
        else if (pstSendParam->enVideoMode==VIDEO_MODE_FRAME && s32Ret!=HI_SUCCESS)
        {
            s32LeftBytes = s32LeftBytes+stStream.u32Len;
        }

        if(stStream.u32Len!=(pstSendParam->s32MinBufSize-s32LeftBytes))
        {
            printf("file end.\n");
            fseek(fp,0,SEEK_SET);
            VDEC_SendEos(pstSendParam->VdChn); // in hi3531, user needn't send eos.
            // break;
        }

        usleep(20000);
    }
    fflush(stdout);
    free(pu8Buf);
    fclose(fp);

    return (HI_VOID *)HI_SUCCESS;
}


HI_S32 SamplePciv_SlaveStartVdec(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 i;
    HI_S32 s32Ret;

    VDEC_CHN_ATTR_S stVdecChnAttr;
	VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];
    
    SAMPLE_PCIV_MSG_VDEC_S *pstVdecArgs = (SAMPLE_PCIV_MSG_VDEC_S*)pMsg->cMsgBody;

    switch (pstVdecArgs->enPicSize)
    {
        case PIC_D1     :
            VDEC_MST_DefaultH264D1_Attr(&stVdecChnAttr);
            break;
        case PIC_HD1080 :
            VDEC_MST_DefaultH264HD_Attr(&stVdecChnAttr);
            break;
        case PIC_960H :
            VDEC_MST_DefaultH264960H_Attr(&stVdecChnAttr);
            break;
        default:
            VDEC_MST_DefaultH264D1_Attr(&stVdecChnAttr);
            break;
    }
    
    for (i = 0; i < pstVdecArgs->u32VdecChnNum; i++)
    {
        s32Ret = SamplePcivStartVdec(i, &stVdecChnAttr);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }
    
    if (HI_TRUE == pstVdecArgs->bReadStream)
    {
        /* 从片读码流，解码后送主片 */
	    SamplePcivSetVdecParam(pstVdecArgs->u32VdecChnNum, &stVdecSend[0], &stVdecChnAttr, SAMPLE_1080P_H264_PATH);	
	    SamplePcivStartVdecSendStream(pstVdecArgs->u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);  
        
        printf("Vdec read stream file thread has started successful! Vdec ChnNum: %d.\n", pstVdecArgs->u32VdecChnNum);
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStopVdec(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 i;
    HI_S32 s32Ret;
	VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];
    SAMPLE_PCIV_MSG_VDEC_S *pstVdecArgs = (SAMPLE_PCIV_MSG_VDEC_S*)pMsg->cMsgBody;

    for (i = 0; i < pstVdecArgs->u32VdecChnNum; i++)
    {
        s32Ret = SamplePcivStopVdec(i);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }

    if (HI_TRUE == pstVdecArgs->bReadStream)
    {
        /* 停止从片读码流，解码后送主片 */
        SamplePcivStopVdecReadStream(pstVdecArgs->u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);
    }

    printf("Vdec read stream file thread has stopped successful! line: %d, Vdec ChnNum: %d.\n", __LINE__, pstVdecArgs->u32VdecChnNum);

    return HI_SUCCESS;
}

HI_S32 SamplePcivStartVpss(SAMPLE_PCIV_MSG_VPSS_S *pstVpssArgs)
{
    HI_S32 i, s32Ret;
    VPSS_GRP vpssGrp = pstVpssArgs->vpssGrp;
    VPSS_GRP_ATTR_S stGrpAttr;
    MPP_CHN_S stDestChn;
    switch (pstVpssArgs->enPicSize)
    {
        case PIC_D1     :
            stGrpAttr.u32MaxW = 720;
            stGrpAttr.u32MaxH = 576;
            break;
        case PIC_HD1080 :
            stGrpAttr.u32MaxW = 1920;
            stGrpAttr.u32MaxH = 1080;
            break;
        case PIC_960H   :
            stGrpAttr.u32MaxW = 960;
            stGrpAttr.u32MaxH = 576;
            break;
        default         :
            stGrpAttr.u32MaxW = 1920;
            stGrpAttr.u32MaxH = 1080;
            break;
    }
    
    stGrpAttr.enPixFmt  = SAMPLE_PIXEL_FORMAT;
    stGrpAttr.bIeEn     = HI_FALSE;
    stGrpAttr.bNrEn     = HI_FALSE;
    stGrpAttr.bHistEn   = HI_FALSE;
    stGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    stGrpAttr.bDciEn    = HI_FALSE;

    s32Ret = HI_MPI_VPSS_CreateGrp(vpssGrp, &stGrpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS create group error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_VPSS_StartGrp(vpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS start group error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }

    if (HI_TRUE == pstVpssArgs->bBindVdec)
    {
        stDestChn.enModId = HI_ID_VPSS;
        stDestChn.s32DevId = vpssGrp;
        stDestChn.s32ChnId = 0;
        s32Ret = HI_MPI_SYS_Bind(&pstVpssArgs->stBInd, &stDestChn);
        if (HI_SUCCESS != s32Ret)
        {
            printf("VPSS bind error, value= 0x%x.\n", s32Ret);
            return HI_FAILURE;
        }
    }
    
    for (i=0; i<VPSS_MAX_CHN_NUM; i++)
    {
        if (pstVpssArgs->vpssChnStart[i])
        {
            s32Ret = HI_MPI_VPSS_EnableChn(vpssGrp, i);
            if (HI_SUCCESS != s32Ret)
            {
                printf("VPSS enable chn error, value= 0x%x.\n", s32Ret);
                return HI_FAILURE;
            }
        }
    }

    return HI_SUCCESS;
}
HI_S32 SamplePcivStopVpss(SAMPLE_PCIV_MSG_VPSS_S *pstVpssArgs)
{
    HI_S32 i, s32Ret;
    VPSS_GRP vpssGrp = pstVpssArgs->vpssGrp;
    MPP_CHN_S stDestChn;

    for (i=0; i<VPSS_MAX_CHN_NUM; i++)
    {
        if (pstVpssArgs->vpssChnStart[i])
        {
            s32Ret = HI_MPI_VPSS_DisableChn(vpssGrp, i);
            if (HI_SUCCESS != s32Ret)
            {
                printf("VPSS disable chn error, value= 0x%x.\n", s32Ret);
                return HI_FAILURE;
            }
        }
    }

    s32Ret = HI_MPI_VPSS_StopGrp(vpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS start group error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }

    if (HI_TRUE == pstVpssArgs->bBindVdec)
    {
        stDestChn.enModId = HI_ID_VPSS;
        stDestChn.s32DevId = vpssGrp;
        stDestChn.s32ChnId = 0;
        s32Ret = HI_MPI_SYS_UnBind(&pstVpssArgs->stBInd, &stDestChn);
        if (HI_SUCCESS != s32Ret)
        {
            printf("VPSS bind error, value= 0x%x.\n", s32Ret);
            return HI_FAILURE;
        }
    }
    
    s32Ret = HI_MPI_VPSS_DestroyGrp(vpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        printf("VPSS destroy group error, value= 0x%x.\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStartVpss(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_VPSS_S *pstVpssArgs = (SAMPLE_PCIV_MSG_VPSS_S*)pMsg->cMsgBody;

    s32Ret = SamplePcivStartVpss(pstVpssArgs);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}
HI_S32 SamplePciv_SlaveStopVpss(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_VPSS_S *pstVpssArgs = (SAMPLE_PCIV_MSG_VPSS_S*)pMsg->cMsgBody;

    s32Ret = SamplePcivStopVpss(pstVpssArgs);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}



HI_S32 SamplePcivStartVirtualVo(SAMPLE_PCIV_MSG_VO_S *pstVoArgs)
{
    HI_S32 i;
    HI_S32 s32Ret;
    HI_S32 s32DispNum;
    VO_LAYER VoLayer;
    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    VO_CHN_ATTR_S astChnAttr[16];
    VO_DEV VirtualVo;
    HI_S32 u32VoChnNum;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;
    VO_INTF_SYNC_E enIntfSync;
    
    VirtualVo   = pstVoArgs->VirtualVo;
    u32VoChnNum = pstVoArgs->s32VoChnNum;
    enIntfSync  = pstVoArgs->enIntfSync;

    s32DispNum = SamplePcivGetVoDisplayNum(u32VoChnNum);
    if(s32DispNum < 0)
    {
        printf("SAMPLE_RGN_GetVoDisplayNum failed! u32VoChnNum: %d.\n", u32VoChnNum);
        return HI_FAILURE;
    }
    printf("Func: %s, line: %d, u32VoChnNum: %d, s32DispNum: %d\n", __FUNCTION__, __LINE__, u32VoChnNum, s32DispNum);
    
    s32Ret = SamplePcivGetVoAttr(VirtualVo, enIntfSync, &stPubAttr, &stLayerAttr, s32DispNum, astChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_RGN_GetVoAttr failed!\n");
        return HI_FAILURE;
    }

    VoLayer = SamplePcivGetVoLayer(VirtualVo);
    if(VoLayer < 0)
    {
        printf("SAMPLE_RGN_GetVoLayer failed! VoDev: %d.\n", VirtualVo);
        return HI_FAILURE;
    }
    
    s32Ret = HI_MPI_VO_Disable(VirtualVo);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_Disable failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VO_SetPubAttr(VirtualVo, &stPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_SetPubAttr failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VO_Enable(VirtualVo);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_Enable failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }
    //printf("VO dev:%d enable ok \n", VoDev);
  
    s32Ret = HI_MPI_VO_SetVideoLayerAttr(VoLayer, &stLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_SetVideoLayerAttr failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }
    
    s32Ret = HI_MPI_VO_EnableVideoLayer(VoLayer);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_EnableVideoLayer failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    //printf("VO video layer:%d enable ok \n", VoDev);

    for (i = 0; i < u32VoChnNum; i++)
    {
        s32Ret = HI_MPI_VO_SetChnAttr(VoLayer, i, &astChnAttr[i]);
        if (HI_SUCCESS != s32Ret)
        {
            printf("HI_MPI_VO_SetChnAttr failed! s32Ret: 0x%x.\n", s32Ret);
            return s32Ret;
        }

        s32Ret = HI_MPI_VO_EnableChn(VoLayer, i);
        if (HI_SUCCESS != s32Ret)
        {
            printf("HI_MPI_VO_EnableChn failed! s32Ret: 0x%x.\n", s32Ret);
            return s32Ret;
        }

        stSrcChn.enModId   = pstVoArgs->stBInd.enModId;
        stSrcChn.s32DevId  = pstVoArgs->stBInd.s32DevId;
        stSrcChn.s32ChnId  = i;

        stDestChn.enModId  = HI_ID_VOU;
        stDestChn.s32DevId = VoLayer;
        stDestChn.s32ChnId = i;
            
        s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
        //printf("VO chn:%d enable ok \n", i);
    }

    //printf("VO: %d enable ok!\n", VoDev);
    
    return HI_SUCCESS;
}


HI_S32 SamplePcivStopVirtualVo(SAMPLE_PCIV_MSG_VO_S *pstVoArgs)
{
    HI_S32 i, s32Ret;
    VO_DEV VirtualVo;
    HI_S32 s32ChnNum;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;
    VO_LAYER VoLayer;
    
    VirtualVo = pstVoArgs->VirtualVo;
    s32ChnNum = pstVoArgs->s32VoChnNum;

    VoLayer = SamplePcivGetVoLayer(VirtualVo);
    if(VoLayer < 0)
    {
        printf("SAMPLE_RGN_GetVoLayer failed! VoDev: %d.\n", VirtualVo);
        return HI_FAILURE;
    }
    
    for (i=0; i<s32ChnNum; i++)
    {
        s32Ret = HI_MPI_VO_DisableChn(VoLayer, i);
        PCIV_CHECK_ERR(s32Ret);

        stSrcChn.enModId   = pstVoArgs->stBInd.enModId;
        stSrcChn.s32DevId  = pstVoArgs->stBInd.s32DevId;
        stSrcChn.s32ChnId  = i;

        stDestChn.enModId  = HI_ID_VOU;
        stDestChn.s32DevId = VoLayer;
        stDestChn.s32ChnId = i;
            
        s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
        
    }    

    s32Ret = HI_MPI_VO_DisableVideoLayer(VoLayer);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_Disable(VirtualVo);
    PCIV_CHECK_ERR(s32Ret);

    return HI_SUCCESS;
}


HI_S32 SamplePciv_SlaveStartVo(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_VO_S *pstVoArgs = (SAMPLE_PCIV_MSG_VO_S*)pMsg->cMsgBody;
    
    s32Ret = SamplePcivStartVirtualVo(pstVoArgs);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStopVo(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_VO_S *pstVoArgs = (SAMPLE_PCIV_MSG_VO_S*)pMsg->cMsgBody;

    s32Ret = SamplePcivStopVirtualVo(pstVoArgs);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}


HI_S32 SamplePciv_SlaveStartPciv(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    VO_LAYER VoLayer;
    PCIV_PCIVCMD_CREATE_S *pstMsgCreate = (PCIV_PCIVCMD_CREATE_S *)pMsg->cMsgBody;
    PCIV_CHN PcivChn = pstMsgCreate->pcivChn;
    PCIV_ATTR_S *pstPicvAttr = &pstMsgCreate->stDevAttr;
    PCIV_BIND_OBJ_S *pstBindObj = &pstMsgCreate->stBindObj[0];
    MPP_CHN_S stSrcChn,stDestChn;
    
    printf("PCIV_ADD_OSD is %d.\n", pstMsgCreate->bAddOsd);
    
    /* 1) create pciv chn */
    s32Ret = HI_MPI_PCIV_Create(PcivChn, pstPicvAttr);
    PCIV_CHECK_ERR(s32Ret);

    /* 2) bind pciv and vi or vdec */
    switch (pstBindObj->enType)
    {
        case PCIV_BIND_VI:
            stSrcChn.enModId  = HI_ID_VIU;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.viDevice.viDev;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.viDevice.viChn;
            break;
        case PCIV_BIND_VO:
            stSrcChn.enModId  = HI_ID_VOU;
            VoLayer = SamplePcivGetVoLayer(pstBindObj->unAttachObj.voDevice.voDev);
            if(VoLayer < 0)
            {
                printf("SAMPLE_RGN_GetVoLayer failed! VoDev: %d.\n", pstBindObj->unAttachObj.voDevice.voDev);
                return HI_FAILURE;
            }
            
            stSrcChn.s32DevId = VoLayer;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.voDevice.voChn;
            break;
        case PCIV_BIND_VDEC:
            stSrcChn.enModId  = HI_ID_VDEC;
            stSrcChn.s32DevId = 0;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.vdecDevice.vdecChn;
            break;
        case PCIV_BIND_VPSS:
            stSrcChn.enModId  = HI_ID_VPSS;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.vpssDevice.vpssGrp;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.vpssDevice.vpssChn;
            break;
        default:
            HI_ASSERT(0);
    }
    stDestChn.enModId  = HI_ID_PCIV;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = PcivChn;
    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    printf("src mod:%d dev:%d chn:%d dest mod:%d dev:%d chn:%d\n",
        stSrcChn.enModId,stSrcChn.s32DevId,stSrcChn.s32ChnId,
        stDestChn.enModId,stDestChn.s32DevId,stDestChn.s32ChnId);
    PCIV_CHECK_ERR(s32Ret);

    /* 3) create region for pciv chn */
    if (1 == pstMsgCreate->bAddOsd)
    {
        s32Ret = SamplePcivChnCreateRegion(PcivChn);
        if (s32Ret != HI_SUCCESS)
        {
            printf("pciv chn %d SamplePcivChnCreateRegion err, value = 0x%x. \n", PcivChn, s32Ret);
            return s32Ret;
        }
    }
    
    /* 4) start pciv chn */
    s32Ret = HI_MPI_PCIV_Start(PcivChn);
    PCIV_CHECK_ERR(s32Ret);

    printf("slave start pciv chn %d ok, remote chn:%d=========\n",
        PcivChn,pstPicvAttr->stRemoteObj.pcivChn);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveStopPicv(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret;
    PCIV_PCIVCMD_DESTROY_S *pstMsgDestroy = (PCIV_PCIVCMD_DESTROY_S*)pMsg->cMsgBody;
    PCIV_CHN pcivChn = pstMsgDestroy->pcivChn;
    PCIV_BIND_OBJ_S *pstBindObj = &pstMsgDestroy->stBindObj[0];
    MPP_CHN_S stSrcChn,stDestChn;

    s32Ret = HI_MPI_PCIV_Stop(pcivChn);
    PCIV_CHECK_ERR(s32Ret);

    switch (pstBindObj->enType)
    {
        case PCIV_BIND_VI:
            stSrcChn.enModId  = HI_ID_VIU;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.viDevice.viDev;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.viDevice.viChn;
            break;
        case PCIV_BIND_VO:
            stSrcChn.enModId  = HI_ID_VOU;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.voDevice.voDev;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.voDevice.voChn;
            break;
        case PCIV_BIND_VDEC:
            stSrcChn.enModId  = HI_ID_VDEC;
            stSrcChn.s32DevId = 0;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.vdecDevice.vdecChn;
            break;
        case PCIV_BIND_VPSS:
            stSrcChn.enModId  = HI_ID_VPSS;
            stSrcChn.s32DevId = pstBindObj->unAttachObj.vpssDevice.vpssGrp;
            stSrcChn.s32ChnId = pstBindObj->unAttachObj.vpssDevice.vpssChn;
            break;
        default:
            HI_ASSERT(0);
    }
    stDestChn.enModId  = HI_ID_PCIV;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = pcivChn;
    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    printf("src mod:%d dev:%d chn:%d dest mod:%d dev:%d chn:%d\n",
        stSrcChn.enModId,stSrcChn.s32DevId,stSrcChn.s32ChnId,
        stDestChn.enModId,stDestChn.s32DevId,stDestChn.s32ChnId);
    PCIV_CHECK_ERR(s32Ret);

    if (1 == pstMsgDestroy->bAddOsd)
    {
        s32Ret = SamplePcivChnDestroyRegion(pcivChn);
        PCIV_CHECK_ERR(s32Ret);
    }
    
    s32Ret = HI_MPI_PCIV_Destroy(pcivChn);
    PCIV_CHECK_ERR(s32Ret);

    printf("pciv chn %d destroy ok \n", pcivChn);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveInitPort(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret, i;
    PCIV_MSGPORT_INIT_S *pstMsgPort = (PCIV_MSGPORT_INIT_S*)pMsg->cMsgBody;

    g_stSamplePcivVenc.s32MsgPortWrite = pstMsgPort->s32VencMsgPortW;
    g_stSamplePcivVenc.s32MsgPortRead  = pstMsgPort->s32VencMsgPortR;
    s32Ret  = PCIV_OpenMsgPort(0, g_stSamplePcivVenc.s32MsgPortWrite);
    s32Ret |= PCIV_OpenMsgPort(0, g_stSamplePcivVenc.s32MsgPortRead);
    HI_ASSERT(HI_SUCCESS == s32Ret);

    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        g_astSamplePcivVdec[i].s32MsgPortWrite  = pstMsgPort->s32VdecMsgPortW[i];
        g_astSamplePcivVdec[i].s32MsgPortRead   = pstMsgPort->s32VdecMsgPortR[i];
        s32Ret  = PCIV_OpenMsgPort(0, g_astSamplePcivVdec[i].s32MsgPortWrite);
        s32Ret |= PCIV_OpenMsgPort(0, g_astSamplePcivVdec[i].s32MsgPortRead);
        HI_ASSERT(HI_SUCCESS == s32Ret);
    }

    printf("Func: %s, line: %d, SamplePciv_SlaveInitPort success!\n", __FUNCTION__, __LINE__);
    
    return HI_SUCCESS;
}

HI_S32 SamplePciv_SlaveExitPort(SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 i;
    PCIV_CloseMsgPort(0, g_stSamplePcivVenc.s32MsgPortWrite);
    PCIV_CloseMsgPort(0, g_stSamplePcivVenc.s32MsgPortRead);
    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        PCIV_CloseMsgPort(0, g_astSamplePcivVdec[i].s32MsgPortWrite);
        PCIV_CloseMsgPort(0, g_astSamplePcivVdec[i].s32MsgPortRead);
    }
	printf("Slave chip close port ok!\n");
    return HI_SUCCESS;
}

int SamplePcivGetLocalId(int *local_id)
{
    int fd;
    struct hi_mcc_handle_attr attr;

    fd = open("/dev/mcc_userdev", O_RDWR);
    if (fd<=0)
    {
        printf("open mcc dev fail\n");
        return -1;
    }

    *local_id = ioctl(fd, HI_MCC_IOC_GET_LOCAL_ID, &attr);
    printf("pci local id is %d \n", *local_id);

    attr.target_id = 0;
    attr.port      = 0;
    attr.priority  = 0;
    ioctl(fd, HI_MCC_IOC_CONNECT, &attr);
    printf("===================close port %d!\n",attr.port);
    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    HI_S32 s32Ret, s32EchoMsgLen = 0;
    VB_CONF_S stVbConf = {0};
    VB_CONF_S stVdecVbConf = {0};
    PCIV_BASEWINDOW_S stPciBaseWindow;
    SAMPLE_PCIV_MSG_S stMsg;

    /* wait for pci host ... */
    s32Ret = PCIV_WaitConnect(0);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    SamplePcivGetLocalId(&g_s32PciLocalId);

    /* open pci msg port for commom cmd */
    s32Ret = PCIV_OpenMsgPort(0, PCIV_MSGPORT_COMM_CMD);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    /* init mpp sys and vb */
    stVbConf.u32MaxPoolCnt             = 2;
    stVbConf.astCommPool[0].u32BlkSize = 768 * 576 * 2;/*D1*/
    stVbConf.astCommPool[0].u32BlkCnt  = 50;
    stVbConf.astCommPool[1].u32BlkSize = SAMPLE_PCIV_VENC_STREAM_BUF_LEN;
    stVbConf.astCommPool[1].u32BlkCnt  = 1;

    stVdecVbConf.u32MaxPoolCnt               = 2;
    stVdecVbConf.astCommPool[0].u32BlkSize   = 1920*1080*2;
    stVdecVbConf.astCommPool[0].u32BlkCnt    = 40;

    stVdecVbConf.astCommPool[1].u32BlkSize   = 1920*1080*3/2;
    stVdecVbConf.astCommPool[1].u32BlkCnt    = 40;
        
    s32Ret = SAMPLE_InitMPP(&stVbConf, &stVdecVbConf);
    PCIV_CHECK_ERR(s32Ret);
    
    SAMPLE_COMM_SYS_MemConfig();
    
    /* get PF Window info of this pci device */
    stPciBaseWindow.s32ChipId = 0;
    s32Ret = HI_MPI_PCIV_GetBaseWindow(0, &stPciBaseWindow);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    g_u32PfAhbBase = stPciBaseWindow.u32PfAHBAddr;
    printf("PF AHB addr:0x%x\n", g_u32PfAhbBase);

    while (1)
    {
        s32EchoMsgLen = 0;
        s32Ret = PCIV_ReadMsg(0, PCIV_MSGPORT_COMM_CMD, &stMsg);
        if (s32Ret != HI_SUCCESS)
        {
            usleep(10000);
            continue;
        }
        printf("\nreceive msg, MsgType:(%d,%s) \n",
            stMsg.stMsgHead.u32MsgType, PCIV_MSG_PRINT_TYPE(stMsg.stMsgHead.u32MsgType));

        switch(stMsg.stMsgHead.u32MsgType)
        {
            case SAMPLE_PCIV_MSG_INIT_MSG_PORG:
            {
                s32Ret = SamplePciv_SlaveInitPort(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_EXIT_MSG_PORG:
            {
                s32Ret = SamplePciv_SlaveExitPort(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_START_VDEC:
            {
                s32Ret = SamplePciv_SlaveStartVdec(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_STOP_VDEC:
            {
                s32Ret = SamplePciv_SlaveStopVdec(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_START_VPSS:
            {
                s32Ret = SamplePciv_SlaveStartVpss(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_STOP_VPSS:
            {
                s32Ret = SamplePciv_SlaveStopVpss(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_START_VO:
            {
                s32Ret = SamplePciv_SlaveStartVo(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_STOP_VO:
            {
                s32Ret = SamplePciv_SlaveStopVo(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_CREATE_PCIV:
            {
                s32Ret = SamplePciv_SlaveStartPciv(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_DESTROY_PCIV:
            {
                s32Ret = SamplePciv_SlaveStopPicv(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_INIT_STREAM_VDEC:
            {
                s32Ret = SamplePciv_SlaveStartVdecStream(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_EXIT_STREAM_VDEC:
            {
                s32Ret = SamplePciv_SlaveStopVdecStream(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_INIT_WIN_VB:
            {
                s32Ret = SamplePciv_SlaveInitWinVb(&stMsg);
                break;
            }
            case SAMPLE_PCIV_MSG_MALLOC:
            {
                s32Ret = SamplePciv_SlaveMalloc(&stMsg);
                s32EchoMsgLen = sizeof(PCIV_PCIVCMD_MALLOC_S);
                break;
            }
            default:
            {
                printf("invalid msg, type:%d \n", stMsg.stMsgHead.u32MsgType);
                s32Ret = HI_FAILURE;
                break;
            }
        }
        /* echo msg to host */
        SamplePcivEchoMsg(s32Ret, s32EchoMsgLen, &stMsg);
    }

    /* exit */
    SAMPLE_ExitMPP();

    return HI_SUCCESS;
}


