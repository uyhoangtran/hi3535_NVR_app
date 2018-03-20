/******************************************************************************
  A simple program of Hisilicon mpp implementation.
  Copyright (C), 2012-2020, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2013-7 Created
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "sample_comm.h"
#include "mpi_vgs.h"

/******************************************************************************
* function : to process abnormal case                                         
******************************************************************************/
void SAMPLE_VGS_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

HI_VOID SAMPLE_VGS_Usage(HI_VOID)
{
    printf("\n\n/************************************/\n");
    printf("please choose the case which you want to run:\n");
    printf("\t0:  Decompress tile picture from vdec\n");
    printf("\tq:  quit the whole sample\n");
    printf("sample command:");
}

HI_VOID SAMPLE_VGS_SaveSP42XToPlanar(FILE *pfile, VIDEO_FRAME_S *pVBuf)
{
	unsigned int w, h;
	char * pVBufVirt_Y;
	char * pVBufVirt_C;
	char * pMemContent;
	unsigned char *TmpBuff;
	HI_U32 size;
	PIXEL_FORMAT_E  enPixelFormat = pVBuf->enPixelFormat;
	HI_U32 u32UvHeight;/* 存为planar 格式时的UV分量的高度 */
	
	if (PIXEL_FORMAT_YUV_SEMIPLANAR_420 == enPixelFormat)
	{
	    size = (pVBuf->u32Stride[0])*(pVBuf->u32Height)*3/2;    
	    u32UvHeight = pVBuf->u32Height/2;
	}
	else
	{
	    size = (pVBuf->u32Stride[0])*(pVBuf->u32Height)*2;   
	    u32UvHeight = pVBuf->u32Height;
	}

	pVBufVirt_Y = pVBuf->pVirAddr[0]; 
	pVBufVirt_C = pVBufVirt_Y + (pVBuf->u32Stride[0])*(pVBuf->u32Height);

    TmpBuff = (unsigned char *)malloc(2048);
    if(NULL == TmpBuff)
    {
        printf("Func:%s line:%d -- unable alloc %dB memory for tmp buffer\n", 
            __FUNCTION__, __LINE__, 2048);
        return;
    }

	/* save Y ----------------------------------------------------------------*/

	for(h=0; h<pVBuf->u32Height; h++)
	{
	    pMemContent = pVBufVirt_Y + h*pVBuf->u32Stride[0];
	    fwrite(pMemContent, 1,pVBuf->u32Width,pfile);
	}

	/* save U ----------------------------------------------------------------*/
	for(h=0; h<u32UvHeight; h++)
	{
	    pMemContent = pVBufVirt_C + h*pVBuf->u32Stride[1];

	    pMemContent += 1;

	    for(w=0; w<pVBuf->u32Width/2; w++)
	    {
	        TmpBuff[w] = *pMemContent;
	        pMemContent += 2;
	    }
	    fwrite(TmpBuff, 1,pVBuf->u32Width/2,pfile);
	}

	/* save V ----------------------------------------------------------------*/
	for(h=0; h<u32UvHeight; h++)    
	{
	    pMemContent = pVBufVirt_C + h*pVBuf->u32Stride[1];

	    for(w=0; w<pVBuf->u32Width/2; w++)
	    {
	        TmpBuff[w] = *pMemContent;
	        pMemContent += 2;
	    }
	    fwrite(TmpBuff, 1,pVBuf->u32Width/2,pfile);
	}
    
    free(TmpBuff);

	return;
}


HI_S32 SAMPLE_VGS_Decompress_TilePicture(HI_VOID)
{
    VB_CONF_S stVbConf, stModVbConf;
	HI_S32 i = 0, s32Ret = HI_SUCCESS;
	VDEC_CHN_ATTR_S stVdecChnAttr[VDEC_MAX_CHN_NUM];
	VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];
    SIZE_S stSize;
    HI_U32  u32BlkSize;
    VB_POOL hPool  = VB_INVALID_POOLID;
	pthread_t   VdecThread[2*VDEC_MAX_CHN_NUM];
    FILE *fpYuv = NULL;
    HI_U32 u32PicLStride            = 0;
    HI_U32 u32PicCStride            = 0;
    HI_U32 u32LumaSize              = 0;
    HI_U32 u32ChrmSize              = 0;
    HI_U32 u32OutWidth              = 720;
    HI_U32 u32OutHeight             = 576;
    HI_CHAR OutFilename[100]        = {0};
	stSize.u32Width  = HD_WIDTH;
	stSize.u32Height = HD_HEIGHT;

    u32BlkSize = u32OutWidth*u32OutHeight*3>>1;

    snprintf(OutFilename, 100, "Sample_VGS_%d_%d_%s.yuv", 
            u32OutWidth, u32OutHeight,  "420");
    fpYuv = fopen(OutFilename, "wb");
	if(fpYuv == NULL)
	{
		printf("SAMPLE_TEST:can't open file %s to save yuv\n","Decompress.yuv");
		return HI_FAILURE;
	}
    
    /************************************************
       step1:  init SYS and common VB 
    *************************************************/
	SAMPLE_COMM_VDEC_Sysconf(&stVbConf, &stSize);
	s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
	if(s32Ret != HI_SUCCESS)
	{
	    SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
	    goto END1;
	}
	
	/************************************************
	  step2:  init mod common VB
    *************************************************/
	SAMPLE_COMM_VDEC_ModCommPoolConf(&stModVbConf, PT_H264, &stSize);	
	s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stModVbConf);
	if(s32Ret != HI_SUCCESS)
	{	    	
	    SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
	    goto END1;
	}


    /******************************************
     step 3: create private pool on ddr0
    ******************************************/
    hPool   = HI_MPI_VB_CreatePool( u32BlkSize, 10,NULL);
    if (hPool == VB_INVALID_POOLID)
    {
        SAMPLE_PRT("HI_MPI_VB_CreatePool failed! \n");
        goto END1;
    }

   
	/************************************************
	  step4:  start VDEC
    *************************************************/
	SAMPLE_COMM_VDEC_ChnAttr(1, &stVdecChnAttr[0], PT_H264, &stSize);
	s32Ret = SAMPLE_COMM_VDEC_Start(1, &stVdecChnAttr[0]);
	if(s32Ret != HI_SUCCESS)
	{	
	    SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
	    goto END2;
	}

	/************************************************
	step4:  send stream to VDEC
    *************************************************/
	SAMPLE_COMM_VDEC_ThreadParam(1, &stVdecSend[0], &stVdecChnAttr[0], SAMPLE_1080P_H264_PATH);	
	SAMPLE_COMM_VDEC_StartSendStream(1, &stVdecSend[0], &VdecThread[0]);


    u32PicLStride = CEILING_2_POWER(u32OutWidth, SAMPLE_SYS_ALIGN_WIDTH);
    u32PicCStride = CEILING_2_POWER(u32OutWidth, SAMPLE_SYS_ALIGN_WIDTH);
    u32LumaSize = (u32PicLStride * u32OutHeight);
    u32ChrmSize = (u32PicCStride * u32OutHeight) >> 2;

    for(i =0;i<10;i++)
    {
        SAMPLE_MEMBUF_S stMem = {0};
        VIDEO_FRAME_INFO_S stFrmInfo;
        VGS_HANDLE hHandle;
        stMem.hPool = hPool;
        VIDEO_FRAME_INFO_S stFrameInfo;
        VGS_TASK_ATTR_S stTask;
        s32Ret = HI_MPI_VDEC_GetImage(0, &stFrameInfo, -1);
        if(s32Ret != HI_SUCCESS)
    	{	
    	    SAMPLE_PRT("get vdec image failed\n");
    	    goto END3;
    	}
        
        while((stMem.hBlock = HI_MPI_VB_GetBlock(stMem.hPool, u32BlkSize,NULL)) == VB_INVALID_HANDLE)
        {
             ;
        }
  
        stMem.u32PhyAddr = HI_MPI_VB_Handle2PhysAddr(stMem.hBlock);

        stMem.pVirAddr = (HI_U8 *) HI_MPI_SYS_Mmap( stMem.u32PhyAddr, u32BlkSize );
        if(stMem.pVirAddr == NULL)
        {
            SAMPLE_PRT("Mem dev may not open\n");
            HI_MPI_VB_ReleaseBlock(stMem.hBlock);
            HI_MPI_VDEC_ReleaseImage(0, &stFrameInfo);
            goto END3;
        }
   
        memset(&stFrmInfo.stVFrame, 0, sizeof(VIDEO_FRAME_S));
        stFrmInfo.stVFrame.u32PhyAddr[0] = stMem.u32PhyAddr;
        stFrmInfo.stVFrame.u32PhyAddr[1] = stFrmInfo.stVFrame.u32PhyAddr[0] + u32LumaSize;
        stFrmInfo.stVFrame.u32PhyAddr[2] = stFrmInfo.stVFrame.u32PhyAddr[1] + u32ChrmSize;


        stFrmInfo.stVFrame.pVirAddr[0] = stMem.pVirAddr;
        stFrmInfo.stVFrame.pVirAddr[1] = (HI_U8 *) stFrmInfo.stVFrame.pVirAddr[0] + u32LumaSize;
        stFrmInfo.stVFrame.pVirAddr[2] = (HI_U8 *) stFrmInfo.stVFrame.pVirAddr[1] + u32ChrmSize;
 
        stFrmInfo.stVFrame.u32Width     = u32OutWidth;
        stFrmInfo.stVFrame.u32Height    = u32OutHeight;
        stFrmInfo.stVFrame.u32Stride[0] = u32PicLStride;
        stFrmInfo.stVFrame.u32Stride[1] = u32PicLStride;
        stFrmInfo.stVFrame.u32Stride[2] = u32PicLStride;

        stFrmInfo.stVFrame.enCompressMode = COMPRESS_MODE_NONE;
        stFrmInfo.stVFrame.enPixelFormat  = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        stFrmInfo.stVFrame.enVideoFormat  = VIDEO_FORMAT_LINEAR;

        stFrmInfo.stVFrame.u64pts     = (i * 40);
        stFrmInfo.stVFrame.u32TimeRef = (i * 2);

        stFrmInfo.u32PoolId = hPool;

        s32Ret = HI_MPI_VGS_BeginJob(&hHandle);
        if(s32Ret != HI_SUCCESS)
    	{	
    	    SAMPLE_PRT("HI_MPI_VGS_BeginJob failed\n");
            HI_MPI_VB_ReleaseBlock(stMem.hBlock);
            HI_MPI_VDEC_ReleaseImage(0, &stFrameInfo);
    	    goto END3;
    	}
   
        memcpy(&stTask.stImgIn,&stFrameInfo,sizeof(VIDEO_FRAME_INFO_S));

        
        memcpy(&stTask.stImgOut ,&stFrmInfo,sizeof(VIDEO_FRAME_INFO_S));
        s32Ret = HI_MPI_VGS_AddScaleTask(hHandle, &stTask);
        if(s32Ret != HI_SUCCESS)
    	{	
    	    SAMPLE_PRT("HI_MPI_VGS_AddScaleTask failed\n");
            HI_MPI_VGS_CancelJob(hHandle);
            HI_MPI_VB_ReleaseBlock(stMem.hBlock);
            HI_MPI_VDEC_ReleaseImage(0, &stFrameInfo);
    	    goto END3;
    	}

        s32Ret = HI_MPI_VGS_EndJob(hHandle);
        if(s32Ret != HI_SUCCESS)
    	{	
    	    SAMPLE_PRT("HI_MPI_VGS_EndJob failed\n");
            HI_MPI_VGS_CancelJob(hHandle);
            HI_MPI_VB_ReleaseBlock(stMem.hBlock);
            HI_MPI_VDEC_ReleaseImage(0, &stFrameInfo);
    	    goto END3;
    	}

        /*Save the yuv*/
        SAMPLE_VGS_SaveSP42XToPlanar(fpYuv, &stFrmInfo.stVFrame);
        fflush(fpYuv);
        HI_MPI_SYS_Munmap(stMem.pVirAddr,u32BlkSize);
        HI_MPI_VB_ReleaseBlock(stMem.hBlock);
        HI_MPI_VDEC_ReleaseImage(0, &stFrameInfo);
        printf("\rfinish saving picure : %d. ", i+1);
        fflush(stdout);   

    }
    
    SAMPLE_COMM_VDEC_StopSendStream(1, &stVdecSend[0], &VdecThread[0]);

    
END3:
 
	SAMPLE_COMM_VDEC_Stop(1);		

END2:
	HI_MPI_VB_DestroyPool( hPool );

END1:

    
	SAMPLE_COMM_SYS_Exit();	
    

    fclose(fpYuv);
    return 	s32Ret;
}


/******************************************************************************
* function    : main()
* Description : vgs sample
******************************************************************************/
int main(int argc, char *argv[])
{
    HI_S32 s32Ret = HI_SUCCESS;
    char ch;
    HI_BOOL bExit = HI_FALSE;
    signal(SIGINT, SAMPLE_VGS_HandleSig);
    signal(SIGTERM, SAMPLE_VGS_HandleSig);

    
    /******************************************
     1 choose the case
    ******************************************/
    while (1)
    {
        SAMPLE_VGS_Usage();
        ch = getchar();
        getchar();
        switch (ch)
        {
            case '0':
            {
                SAMPLE_VGS_Decompress_TilePicture();
                break;
            }
            case 'q':
            case 'Q':
            {
                bExit = HI_TRUE;
                break;
            }

            default :
            {
                printf("input invaild! please try again.\n");
                break;
            }
        }
        
        if (bExit)
        {
            break;
        }
    }

    return s32Ret;
}

