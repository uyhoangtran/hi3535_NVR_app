/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : sample_pciv_host.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2009/09/22
  Description   : this sample of pciv in PCI host
  History       :
  1.Date        : 2009/09/22
    Author      : Hi3520MPP
    Modification: Created file
  2.Date        : 2010/02/12
    Author      : Hi3520MPP
    Modification: 将消息端口的打开操作放到最开始的初始化过程中
  3.Date        : 2010/06/10
    Author      : Hi3520MPP
    Modification: 调整启动PCIV通道的相关函数封装；并支持同一输入画面输出到主片的多个VO设备上显示
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
#include <signal.h>

#include "hi_comm_pciv.h"
#include "mpi_pciv.h"
#include "mpi_vdec.h"
#include "mpi_vpss.h"
#include "pciv_msg.h"
#include "pciv_trans.h"
#include "sample_pciv_comm.h"
#include "hi_debug.h"

#include "sample_comm.h"
#include "sample_common.h"

typedef struct hiSAMPLE_PCIV_DISP_CTX_S
{
    VO_DEV  VoDev;       /* VO 设备号 */
    HI_BOOL bValid;     /* 是否使用此显示设备*/
    HI_U32  u32PicDiv;   /* 当前显示设备的画面分割数 */
    VO_CHN  VoChnStart;  /* 当前显示设备起始VO通道号 */
    VO_CHN  VoChnEnd;    /* 当前显示设备结束VO通道号 */
} SAMPLE_PCIV_DISP_CTX_S;


extern VIDEO_NORM_E   gs_enViNorm;
extern VO_INTF_SYNC_E gs_enSDTvMode;

#define SAMPLE_PCIV_VDEC_SIZE PIC_HD1080
#define SAMPLE_PCIV_VDEC_FILE "sample_cif_25fps.h264"


/* max pciv chn count in one vo dev */
#define SAMPLE_PCIV_CNT_PER_VO      16
#define PCIV_FRMNUM_ONCEDMA 5
#define PCIE_BAR0_ADDRESS   0x30800000

/* max pciv chn count in one pci dev */
#define SAMPLE_PCIV_CNT_PER_DEV     SAMPLE_PCIV_CNT_PER_VO * VO_MAX_DEV_NUM

static HI_U32 g_u32PfWinBase[PCIV_MAX_CHIPNUM]  = {0};
VIDEO_NORM_E   g_enViNorm   = VIDEO_ENCODING_MODE_PAL;

static SAMPLE_PCIV_VENC_CTX_S g_stPcivVencCtx = {0};
static SAMPLE_PCIV_VDEC_CTX_S astPcivVdecCtx[VDEC_MAX_CHN_NUM];
static SAMPLE_PCIV_DISP_CTX_S s_astPcivDisp[VO_MAX_DEV_NUM] =
{
    {.bValid = HI_FALSE, .VoDev =  0, .u32PicDiv = 16, .VoChnStart = 0, .VoChnEnd = 15},
    {.bValid = HI_FALSE, .VoDev =  2, .u32PicDiv = 16, .VoChnStart = 0, .VoChnEnd = 15},
    {.bValid = HI_FALSE, .VoDev =  1, .u32PicDiv = 16, .VoChnStart = 0, .VoChnEnd = 15}
};
static PCIV_BIND_OBJ_S g_stLocBind[PCIV_MAX_CHN_NUM], g_stRmtBind[PCIV_MAX_CHN_NUM];

#define STREAM_SEND_VDEC    1
#define STREAM_SAVE_FILE    0
#define PCIV_START_STREAM   1

static int test_idx = 0;
static int vdec_idx = 0;
static int Add_osd  = 0;
static HI_BOOL bExit[VDEC_MAX_CHN_NUM] = {HI_FALSE};
static HI_BOOL bQuit = HI_FALSE;


pthread_t   VdecHostThread[2*VDEC_MAX_CHN_NUM];
pthread_t   VdecThread[2*VDEC_MAX_CHN_NUM];

HI_S32 SamplePcivGetPicSize(HI_S32 s32VoDiv, SIZE_S *pstPicSize)
{
    SIZE_S stScreemSize;

    stScreemSize.u32Width  = 1920;
    stScreemSize.u32Height = 1088;

    switch (s32VoDiv)
    {
        case 1 :
            pstPicSize->u32Width  = stScreemSize.u32Width;
            pstPicSize->u32Height = stScreemSize.u32Height;
            break;
        case 4 :
            pstPicSize->u32Width  = stScreemSize.u32Width  / 2;
            pstPicSize->u32Height = stScreemSize.u32Height / 2;
            break;
        case 9 :
            pstPicSize->u32Width  = stScreemSize.u32Width  / 3;
            pstPicSize->u32Height = stScreemSize.u32Height / 3;
            break;
        case 16 :
            pstPicSize->u32Width  = stScreemSize.u32Width  / 4;
            pstPicSize->u32Height = stScreemSize.u32Height / 4;
            break;
        default:
            printf("not support this vo div %d \n", s32VoDiv);
            return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SamplePcivGetPicAttr(SIZE_S *pstPicSize, PCIV_PIC_ATTR_S *pstPicAttr)
{
    pstPicAttr->u32Width  = pstPicSize->u32Width;
    pstPicAttr->u32Height = pstPicSize->u32Height;
    pstPicAttr->u32Stride[0] = SAMPLE_PCIV_GET_STRIDE(pstPicAttr->u32Width, 16);
    pstPicAttr->u32Stride[1] = pstPicAttr->u32Stride[2] = pstPicAttr->u32Stride[0];

    pstPicAttr->u32Field      = VIDEO_FIELD_FRAME;
    pstPicAttr->enPixelFormat = SAMPLE_PIXEL_FORMAT;

    return HI_SUCCESS;
}


HI_S32 SamplePcivGetBlkSize(PCIV_PIC_ATTR_S *pstPicAttr, HI_U32 *pu32BlkSize)
{
    switch (pstPicAttr->enPixelFormat)
    {
        case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
            *pu32BlkSize = pstPicAttr->u32Stride[0]*pstPicAttr->u32Height*3/2;
            break;
        case PIXEL_FORMAT_YUV_SEMIPLANAR_422:
            *pu32BlkSize = pstPicAttr->u32Stride[0]*pstPicAttr->u32Height*2;
            break;
        case PIXEL_FORMAT_VYUY_PACKAGE_422:
            *pu32BlkSize = pstPicAttr->u32Stride[0]*pstPicAttr->u32Height;
            break;
        default:
            return -1;
    }
    
    return HI_SUCCESS;
}


HI_S32 SamplePcivSendMsgCreate(HI_S32 s32TargetId,PCIV_PCIVCMD_CREATE_S *pstMsgCreate)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S stMsg;

    stMsg.stMsgHead.u32Target  = s32TargetId;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_CREATE_PCIV;
    stMsg.stMsgHead.u32MsgLen  = sizeof(PCIV_PCIVCMD_CREATE_S);
    memcpy(stMsg.cMsgBody, pstMsgCreate, sizeof(PCIV_PCIVCMD_CREATE_S));
    
    printf("=======PCIV_SendMsg SAMPLE_PCIV_MSG_CREATE_PCIV==========\n");
    
    s32Ret = PCIV_SendMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);
    
    return HI_SUCCESS;
}


HI_S32 SamplePcivSendMsgDestroy(HI_S32 s32TargetId,PCIV_PCIVCMD_DESTROY_S *pstMsgDestroy)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S stMsg;

    stMsg.stMsgHead.u32Target  = 1;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_DESTROY_PCIV;
    stMsg.stMsgHead.u32MsgLen  = sizeof(PCIV_PCIVCMD_DESTROY_S);
    memcpy(stMsg.cMsgBody, pstMsgDestroy, sizeof(PCIV_PCIVCMD_DESTROY_S));
    
    printf("=======PCIV_SendMsg SAMPLE_PCIV_MSG_DESTROY_PCIV==========\n");
    
    s32Ret = PCIV_SendMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32TargetId, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}


/*****************************************************************************
* function : start vpss. VPSS chn with frame
*****************************************************************************/
HI_S32 SamplePciv_StartVpss(HI_S32 VpssGrp, HI_S32 VpssChn, SIZE_S *pstSize, VPSS_GRP_ATTR_S *pstVpssGrpAttr)
{
    VPSS_GRP_ATTR_S stGrpAttr = {0};
    VPSS_CHN_ATTR_S stChnAttr = {0};
    VPSS_GRP_PARAM_S stVpssParam = {0};
    HI_S32 s32Ret;

    /*** Set Vpss Grp Attr ***/

    if(NULL == pstVpssGrpAttr)
    {
        stGrpAttr.u32MaxW   = pstSize->u32Width;
        stGrpAttr.u32MaxH   = pstSize->u32Height;
        stGrpAttr.bDciEn    = HI_FALSE;
        stGrpAttr.bIeEn     = HI_FALSE;
        stGrpAttr.bNrEn     = HI_TRUE;
        stGrpAttr.bHistEn   = HI_FALSE;
        stGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
        stGrpAttr.enPixFmt  = SAMPLE_PIXEL_FORMAT;
    }
    else
    {
        memcpy(&stGrpAttr, pstVpssGrpAttr, sizeof(VPSS_GRP_ATTR_S));
    }
    
    /*** create vpss group ***/
    s32Ret = HI_MPI_VPSS_CreateGrp(VpssGrp, &stGrpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_CreateGrp failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /*** set vpss param ***/
    s32Ret = HI_MPI_VPSS_GetGrpParam(VpssGrp, &stVpssParam);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    
    stVpssParam.u32IeStrength = 0;
    s32Ret = HI_MPI_VPSS_SetGrpParam(VpssGrp, &stVpssParam);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /*** enable vpss chn, with frame ***/
    /* Set Vpss Chn attr */
    stChnAttr.bSpEn = HI_FALSE;
    stChnAttr.bBorderEn = HI_TRUE;
    stChnAttr.stBorder.u32Color = 0xff00;
    stChnAttr.stBorder.u32LeftWidth = 2;
    stChnAttr.stBorder.u32RightWidth = 2;
    stChnAttr.stBorder.u32TopWidth = 2;
    stChnAttr.stBorder.u32BottomWidth = 2;
    
    s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_SetChnAttr failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VPSS_EnableChn(VpssGrp, VpssChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_EnableChn failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }

    /*** start vpss group ***/
    s32Ret = HI_MPI_VPSS_StartGrp(VpssGrp);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_StartGrp failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}


HI_S32 SamplePciv_StopVpss(HI_S32 s32VpssGrpNum)
{
    HI_S32 i = 0;
    HI_S32 s32Ret;


    for (i = 0; i < s32VpssGrpNum; i++)
    {
        s32Ret =  HI_MPI_VPSS_StopGrp(i);
        if (HI_SUCCESS != s32Ret)
        {
            printf("stop vpss grp%d fail! s32Ret: 0x%x.\n", i, s32Ret);
            return s32Ret;
        }

        s32Ret =  HI_MPI_VPSS_DestroyGrp(i);
        if (HI_SUCCESS != s32Ret)
        {
            printf("destroy vpss grp%d fail! s32Ret: 0x%x.\n", i, s32Ret);
            return s32Ret;
        }
        
    }
    printf("destroy vpss ok!\n");

    return HI_SUCCESS;
}

/* read stream data from file, write to local buffer, then send to pci target direct at once */
HI_VOID * SamplePciv_SendVdStrmThread(HI_VOID *p)
{
    HI_S32          s32Ret, s32ReadLen;
    HI_U32          u32FrmSeq = 0;
    HI_U8           *pu8VdecBuf = NULL;
	HI_VOID         *pCreator    = NULL;
    FILE* file = NULL;
    PCIV_STREAM_HEAD_S      stHeadTmp;
    PCIV_TRANS_LOCBUF_STAT_S stLocBufSta;
    SAMPLE_PCIV_VDEC_CTX_S *pstCtx = (SAMPLE_PCIV_VDEC_CTX_S*)p;

    pCreator = pstCtx->pTransHandle;
    printf("%s -> Sender:%p, chnid:%d\n", __FUNCTION__, pCreator, pstCtx->VdecChn);

    /*open the stream file*/
    file = fopen(pstCtx->aszFileName, "r");
    if (HI_NULL == file)
    {
        printf("open file %s err\n", pstCtx->aszFileName);
        exit(-1);
    }

    pu8VdecBuf = (HI_U8*)malloc(SAMPLE_PCIV_SEND_VDEC_LEN);
    HI_ASSERT(pu8VdecBuf);

    while(pstCtx->bThreadStart)
    {
        s32ReadLen = fread(pu8VdecBuf, 1, SAMPLE_PCIV_SEND_VDEC_LEN, file);
        if (s32ReadLen <= 0)
        {
            fseek(file, 0, SEEK_SET);/*read file again*/
            continue;
        }

        /* you should insure buf len is enough */
        PCIV_Trans_QueryLocBuf(pCreator, &stLocBufSta);
        if (stLocBufSta.u32FreeLen < s32ReadLen + sizeof(PCIV_STREAM_HEAD_S))
        {
            printf("venc stream local buffer not enough, %d < %d\n",stLocBufSta.u32FreeLen,s32ReadLen);
            break;
        }

        /* fill stream header info */
        stHeadTmp.u32Magic         = PCIV_STREAM_MAGIC;
        stHeadTmp.enPayLoad        = PT_H264;
        stHeadTmp.s32ChnID         = pstCtx->VdecChn;
        stHeadTmp.u32StreamDataLen = s32ReadLen;
        stHeadTmp.u32Seq           = u32FrmSeq ++;

        if (s32ReadLen%4)
        {
            stHeadTmp.u32DMADataLen = (s32ReadLen/4 + 1)*4;
        }
        else
        {
            stHeadTmp.u32DMADataLen = s32ReadLen;
        }
        
        if (0 == pstCtx->VdecChn)
        {
            //printf("vdec chn: %d stream data len: 0x%x, DMA data len, 0x%x.####\n", pstCtx->VdecChn, 
            //stHeadTmp.u32StreamDataLen, stHeadTmp.u32DMADataLen);
        }
        
        /* write stream header */
        s32Ret = PCIV_Trans_WriteLocBuf(pCreator, (HI_U8*)&stHeadTmp, sizeof(stHeadTmp));
        HI_ASSERT((HI_SUCCESS == s32Ret));

        /* write stream data */
        s32Ret = PCIV_Trans_WriteLocBuf(pCreator, pu8VdecBuf, stHeadTmp.u32DMADataLen);
        HI_ASSERT((HI_SUCCESS == s32Ret));
        //printf("Func: %s, Line: %d, vdec chn: %d.#####################\n", __FUNCTION__, __LINE__, pstCtx->VdecChn);
        /* send local data to pci target */
        while (PCIV_Trans_SendData(pCreator) && pstCtx->bThreadStart)
        {
            //printf("Func: %s, Line: %d, vdec chn: %d.#####################\n", __FUNCTION__, __LINE__, pstCtx->VdecChn);
            usleep(10000);
        }
    }

    fclose(file);
    free(pu8VdecBuf);
    return NULL;
}

HI_S32 SamplePciv_HostInitWinVb(HI_S32 s32RmtChip, HI_U32 u32BlkCount)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_WINVB_S stWinVbArgs;

    stWinVbArgs.stPciWinVbCfg.u32PoolCount   = 1;
    stWinVbArgs.stPciWinVbCfg.u32BlkCount[0] = u32BlkCount;
    stWinVbArgs.stPciWinVbCfg.u32BlkSize[0]  = SAMPLE_PCIV_VDEC_STREAM_BUF_LEN;

    memcpy(stMsg.cMsgBody, &stWinVbArgs, sizeof(stWinVbArgs));
    stMsg.stMsgHead.u32Target  = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_INIT_WIN_VB;
    stMsg.stMsgHead.u32MsgLen  = sizeof(stWinVbArgs);
    
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_INIT_WIN_VB==========\n");
    
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStartVdecChn(HI_S32 s32RmtChip, HI_U32 u32VdecChnNum, HI_BOOL bReadStream)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_VDEC_S stVdecArgs;

    stVdecArgs.u32VdecChnNum = u32VdecChnNum;
    stVdecArgs.enPicSize     = SAMPLE_PCIV_VDEC_SIZE;
    stVdecArgs.bReadStream   = bReadStream;
    
    memcpy(stMsg.cMsgBody, &stVdecArgs, sizeof(stVdecArgs));
    stMsg.stMsgHead.u32Target  = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_START_VDEC;
    stMsg.stMsgHead.u32MsgLen  = sizeof(stVdecArgs);
    
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_START_VDEC==========\n");
    
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStopVdecChn(HI_S32 s32RmtChip, HI_U32 u32VdecChnNum, HI_BOOL bReadStream)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_VDEC_S stVdecArgs={0};

    stVdecArgs.u32VdecChnNum = u32VdecChnNum;
    stVdecArgs.bReadStream   = bReadStream;
    memcpy(stMsg.cMsgBody, &stVdecArgs, sizeof(stVdecArgs));
    stMsg.stMsgHead.u32Target  = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_STOP_VDEC;
    stMsg.stMsgHead.u32MsgLen  = sizeof(PCIV_PCIVCMD_MALLOC_S);
    
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_STOP_VDEC==========\n");
    
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}


HI_S32 SamplePciv_HostStartVdecStream(HI_S32 s32RmtChip, VDEC_CHN VdecChn)
{
    HI_S32 s32Ret;
    //HI_S32 s32MsgPortWirte, s32MsgPortRead;
    SAMPLE_PCIV_MSG_S  stMsg;
    PCIV_PCIVCMD_MALLOC_S stMallocCmd;
    PCIV_PCIVCMD_MALLOC_S *pstMallocEcho;
    PCIV_TRANS_ATTR_S stInitPara;
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = &astPcivVdecCtx[VdecChn];

    /* send msg to slave(PCI Device), for malloc Dest Addr of stream buffer */
    /* PCI transfer data from Host to Device, Dest Addr must in PCI WINDOW of PCI Device */
    stMallocCmd.u32BlkCount = 1;
    stMallocCmd.u32BlkSize = SAMPLE_PCIV_VDEC_STREAM_BUF_LEN;
    memcpy(stMsg.cMsgBody, &stMallocCmd, sizeof(PCIV_PCIVCMD_MALLOC_S));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_MALLOC;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_PCIVCMD_MALLOC_S);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_MALLOC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    /* read msg, phyaddr will return */
    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    pstMallocEcho = (PCIV_PCIVCMD_MALLOC_S *)stMsg.cMsgBody;
    printf("func:%s, line: %d, phyaddr:0x%x\n", __FUNCTION__, __LINE__, pstMallocEcho->u32PhyAddr[0]);

    /* init vdec stream sender in local chip */
    stInitPara.s32RmtChip       = s32RmtChip;
    stInitPara.s32ChnId         = VdecChn;
    stInitPara.u32BufSize       = pstMallocEcho->u32BlkSize;
    //stInitPara.u32PhyAddr       = pstMallocEcho->u32PhyAddr[0] + g_u32PfWinBase[s32RmtChip];
    stInitPara.u32PhyAddr       = pstMallocEcho->u32PhyAddr[0] + PCIE_BAR0_ADDRESS;
    stInitPara.s32MsgPortWrite  = pstVdecCtx->s32MsgPortWrite;
    stInitPara.s32MsgPortRead   = pstVdecCtx->s32MsgPortRead;
    printf("func:%s, line: %d, phyaddr:0x%x\n", __FUNCTION__, __LINE__, stInitPara.u32PhyAddr);
    s32Ret = PCIV_Trans_InitSender(&stInitPara, &pstVdecCtx->pTransHandle);
    HI_ASSERT(HI_SUCCESS == s32Ret);

    /* send msg to slave chip to init vdec stream transfer */
    stInitPara.s32RmtChip   = 0;
    stInitPara.u32PhyAddr   = pstMallocEcho->u32PhyAddr[0];
    memcpy(stMsg.cMsgBody, &stInitPara, sizeof(stInitPara));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_INIT_STREAM_VDEC;
    stMsg.stMsgHead.u32MsgLen = sizeof(stInitPara);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_INIT_STREAM_VDEC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    /* after target inited stream receiver, local start sending stream thread */
    sprintf(pstVdecCtx->aszFileName, "%s", SAMPLE_1080P_H264_PATH);
    pstVdecCtx->VdecChn = VdecChn;
    pstVdecCtx->bThreadStart = HI_TRUE;
    s32Ret = pthread_create(&pstVdecCtx->pid, NULL, SamplePciv_SendVdStrmThread, pstVdecCtx);

    printf("init vdec:%d stream transfer ok ==================\n", VdecChn);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStopVdecStream(HI_S32 s32RmtChip, VDEC_CHN VdecChn)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    PCIV_TRANS_ATTR_S stInitPara;
    SAMPLE_PCIV_VDEC_CTX_S *pstVdecCtx = &astPcivVdecCtx[VdecChn];

    /* send msg to slave chip to exit vdec stream transfer */
    stInitPara.s32RmtChip   = 0;
    stInitPara.s32ChnId     = VdecChn;
    memcpy(stMsg.cMsgBody, &stInitPara, sizeof(stInitPara));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_EXIT_STREAM_VDEC;
    stMsg.stMsgHead.u32MsgLen = sizeof(stInitPara);
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_EXIT_STREAM_VDEC==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);
    while( PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    /* exit local sending pthread */
    if (HI_TRUE == pstVdecCtx->bThreadStart)
    {
        pstVdecCtx->bThreadStart = HI_FALSE;
        pthread_join(pstVdecCtx->pid, 0);
    }

    /* exit local sender */
    s32Ret = PCIV_Trans_DeInitSender(pstVdecCtx->pTransHandle);
    HI_ASSERT(HI_SUCCESS == s32Ret);

    printf("exit vdec:%d stream transfer ok ==================\n", VdecChn);
    return HI_SUCCESS;
}

HI_S32 SamplePciv_StartVdecByChip(HI_S32 s32RmtChipId, HI_U32 u32VdecCnt, HI_BOOL bReadStream)
{
    HI_S32 s32Ret, j;
    s32Ret = SamplePciv_HostInitWinVb(s32RmtChipId, u32VdecCnt);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    s32Ret = SamplePciv_HostStartVdecChn(s32RmtChipId, u32VdecCnt, bReadStream);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }
    
    for (j=0; j<u32VdecCnt; j++)
    {
        s32Ret = SamplePciv_HostStartVdecStream(s32RmtChipId, j);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }
    
    return HI_SUCCESS;
}
HI_VOID SamplePciv_StopVdecByChip(HI_S32 s32RmtChipId, HI_U32 u32VdecCnt, HI_BOOL bReadStream)
{
    HI_S32 j;
    for (j=0; j<u32VdecCnt; j++)
    {
        SamplePciv_HostStopVdecStream(s32RmtChipId, j);
    }
    SamplePciv_HostStopVdecChn(s32RmtChipId, u32VdecCnt, bReadStream);
}

HI_S32 SamplePciv_VpssCropPic(HI_U32 u32VpssCnt)
{
    HI_S32 i;
    HI_S32 s32Ret;
    HI_S32 s32Div;
    VPSS_CROP_INFO_S stCropInfo;

    s32Div = sqrt(u32VpssCnt);

    printf("Func: %s, u32VpssCnt: %d, s32Div: %d.\n", __FUNCTION__, u32VpssCnt, s32Div);

    stCropInfo.bEnable              = HI_TRUE;
    stCropInfo.enCropCoordinate     = VPSS_CROP_ABS_COOR;
    stCropInfo.stCropRect.u32Width  = 960;
    stCropInfo.stCropRect.u32Height = 544;
    stCropInfo.stCropRect.s32X      = 0;
    stCropInfo.stCropRect.s32Y      = 0;
        
    for (i = 0; i < u32VpssCnt; i++)
    {
        
        stCropInfo.stCropRect.s32X  = stCropInfo.stCropRect.u32Width * (i % s32Div);
        stCropInfo.stCropRect.s32Y  = stCropInfo.stCropRect.u32Height * (i / s32Div);
        s32Ret = HI_MPI_VPSS_SetGrpCrop(i, &stCropInfo);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SamplePcivStartSlaveVirtualVo(HI_S32 s32RmtChip, VO_DEV VoDev, HI_U32 u32VoChnNum)
{
    HI_S32               s32Ret;
    SAMPLE_PCIV_MSG_S    stMsg;
    SAMPLE_PCIV_MSG_VO_S stVoArgs;

    stVoArgs.VirtualVo       = VoDev;
    stVoArgs.stBInd.enModId  = HI_ID_VDEC;
    stVoArgs.stBInd.s32DevId = 0;
    stVoArgs.stBInd.s32ChnId = 0;
    stVoArgs.enPicSize       = PIC_HD1080;
    stVoArgs.enIntfSync      = VO_OUTPUT_1080P30;
    stVoArgs.s32VoChnNum     = u32VoChnNum;
    
    memcpy(stMsg.cMsgBody, &stVoArgs, sizeof(stVoArgs));
    stMsg.stMsgHead.u32Target  = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_START_VO;
    stMsg.stMsgHead.u32MsgLen  = sizeof(stVoArgs);
    
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_START_VO==========\n");
    
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}


HI_S32 SamplePcivStopSlaveVirtualVo(HI_S32 s32RmtChip, VO_DEV VoDev, HI_U32 u32VoChnNum)
{
    HI_S32            s32Ret;
    SAMPLE_PCIV_MSG_S stMsg;
    SAMPLE_PCIV_MSG_VO_S stVoArgs;

    stVoArgs.VirtualVo       = VoDev;
    stVoArgs.stBInd.enModId  = HI_ID_VDEC;
    stVoArgs.stBInd.s32DevId = 0;
    stVoArgs.stBInd.s32ChnId = 0;
    stVoArgs.enPicSize       = PIC_HD1080;
    stVoArgs.s32VoChnNum     = u32VoChnNum;

    
    memcpy(stMsg.cMsgBody, &stVoArgs, sizeof(stVoArgs));
    stMsg.stMsgHead.u32Target  = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_STOP_VO;
    stMsg.stMsgHead.u32MsgLen  = sizeof(stVoArgs);
    
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_STOP_VO==========\n");
    
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}


HI_S32 SamplePciv_HostStartVpssChn(HI_S32 s32RmtChip, VPSS_GRP VpssGrp, HI_BOOL bBindVdec)
{
    HI_S32 s32Ret;        
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_VPSS_S stVpssArgs = {0};

    stVpssArgs.vpssGrp   = VpssGrp;
    stVpssArgs.enPicSize = PIC_HD1080;
    
    stVpssArgs.stBInd.enModId              = HI_ID_VDEC;
    stVpssArgs.stBInd.s32DevId             = 0;
    stVpssArgs.stBInd.s32ChnId             = VpssGrp;
    stVpssArgs.bBindVdec                   = bBindVdec;
    stVpssArgs.vpssChnStart[VPSS_PRE0_CHN] = HI_TRUE;

    memcpy(stMsg.cMsgBody, &stVpssArgs, sizeof(stVpssArgs));
    stMsg.stMsgHead.u32Target  = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_START_VPSS;
    stMsg.stMsgHead.u32MsgLen  = sizeof(stVpssArgs);
    
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_START_VPSS==========\n");
    
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStopVpssChn(HI_S32 s32RmtChip, VPSS_GRP VpssGrp, HI_BOOL bBindVdec)
{
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S  stMsg;
    SAMPLE_PCIV_MSG_VPSS_S stVpssArgs = {0};

    stVpssArgs.vpssGrp = VpssGrp;
    stVpssArgs.enPicSize = PIC_D1;
    stVpssArgs.stBInd.enModId = HI_ID_VDEC;
    stVpssArgs.stBInd.s32DevId = 0;
    stVpssArgs.stBInd.s32ChnId = 0;
    stVpssArgs.bBindVdec       = bBindVdec;
    stVpssArgs.vpssChnStart[VPSS_BSTR_CHN] = HI_TRUE;
    stVpssArgs.vpssChnStart[VPSS_PRE0_CHN] = HI_TRUE;
    
    memcpy(stMsg.cMsgBody, &stVpssArgs, sizeof(stVpssArgs));
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_STOP_VPSS;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_PCIVCMD_MALLOC_S);
    printf("=======PCIV_SendMsg SAMPLE_PCIV_MSG_STOP_VPSS==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);

    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_S32 SamplePciv_StartVpssByChip(HI_S32 s32RmtChipId, HI_U32 u32VpssCnt, HI_BOOL bBindVdec)
{
    HI_S32 s32Ret, j;

    for (j=0; j<u32VpssCnt; j++)
    {
        s32Ret  = SamplePciv_HostStartVpssChn(s32RmtChipId, j, bBindVdec);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }
    
    return HI_SUCCESS;
}
HI_VOID SamplePciv_StopVpssByChip(HI_S32 s32RmtChipId, HI_U32 u32VpssCnt, HI_BOOL bBindVdec)
{
    HI_S32 j;

    for (j=0; j<u32VpssCnt; j++)
    {
        SamplePciv_HostStopVpssChn(s32RmtChipId, j, bBindVdec);
    }
}

HI_S32 SamplePciv_GetDefVoAttr(VO_DEV VoDev, VO_INTF_SYNC_E enIntfSync, VO_PUB_ATTR_S *pstPubAttr,
    VO_VIDEO_LAYER_ATTR_S *pstLayerAttr, HI_S32 s32SquareSort, VO_CHN_ATTR_S *astChnAttr)
{
    VO_INTF_TYPE_E enIntfType;
    HI_U32 u32Frmt, u32Width, u32Height, j;

    switch (VoDev)
    {
        default:
        case 0: enIntfType = VO_INTF_VGA;  break;
        case 1: enIntfType = VO_INTF_HDMI; break;
        case 2: enIntfType = VO_INTF_CVBS; break;
        case 3: enIntfType = VO_INTF_CVBS; break;
    }

    printf("Func: %s, line: %d.....\n", __FUNCTION__, __LINE__);
    switch (enIntfSync)
    {
        case VO_OUTPUT_PAL      :  u32Width = 720;  u32Height = 576;  u32Frmt = 25; break;
        case VO_OUTPUT_NTSC     :  u32Width = 720;  u32Height = 480;  u32Frmt = 30; break;
        case VO_OUTPUT_1080P24  :  u32Width = 1920; u32Height = 1080; u32Frmt = 24; break;
        case VO_OUTPUT_1080P25  :  u32Width = 1920; u32Height = 1080; u32Frmt = 25; break;
        case VO_OUTPUT_1080P30  :  u32Width = 1920; u32Height = 1080; u32Frmt = 30; break;
        case VO_OUTPUT_720P50   :  u32Width = 1280; u32Height = 720;  u32Frmt = 50; break;
        case VO_OUTPUT_720P60   :  u32Width = 1280; u32Height = 720;  u32Frmt = 60; break;
        case VO_OUTPUT_1080I50  :  u32Width = 1920; u32Height = 1080; u32Frmt = 50; break;
        case VO_OUTPUT_1080I60  :  u32Width = 1920; u32Height = 1080; u32Frmt = 60; break;
        case VO_OUTPUT_1080P50  :  u32Width = 1920; u32Height = 1080; u32Frmt = 50; break;
        case VO_OUTPUT_1080P60  :  u32Width = 1920; u32Height = 1080; u32Frmt = 60; break;
        case VO_OUTPUT_576P50   :  u32Width = 720;  u32Height = 576;  u32Frmt = 50; break;
        case VO_OUTPUT_480P60   :  u32Width = 720;  u32Height = 480;  u32Frmt = 60; break;
        case VO_OUTPUT_800x600_60: u32Width = 800;  u32Height = 600;  u32Frmt = 60; break;
        case VO_OUTPUT_1024x768_60:u32Width = 1024; u32Height = 768;  u32Frmt = 60; break;
        case VO_OUTPUT_1280x1024_60:u32Width =1280; u32Height = 1024; u32Frmt = 60; break;
        case VO_OUTPUT_1366x768_60:u32Width = 1366; u32Height = 768;  u32Frmt = 60; break;
        case VO_OUTPUT_1440x900_60:u32Width = 1440; u32Height = 900;  u32Frmt = 60; break;
        case VO_OUTPUT_1280x800_60:u32Width = 1280; u32Height = 800;  u32Frmt = 60; break;

        default: return HI_FAILURE;
    }
    printf("Func: %s, line: %d.....\n", __FUNCTION__, __LINE__);
    if (NULL != pstPubAttr)
    {
        pstPubAttr->enIntfSync = enIntfSync;
        pstPubAttr->u32BgColor = 0; 
        pstPubAttr->enIntfType = enIntfType;
    }
    printf("Func: %s, line: %d.....\n", __FUNCTION__, __LINE__);
    if (NULL != pstLayerAttr)
    {
        pstLayerAttr->stDispRect.s32X       = 0;
        pstLayerAttr->stDispRect.s32Y       = 0;
        pstLayerAttr->stDispRect.u32Width   = u32Width;
        pstLayerAttr->stDispRect.u32Height  = u32Height;
        pstLayerAttr->stImageSize.u32Width  = u32Width;
        pstLayerAttr->stImageSize.u32Height = u32Height;
        pstLayerAttr->u32DispFrmRt          = 25;
        pstLayerAttr->enPixFormat           = SAMPLE_PIXEL_FORMAT;
        pstLayerAttr->bDoubleFrame          = HI_FALSE;
        pstLayerAttr->bClusterMode          = HI_FALSE;
    }
    printf("Func: %s, line: %d.....\n", __FUNCTION__, __LINE__);
    if (NULL != astChnAttr)
    {
        for (j=0; j<(s32SquareSort * s32SquareSort); j++)
        {
            astChnAttr[j].stRect.s32X       = ALIGN_BACK((u32Width/s32SquareSort) * (j%s32SquareSort), 4);
            astChnAttr[j].stRect.s32Y       = ALIGN_BACK((u32Height/s32SquareSort) * (j/s32SquareSort), 4);
            astChnAttr[j].stRect.u32Width   = ALIGN_BACK(u32Width/s32SquareSort, 4);
            astChnAttr[j].stRect.u32Height  = ALIGN_BACK(u32Height/s32SquareSort, 4);
            astChnAttr[j].u32Priority       = 0;
            astChnAttr[j].bDeflicker        = HI_FALSE;
        }
    }
    printf("Func: %s, line: %d.....\n", __FUNCTION__, __LINE__);
    return HI_SUCCESS;
}
HI_S32 SamplePciv_StartVO(VO_DEV VoDev, VO_PUB_ATTR_S *pstPubAttr,
    VO_VIDEO_LAYER_ATTR_S *astLayerAttr, VO_CHN_ATTR_S *astChnAttr, HI_S32 s32ChnNum)
{
    HI_S32 i, s32Ret;

    s32Ret = HI_MPI_VO_DisableVideoLayer(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_Disable(VoDev);
    PCIV_CHECK_ERR(s32Ret);    

    s32Ret = HI_MPI_VO_SetPubAttr(VoDev, pstPubAttr);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_Enable(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_SetVideoLayerAttr(VoDev, &astLayerAttr[0]);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_EnableVideoLayer(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    for (i=0; i<s32ChnNum; i++)
    {
        s32Ret = HI_MPI_VO_SetChnAttr(VoDev, i, &astChnAttr[i]);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VO_EnableChn(VoDev, i);
        PCIV_CHECK_ERR(s32Ret);
    }

    return 0;
}
HI_S32 SamplePciv_StopVO(VO_DEV VoDev, HI_S32 s32ChnNum)
{
    HI_S32 i, s32Ret;

    for (i=0; i<s32ChnNum; i++)
    {
        s32Ret = HI_MPI_VO_DisableChn(VoDev, i);
        PCIV_CHECK_ERR(s32Ret);
    }    

    s32Ret = HI_MPI_VO_DisableVideoLayer(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = HI_MPI_VO_Disable(VoDev);
    PCIV_CHECK_ERR(s32Ret);

    return 0;
}
HI_S32 SamplePciv_HostCreateVdec(HI_U32 s32ChnNum,PIC_SIZE_E enPicSize, VDEC_CHN_ATTR_S *pstVdecChnAttr)
{
    HI_S32 i;
    HI_S32 s32Ret;

    for (i=0; i<s32ChnNum; i++)
    {
        s32Ret = HI_MPI_VDEC_CreateChn(i, pstVdecChnAttr);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VDEC_StartRecvStream(i);
        PCIV_CHECK_ERR(s32Ret);
    }

    return HI_SUCCESS;
}
HI_S32 SamplePciv_HostDestroyVdec(HI_U32 s32ChnNum)
{
    HI_S32 i;
    HI_S32 s32Ret;
    
    for (i=0; i<s32ChnNum; i++)
    {
        s32Ret = HI_MPI_VDEC_StopRecvStream(i);
        PCIV_CHECK_ERR(s32Ret);

        s32Ret = HI_MPI_VDEC_DestroyChn(i);
        PCIV_CHECK_ERR(s32Ret);
    }
    
    return HI_SUCCESS;
}

HI_S32 SamplePciv_HostStartPciv(PCIV_CHN PcivChn, PCIV_REMOTE_OBJ_S *pstRemoteObj,
        PCIV_BIND_OBJ_S *pstLocBind, PCIV_BIND_OBJ_S *pstRmtBind, PCIV_PIC_ATTR_S *pstPicAttr)
{
    HI_S32                s32Ret;
    PCIV_ATTR_S           stPcivAttr;
    PCIV_PCIVCMD_CREATE_S stMsgCreate;
    MPP_CHN_S             stSrcChn, stDestChn;
    
    stPcivAttr.stRemoteObj.s32ChipId = pstRemoteObj->s32ChipId;
    stPcivAttr.stRemoteObj.pcivChn = pstRemoteObj->pcivChn;

    /* memcpy pciv pic attr */
    memcpy(&stPcivAttr.stPicAttr, pstPicAttr, sizeof(PCIV_PIC_ATTR_S));

    /* 1) config pic buffer info, count/size/addr */
    stPcivAttr.u32Count = 4;
    SamplePcivGetBlkSize(&stPcivAttr.stPicAttr, &stPcivAttr.u32BlkSize);
    s32Ret = HI_MPI_PCIV_MallocChnBuffer(PcivChn, stPcivAttr.u32BlkSize, stPcivAttr.u32Count, stPcivAttr.u32PhyAddr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv malloc err, size:%d, count:%d\n", stPcivAttr.u32BlkSize, stPcivAttr.u32Count);
        return s32Ret;
    }

    /* 2) create pciv chn */
    s32Ret = HI_MPI_PCIV_Create(PcivChn, &stPcivAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv chn %d create failed \n", PcivChn);
        return s32Ret;
    }

    /* 3) pciv chn bind vo chn (for display pic in host)*/
    stSrcChn.enModId  = HI_ID_PCIV;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = PcivChn;
    switch (pstLocBind->enType)
    {
        case PCIV_BIND_VO:
            stDestChn.enModId  = HI_ID_VOU;
            stDestChn.s32DevId = pstLocBind->unAttachObj.voDevice.voDev;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.voDevice.voChn;
            break;
        case PCIV_BIND_VPSS:
            stDestChn.enModId  = HI_ID_VPSS;
            stDestChn.s32DevId = pstLocBind->unAttachObj.vpssDevice.vpssGrp;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.vpssDevice.vpssChn;
            break;
        default:
            printf("pstLocBind->enType = %d\n",pstLocBind->enType);
            HI_ASSERT(0);
    }
    
    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv chn %d bind err. s32Ret=%#x\n", PcivChn,s32Ret);
        printf("src mod:%d dev:%d chn:%d dest mod:%d dev:%d chn:%d\n",
            stSrcChn.enModId,stSrcChn.s32DevId,stSrcChn.s32ChnId,
            stDestChn.enModId,stDestChn.s32DevId,stDestChn.s32ChnId);
        return s32Ret;
    }

    /* 4) start pciv chn (now vo will display pic from slave chip) */
    s32Ret = HI_MPI_PCIV_Start(PcivChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv chn %d start err \n", PcivChn);
        return s32Ret;
    }

    printf("pciv chn%d start ok, remote(%d,%d), bindvo(%d,%d); then send msg to slave chip !\n",
        PcivChn, stPcivAttr.stRemoteObj.s32ChipId,stPcivAttr.stRemoteObj.pcivChn,
        pstLocBind->unAttachObj.voDevice.voDev, pstLocBind->unAttachObj.voDevice.voChn);

    /* 5) send msg to slave chip to start picv chn ========================================*/

    stMsgCreate.pcivChn = pstRemoteObj->pcivChn;
    memcpy(&stMsgCreate.stDevAttr, &stPcivAttr, sizeof(stPcivAttr));
    /* reconfig remote obj for slave device */
    stMsgCreate.stDevAttr.stRemoteObj.s32ChipId = 0;
    stMsgCreate.stDevAttr.stRemoteObj.pcivChn   = PcivChn;
    stMsgCreate.bAddOsd                         = Add_osd;
    /* bind object of remote dev */
    memcpy(&stMsgCreate.stBindObj[0], pstRmtBind, sizeof(PCIV_BIND_OBJ_S));

    /* send msg */
    s32Ret = SamplePcivSendMsgCreate(pstRemoteObj->s32ChipId, &stMsgCreate);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }
    
    printf("send msg to slave chip to start pciv chn %d ok! \n\n", PcivChn);
    
    return HI_SUCCESS;
    
}

HI_S32 SamplePcivStartHostVpss(HI_U32 u32VpssCnt, HI_U32 s32VpssChn, VPSS_GRP_ATTR_S *pstVpssGrpAttr)
{
    HI_S32 i;
    HI_S32 s32Ret;
    SIZE_S stSize;
    /* 主片创建VPSS group */
    if (NULL == pstVpssGrpAttr)
    {
        stSize.u32Width  = 1920;
        stSize.u32Height = 1088;

        pstVpssGrpAttr->bDciEn    = HI_FALSE;
        pstVpssGrpAttr->bHistEn   = HI_FALSE;
        pstVpssGrpAttr->bIeEn     = HI_FALSE;
        pstVpssGrpAttr->bNrEn     = HI_TRUE;
        pstVpssGrpAttr->enDieMode = VPSS_DIE_MODE_NODIE;
        pstVpssGrpAttr->enPixFmt  = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pstVpssGrpAttr->u32MaxW   = stSize.u32Width;
        pstVpssGrpAttr->u32MaxH   = stSize.u32Height;
    }

    for (i = 0; i < u32VpssCnt; i++)
    {
        s32Ret = SamplePciv_StartVpss(i, s32VpssChn, &stSize, pstVpssGrpAttr);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }
    
    return HI_SUCCESS;
}



HI_S32 SamplePcivStopHostVpss(HI_U32 u32VpssCnt)
{
    HI_S32 s32Ret;
    
    s32Ret = SamplePciv_StopVpss(u32VpssCnt);
    if (s32Ret != HI_SUCCESS)
    {
        return s32Ret;
    }

    return HI_SUCCESS;
}


HI_S32 SamplePciv_HostStopPciv(PCIV_CHN PcivChn, PCIV_BIND_OBJ_S *pstLocBind, PCIV_BIND_OBJ_S *pstRmtBind)
{
    HI_S32 s32Ret;
    PCIV_ATTR_S  stPciAttr;
    PCIV_PCIVCMD_DESTROY_S stMsgDestroy;
    MPP_CHN_S stSrcChn, stDestChn;

    /* 1, stop */
    s32Ret = HI_MPI_PCIV_Stop(PcivChn);
    PCIV_CHECK_ERR(s32Ret);

    /* 2, unbind */
    stSrcChn.enModId  = HI_ID_PCIV;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = PcivChn;
    switch (pstLocBind->enType)
    {
        case PCIV_BIND_VO:
            stDestChn.enModId  = HI_ID_VOU;
            stDestChn.s32DevId = pstLocBind->unAttachObj.voDevice.voDev;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.voDevice.voChn;
            break;
        case PCIV_BIND_VPSS:
            stDestChn.enModId  = HI_ID_VPSS;
            stDestChn.s32DevId = 0;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.vpssDevice.vpssGrp;
            stDestChn.s32ChnId = pstLocBind->unAttachObj.vpssDevice.vpssChn;
            break;
        default:
            printf("pstLocBind->enType = %d\n",pstLocBind->enType);
            HI_ASSERT(0);
    }

    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("pciv chn %d bind err. s32Ret=%#x\n", PcivChn,s32Ret);
        printf("src mod:%d dev:%d chn:%d dest mod:%d dev:%d chn:%d\n",
            stSrcChn.enModId,stSrcChn.s32DevId,stSrcChn.s32ChnId,
            stDestChn.enModId,stDestChn.s32DevId,stDestChn.s32ChnId);
        return s32Ret;
    }

    /* 3, free */
    s32Ret = HI_MPI_PCIV_GetAttr(PcivChn, &stPciAttr);
    s32Ret = HI_MPI_PCIV_FreeChnBuffer(PcivChn, stPciAttr.u32Count);
    PCIV_CHECK_ERR(s32Ret);

    /* 4, destroy */
    s32Ret = HI_MPI_PCIV_Destroy(PcivChn);
    PCIV_CHECK_ERR(s32Ret);

    printf("start send msg to slave chip to destroy pciv chn %d\n", PcivChn);
    stMsgDestroy.pcivChn = PcivChn;
	stMsgDestroy.bAddOsd = Add_osd;
    memcpy(&stMsgDestroy.stBindObj[0], pstRmtBind, sizeof(PCIV_BIND_OBJ_S));
    s32Ret = SamplePcivSendMsgDestroy(stPciAttr.stRemoteObj.s32ChipId, &stMsgDestroy);
    PCIV_CHECK_ERR(s32Ret);
    printf("destroy pciv chn %d ok \n\n", PcivChn);

    return HI_SUCCESS;
}

/*
 * Start all pciv chn for one vo dev,
 * @u32RmtIdx: 对端PCI设备数组下标，用于计算PCIV通道号
 * @s32RmtChipId: 对端PCI设备号
 * @u32DispIdx: 本地输出显示s_astPcivDisp的序号，用于计算PCIV通道号
 * @pstDispCtx: 本地输出显示上下文，记录了指定VO设备中与PCIV绑定的通道范围，画面分割等
 */
HI_S32 SamplePciv_StartPcivByVo(HI_U32 u32RmtIdx, HI_S32 s32RmtChipId,
        HI_U32 u32DispIdx, SAMPLE_PCIV_DISP_CTX_S *pstDispCtx)
{
    HI_S32 s32Ret, j, s32ChnCnt;
    PCIV_CHN RmtPcivChn, LocPcivChn;
    SIZE_S stPicSize;
    PCIV_REMOTE_OBJ_S stRemoteObj;
    PCIV_PIC_ATTR_S stPicAttr;

    s32ChnCnt = pstDispCtx->VoChnEnd - pstDispCtx->VoChnStart + 1;
    
    switch(s32ChnCnt)
    {
        case 76:
        case 100:
        case 114: 
        case 132:
        {
            s32ChnCnt = 16;
            break;
        }
        case 174:
        {
            s32ChnCnt = 9;
            break;
        }
        case 200:
        case 242:    
        {
            s32ChnCnt = 4;
            break;
        }
        case 300:
        case 320:
        case 640:    
        {
            s32ChnCnt = 1;
            break;
        }
        default:
            break;
        
    }
    
    printf("s32ChnCnt is %d.....\n", s32ChnCnt);
    HI_ASSERT(s32ChnCnt >= 0 && s32ChnCnt <= SAMPLE_PCIV_CNT_PER_VO);

    for (j=0; j<s32ChnCnt; j++)
    {
        /* 1) local pciv chn and remote pciv chn */
        LocPcivChn = j + u32DispIdx*SAMPLE_PCIV_CNT_PER_VO + u32RmtIdx*SAMPLE_PCIV_CNT_PER_DEV;
        RmtPcivChn = j + u32DispIdx*SAMPLE_PCIV_CNT_PER_VO;
        HI_ASSERT(LocPcivChn < PCIV_MAX_CHN_NUM);

        /* 2) remote dev and chn */
        stRemoteObj.s32ChipId   = s32RmtChipId;
        stRemoteObj.pcivChn     = RmtPcivChn;

        /* 3) local bind object */
        /* 4) remote bind object */
        if (0 == test_idx)
        {
            g_stRmtBind[j].enType = PCIV_BIND_VPSS;
            g_stRmtBind[j].unAttachObj.vpssDevice.vpssGrp = j;
            g_stRmtBind[j].unAttachObj.vpssDevice.vpssChn = VPSS_PRE0_CHN;

            g_stLocBind[j].enType = PCIV_BIND_VPSS;
            g_stLocBind[j].unAttachObj.vpssDevice.vpssGrp = j;
            g_stLocBind[j].unAttachObj.vpssDevice.vpssChn = VPSS_PRE0_CHN;
        }
        else if (1 == test_idx)
        {
            g_stRmtBind[j].enType = PCIV_BIND_VO;
            g_stRmtBind[j].unAttachObj.voDevice.voDev = pstDispCtx->VoDev;
            g_stRmtBind[j].unAttachObj.voDevice.voChn = pstDispCtx->VoChnStart + j;

            g_stLocBind[j].enType = PCIV_BIND_VPSS;
            g_stLocBind[j].unAttachObj.vpssDevice.vpssGrp = j;
            g_stLocBind[j].unAttachObj.vpssDevice.vpssChn = VPSS_PRE0_CHN;
        }
        else if (2 == test_idx)
        {
            g_stRmtBind[j].enType = PCIV_BIND_VDEC;
            g_stRmtBind[j].unAttachObj.vdecDevice.vdecChn = j;

            g_stLocBind[j].enType = PCIV_BIND_VPSS;
            g_stLocBind[j].unAttachObj.voDevice.voDev = j;
            g_stLocBind[j].unAttachObj.voDevice.voChn = VPSS_PRE0_CHN;
        }
        else
        {
            printf("test_idx:%d error.\n",test_idx);
            return HI_FAILURE;
        }

        /* 5) pciv pic attr */
        SamplePcivGetPicSize(pstDispCtx->u32PicDiv, &stPicSize);
        SamplePcivGetPicAttr(&stPicSize, &stPicAttr);

        /* start local and remote pciv chn */
        s32Ret = SamplePciv_HostStartPciv(LocPcivChn, &stRemoteObj, &g_stLocBind[j], &g_stRmtBind[j], &stPicAttr);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_StopPcivByVo(HI_U32 u32RmtIdx, HI_S32 s32RmtChipId,
        HI_U32 u32DispIdx, SAMPLE_PCIV_DISP_CTX_S *pstDispCtx)
{
    HI_S32 j, s32ChnCnt;
    PCIV_CHN LocPcivChn;

    s32ChnCnt = pstDispCtx->VoChnEnd - pstDispCtx->VoChnStart + 1;
    
    switch(s32ChnCnt)
    {
        case 76:
        case 100:
        case 114: 
        case 132:
        {
            s32ChnCnt = 16;
            break;
        }
        case 174:
        {
            s32ChnCnt = 9;
            break;
        }
        case 200:
        case 242:    
        {
            s32ChnCnt = 4;
            break;
        }
        case 300:
        case 320:
        case 640:    
        {
            s32ChnCnt = 1;
            break;
        }
        default:
            break;
        
    }
    
    printf("s32ChnCnt is %d.....\n", s32ChnCnt);
    HI_ASSERT(s32ChnCnt >= 0 && s32ChnCnt <= SAMPLE_PCIV_CNT_PER_VO);

    for (j=0; j<s32ChnCnt; j++)
    {
        LocPcivChn = j + u32DispIdx*SAMPLE_PCIV_CNT_PER_VO + u32RmtIdx*SAMPLE_PCIV_CNT_PER_DEV;
        HI_ASSERT(LocPcivChn < PCIV_MAX_CHN_NUM);

        SamplePciv_HostStopPciv(LocPcivChn, &g_stLocBind[j], &g_stRmtBind[j]);
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_StartPcivByChip(HI_U32 u32RmtIdx, HI_S32 s32RmtChipId)
{
    HI_S32 s32Ret, i;

    for (i=0; i<VO_MAX_DEV_NUM; i++)
    {
        if (s_astPcivDisp[i].bValid != HI_TRUE)
        {
            continue;
        }
        s32Ret = SamplePciv_StartPcivByVo(u32RmtIdx,  s32RmtChipId, i, &s_astPcivDisp[i]);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SamplePciv_StopPcivByChip(HI_S32 s32RmtChipId, HI_U32 u32RmtIdx)
{
    HI_S32 s32Ret, i;

    for (i=0; i<VO_MAX_DEV_NUM; i++)
    {
        if (s_astPcivDisp[i].bValid != HI_TRUE)
        {
            continue;
        }
        s32Ret = SamplePciv_StopPcivByVo(u32RmtIdx,  s32RmtChipId, i, &s_astPcivDisp[i]);
        if (s32Ret != HI_SUCCESS)
        {
            return s32Ret;
        }
    }

    return HI_SUCCESS;
}


HI_S32 SamplePcivInitMsgPort(HI_S32 s32RmtChip)
{
    HI_S32 i, s32Ret;
    HI_S32 s32MsgPort = PCIV_MSG_BASE_PORT+1;

    SAMPLE_PCIV_MSG_S stMsg;
    PCIV_MSGPORT_INIT_S stMsgPort;

    /* all venc stream use one pci transfer port */
    g_stPcivVencCtx.s32MsgPortWrite = s32MsgPort++;
    g_stPcivVencCtx.s32MsgPortRead  = s32MsgPort++;
    s32Ret = PCIV_OpenMsgPort(s32RmtChip, g_stPcivVencCtx.s32MsgPortWrite);
    s32Ret += PCIV_OpenMsgPort(s32RmtChip, g_stPcivVencCtx.s32MsgPortRead);
    HI_ASSERT(HI_SUCCESS == s32Ret);

    /* each vdec stream use one pci transfer port */
    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        astPcivVdecCtx[i].s32MsgPortWrite   = s32MsgPort++;
        astPcivVdecCtx[i].s32MsgPortRead    = s32MsgPort++;
        s32Ret = PCIV_OpenMsgPort(s32RmtChip, astPcivVdecCtx[i].s32MsgPortWrite);
        s32Ret += PCIV_OpenMsgPort(s32RmtChip, astPcivVdecCtx[i].s32MsgPortRead);
        HI_ASSERT(HI_SUCCESS == s32Ret);
    }

    /* send msg port to pci slave device --------------------------------------------------*/
    stMsgPort.s32VencMsgPortW = g_stPcivVencCtx.s32MsgPortWrite;
    stMsgPort.s32VencMsgPortR = g_stPcivVencCtx.s32MsgPortRead;
    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        stMsgPort.s32VdecMsgPortW[i] = astPcivVdecCtx[i].s32MsgPortWrite;
        stMsgPort.s32VdecMsgPortR[i] = astPcivVdecCtx[i].s32MsgPortRead;
    }
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_INIT_MSG_PORG;
    stMsg.stMsgHead.u32MsgLen = sizeof(PCIV_MSGPORT_INIT_S);
    memcpy(stMsg.cMsgBody, &stMsgPort, sizeof(PCIV_MSGPORT_INIT_S));
    printf("\n============Prepare to open venc and vdec ports!=======\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);
    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_VOID SamplePcivExitMsgPort(HI_S32 s32RmtChip)
{
    HI_S32 i;
    HI_S32 s32Ret;
    SAMPLE_PCIV_MSG_S stMsg;

    /* close all stream msg port in local */
    PCIV_CloseMsgPort(s32RmtChip, g_stPcivVencCtx.s32MsgPortWrite);
    PCIV_CloseMsgPort(s32RmtChip, g_stPcivVencCtx.s32MsgPortRead);
    for (i=0; i<VDEC_MAX_CHN_NUM; i++)
    {
        PCIV_CloseMsgPort(s32RmtChip, astPcivVdecCtx[i].s32MsgPortWrite);
        PCIV_CloseMsgPort(s32RmtChip, astPcivVdecCtx[i].s32MsgPortRead);
    }

    /* close all stream msg port in remote */
    stMsg.stMsgHead.u32Target = s32RmtChip;
    stMsg.stMsgHead.u32MsgType = SAMPLE_PCIV_MSG_EXIT_MSG_PORG;
    stMsg.stMsgHead.u32MsgLen = 0;
    printf("\n=======PCIV_SendMsg SAMPLE_PCIV_MSG_EXIT_MSG_PORG==========\n");
    s32Ret = PCIV_SendMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg);
    HI_ASSERT(HI_FAILURE != s32Ret);
    while (PCIV_ReadMsg(s32RmtChip, PCIV_MSGPORT_COMM_CMD, &stMsg))
    {
        usleep(10000);
    }
    HI_ASSERT(stMsg.stMsgHead.u32MsgType == SAMPLE_PCIV_MSG_ECHO);
    HI_ASSERT(stMsg.stMsgHead.s32RetVal == HI_SUCCESS);
}


static HI_S32 SamplePcivInitMpp(HI_VOID)
{
    HI_S32 s32Ret;
    VB_CONF_S stVbConf, stVdecVbConf;

    memset(&stVbConf, 0, sizeof(VB_CONF_S));

    stVbConf.u32MaxPoolCnt             = 2;
    stVbConf.astCommPool[0].u32BlkSize = SAMPLE_PCIV_VENC_STREAM_BUF_LEN;
    stVbConf.astCommPool[0].u32BlkCnt  = 1;
    stVbConf.astCommPool[1].u32BlkSize = 1920*1088*2;
    stVbConf.astCommPool[1].u32BlkCnt  = 32;
    
    memset(&stVdecVbConf, 0, sizeof(VB_CONF_S));
    
    stVdecVbConf.u32MaxPoolCnt               = 2;
    stVdecVbConf.astCommPool[0].u32BlkSize   = 1920*1080*2;
    stVdecVbConf.astCommPool[0].u32BlkCnt    = 20;

    stVdecVbConf.astCommPool[1].u32BlkSize   = 1920*1080*3/2;
    stVdecVbConf.astCommPool[1].u32BlkCnt    = 20;
    
    s32Ret = SAMPLE_InitMPP(&stVbConf, &stVdecVbConf);
    PCIV_CHECK_ERR(s32Ret);
    
    return HI_SUCCESS;
}

HI_S32 SamplePcivInitComm(HI_S32 s32RmtChipId)
{
    HI_S32 s32Ret;

    /* wait for pci device connect ... ...  */
    s32Ret = PCIV_WaitConnect(s32RmtChipId);
    PCIV_CHECK_ERR(s32Ret);
    /* open pci msg port for commom cmd */
    s32Ret = PCIV_OpenMsgPort(s32RmtChipId, PCIV_MSGPORT_COMM_CMD);
    PCIV_CHECK_ERR(s32Ret);

    /* open pci msg port for all stream transfer(venc stream and vdec stream) */
    s32Ret = SamplePcivInitMsgPort(s32RmtChipId);
    PCIV_CHECK_ERR(s32Ret);

    return HI_SUCCESS;
}

int SamplePcivEnumChip(int *local_id,int remote_id[HISI_MAX_MAP_DEV-1], int *count)
{
    int fd, i;
    struct hi_mcc_handle_attr attr;

    fd = open("/dev/mcc_userdev", O_RDWR);
    if (fd<=0)
    {
        printf("open mcc dev fail\n");
        return -1;
    }

    /* HI_MCC_IOC_ATTR_INIT should be sent first ! */
    if (ioctl(fd, HI_MCC_IOC_ATTR_INIT, &attr))
    {
	    printf("initialization for attr failed!\n");
	    return -1;
    }

    *local_id = ioctl(fd, HI_MCC_IOC_GET_LOCAL_ID, &attr);
    printf("pci local id is %d \n", *local_id);

    if (ioctl(fd, HI_MCC_IOC_GET_REMOTE_ID, &attr))
    {
        printf("get pci remote id fail \n");
        return -1;
    }
    for (i=0; i<HISI_MAX_MAP_DEV-1; i++)
    {
        if (-1 == attr.remote_id[i]) break;
        *(remote_id++) = attr.remote_id[i];
        printf("get pci remote id : %d \n", attr.remote_id[i]);
    }

    *count = i;

    printf("===================close port %d!\n", attr.port);
    close(fd);
    return 0;
}

HI_S32 SamplePcivGetPfWin(HI_S32 s32ChipId)
{
    HI_S32 s32Ret;
    PCIV_BASEWINDOW_S stPciBaseWindow;
    s32Ret = HI_MPI_PCIV_GetBaseWindow(s32ChipId, &stPciBaseWindow);
    PCIV_CHECK_ERR(s32Ret);
    printf("pci device %d -> slot:%d, pf:%x,np:%x,cfg:%x\n", s32ChipId, s32ChipId-1,
        stPciBaseWindow.u32PfWinBase,stPciBaseWindow.u32NpWinBase,stPciBaseWindow.u32CfgWinBase);
    g_u32PfWinBase[s32ChipId] = stPciBaseWindow.u32PfWinBase;
    return HI_SUCCESS;
}

int SamplePcivSwitchPic(HI_U32 u32RmtIdx, HI_S32 s32RemoteChip, VO_DEV VoDev)
{
    char ch;
    HI_S32 i, k=0, s32Ret, u32DispIdx, s32VoPicDiv;
    HI_U32 u32Width, u32Height;
#if TEST_OTHER_DIVISION    
    HI_S32 as32VoPicDiv[] = {1, 4, 9, 16, 76, 100, 114, 132, 174, 200, 242, 300, 320, 640};
#else
    HI_S32 as32VoPicDiv[] = {1, 4, 9, 16};
#endif    
    SAMPLE_PCIV_DISP_CTX_S *pstDispCtx = NULL;

    /* find display contex by vo dev */
    for (u32DispIdx=0; u32DispIdx<VO_MAX_DEV_NUM; u32DispIdx++)
    {
        if (VoDev == s_astPcivDisp[u32DispIdx].VoDev)
        {
            pstDispCtx = &s_astPcivDisp[u32DispIdx];
            break;
        }
    }
    if (NULL == pstDispCtx) return HI_FAILURE;

    s32VoPicDiv = pstDispCtx->u32PicDiv;

    while (1)
    {
        printf("\n >>>> Please input commond as follow : \n");
        printf("\t ENTER : switch display div , 1 -> 4-> 9 -> 16 -> 1 -> ... \n");
        printf("\t s : step play by one frame \n");
        printf("\t q : quit the pciv sample \n");
        printf("------------------------------------------------------------------\n");

        ch = getchar();
        if (ch == 'q')
            break;
        else if ((1 == vdec_idx) && ('s' == ch))
        {
            //s32Ret = HI_MPI_VO_ChnStep(VoDev, 0);
            //PCIV_CHECK_ERR(s32Ret);
        }
        else
        {
            /* 1, disable all vo chn */
            for (i=0; i<s32VoPicDiv; i++)
            {
                HI_MPI_VO_DisableChn(VoDev, i);
            }

            /* 2, stop all pciv chn */
            SamplePciv_StopPcivByVo(u32RmtIdx, s32RemoteChip, u32DispIdx, pstDispCtx);

            /* switch pic 1 -> 4-> 9 -> 16 -> 1 ->... */
            s32VoPicDiv = as32VoPicDiv[(k++)%(sizeof(as32VoPicDiv)/sizeof(HI_S32))];

            /* 3, restart pciv chn by new pattern */
            pstDispCtx->u32PicDiv  = s32VoPicDiv;
            pstDispCtx->VoChnEnd   = pstDispCtx->VoChnStart + s32VoPicDiv - 1;
            s32Ret = SamplePciv_StartPcivByVo(u32RmtIdx, s32RemoteChip, u32DispIdx, pstDispCtx);
            PCIV_CHECK_ERR(s32Ret);

            /* 4, recfg vo chn and enable them by new pattern */
            u32Width  = 720;
            u32Height = 576;
            
            s32Ret = SAMPLE_SetVoChnMScreen(VoDev, s32VoPicDiv, u32Width, u32Height);
            PCIV_CHECK_ERR(s32Ret);

            for (i=0; i<s32VoPicDiv; i++)
            {
                s32Ret = HI_MPI_VO_EnableChn(VoDev, i);
                PCIV_CHECK_ERR(s32Ret);
            }
        }
    }

    return HI_SUCCESS;
}

HI_S32 SamplePcivBindHostChipVdecVpssVo(HI_U32 u32VdecCnt, HI_U32 u32VpssCnt, HI_U32 u32VpssChn, VO_DEV VoDev)
{
    HI_S32 i;
    HI_S32 s32Ret;
    MPP_CHN_S stSrcChn, stDestChn;

    for (i = 0; i < u32VdecCnt; i++)
    {
        stSrcChn.enModId   = HI_ID_VDEC;
        stSrcChn.s32DevId  = 0;
        stSrcChn.s32ChnId  = i;
        
        stDestChn.enModId  = HI_ID_VPSS;
        stDestChn.s32DevId = i + (u32VpssCnt/2);
        stDestChn.s32ChnId = 0;
        s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
    }

    for (i = 0; i < u32VpssCnt; i++)
    {
        stSrcChn.enModId = HI_ID_VPSS;
        stSrcChn.s32DevId = i;
        stSrcChn.s32ChnId = u32VpssChn;
            
        stDestChn.enModId = HI_ID_VOU;
        stDestChn.s32DevId = VoDev;
        stDestChn.s32ChnId = i;
        s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
    }

    return HI_SUCCESS;
}


HI_S32 SamplePcivUnBindHostChipVdecVpssVo(HI_U32 u32VdecCnt, HI_U32 u32VpssCnt, HI_U32 u32VpssChn, VO_DEV VoDev)
{
    HI_S32 i;
    HI_S32 s32Ret;
    MPP_CHN_S stSrcChn, stDestChn;

    for (i = 0; i < u32VdecCnt; i++)
    {
        stSrcChn.enModId   = HI_ID_VDEC;
        stSrcChn.s32DevId  = 0;
        stSrcChn.s32ChnId  = i;
        
        stDestChn.enModId  = HI_ID_VPSS;
        stDestChn.s32DevId = i + (u32VpssCnt/2);
        stDestChn.s32ChnId = 0;
        s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
    }

    for (i = 0; i < u32VpssCnt; i++)
    {
        stSrcChn.enModId = HI_ID_VPSS;
        stSrcChn.s32DevId = i;
        stSrcChn.s32ChnId = u32VpssChn;
            
        stDestChn.enModId = HI_ID_VOU;
        stDestChn.s32DevId = VoDev;
        stDestChn.s32ChnId = i;
        s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
    }

    return HI_SUCCESS;
}


HI_S32 SamplePcivBindHostChipVpssVo(HI_U32 u32VpssCnt, HI_U32 u32VpssChn, VO_DEV VoDev)
{
    HI_S32 i;
    HI_S32 s32Ret;
    MPP_CHN_S stSrcChn, stDestChn;

    for (i = 0; i < u32VpssCnt; i++)
    {
        stSrcChn.enModId = HI_ID_VPSS;
        stSrcChn.s32DevId = i;
        stSrcChn.s32ChnId = u32VpssChn;
            
        stDestChn.enModId = HI_ID_VOU;
        stDestChn.s32DevId = VoDev;
        stDestChn.s32ChnId = i;
        s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
    }

    return HI_SUCCESS;
}



HI_S32 SamplePcivUnBindHostChipVpssVo(HI_U32 u32VpssCnt, HI_U32 u32VpssChn, VO_DEV VoDev)
{
    HI_S32 i;
    HI_S32 s32Ret;
    MPP_CHN_S stSrcChn, stDestChn;

    for (i = 0; i < u32VpssCnt; i++)
    {
        stSrcChn.enModId = HI_ID_VPSS;
        stSrcChn.s32DevId = i;
        stSrcChn.s32ChnId = u32VpssChn;
            
        stDestChn.enModId = HI_ID_VOU;
        stDestChn.s32DevId = VoDev;
        stDestChn.s32ChnId = i;
        s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
    }

    return HI_SUCCESS;
}


HI_S32 SamplePcivBindHostChipPcivVpss(HI_U32 u32VpssCnt, HI_U32 u32VpssChn, HI_U32 u32VpssChnStart)
{
    HI_S32 i;
    HI_S32 s32Ret;
    MPP_CHN_S stSrcChn, stDestChn;

    for (i = u32VpssChnStart; i <= u32VpssCnt; i++)
    {
        stSrcChn.enModId = HI_ID_PCIV;
        stSrcChn.s32DevId = 0;
        stSrcChn.s32ChnId = 0;
            
        stDestChn.enModId = HI_ID_VPSS;
        stDestChn.s32DevId = i;
        stDestChn.s32ChnId = u32VpssChn;
        s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
    }

    return HI_SUCCESS;
}


HI_S32 SamplePcivUnBindHostChipPcivVpss(HI_U32 u32VpssCnt, HI_U32 u32VpssChn, HI_U32 u32VpssChnStart)
{
    HI_S32 i;
    HI_S32 s32Ret;
    MPP_CHN_S stSrcChn, stDestChn;

    for (i = u32VpssChnStart; i <= u32VpssCnt; i++)
    {
        stSrcChn.enModId = HI_ID_PCIV;
        stSrcChn.s32DevId = 0;
        stSrcChn.s32ChnId = 0;
            
        stDestChn.enModId = HI_ID_VPSS;
        stDestChn.s32DevId = i;
        stDestChn.s32ChnId = u32VpssChn;
        s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        PCIV_CHECK_ERR(s32Ret);
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
HI_VOID* SamplePcivHostStartVdecSendStreamThread(void* p)
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


HI_S32 SamplePcivStartHostVdecAndReadStream(HI_U32 u32VdecCnt, PIC_SIZE_E enPicSize)
{
    HI_S32 s32Ret;
	VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];
    VDEC_CHN_ATTR_S stVdecChnAttr;

    switch (enPicSize)
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
        default         :
            VDEC_MST_DefaultH264D1_Attr(&stVdecChnAttr);
            break;
    }

    s32Ret = SamplePciv_HostCreateVdec(u32VdecCnt, enPicSize, &stVdecChnAttr);
    PCIV_CHECK_ERR(s32Ret);
    
    /* craete stream read pthread */
    SamplePcivSetVdecParam(u32VdecCnt, &stVdecSend[0], &stVdecChnAttr, SAMPLE_1080P_H264_PATH);	
	SamplePcivStartVdecSendStream(u32VdecCnt, &stVdecSend[0], &VdecThread[0]);

    return HI_SUCCESS;
}


HI_S32 SamplePcivStopHostVdecAndReadStream(HI_U32 u32VdecCnt)
{
    HI_S32 s32Ret;
    VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];
    
    /* destroy stream read pthread */    
    SamplePcivStopVdecReadStream(u32VdecCnt, &stVdecSend[0], &VdecThread[0]);

    s32Ret = SamplePciv_HostDestroyVdec(u32VdecCnt);
    PCIV_CHECK_ERR(s32Ret);
    
    return HI_SUCCESS;
}


HI_S32 SamplePcivStartHostVoDev(VO_DEV VoDev, VO_INTF_SYNC_E enIntfSync, HI_U32 u32VoChnNum)
{
    HI_S32 i;
    HI_S32 s32Ret;
    HI_S32 s32DispNum;
    VO_LAYER VoLayer;
    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    VO_CHN_ATTR_S astChnAttr[16];

    s32DispNum = SamplePcivGetVoDisplayNum(u32VoChnNum);
    if(s32DispNum < 0)
    {
        printf("SAMPLE_RGN_GetVoDisplayNum failed! u32VoChnNum: %d.\n", u32VoChnNum);
        return HI_FAILURE;
    }
    
    s32Ret = SamplePcivGetVoAttr(VoDev, enIntfSync, &stPubAttr, &stLayerAttr, s32DispNum, astChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_RGN_GetVoAttr failed!\n");
        return HI_FAILURE;
    }

    VoLayer = SamplePcivGetVoLayer(VoDev);
    if(VoLayer < 0)
    {
        printf("SAMPLE_RGN_GetVoLayer failed! VoDev: %d.\n", VoDev);
        return HI_FAILURE;
    }
  
    s32Ret = HI_MPI_VO_Disable(VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_Disable failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VO_SetPubAttr(VoDev, &stPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_SetPubAttr failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VO_Enable(VoDev);
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
        
        //printf("VO chn:%d enable ok \n", i);
    }

    //printf("VO: %d enable ok!\n", VoDev);
    
    return HI_SUCCESS;
}


HI_S32 SamplePcivStopHostVoDev(VO_DEV VoDev, HI_U32 u32VoChnNum)
{
    HI_S32 i;
    HI_S32 s32Ret;
    VO_LAYER VoLayer;

    VoLayer = SamplePcivGetVoLayer(VoDev);
    if(VoLayer < 0)
    {
        printf("SAMPLE_RGN_GetVoLayer failed! VoDev: %d.\n", VoDev);
        return HI_FAILURE;
    }

    for (i = 0; i< u32VoChnNum; i++)
    {
        s32Ret = HI_MPI_VO_DisableChn(VoLayer, i);
        if (HI_SUCCESS != s32Ret)
        {
            printf("HI_MPI_VO_DisableChn failed! s32Ret: 0x%x.\n", s32Ret);
            return s32Ret;
        }

        //printf("VO chn : %d stop ok!\n", i);
    }

    s32Ret = HI_MPI_VO_DisableVideoLayer(VoLayer);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_DisableVideoLayer failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VO_Disable(VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_Disable failed! s32Ret:0x%x.\n", s32Ret);
        return s32Ret;
    }

    //printf("VO dev: %d stop ok!\n", VoDev);
    
    return 0;
}



HI_S32 SamplePcivSlaveSendPicToHostInIndependentMode(HI_VOID)
{
    HI_S32 s32Ret;
    HI_BOOL bReadStream = HI_FALSE;
    HI_S32 s32PciRmtChipCnt = 1;

    HI_S32 i, s32RmtChipId;
    PIC_SIZE_E enPicSize = PIC_HD1080;
    HI_U32 u32VdecCnt = 4;
    HI_S32 s32PciLocalId, as32PciRmtId[PCIV_MAX_CHIPNUM], s32AllPciRmtCnt;

    HI_U32 u32VpssCnt = 8;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    HI_U32 u32VpssChn;
    SIZE_S stSize;

    VO_DEV VoDev;
    VO_INTF_SYNC_E enIntfSync;
    HI_U32 u32VoChnNum;
    HI_BOOL bBindVdec;
    /* system init ----------------------------------------------------------------------- */
    /* Init mpp sys and video buffer */
    s32Ret = SamplePcivInitMpp();
    PCIV_CHECK_ERR(s32Ret);

    s32Ret = SAMPLE_COMM_SYS_MemConfig();
    PCIV_CHECK_ERR(s32Ret);
    
    /* Get pci local id and all target id */
    s32Ret = SamplePcivEnumChip(&s32PciLocalId, as32PciRmtId, &s32AllPciRmtCnt);
    PCIV_CHECK_ERR(s32Ret);

    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        /* Wait for slave dev connect, and init message communication */
        s32Ret = SamplePcivInitComm(as32PciRmtId[i]);
        PCIV_CHECK_ERR(s32Ret);

        /* Get PCI PF Window info of pci device, used for DMA trans that host to slave */
        (HI_VOID)SamplePcivGetPfWin(as32PciRmtId[i]);
    }
    printf("Func: %s, line: %d.....\n", __FUNCTION__, __LINE__);
    /* start host chip vo dev ------------------------------------------------------------ */
#if 1    
    VoDev       = 1;
    u32VoChnNum = 9;
    enIntfSync  = VO_OUTPUT_1080P30;
    s32Ret = SamplePcivStartHostVoDev(VoDev, enIntfSync, u32VoChnNum);
    PCIV_CHECK_ERR(s32Ret);
#endif    
    printf("Func: %s, line: %d.....\n", __FUNCTION__, __LINE__);

    /* start host chip vpss group -------------------------------------------------------- */
    u32VpssChn       = VPSS_PRE0_CHN;
    stSize.u32Width  = 1920;
    stSize.u32Height = 1080;

    stVpssGrpAttr.bDciEn    = HI_FALSE;
    stVpssGrpAttr.bHistEn   = HI_FALSE;
    stVpssGrpAttr.bIeEn     = HI_FALSE;
    stVpssGrpAttr.bNrEn     = HI_TRUE;
    stVpssGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    stVpssGrpAttr.enPixFmt  = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    stVpssGrpAttr.u32MaxW   = stSize.u32Width;
    stVpssGrpAttr.u32MaxH   = stSize.u32Height;
        
    s32Ret = SamplePcivStartHostVpss(u32VpssCnt, u32VpssChn, &stVpssGrpAttr);
    PCIV_CHECK_ERR(s32Ret);

#if 0
    /* start host chip vdec and create pthread to read stream from file ------------------ */
    s32Ret = SamplePcivStartHostVdecAndReadStream(u32VdecCnt, enPicSize);
    PCIV_CHECK_ERR(s32Ret);
#endif

    /* host chip vdec->vpss->vo bind ---------------------------------------------------------------- */
    s32Ret = SamplePcivBindHostChipVdecVpssVo(u32VdecCnt, u32VpssCnt, u32VpssChn, VoDev);
    PCIV_CHECK_ERR(s32Ret);
    
    /* Init ------------------------------------------------------------------------------ */
    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        s32RmtChipId = as32PciRmtId[i];
    #if 0
        /* start slave chip vdec and read stream file */
        for (j=0; j<u32VdecCnt; j++)
        {
            bReadStream = HI_TRUE;
            s32Ret = SamplePciv_HostStartVdecChn(s32RmtChipId, j, bReadStream);
            PCIV_CHECK_ERR(s32Ret);
        }
    #endif
        /* start slave chip vpss */
        bBindVdec = HI_TRUE;
        s32Ret = SamplePciv_StartVpssByChip(s32RmtChipId, u32VpssCnt, bBindVdec);
        PCIV_CHECK_ERR(s32Ret);
        
        /* start host and slave chip pciv */
        s_astPcivDisp[0].bValid = HI_TRUE;
        s_astPcivDisp[0].u32PicDiv = 4;
        s_astPcivDisp[0].VoChnStart = 0;
        s_astPcivDisp[0].VoChnEnd  = 3;
        s_astPcivDisp[0].VoDev = 0;
        s32Ret = SamplePciv_StartPcivByChip(i, s32RmtChipId);
        PCIV_CHECK_ERR(s32Ret);

        /* start slave chip vdec and read stream file */
        bReadStream = HI_TRUE;
        s32Ret = SamplePciv_HostStartVdecChn(s32RmtChipId, u32VdecCnt, bReadStream);
        PCIV_CHECK_ERR(s32Ret);
        
    }

    /* start host chip vdec and create pthread to read stream from file ------------------ */
    s32Ret = SamplePcivStartHostVdecAndReadStream(u32VdecCnt, enPicSize);
    PCIV_CHECK_ERR(s32Ret);

    printf("\n#################Press enter to exit########################\n");
    getchar();

    /* Exit ------------------------------------------------------------------------------ */
    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        s32RmtChipId = as32PciRmtId[i];
        
        /* stop host and slave chip pciv */
        s32Ret = SamplePciv_StopPcivByChip(s32RmtChipId, i);
        PCIV_CHECK_ERR(s32Ret);
        
        /* stop  slave chip vpss */
        SamplePciv_StopVpssByChip(s32RmtChipId, u32VpssCnt, bBindVdec);
        
        /* stop slave chip vdec and stop read stream file */
        s32Ret = SamplePciv_HostStopVdecChn(s32RmtChipId, u32VdecCnt, bReadStream);
        PCIV_CHECK_ERR(s32Ret);
        
        /* close all msg port */
        SamplePcivExitMsgPort(s32RmtChipId);
        
        s32Ret = PCIV_CloseMsgPort(s32RmtChipId, PCIV_MSGPORT_COMM_CMD);
        PCIV_CHECK_ERR(s32Ret);
    }

    /* unbind host chip vdec->vpss->vo */
    s32Ret = SamplePcivUnBindHostChipVdecVpssVo(u32VdecCnt, u32VpssCnt, u32VpssChn, VoDev);
    PCIV_CHECK_ERR(s32Ret);
    
    /* stop host chip vdec and read stream file */
    s32Ret = SamplePcivStopHostVdecAndReadStream(u32VdecCnt);
    PCIV_CHECK_ERR(s32Ret);
    
    /* stop host chip vpss */
    s32Ret = SamplePcivStopHostVpss(u32VpssCnt);
    PCIV_CHECK_ERR(s32Ret);
    
    /* stop host chip vo */
    s32Ret = SamplePcivStopHostVoDev(VoDev, u32VoChnNum);
    PCIV_CHECK_ERR(s32Ret);
    
    /* Exit whole mpp sys  */
    s32Ret = SAMPLE_ExitMPP();
    PCIV_CHECK_ERR(s32Ret);
    
    return HI_SUCCESS;
    
}



HI_S32 SamplePcivSlaveSendPicToHostInCombineMode(HI_VOID)
{
    HI_S32 s32Ret;
    HI_BOOL bReadStream = HI_FALSE;
    HI_S32 s32PciRmtChipCnt = 1;

    HI_S32 i, s32RmtChipId;
    PIC_SIZE_E enPicSize = PIC_HD1080;
    HI_U32 u32VdecCnt = 4;
    HI_S32 s32PciLocalId, as32PciRmtId[PCIV_MAX_CHIPNUM], s32AllPciRmtCnt;

    VO_DEV VirtualVo  = 3;
    HI_U32 u32VpssCnt = 8;
    HI_U32 u32PicDiv  = 0;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    HI_U32 u32VpssChn;
    SIZE_S stSize;

    VO_DEV VoDev;
    VO_INTF_SYNC_E enIntfSync;
    HI_U32 u32VoChnNum;
    HI_U32 u32VirtVoChnNum;
    
    HI_BOOL bBindVdec;
    HI_U32 u32VpssChnStart;
    
    /* system init ----------------------------------------------------------------------- */
    /* Init mpp sys and video buffer */
    s32Ret = SamplePcivInitMpp();
    PCIV_CHECK_ERR(s32Ret);
    
    s32Ret = SAMPLE_COMM_SYS_MemConfig();
    PCIV_CHECK_ERR(s32Ret);
    
    /* Get pci local id and all target id */
    s32Ret = SamplePcivEnumChip(&s32PciLocalId, as32PciRmtId, &s32AllPciRmtCnt);
    PCIV_CHECK_ERR(s32Ret);

    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        /* Wait for slave dev connect, and init message communication */
        s32Ret = SamplePcivInitComm(as32PciRmtId[i]);
        PCIV_CHECK_ERR(s32Ret);

        /* Get PCI PF Window info of pci device, used for DMA trans that host to slave */
        (HI_VOID)SamplePcivGetPfWin(as32PciRmtId[i]);
    }

    /* start host chip vo dev ------------------------------------------------------------ */
    VoDev       = 1;
    u32VoChnNum = 9;
    enIntfSync  = VO_OUTPUT_1080P30;
    s32Ret = SamplePcivStartHostVoDev(VoDev, enIntfSync, u32VoChnNum);
    PCIV_CHECK_ERR(s32Ret);


    /* start host chip vpss group -------------------------------------------------------- */
    u32VpssChn       = VPSS_PRE0_CHN;
    stSize.u32Width  = 1920;
    stSize.u32Height = 1088;

    stVpssGrpAttr.bDciEn    = HI_FALSE;
    stVpssGrpAttr.bHistEn   = HI_FALSE;
    stVpssGrpAttr.bIeEn     = HI_FALSE;
    stVpssGrpAttr.bNrEn     = HI_TRUE;
    stVpssGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    stVpssGrpAttr.enPixFmt  = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    stVpssGrpAttr.u32MaxW   = stSize.u32Width;
    stVpssGrpAttr.u32MaxH   = stSize.u32Height;
        
    s32Ret = SamplePcivStartHostVpss(u32VpssCnt, u32VpssChn, &stVpssGrpAttr);
    PCIV_CHECK_ERR(s32Ret);

#if 0
    /* start host chip vdec and create pthread to read stream from file ------------------ */
    s32Ret = SamplePcivStartHostVdecAndReadStream(u32VdecCnt, enPicSize);
    PCIV_CHECK_ERR(s32Ret);
#endif

    /* host chip vdec->vpss->vo bind ---------------------------------------------------------------- */
    s32Ret = SamplePcivBindHostChipVdecVpssVo(u32VdecCnt, u32VpssCnt, u32VpssChn, VoDev);
    PCIV_CHECK_ERR(s32Ret);
    
    /* Init ------------------------------------------------------------------------------ */
    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        s32RmtChipId = as32PciRmtId[i];
    #if 0
        /* start slave chip vdec and read stream file */
        bReadStream = HI_TRUE;
        s32Ret = SamplePciv_HostStartVdecChn(s32RmtChipId, u32VdecCnt, bReadStream);
        PCIV_CHECK_ERR(s32Ret);
    #endif      
    
        /* start slave chip vpss */
        bBindVdec = HI_FALSE;
        s32Ret = SamplePciv_StartVpssByChip(s32RmtChipId, u32VpssCnt, bBindVdec);
        PCIV_CHECK_ERR(s32Ret);
    #if 1
        /* start slave chip vpss crop */
        s32Ret = SamplePciv_VpssCropPic(u32VpssCnt/2);
        PCIV_CHECK_ERR(s32Ret);
    #endif      
        /* start slave chip virtual vo */
        u32VirtVoChnNum = 4;
        s32Ret = SamplePcivStartSlaveVirtualVo(s32RmtChipId, VirtualVo, u32VirtVoChnNum);
        PCIV_CHECK_ERR(s32Ret);
        
        /* start host and slave chip pciv */
        s_astPcivDisp[0].bValid = HI_TRUE;
        s_astPcivDisp[0].u32PicDiv = 1;
        s_astPcivDisp[0].VoChnStart = 0;
        s_astPcivDisp[0].VoChnEnd  = 0;
        s_astPcivDisp[0].VoDev = 3;
        s32Ret = SamplePciv_StartPcivByChip(i, s32RmtChipId);
        PCIV_CHECK_ERR(s32Ret);

        u32VpssChnStart = 1;
        u32PicDiv       = (u32VpssCnt/2) - s_astPcivDisp[0].u32PicDiv;
        s32Ret = SamplePcivBindHostChipPcivVpss(u32PicDiv, u32VpssChn, u32VpssChnStart);
        PCIV_CHECK_ERR(s32Ret);
    #if 0
        /* start slave chip vpss crop */
        s32Ret = SamplePciv_VpssCropPic(u32VpssCnt);
        PCIV_CHECK_ERR(s32Ret);
    #endif      
        /* start slave chip vdec and read stream file */
        bReadStream = HI_TRUE;
        s32Ret = SamplePciv_HostStartVdecChn(s32RmtChipId, u32VdecCnt, bReadStream);
        PCIV_CHECK_ERR(s32Ret);
    }

    /* start host chip vdec and create pthread to read stream from file ------------------ */
    s32Ret = SamplePcivStartHostVdecAndReadStream(u32VdecCnt, enPicSize);
    PCIV_CHECK_ERR(s32Ret);

    printf("\n#################Press enter to exit########################\n");
    getchar();

    /* Exit ------------------------------------------------------------------------------ */
    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        s32RmtChipId = as32PciRmtId[i];

        s32Ret = SamplePcivUnBindHostChipPcivVpss(u32VpssCnt, u32VpssChn, u32VpssChnStart);
        PCIV_CHECK_ERR(s32Ret);
        
        /* stop host and slave chip pciv */
        s32Ret = SamplePciv_StopPcivByChip(s32RmtChipId, i);
        PCIV_CHECK_ERR(s32Ret);

        /* stop slave chip virtual vo */
        s32Ret = SamplePcivStopSlaveVirtualVo(s32RmtChipId, VirtualVo, u32VirtVoChnNum);
        PCIV_CHECK_ERR(s32Ret);
        
        /* stop  slave chip vpss */
        SamplePciv_StopVpssByChip(s32RmtChipId, u32VpssCnt, bBindVdec);
        
        /* stop slave chip vdec and stop read stream file */
        s32Ret = SamplePciv_HostStopVdecChn(s32RmtChipId, u32VdecCnt, bReadStream);
        PCIV_CHECK_ERR(s32Ret);
        
        /* close all msg port */
        SamplePcivExitMsgPort(s32RmtChipId);
        
        s32Ret = PCIV_CloseMsgPort(s32RmtChipId, PCIV_MSGPORT_COMM_CMD);
        PCIV_CHECK_ERR(s32Ret);
    }

    /* unbind host chip vdec->vpss->vo */
    s32Ret = SamplePcivUnBindHostChipVdecVpssVo(u32VdecCnt, u32VpssCnt, u32VpssChn, VoDev);
    PCIV_CHECK_ERR(s32Ret);
    
    /* stop host chip vdec and read stream file */
    s32Ret = SamplePcivStopHostVdecAndReadStream(u32VdecCnt);
    PCIV_CHECK_ERR(s32Ret);
    
    /* stop host chip vpss */
    s32Ret = SamplePcivStopHostVpss(u32VpssCnt);
    PCIV_CHECK_ERR(s32Ret);
    
    /* stop host chip vo */
    s32Ret = SamplePcivStopHostVoDev(VoDev, u32VoChnNum);
    PCIV_CHECK_ERR(s32Ret);
    
    /* Exit whole mpp sys  */
    s32Ret = SAMPLE_ExitMPP();
    PCIV_CHECK_ERR(s32Ret);
    
    return HI_SUCCESS;
    
}

HI_S32 SamplePcivHostSendStreamToSalveAndReceiveSlavePic(HI_VOID)
{
    HI_S32 s32Ret;
    VB_CONF_S stVbConf, stVdecVbConf;
    HI_BOOL bReadStream = HI_FALSE;

    HI_S32 i, s32RmtChipId;
    HI_U32 u32VdecCnt = 4;
    HI_S32 s32PciRmtChipCnt = 1;   /* only test one slave dev */
    HI_S32 s32PciLocalId, as32PciRmtId[PCIV_MAX_CHIPNUM], s32AllPciRmtCnt;

    VO_DEV VoDev;
    VO_INTF_SYNC_E enIntfSync;
    HI_U32 u32VoChnNum;

    HI_U32 u32VpssCnt = 4;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    HI_U32 u32VpssChn;
    SIZE_S stSize;
    
    /* Init ------------------------------------------------------------------------------ */
    /* Init mpp sys and video buffer */
    memset(&stVbConf, 0, sizeof(VB_CONF_S));

    stVbConf.u32MaxPoolCnt             = 2;
    stVbConf.astCommPool[0].u32BlkSize = SAMPLE_PCIV_VENC_STREAM_BUF_LEN;
    stVbConf.astCommPool[0].u32BlkCnt  = 1;
    stVbConf.astCommPool[1].u32BlkSize = 1920*1088*2;
    stVbConf.astCommPool[1].u32BlkCnt  = 40;
    
    memset(&stVdecVbConf, 0, sizeof(VB_CONF_S));
    
    stVdecVbConf.u32MaxPoolCnt               = 2;
    stVdecVbConf.astCommPool[0].u32BlkSize   = 1920*1080*2;
    stVdecVbConf.astCommPool[0].u32BlkCnt    = 30;

    stVdecVbConf.astCommPool[1].u32BlkSize   = 1920*1080*3/2;
    stVdecVbConf.astCommPool[1].u32BlkCnt    = 30;
    
    s32Ret = SAMPLE_InitMPP(&stVbConf, &stVdecVbConf);
    PCIV_CHECK_ERR(s32Ret);
    
    s32Ret = SAMPLE_COMM_SYS_MemConfig();
    PCIV_CHECK_ERR(s32Ret);
    
    /* Get pci local id and all target id */
    s32Ret = SamplePcivEnumChip(&s32PciLocalId, as32PciRmtId, &s32AllPciRmtCnt);
    PCIV_CHECK_ERR(s32Ret);

    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        /* Wait for slave dev connect, and init message communication */
        s32Ret = SamplePcivInitComm(as32PciRmtId[i]);
        PCIV_CHECK_ERR(s32Ret);

        /* Get PCI PF Window info of pci device, used for DMA trans that host to slave */
        (HI_VOID)SamplePcivGetPfWin(as32PciRmtId[i]);
    }

    /* start host chip vo dev ------------------------------------------------------------ */
    VoDev       = 1;
    u32VoChnNum = 4;
    enIntfSync  = VO_OUTPUT_1080P30;
    s32Ret = SamplePcivStartHostVoDev(VoDev, enIntfSync, u32VoChnNum);
    PCIV_CHECK_ERR(s32Ret);
    
    /* start host chip vpss group -------------------------------------------------------- */
    u32VpssChn       = VPSS_PRE0_CHN;
    stSize.u32Width  = 1920;
    stSize.u32Height = 1080;

    stVpssGrpAttr.bDciEn    = HI_FALSE;
    stVpssGrpAttr.bHistEn   = HI_FALSE;
    stVpssGrpAttr.bIeEn     = HI_FALSE;
    stVpssGrpAttr.bNrEn     = HI_TRUE;
    stVpssGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    stVpssGrpAttr.enPixFmt  = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    stVpssGrpAttr.u32MaxW   = stSize.u32Width;
    stVpssGrpAttr.u32MaxH   = stSize.u32Height;
        
    s32Ret = SamplePcivStartHostVpss(u32VpssCnt, u32VpssChn, &stVpssGrpAttr);
    PCIV_CHECK_ERR(s32Ret);


    s32Ret = SamplePcivBindHostChipVpssVo(u32VpssCnt, u32VpssChn, VoDev);
    PCIV_CHECK_ERR(s32Ret);
    
    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        s32RmtChipId = as32PciRmtId[i];       

        /* start host and slave chip pciv */
        s_astPcivDisp[0].bValid = HI_TRUE;
        s_astPcivDisp[0].u32PicDiv = 4;
        s_astPcivDisp[0].VoChnStart = 0;
        s_astPcivDisp[0].VoChnEnd  = 3;
        s_astPcivDisp[0].VoDev = 1;
        s32Ret = SamplePciv_StartPcivByChip(i, s32RmtChipId);
        PCIV_CHECK_ERR(s32Ret);

        /* start slave chip vdec */
        bReadStream = HI_FALSE;
        s32Ret = SamplePciv_StartVdecByChip(s32RmtChipId, u32VdecCnt, bReadStream);
        PCIV_CHECK_ERR(s32Ret);
    }

    printf("\n#################Press enter to exit########################\n");
    getchar();

    /* Exit ------------------------------------------------------------------------------ */
    for (i=0; i<s32PciRmtChipCnt; i++)
    {
        s32RmtChipId = as32PciRmtId[i];
        
        /* stop host and slave chip pciv */
        s32Ret = SamplePciv_StopPcivByChip(s32RmtChipId, i);
        PCIV_CHECK_ERR(s32Ret);
        
        /* stop vdec in host and slave*/
        SamplePciv_StopVdecByChip(s32RmtChipId, u32VdecCnt, bReadStream);

        /* close all msg port */
        SamplePcivExitMsgPort(s32RmtChipId);
        
        s32Ret = PCIV_CloseMsgPort(s32RmtChipId, PCIV_MSGPORT_COMM_CMD);
        PCIV_CHECK_ERR(s32Ret);
    }

    /* unbind host chip vpss->vo */
    s32Ret = SamplePcivUnBindHostChipVpssVo(u32VpssCnt, u32VpssChn, VoDev);
    PCIV_CHECK_ERR(s32Ret);
        
    /* stop host chip vpss */
    s32Ret = SamplePcivStopHostVpss(u32VpssCnt);
    PCIV_CHECK_ERR(s32Ret);
    
    /* stop host chip vo */
    s32Ret = SamplePcivStopHostVoDev(VoDev, u32VoChnNum);
    PCIV_CHECK_ERR(s32Ret);

    /* Exit whole mpp sys  */
    s32Ret = SAMPLE_ExitMPP();
    PCIV_CHECK_ERR(s32Ret);
    
    return HI_SUCCESS;
    
}

HI_VOID SAMPLE_PCIV_Usage(HI_VOID)
{
    printf("press sample command as follows!\n");
    printf("\t 0) IndependentMode: file->VDEC->VPSS(NR)->PCIV(slave)->PCIV(host)->VPSS->VO(VGA+HDMI)\n");
    printf("\t 1) CombineMode:     file->VDEC->VPSS(NR+OSD)->VO->PCIV(slave)->PCIV(host)->VPSS(CROP)->VO(VGA+HDMI)\n");
	printf("\t 2) Host send stream to slave, slave vdec and send picture back: file->PCIT(host)->PCIT(slave)->VDEC->VPSS->PCIV(slave)->PCIV(host)->VO(VGA+HDMI) \n");
    printf("sample command:");
    return;
}

/******************************************************************************
* function : to process abnormal case                                        
******************************************************************************/
HI_VOID SAMPLE_PCIV_HandleSig(HI_S32 signo)
{    
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_ExitMPP();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(0);
}


/******************************************************************************
* function    : main()
* Description : region
******************************************************************************/
int main(int argc, char *argv[])
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_CHAR ch;
    
    bQuit = HI_FALSE;

    signal(SIGINT, SAMPLE_PCIV_HandleSig);
    signal(SIGTERM, SAMPLE_PCIV_HandleSig);

    while (1)
    {
        SAMPLE_PCIV_Usage();
        ch = getchar();
        getchar();
        switch (ch)
        {
            case '0': 
            {
                /* file->VDEC->VPSS(NR)->PCIV(slave)->PCIV(host)->VPSS->VO(VGA+HDMI) */
                test_idx = 0;
                Add_osd  = 0;
                s32Ret = SamplePcivSlaveSendPicToHostInIndependentMode();
                PCIV_CHECK_ERR(s32Ret);
                bQuit = HI_TRUE;
                break;
            }
            case '1': 
            {
                /* file->VDEC->VPSS(NR+OSD)->VO->PCIV(slave)->PCIV(host)->VPSS(CROP)->VO(VGA+HDMI) */
                test_idx = 1;
                Add_osd  = 1;
                s32Ret = SamplePcivSlaveSendPicToHostInCombineMode();
                PCIV_CHECK_ERR(s32Ret);
                bQuit = HI_TRUE;
                break;
            }
	        case '2': 
            {
	            /* file->PCIT(host)->PCIT(slave)->VDEC->VPSS->PCIV(slave)->PCIV(host)->VO(VGA+HDMI) */
                test_idx = 2;
                Add_osd  = 0;
                s32Ret = SamplePcivHostSendStreamToSalveAndReceiveSlavePic();
                PCIV_CHECK_ERR(s32Ret);
                bQuit = HI_TRUE;
			    break;
	        }
            default :
            {
                printf("input invaild! please try again.\n");
                break;
            }
        }
        
        if (bQuit)
        {
            break;
        }
    }
            
    return s32Ret;
    
}


