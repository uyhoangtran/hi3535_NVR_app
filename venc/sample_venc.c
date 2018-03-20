/******************************************************************************
  A simple program of Hisilicon mpp.
  Copyright (C), 2010-2021, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2013-7 Created
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "sample_comm.h"


VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;


//#define SAMPLE_YUV_1080P_FILEPATH         "sample_420_1080p.yuv"

/******************************************************************************
* function : show usage
******************************************************************************/
HI_VOID SAMPLE_VENC_Usage(HI_VOID)
{
    printf("\n\n/************************************/\n");
    printf("please choose the case which you want to run:\n");
    printf("\t0:  H264 \n");
    printf("\t1:  Jpeg\n");
    printf("\t2:  Venc transfer\n");    
    printf("\tq:  quit\n");
    printf("sample command:");
    return;
}

/******************************************************************************
* function : to process abnormal case                                         
******************************************************************************/
void SAMPLE_VENC_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}


/******************************************************************************
* function :  VDH->VPSS->VO->WBC->H264
******************************************************************************/
HI_S32 SAMPLE_VENC_H264(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;
	VENC_CHN VeH264Chn   = 0;
	SAMPLE_RC_E enRcMode;
	PIC_SIZE_E enSize = PIC_HD1080;
    PAYLOAD_TYPE_E enPtH264Type = PT_H264;
    #ifdef HI_FPGA
    HI_U32 u32PicWidth         = 1280;
    HI_U32 u32PicHeight        = 720;
    #else
	HI_U32 u32PicWidth         = 1920;
    HI_U32 u32PicHeight        = 1080;
    #endif
    VDEC_CHN_ATTR_S stVdecChnAttr;
    VdecThreadParam stSampleVdecSendParam[VDEC_MAX_CHN_NUM];
    pthread_t SampleVdecSendStream[VDEC_MAX_CHN_NUM];

    VPSS_CHN VpssChn = 0;
    VPSS_GRP_ATTR_S stGrpVpssAttr;
	
	VO_DEV VoDev	= 0;// 1;
	VO_LAYER VoLayer = 0;
    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
	SIZE_S  stSize; 
	HI_U32 u32VoFrmRate;
	SAMPLE_VO_MODE_E enMode = VO_MODE_1MUX;
	VO_WBC_SOURCE_S stWbcSource;
	VO_WBC VoWbc = 0;
	VO_WBC_ATTR_S stWbcAttr;

	VB_CONF_S       stVbConf,stModVbConf;
	HI_CHAR ch;
	HI_U32 u32BlkSize,u32WndNum,u32ChnCnt= 1;
	HI_S32 i;
	

	/******************************************
     step  1: init variable 
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

	u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                enSize, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);

	stVbConf.u32MaxPoolCnt = 128;
    
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt =  6;

    stVbConf.astCommPool[1].u32BlkSize = (1280*720)*2;
    stVbConf.astCommPool[1].u32BlkCnt =  1;

	/******************************************
     step 2: mpp system init. 
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_0;
    }

	/******************************************
     step 3: vdec mode vb init. 
    *****************************************/
    memset(&stModVbConf,0,sizeof(VB_CONF_S));

    s32Ret=SAMPLE_COMM_SYS_GetPicSize(gs_enNorm,enSize,&stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get pic size failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_0;
    }
    
	SAMPLE_COMM_VDEC_ModCommPoolConf(&stModVbConf, enPtH264Type,&stSize);
    s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stModVbConf);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vdec mode vb init failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_0;
    }
	/******************************************
     step 4:  start vdec.
    *****************************************/
    SAMPLE_COMM_VDEC_ChnAttr(u32ChnCnt,&stVdecChnAttr,enPtH264Type,&stSize);
	s32Ret =SAMPLE_COMM_VDEC_Start(u32ChnCnt,&stVdecChnAttr);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vdec failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_1;
    }

	/******************************************
     step 5:  start vpss and bind vdec. 
    *****************************************/
    stGrpVpssAttr.bHistEn   = 0;
    stGrpVpssAttr.bIeEn     = 0;
    stGrpVpssAttr.bNrEn     = 0;
    stGrpVpssAttr.bDciEn    = 0;
    stGrpVpssAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    stGrpVpssAttr.enPixFmt  = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    stGrpVpssAttr.u32MaxW   = 1920;
    stGrpVpssAttr.u32MaxH   = 1088;

	s32Ret = SAMPLE_COMM_VPSS_Start(u32ChnCnt+1,&stSize, u32ChnCnt+1,&stGrpVpssAttr );
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_2;
    }
    for(i=0;i<u32ChnCnt;i++)
    {	
        s32Ret = SAMPLE_COMM_VDEC_BindVpss(i,i);
    	if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("bind vpss to vdec failed with %d!\n", s32Ret);
            goto SAMPLE_VENC_H264_2;
        }
    }
	/******************************************
     step 6:  start vo hd0 and bind vpss. 
    *****************************************/
    #ifdef HI_FPGA
	stPubAttr.enIntfSync = VO_OUTPUT_720P60;
	#else
	stPubAttr.enIntfSync = VO_OUTPUT_1080P60;
	#endif

	stPubAttr.enIntfType = VO_INTF_HDMI| VO_INTF_BT1120|VO_INTF_VGA;
	stPubAttr.u32BgColor = 0x0000FF;


	stLayerAttr.bClusterMode = HI_FALSE;
	stLayerAttr.bDoubleFrame = HI_FALSE;
	stLayerAttr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;

	s32Ret = SAMPLE_COMM_VO_GetWH(stPubAttr.enIntfSync,&stSize.u32Width,&stSize.u32Height,&u32VoFrmRate);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get vo wh failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_3;
    }
	memcpy(&stLayerAttr.stImageSize,&stSize,sizeof(stSize)); 

	stLayerAttr.u32DispFrmRt = 30 ;
	stLayerAttr.stDispRect.s32X = 0;
	stLayerAttr.stDispRect.s32Y = 0;
	stLayerAttr.stDispRect.u32Width = stSize.u32Width;
	stLayerAttr.stDispRect.u32Height = stSize.u32Height;

	
	s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stPubAttr);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo dev failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_3;
	}
	
	s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo layer failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_4;
	}
	
	s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer, enMode);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo chn failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_5;
	}
	
	stWbcSource.enSourceType = VO_WBC_SOURCE_DEV;
	stWbcSource.u32SourceId = SAMPLE_VO_DEV_DHD0;
	s32Ret = SAMPLE_COMM_WBC_BindVo(VoWbc, &stWbcSource);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set vo wbc source failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_6;
	}

	stWbcAttr.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
	#ifdef HI_FPGA
	stWbcAttr.stTargetSize.u32Width = u32PicWidth;
	stWbcAttr.stTargetSize.u32Height = u32PicHeight;
	#else
	stWbcAttr.stTargetSize.u32Width = u32PicWidth;
	stWbcAttr.stTargetSize.u32Height = u32PicHeight;
	#endif
	stWbcAttr.u32FrameRate = 30;

	s32Ret = SAMPLE_COMM_VO_StartWbc(VoWbc,&stWbcAttr);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set wbc wbc attr failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_H264_7;
	}

    u32WndNum = 1;
    for(i = 0;i < u32WndNum;i++)
    {
        s32Ret = SAMPLE_COMM_VO_BindVpss(VoLayer,i,i,VpssChn);
        if (HI_SUCCESS != s32Ret)
        {
        	SAMPLE_PRT("bind vo to vpss failed with %d!\n", s32Ret);
        	goto SAMPLE_VENC_H264_7;
        }
    }
	/******************************************
     step 7: select rc mode and start h264e venc
    ******************************************/
    while(1)
    {
        printf("please choose rc mode:\n"); 
        printf("\t0) CBR\n"); 
        printf("\t1) VBR\n"); 
        printf("\t2) FIXQP\n"); 
        ch = getchar();
        getchar();
        if ('0' == ch)
        {
            enRcMode = SAMPLE_RC_CBR;
            break;
        }
        else if ('1' == ch)
        {
            enRcMode = SAMPLE_RC_VBR;
            break;
        }
        else if ('2' == ch)
        {
            enRcMode = SAMPLE_RC_FIXQP;
            break;
        }
        else
        {
            printf("rc mode invaild! please try again.\n");
            continue;
        }
    }
	#ifdef HI_FPGA
    s32Ret = SAMPLE_COMM_VENC_Start(VeH264Chn, enPtH264Type,gs_enNorm,PIC_HD720,enRcMode);
	#else
	s32Ret = SAMPLE_COMM_VENC_Start(VeH264Chn, enPtH264Type,gs_enNorm,enSize,enRcMode);
	#endif
	if (HI_SUCCESS != s32Ret)
    {
	   SAMPLE_PRT("Start h264 Venc failed!\n");
	   goto SAMPLE_VENC_H264_8;
    }

	s32Ret = SAMPLE_COMM_VENC_BindVo(VoDev,0,VeH264Chn);
	if (HI_SUCCESS != s32Ret)
    {
	   SAMPLE_PRT("bind venc and vo failed!\n");
	   goto SAMPLE_VENC_H264_9;
    }

    /******************************************
	step 8: vdec start send stream. 
	******************************************/
    SAMPLE_COMM_VDEC_ThreadParam(u32ChnCnt, stSampleVdecSendParam, &stVdecChnAttr, SAMPLE_1080P_H264_PATH);
    SAMPLE_COMM_VDEC_StartSendStream(u32ChnCnt,stSampleVdecSendParam,SampleVdecSendStream);
    
	/******************************************
	step 9: stream venc process -- get stream, then save it to file. 
	******************************************/
	s32Ret = SAMPLE_COMM_VENC_StartGetStream(u32ChnCnt);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Start Venc failed!\n");
		goto SAMPLE_VENC_H264_10;
	}

    SAMPLE_COMM_VDEC_CmdCtrl(u32ChnCnt,stSampleVdecSendParam);
    
	/******************************************
     step 10: exit process
    ******************************************/
	SAMPLE_COMM_VENC_StopGetStream();
    SAMPLE_COMM_VDEC_StopSendStream(u32ChnCnt,stSampleVdecSendParam,SampleVdecSendStream);
    
SAMPLE_VENC_H264_10:
	SAMPLE_COMM_VENC_UnBindVo(VoDev,0,VeH264Chn);
    
SAMPLE_VENC_H264_9:
    SAMPLE_COMM_VENC_Stop(VeH264Chn);
    
SAMPLE_VENC_H264_8:
    for(i = 0;i < u32WndNum;i++)
    {
        SAMPLE_COMM_VO_UnBindVpss(VoLayer,i,i,VpssChn);
    }
    
SAMPLE_VENC_H264_7:
	SAMPLE_COMM_VO_StopWbc(VoWbc);
    
SAMPLE_VENC_H264_6:
	SAMPLE_COMM_VO_StopChn(VoLayer, enMode);
    
SAMPLE_VENC_H264_5:
	SAMPLE_COMM_VO_StopLayer(VoLayer);
    
SAMPLE_VENC_H264_4:
	SAMPLE_COMM_VO_StopDev(VoDev);

SAMPLE_VENC_H264_3:
	for(i=0;i<u32ChnCnt;i++)
    {
	    SAMPLE_COMM_VDEC_UnBindVpss(i,i);
    }
    
SAMPLE_VENC_H264_2:
  SAMPLE_COMM_VPSS_Stop(u32ChnCnt,u32ChnCnt);
    
SAMPLE_VENC_H264_1:
	SAMPLE_COMM_VDEC_Stop(u32ChnCnt);
    
SAMPLE_VENC_H264_0:
	SAMPLE_COMM_SYS_Exit();
    
    return s32Ret;
}

/******************************************************************************
* function :  VDH->VPSS->JPEG

******************************************************************************/
HI_S32 SAMPLE_VENC_Jpeg(HI_VOID)
{   
    HI_S32 s32Ret = HI_SUCCESS;
    VENC_CHN VeJpegChn   = 0;
	PIC_SIZE_E enSize = PIC_HD1080;
    PAYLOAD_TYPE_E enPtH264Type = PT_H264;
    VDEC_CHN_ATTR_S stVdecChnAttr;
    VIDEO_DISPLAY_MODE_E enDisplayMode = VIDEO_DISPLAY_MODE_PREVIEW;

    VdecThreadParam stSampleVdecSendParam[VDEC_MAX_CHN_NUM];
    pthread_t SampleVdecSendStream[VDEC_MAX_CHN_NUM];

	VPSS_GRP VpssGrp = 0;
    VPSS_CHN VpssChn = 0;
    VPSS_GRP_ATTR_S stGrpVpssAttr;
    VB_CONF_S stVbConf,stModVbConf;
	HI_U32 u32BlkSize,u32ChnCnt= 1;
	HI_S32 i;
    SIZE_S  stSize; 

    /******************************************
     step  1: init variable 
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

	u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                enSize, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);

	stVbConf.u32MaxPoolCnt = 128;
    
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt =  6;

    stVbConf.astCommPool[1].u32BlkSize = (1280*720)*2;
    stVbConf.astCommPool[1].u32BlkCnt =  1;

	/******************************************
     step 2: mpp system init. 
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_Jpeg_0;
    }

	/******************************************
     step 3: vdec mode vb init. 
    *****************************************/
    memset(&stModVbConf,0,sizeof(VB_CONF_S));

    s32Ret=SAMPLE_COMM_SYS_GetPicSize(gs_enNorm,enSize,&stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get pic size failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_Jpeg_0;
    }
    
	SAMPLE_COMM_VDEC_ModCommPoolConf(&stModVbConf, enPtH264Type,&stSize);

    s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stModVbConf);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vdec mode vb init failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_Jpeg_0;
    }
	/******************************************
     step 4:  start vdec and send stream.
    *****************************************/
    SAMPLE_COMM_VDEC_ChnAttr(u32ChnCnt,&stVdecChnAttr,enPtH264Type,&stSize);
	s32Ret =SAMPLE_COMM_VDEC_Start(u32ChnCnt,&stVdecChnAttr);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vdec failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_Jpeg_1;
    }
    
    for (i = 0; i < u32ChnCnt; i++)
    {
        s32Ret = HI_MPI_VDEC_SetDisplayMode(i, enDisplayMode);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("Chn %d SetDisplayMode err%#x! \n", i, s32Ret);
            goto SAMPLE_VENC_Jpeg_1;
        }
    }
	/******************************************
     step 5:  start vpss and bind vdec. 
    *****************************************/
    stGrpVpssAttr.bHistEn   = 0;
    stGrpVpssAttr.bIeEn     = 0;
    stGrpVpssAttr.bNrEn     = 0;
    stGrpVpssAttr.bDciEn    = 0;
    stGrpVpssAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    stGrpVpssAttr.enPixFmt  = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    stGrpVpssAttr.u32MaxW   = 1920;
    stGrpVpssAttr.u32MaxH   = 1088;

	s32Ret = SAMPLE_COMM_VPSS_Start(u32ChnCnt,&stSize, u32ChnCnt,&stGrpVpssAttr );
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_Jpeg_2;
    }

    for(i=0;i<u32ChnCnt;i++)
    {	
        s32Ret = SAMPLE_COMM_VDEC_BindVpss(i,i);
    	if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("bind vpss to vdec failed with %d!\n", s32Ret);
            goto SAMPLE_VENC_Jpeg_2;
        }
    }

    /******************************************
     step 6:  start jpege snap.
    *****************************************/
    s32Ret = SAMPLE_COMM_VENC_SnapStart(VeJpegChn,&stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_Jpeg_3;
    }

    /******************************************
     step 7: vdec start send stream.
    *****************************************/
    SAMPLE_COMM_VDEC_ThreadParam(u32ChnCnt, stSampleVdecSendParam, &stVdecChnAttr, SAMPLE_1080P_H264_PATH);
    SAMPLE_COMM_VDEC_StartSendStream(u32ChnCnt,stSampleVdecSendParam,SampleVdecSendStream);

    /******************************************
     step 8:  start jpege process.
    *****************************************/
    s32Ret = SAMPLE_COMM_VENC_SnapProcess(VeJpegChn,VpssGrp,VpssChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start jpegsnap failed with %d!\n", s32Ret);
        goto SAMPLE_VENC_Jpeg_4;
    }
      
SAMPLE_VENC_Jpeg_4:
    SAMPLE_COMM_VENC_SnapStop(VeJpegChn);
    SAMPLE_COMM_VDEC_StopSendStream(u32ChnCnt,stSampleVdecSendParam,SampleVdecSendStream);

SAMPLE_VENC_Jpeg_3:
    for(i=0;i<u32ChnCnt;i++)
    {
	    SAMPLE_COMM_VDEC_UnBindVpss(i,i);
    }

SAMPLE_VENC_Jpeg_2:
	SAMPLE_COMM_VPSS_Stop(u32ChnCnt,u32ChnCnt);
    
SAMPLE_VENC_Jpeg_1:
	SAMPLE_COMM_VDEC_Stop(u32ChnCnt);
      
SAMPLE_VENC_Jpeg_0:
	SAMPLE_COMM_SYS_Exit();
    
    return s32Ret;
}


/******************************************************************************
* function :  H264 Transfer(1080P -> 720P)
******************************************************************************/
HI_S32 SAMPLE_VENC_Transfer(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;
	VENC_CHN VeH264Chn   = 0;
	SAMPLE_RC_E enRcMode;
	PIC_SIZE_E enSize = PIC_HD1080;
    PAYLOAD_TYPE_E enPtH264Type = PT_H264;
    VDEC_CHN_ATTR_S stVdecChnAttr;
    
    VdecThreadParam stSampleVdecSendParam[VDEC_MAX_CHN_NUM];
    pthread_t SampleVdecSendStream[VDEC_MAX_CHN_NUM];

    VB_CONF_S       stVbConf,stModVbConf;
	HI_CHAR ch;
	HI_U32 u32BlkSize,u32ChnCnt= 1;
	HI_S32 i;
    SIZE_S  stSize;

    /******************************************
     step  1: init variable 
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

	u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                enSize, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
	stVbConf.u32MaxPoolCnt = 64;

    u32BlkSize = 1920*1088*2;
    
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt =  6;

    stVbConf.astCommPool[1].u32BlkSize = (1280*720)*2;
    stVbConf.astCommPool[1].u32BlkCnt =  6;

	/******************************************
     step 2: mpp system init. 
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("System init failed with %d!\n", s32Ret);
        goto END_VENC_TRANSFER_0;
    }
    
    /******************************************
    step 3: vdec mode vb init. 
    *****************************************/
    memset(&stModVbConf,0,sizeof(VB_CONF_S));
    
    s32Ret=SAMPLE_COMM_SYS_GetPicSize(gs_enNorm,enSize,&stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get pic size failed with %d!\n", s32Ret);
        goto END_VENC_TRANSFER_0;
    }
	SAMPLE_COMM_VDEC_ModCommPoolConf(&stModVbConf, enPtH264Type,&stSize);

    s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stModVbConf);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Vdec mode vb init failed with %d!\n", s32Ret);
        goto END_VENC_TRANSFER_0;
    }
    
	/******************************************
     step 4: start vdec chn and send stream
    ******************************************/
    SAMPLE_COMM_VDEC_ChnAttr(u32ChnCnt,&stVdecChnAttr,enPtH264Type,&stSize);
    s32Ret = SAMPLE_COMM_VDEC_Start(u32ChnCnt,&stVdecChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start vdec failed with %d!\n", s32Ret);
        goto END_VENC_TRANSFER_1;
    }

	/******************************************
     step 5: select rc mode and start venc
    ******************************************/
    while(1)
    {
        printf("please choose rc mode:\n"); 
        printf("\t0) CBR\n"); 
        printf("\t1) VBR\n"); 
        printf("\t2) FIXQP\n"); 
        ch = getchar();
        getchar();
        if ('0' == ch)
        {
            enRcMode = SAMPLE_RC_CBR;
            break;
        }
        else if ('1' == ch)
        {
            enRcMode = SAMPLE_RC_VBR;
            break;
        }
        else if ('2' == ch)
        {
            enRcMode = SAMPLE_RC_FIXQP;
            break;
        }
        else
        {
            printf("rc mode invaild! please try again.\n");
            continue;
        }
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enSize, &stSize);
	if (HI_SUCCESS != s32Ret)
    {
	   SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
	   goto END_VENC_TRANSFER_1;
    }
    
    s32Ret = SAMPLE_COMM_VENC_Start(VeH264Chn, enPtH264Type,gs_enNorm,PIC_HD720,enRcMode);
    if (HI_SUCCESS != s32Ret)
    {
	   SAMPLE_PRT("Start Venc failed!\n");
	   goto END_VENC_TRANSFER_2;
    }
    
    /******************************************
     step 6: bind vdec and venc
    ******************************************/
    for(i=0;i<u32ChnCnt;i++)
    {
       s32Ret = SAMPLE_COMM_VDEC_BindVenc(i,i);
       if (HI_SUCCESS != s32Ret)
       {
            SAMPLE_PRT("Start vdec failed with %d!\n", s32Ret);
            goto END_VENC_TRANSFER_2;
       }
    }

    /******************************************
     step 7: vdec start send stream.  
    ******************************************/
    SAMPLE_COMM_VDEC_ThreadParam(u32ChnCnt, stSampleVdecSendParam, &stVdecChnAttr, SAMPLE_1080P_H264_PATH);
    SAMPLE_COMM_VDEC_StartSendStream(u32ChnCnt,stSampleVdecSendParam,SampleVdecSendStream);

	/******************************************
     step 8: stream venc process -- get stream, then save it to file. 
    ******************************************/
    s32Ret = SAMPLE_COMM_VENC_StartGetStream(u32ChnCnt);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_TRANSFER_3;
    } 

    SAMPLE_COMM_VDEC_CmdCtrl(u32ChnCnt,stSampleVdecSendParam);

    /******************************************
     step 9: exit process
    ******************************************/
	SAMPLE_COMM_VENC_StopGetStream();
    SAMPLE_COMM_VDEC_StopSendStream(u32ChnCnt,stSampleVdecSendParam,SampleVdecSendStream);

END_VENC_TRANSFER_3:
    for(i=0;i<u32ChnCnt;i++)
    {
        SAMPLE_COMM_VDEC_UnBindVenc(i,i);
    }

END_VENC_TRANSFER_2:
	s32Ret=SAMPLE_COMM_VENC_Stop(VeH264Chn);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Stop encode failed!\n");
        goto END_VENC_TRANSFER_2;
    } 
    
END_VENC_TRANSFER_1:
	SAMPLE_COMM_VDEC_Stop(u32ChnCnt);
	 
END_VENC_TRANSFER_0 :
	SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}


/******************************************************************************
* function    : main()
* Description : video venc sample
******************************************************************************/
int main(int argc, char *argv[])
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_CHAR ch;
    HI_BOOL bExit = HI_FALSE;

    signal(SIGINT, SAMPLE_VENC_HandleSig);
    signal(SIGTERM, SAMPLE_VENC_HandleSig);

    while (1)
    {
        SAMPLE_VENC_Usage();
        ch = getchar();
        getchar();
        switch (ch)
        {
            case '0':   
            {
                s32Ret = SAMPLE_VENC_H264();
                break;
            }
            case '1':   
            {
                s32Ret = SAMPLE_VENC_Jpeg();
                break;
            }
            case '2':   
            {
                s32Ret = SAMPLE_VENC_Transfer();
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

