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
 
/******************************************************************************
* function : show usage
******************************************************************************/
HI_VOID SAMPLE_VO_Usage(HI_VOID)
{
    printf("\n\n/************************************/\n");
    printf("please choose the case which you want to run:\n");
    printf("\t0:  Preview_HD0_HD1_SD0\n");
    printf("\t1:  Playback_HD0\n");
    printf("\t2:  ZoomIn_HD0\n");
    printf("\t3:  ZoomIn_SD0\n");    
    printf("\t4:  MutiArea_HD0\n");
    printf("\tq:  quit\n");
    printf("sample command:");
    return;
}

/******************************************************************************
* function : to process abnormal case                                         
******************************************************************************/
void SAMPLE_VO_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}


/******************************************************************************
* function : HD0(1080P: 4 windows)->WBC->SD0
             HD1(1080P: 1 full view window and 3 pip windows)
******************************************************************************/
HI_S32 SAMPLE_VO_Preview_HD0_HD1_SD0(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;    
    VB_CONF_S stVbConf;    
    HI_U32 u32BlkSize;
    VDEC_CHN VdChn = 4;
    PAYLOAD_TYPE_E enType;    
    SIZE_S stSize;    
    HI_S32 s32VpssGrpCnt = 4;    
    HI_S32 i;
    HI_U32 u32WndNum;
    VPSS_GRP_ATTR_S stGrpAttr;     
    VO_DEV VoDev;    
    VO_LAYER VoLayer;
    VO_PUB_ATTR_S stVoPubAttr; 
    SAMPLE_VO_MODE_E enVoMode;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    VO_WBC VoWbc;
    VO_WBC_ATTR_S stWbcAttr;    
    VO_WBC_SOURCE_S stWbcSource;    
    VO_CHN_ATTR_S stChnAttr;
    POINT_S stDispPos;    
    HI_CHAR ch;
	VDEC_CHN_ATTR_S stVdecChnAttr[VDEC_MAX_CHN_NUM];
	VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];    
	pthread_t   VdecThread[2 * VDEC_MAX_CHN_NUM];
    /******************************************
     step  1: init variable 
    ******************************************/    
    memset(&stVbConf,0,sizeof(VB_CONF_S));     
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(VIDEO_ENCODING_MODE_PAL,\
                PIC_HD1080, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.u32MaxPoolCnt = 128;
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt  = 8;
    
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(VIDEO_ENCODING_MODE_PAL, PIC_HD1080, &stSize);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return HI_FAILURE;
    }
    
    if (704 == stSize.u32Width)
    {
        stSize.u32Width = 720;
    }
    else if (352 == stSize.u32Width)
    {
        stSize.u32Width = 360;
    }
    else if (176 == stSize.u32Width)
    {
        stSize.u32Width = 180;
    }
    /******************************************
     step 2: mpp system init. 
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto END_1080P_1;
    }
    
    /******************************************
     step 3: start vdec 
    ******************************************/     
    enType = PT_H264;
    memset(&stVbConf,0,sizeof(VB_CONF_S));        
	SAMPLE_COMM_VDEC_ModCommPoolConf(&stVbConf, enType, &stSize);	
	s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stVbConf);
	if(s32Ret != HI_SUCCESS)
	{	    	
	    SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
	    goto END_1080P_1;
	}
    /**************create vdec chn****************************/    
	SAMPLE_COMM_VDEC_ChnAttr(VdChn, &stVdecChnAttr[0], enType, &stSize);
	s32Ret = SAMPLE_COMM_VDEC_Start(VdChn, &stVdecChnAttr[0]);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vdec failed!\n");
        goto END_1080P_2;
    } 
    /**************send stream****************************/
	SAMPLE_COMM_VDEC_ThreadParam(VdChn, &stVdecSend[0], &stVdecChnAttr[0], SAMPLE_1080P_H264_PATH);	
	SAMPLE_COMM_VDEC_StartSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
    /******************************************
     step 4: start vpss with vdec bind vpss 
    ******************************************/
    stGrpAttr.u32MaxW = stSize.u32Width;
    stGrpAttr.u32MaxH = stSize.u32Height;
    stGrpAttr.bIeEn = HI_FALSE;
    stGrpAttr.bNrEn = HI_TRUE;
    stGrpAttr.bHistEn = HI_FALSE;
    stGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;
    stGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;

    s32Ret = SAMPLE_COMM_VPSS_Start(s32VpssGrpCnt, &stSize, VPSS_MAX_CHN_NUM,NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vpss failed!\n");
        goto END_1080P_3;
    }
    for(i = 0;i < VdChn;i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_BindVpss(i,i);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VDEC_BindVpss failed!\n");
            goto END_1080P_4;
        }
    }
    
    /***************************************************
     step 5: start  HD0 with 4 windows and bind to vpss (WBC source) 
    ***************************************************/    
    /**************start Dev****************************/
    VoDev = SAMPLE_VO_DEV_DHD0;    
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60; 
    stVoPubAttr.enIntfType = VO_INTF_VGA|VO_INTF_HDMI;
    stVoPubAttr.u32BgColor = 0x000000ff;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);    
	if (HI_SUCCESS != s32Ret)
	{
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
        goto END_1080P_4;
	}
    
    if (HI_SUCCESS != SAMPLE_COMM_VO_HdmiStart(stVoPubAttr.enIntfSync))
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_HdmiStart failed!\n");
        goto END_1080P_4;
    }
    /**************start Layer****************************/
    VoLayer = SAMPLE_VO_LAYER_VHD0;
    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;    
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_5;
    }
    stLayerAttr.stImageSize.u32Width = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
	if (HI_SUCCESS != s32Ret)
	{
	   SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
	   goto END_1080P_6;
	}    
    /**************start Chn****************************/    
    enVoMode = VO_MODE_4MUX;
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer,enVoMode);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_6;
    }      
    /**************vo bind to vpss****************************/
    u32WndNum = 4;
    for(i = 0;i < u32WndNum;i++)
    {
    	s32Ret = SAMPLE_COMM_VO_BindVpss(VoLayer, i, i, 0);
    	if (HI_SUCCESS != s32Ret)
    	{
    	   SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
    	   goto END_1080P_7;
    	}    
    }
    
    /***************************************************
     step 6: start vo SD0 (CVBS) (WBC target) 
    ***************************************************/
    /**************start Dev****************************/
    VoDev = SAMPLE_VO_DEV_DSD0;    
    stVoPubAttr.enIntfSync = VO_OUTPUT_PAL;
    stVoPubAttr.enIntfType = VO_INTF_CVBS;
    stVoPubAttr.u32BgColor = 0x000000ff;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);    
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
       goto END_1080P_7;
    }
    /**************start Layer****************************/
    VoLayer = SAMPLE_VO_LAYER_VSD0;
    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;    
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_8;
    }
    stLayerAttr.stImageSize.u32Width = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;
    
    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
       goto END_1080P_9;
    }    
    /**************start Chn****************************/    
    enVoMode = VO_MODE_1MUX;
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer,enVoMode);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_9;
    }
    /***************************************************
     step 7: start  Wbc and bind to SD0
    ***************************************************/    
    VoWbc = SAMPLE_VO_WBC_BASE;
    /**************Wbc bind source*********************/
    stWbcSource.enSourceType = VO_WBC_SOURCE_DEV;
    stWbcSource.u32SourceId = SAMPLE_VO_DEV_DHD0;
    
    s32Ret = SAMPLE_COMM_WBC_BindVo(VoWbc,&stWbcSource);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_9;
    }    
    /**************start Wbc****************************/
    stWbcAttr.enPixelFormat = SAMPLE_PIXEL_FORMAT;
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stWbcAttr.stTargetSize.u32Width, &stWbcAttr.stTargetSize.u32Height, &stWbcAttr.u32FrameRate);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_9;
    }
    s32Ret = SAMPLE_COMM_VO_StartWbc(VoWbc,&stWbcAttr);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_9;
    }
    /**************bind wbc to target****************************/       
    s32Ret = SAMPLE_COMM_VO_BindVoWbc(SAMPLE_VO_DEV_DHD0,SAMPLE_VO_LAYER_VSD0,0);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_10;
    }
    /***************************************************
     step 8: start  HD1 with 1 window and 3 Pip window
    ***************************************************/
    /**************start Dev DHD1****************************/
    VoDev = SAMPLE_VO_DEV_DHD1;    
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;
    stVoPubAttr.enIntfType = VO_INTF_BT1120;
    stVoPubAttr.u32BgColor = 0x000000ff;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
        goto END_1080P_11;
    }
    /**************start Layer VHD1  for PIP****************************/
    VoLayer = SAMPLE_VO_LAYER_VHD1;
    stLayerAttr.bClusterMode = HI_TRUE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;    
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_12;
    }
    stLayerAttr.stImageSize.u32Width = 360 * 3;
    stLayerAttr.stImageSize.u32Height = 270;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
        goto END_1080P_13;
    }    
    /**************start 3 Chn in  VHD1 Layer****************************/
    u32WndNum = 3;
    stChnAttr.bDeflicker = HI_FALSE;
    stChnAttr.u32Priority = 0;
    for(i = 0;i < u32WndNum;i++)
    {
        stChnAttr.stRect.s32X = i * 360 ;
        stChnAttr.stRect.s32Y = 0;
        stChnAttr.stRect.u32Width = 360;
        stChnAttr.stRect.u32Height = 270;
        
        s32Ret = HI_MPI_VO_SetChnAttr(VoLayer, i, &stChnAttr);
        if (s32Ret != HI_SUCCESS)
        {
            printf("%s(%d):failed with %#x!\n",\
                   __FUNCTION__,__LINE__,  s32Ret);
            goto END_1080P_13;
        }

        s32Ret = HI_MPI_VO_EnableChn(VoLayer, i);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            goto END_1080P_13;
        }

        stDispPos.s32X = 420 + stChnAttr.stRect.s32X;
        stDispPos.s32Y = 800;
        s32Ret = HI_MPI_VO_SetChnDispPos(VoLayer,i,&stDispPos);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            goto END_1080P_13;
        }
        
    }     
    /**************vo bind to vpss 1 chn in group*************/
    for(i = 0;i < u32WndNum;i++)
    {
        s32Ret = SAMPLE_COMM_VO_BindVpss(VoLayer, i, i, 1);
        if (HI_SUCCESS != s32Ret)
        {
           SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
           goto END_1080P_14;
        }    
    }
    /************start Layer VPIP  for base layer***********/
    VoLayer = SAMPLE_VO_LAYER_VPIP;
    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;    
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_14;
    }
    stLayerAttr.stImageSize.u32Width = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
       goto END_1080P_14;
    }  
    s32Ret = HI_MPI_VO_SetVideoLayerPriority(VoLayer,SAMPLE_VO_LAYER_PRIORITY_BASE);    
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
       goto END_1080P_15;
    }     
    /**************start 1 Chn in  VPIP Layer****************************/    
    enVoMode = VO_MODE_1MUX;
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer,enVoMode);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_1080P_16;
    }
    /**************vo bind to vpss 1 chn in group*************/    
    s32Ret = SAMPLE_COMM_VO_BindVpss(VoLayer, 0, 3, 1);
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
       goto END_1080P_16;
    }    

    while(1)
    {
        printf("press 'q' to exit this sample.\n");        
        ch = getchar();
        getchar();        
        if ('q' == ch)
        {
            break;
        }
        else
        {
            SAMPLE_PRT("the input is invaild! please try again.\n");
            continue;
        }

    }
    /******************************************
     step 9: exit process
    ******************************************/
    END_1080P_16:        
        VoLayer = SAMPLE_VO_LAYER_VPIP;        
        enVoMode = VO_MODE_1MUX;        
        SAMPLE_COMM_VO_StopChn(VoLayer,enVoMode);
    END_1080P_15:
        VoLayer = SAMPLE_VO_LAYER_VPIP;        
        SAMPLE_COMM_VO_StopLayer(VoLayer);
    END_1080P_14:        
        VoLayer = SAMPLE_VO_LAYER_VHD1;        
        u32WndNum = 3;
        for(i = 0;i < u32WndNum;i++)
        {
            SAMPLE_COMM_VO_UnBindVpss(VoLayer,i,i,1);
        }
    END_1080P_13:
        VoLayer = SAMPLE_VO_LAYER_VHD1;        
        u32WndNum = 3;
        for(i = 0; i< u32WndNum;i++)
        {
            HI_MPI_VO_DisableChn(VoLayer, i);
        }
        SAMPLE_COMM_VO_StopLayer(VoLayer);
    END_1080P_12:
        VoDev = SAMPLE_VO_DEV_DHD1;
        SAMPLE_COMM_VO_StopDev(VoDev); 
    END_1080P_11:
        VoWbc = SAMPLE_VO_WBC_BASE;        
        VoLayer = SAMPLE_VO_LAYER_VSD0;
        SAMPLE_COMM_VO_BindVoWbc(VoWbc,VoLayer,0);
    END_1080P_10:         
        VoWbc = SAMPLE_VO_WBC_BASE;
        SAMPLE_COMM_VO_StopWbc(VoWbc);
    END_1080P_9:
        VoLayer = SAMPLE_VO_LAYER_VSD0;
        enVoMode = VO_MODE_1MUX;        
        SAMPLE_COMM_VO_StopChn(VoLayer,enVoMode);
        SAMPLE_COMM_VO_StopLayer(VoLayer);
    END_1080P_8:        
        VoDev = SAMPLE_VO_DEV_DSD0;
        SAMPLE_COMM_VO_StopDev(VoDev);        
    END_1080P_7:        
        VoLayer = SAMPLE_VO_LAYER_VHD0;        
        u32WndNum = 4;        
        for(i = 0;i < u32WndNum;i++)
        {
            SAMPLE_COMM_VO_UnBindVpss(VoLayer,i,i,0);
        }
    END_1080P_6:
        VoLayer = SAMPLE_VO_LAYER_VHD0;
        enVoMode = VO_MODE_4MUX;        
        SAMPLE_COMM_VO_StopChn(VoLayer,enVoMode);        
        SAMPLE_COMM_VO_StopLayer(VoLayer);
    END_1080P_5:        
        VoDev = SAMPLE_VO_DEV_DHD0;        
        SAMPLE_COMM_VO_HdmiStop();
        SAMPLE_COMM_VO_StopDev(VoDev);   
    END_1080P_4:        
        for(i = 0;i < VdChn;i++)
        {
           SAMPLE_COMM_VDEC_UnBindVpss(i,i);
        }
    END_1080P_3:        
        SAMPLE_COMM_VPSS_Stop(s32VpssGrpCnt, VPSS_MAX_CHN_NUM);
    END_1080P_2:
        SAMPLE_COMM_VDEC_Stop(VdChn); 
         SAMPLE_COMM_VDEC_StopSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
    END_1080P_1:
        SAMPLE_COMM_SYS_Exit();
    
    return s32Ret;
}


/******************************************************************************
* function :  HD0 Playback with control
******************************************************************************/
HI_S32 SAMPLE_VO_Playback_HD0(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;    
    VB_CONF_S stVbConf;    
    HI_U32 u32BlkSize;
    VDEC_CHN VdChn = 1;
    PAYLOAD_TYPE_E enType;    
    SIZE_S stSize;    
    HI_S32 s32VpssGrpCnt = 1;    
    HI_S32 i;
    HI_U32 u32WndNum;
    VPSS_GRP_ATTR_S stGrpAttr;    
    VO_DEV VoDev;    
    VO_LAYER VoLayer;
    VO_CHN VoChn;
    VO_PUB_ATTR_S stVoPubAttr; 
    SAMPLE_VO_MODE_E enVoMode;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;    
    HI_CHAR ch;    
	VDEC_CHN_ATTR_S stVdecChnAttr[VDEC_MAX_CHN_NUM];
	VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];    
	pthread_t   VdecThread[2 * VDEC_MAX_CHN_NUM];
    /******************************************
     step  1: init variable 
    ******************************************/    
    memset(&stVbConf,0,sizeof(VB_CONF_S));     
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(VIDEO_ENCODING_MODE_PAL,\
                PIC_HD1080, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.u32MaxPoolCnt = 128;
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt  = 8;
    
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(VIDEO_ENCODING_MODE_PAL, PIC_HD1080, &stSize);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return HI_FAILURE;
    }
    
    if (704 == stSize.u32Width)
    {
        stSize.u32Width = 720;
    }
    else if (352 == stSize.u32Width)
    {
        stSize.u32Width = 360;
    }
    else if (176 == stSize.u32Width)
    {
        stSize.u32Width = 180;
    }
    /******************************************
     step 2: mpp system init. 
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto END_PLAYBACK_1;
    }
    
    /******************************************
     step 3: start vdec 
    ******************************************/    
    enType = PT_H264;
    memset(&stVbConf,0,sizeof(VB_CONF_S));        
    SAMPLE_COMM_VDEC_ModCommPoolConf(&stVbConf, enType, &stSize);    
    s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stVbConf);
    if(s32Ret != HI_SUCCESS)
    {           
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END_PLAYBACK_1;
    }
    /**************create vdec chn****************************/    
    SAMPLE_COMM_VDEC_ChnAttr(VdChn, &stVdecChnAttr[0], enType, &stSize);
    s32Ret = SAMPLE_COMM_VDEC_Start(VdChn, &stVdecChnAttr[0]);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vdec failed!\n");
        goto END_PLAYBACK_2;
    }    
    /**************send stream****************************/
	SAMPLE_COMM_VDEC_ThreadParam(VdChn, &stVdecSend[0], &stVdecChnAttr[0], SAMPLE_1080P_H264_PATH);	
	SAMPLE_COMM_VDEC_StartSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
    /******************************************
     step 4: start vpss with vdec bind vpss 
    ******************************************/
    stGrpAttr.u32MaxW = stSize.u32Width;
    stGrpAttr.u32MaxH = stSize.u32Height;
    stGrpAttr.bIeEn = HI_FALSE;
    stGrpAttr.bNrEn = HI_TRUE;
    stGrpAttr.bHistEn = HI_FALSE;
    stGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;
    stGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;

    s32Ret = SAMPLE_COMM_VPSS_Start(s32VpssGrpCnt, &stSize, VPSS_MAX_CHN_NUM,NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vpss failed!\n");
        goto END_PLAYBACK_3;
    }
    for(i = 0;i < VdChn;i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_BindVpss(i,i);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VDEC_BindVpss failed!\n");
            goto END_PLAYBACK_4;
        }
    }    
    /******************************************
     step 5: start DHD0 for playback
    ******************************************/    
    /**************start Dev****************************/
    VoDev = SAMPLE_VO_DEV_DHD0;    
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;
    stVoPubAttr.enIntfType = VO_INTF_VGA|VO_INTF_HDMI;
    stVoPubAttr.u32BgColor = 0x000000ff;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);    
	if (HI_SUCCESS != s32Ret)
	{
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
        goto END_PLAYBACK_4;
	}
    
    if (HI_SUCCESS != SAMPLE_COMM_VO_HdmiStart(stVoPubAttr.enIntfSync))
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_HdmiStart failed!\n");
        goto END_PLAYBACK_4;
    }
    /**************start Layer****************************/
    VoLayer = SAMPLE_VO_LAYER_VHD0;
    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;    
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_PLAYBACK_5;
    }
    stLayerAttr.stImageSize.u32Width = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
	if (HI_SUCCESS != s32Ret)
	{
	   SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
	   goto END_PLAYBACK_5;
	}    
    /**************start Chn****************************/    
    enVoMode = VO_MODE_1MUX;
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer,enVoMode);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_PLAYBACK_6;
    }    
    /**************vo bind to vpss****************************/
    u32WndNum = 1;
    for(i = 0;i < u32WndNum;i++)
    {
    	s32Ret = SAMPLE_COMM_VO_BindVpss(VoLayer, i, i, 0);
    	if (HI_SUCCESS != s32Ret)
    	{
    	   SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
    	   goto END_PLAYBACK_7;
    	}    
    }
    /******************************************
     step 6:  playback
    ******************************************/     
    VoChn = 0; 
    sleep(4);
    printf("================picture is  Pausing now!!===================\n");
    s32Ret = HI_MPI_VO_PauseChn(VoLayer, VoChn);  
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("HI_MPI_VO_PauseChn failed!\n");
       goto END_PLAYBACK_7;
    }
    sleep(4);
    printf("================picture go  to Step!!===================\n");
    for (i = 0;i < 5; i++)
    {
        s32Ret = HI_MPI_VO_StepChn(VoLayer, VoChn);  
        if (HI_SUCCESS != s32Ret)
        {
           SAMPLE_PRT("HI_MPI_VO_StepChn failed!\n");
           goto END_PLAYBACK_7;
        }
        sleep(1);
    }    
    sleep(4);
    printf("================picture go  to Resume!!===================\n");
    s32Ret = HI_MPI_VO_ResumeChn(VoLayer, VoChn);  
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("HI_MPI_VO_ResumeChn failed!\n");
       goto END_PLAYBACK_7;
    }
    sleep(4);
    printf("================picture go  to Hide!!===================\n");
    s32Ret = HI_MPI_VO_HideChn(VoLayer, VoChn);  
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("HI_MPI_VO_HideChn failed!\n");
       goto END_PLAYBACK_7;
    }
    sleep(4);
    printf("================picture go  to Show!!===================\n");
    s32Ret = HI_MPI_VO_ShowChn(VoLayer, VoChn);  
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("HI_MPI_VO_ShowChn failed!\n");
       goto END_PLAYBACK_7;
    }
    sleep(4);
    printf("================picture go  to 1/2X speed play!!===================\n");
    s32Ret = HI_MPI_VO_SetChnFrameRate(VoLayer, VoChn,(stLayerAttr.u32DispFrmRt) / 2);  
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("HI_MPI_VO_ShowChn failed!\n");
       goto END_PLAYBACK_7;
    }    
    sleep(4);
    printf("================picture go  to 1/4X speed play!!===================\n");
    s32Ret = HI_MPI_VO_SetChnFrameRate(VoLayer, VoChn,(stLayerAttr.u32DispFrmRt) / 4);  
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("HI_MPI_VO_ShowChn failed!\n");
       goto END_PLAYBACK_7;
    }    
    sleep(4);
    printf("================picture go  to normal!!===================\n");
    s32Ret = HI_MPI_VO_SetChnFrameRate(VoLayer, VoChn,stLayerAttr.u32DispFrmRt);  
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("HI_MPI_VO_ShowChn failed!\n");
       goto END_PLAYBACK_7;
    }
    
    while(1)
    {
        printf("press 'q' to exit this sample.\n");        
        ch = getchar();
        getchar();     
        if ('q' == ch)
        {
            break;
        }
        else
        {
            SAMPLE_PRT("the input is invaild! please try again.\n");
            continue;
        }

    }
    /******************************************
     step 7: exit process
    ******************************************/
    END_PLAYBACK_7:    
        VoLayer = SAMPLE_VO_LAYER_VHD0;        
        u32WndNum = 1;
        for(i = 0;i < u32WndNum;i++)
        {
            SAMPLE_COMM_VO_UnBindVpss(VoLayer,i,i,0);
        }
    END_PLAYBACK_6:
        VoLayer = SAMPLE_VO_LAYER_VHD0;
        enVoMode = VO_MODE_1MUX;        
        SAMPLE_COMM_VO_StopChn(VoLayer,enVoMode);        
        SAMPLE_COMM_VO_StopLayer(VoLayer);
    END_PLAYBACK_5:        
        VoDev = SAMPLE_VO_DEV_DHD0;        
        SAMPLE_COMM_VO_HdmiStop();
        SAMPLE_COMM_VO_StopDev(VoDev);

    END_PLAYBACK_4:        
        for(i = 0;i < VdChn;i++)
        {
           SAMPLE_COMM_VDEC_UnBindVpss(i,i);
        }
    END_PLAYBACK_3:        
        SAMPLE_COMM_VPSS_Stop(s32VpssGrpCnt, VPSS_MAX_CHN_NUM);
    END_PLAYBACK_2:
        SAMPLE_COMM_VDEC_Stop(VdChn); 
         SAMPLE_COMM_VDEC_StopSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
    END_PLAYBACK_1:
        SAMPLE_COMM_SYS_Exit();
    
    return s32Ret;
}

/******************************************************************************
* function :  HD0 zoom in
******************************************************************************/
HI_S32 SAMPLE_VO_ZoomIn_HD0(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;
    VB_CONF_S stVbConf;    
    HI_U32 u32BlkSize;
    VDEC_CHN VdChn = 1;
    PAYLOAD_TYPE_E enType;    
    SIZE_S stSize;    
    HI_S32 s32VpssGrpCnt = 2;    
    HI_S32 i;
    HI_U32 u32WndNum;
    VPSS_GRP_ATTR_S stGrpAttr;   
    VO_DEV VoDev;    
    VO_LAYER VoLayer;
    VO_PUB_ATTR_S stVoPubAttr; 
    SAMPLE_VO_MODE_E enVoMode;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;    
    VPSS_GRP VpssGrp_Clip = 0;
    VPSS_GRP VpssGrp_Full = 1;
    HI_CHAR ch;
    HI_U32 u32FrameRate;    
    VPSS_CROP_INFO_S stVpssClip;      
	VDEC_CHN_ATTR_S stVdecChnAttr[VDEC_MAX_CHN_NUM];
	VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];    
	pthread_t   VdecThread[2 * VDEC_MAX_CHN_NUM];
    
    /******************************************
     step  1: init variable 
    ******************************************/    
    memset(&stVbConf,0,sizeof(VB_CONF_S));     
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(VIDEO_ENCODING_MODE_PAL,\
                PIC_HD1080, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.u32MaxPoolCnt = 128;
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt  = 8;
    
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(VIDEO_ENCODING_MODE_PAL, PIC_HD1080, &stSize);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return HI_FAILURE;
    }
    
    if (704 == stSize.u32Width)
    {
        stSize.u32Width = 720;
    }
    else if (352 == stSize.u32Width)
    {
        stSize.u32Width = 360;
    }
    else if (176 == stSize.u32Width)
    {
        stSize.u32Width = 180;
    }
    /******************************************
     step 2: mpp system init. 
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto END_HDZOOMIN_1;
    }
    
    /******************************************
     step 3: start vdec 
    ******************************************/    
    enType = PT_H264;
    memset(&stVbConf,0,sizeof(VB_CONF_S));        
    SAMPLE_COMM_VDEC_ModCommPoolConf(&stVbConf, enType, &stSize);    
    s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stVbConf);
    if(s32Ret != HI_SUCCESS)
    {           
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END_HDZOOMIN_1;
    }
    /**************create vdec chn****************************/    
    SAMPLE_COMM_VDEC_ChnAttr(VdChn, &stVdecChnAttr[0], enType, &stSize);
    s32Ret = SAMPLE_COMM_VDEC_Start(VdChn, &stVdecChnAttr[0]);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vdec failed!\n");
        goto END_HDZOOMIN_2;
    }    
    /**************send stream****************************/
    SAMPLE_COMM_VDEC_ThreadParam(VdChn, &stVdecSend[0], &stVdecChnAttr[0], SAMPLE_1080P_H264_PATH);  
    SAMPLE_COMM_VDEC_StartSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
    /******************************************
     step 4: start vpss with vdec bind vpss 
    ******************************************/
    stGrpAttr.u32MaxW = stSize.u32Width;
    stGrpAttr.u32MaxH = stSize.u32Height;
    stGrpAttr.bIeEn = HI_FALSE;
    stGrpAttr.bNrEn = HI_TRUE;
    stGrpAttr.bHistEn = HI_FALSE;
    stGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;
    stGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;

    s32Ret = SAMPLE_COMM_VPSS_Start(s32VpssGrpCnt, &stSize, VPSS_MAX_CHN_NUM,NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vpss failed!\n");
        goto END_HDZOOMIN_3;
    }
    for(i = 0;i < VdChn;i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_BindVpss(i,VpssGrp_Clip);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VDEC_BindVpss failed!\n");
            goto END_HDZOOMIN_4;
        }
    }
    /******************************************
     step 4: start DHD0 
    ******************************************/    
    /**************start Dev DHD0****************************/
    VoDev = SAMPLE_VO_DEV_DHD0;    
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;
    stVoPubAttr.enIntfType = VO_INTF_BT1120;
    stVoPubAttr.u32BgColor = 0x000000ff;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
        goto END_HDZOOMIN_4;
    }
    /**************start Layer VHD0  for base show****************************/
    VoLayer = SAMPLE_VO_LAYER_VHD0;
    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;    
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_HDZOOMIN_5;
    }
    stLayerAttr.stImageSize.u32Width = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
        goto END_HDZOOMIN_5;
    }    
    /**************start 1 Chn in  VHD0 Layer****************************/
    enVoMode = VO_MODE_1MUX;
    u32WndNum = 1;
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer,enVoMode);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_HDZOOMIN_6;
    }        
    /**************vo bind to vpss 1 chn in group*************/
    for(i = 0;i < u32WndNum;i++)
    {
        s32Ret = SAMPLE_COMM_VO_BindVpss(VoLayer, i, VpssGrp_Clip, 0);
        if (HI_SUCCESS != s32Ret)
        {
           SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
           goto END_HDZOOMIN_7;
        }    
    }    
    /******************************************
     step 5: Clip process
    ******************************************/
    printf("press any key to show hd zoom.\n");
    getchar();
    /************start Layer VPIP  for Full screen***********/
    VoLayer = SAMPLE_VO_LAYER_VPIP;
    stLayerAttr.bClusterMode = HI_TRUE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;    
    s32Ret = SAMPLE_COMM_VO_GetWH(VO_OUTPUT_PAL, \
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &u32FrameRate);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_HDZOOMIN_7;
    }
    stLayerAttr.stImageSize.u32Width = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
       goto END_HDZOOMIN_8;
    }  
    /**************start 1 Chn in  VPIP Layer****************************/    
    enVoMode = VO_MODE_1MUX;
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer,enVoMode);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_HDZOOMIN_8;
    }
    /**************bind this chn to vpss another group*************/    
    s32Ret = SAMPLE_COMM_VO_BindVpss(VoLayer, 0, VpssGrp_Full, 0);
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
       goto END_HDZOOMIN_8;
    }     
    /**************bind this  vpss group to  vdec*************/    
    s32Ret = SAMPLE_COMM_VDEC_BindVpss(0,VpssGrp_Full);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VDEC_BindVpss failed!\n");
        goto END_HDZOOMIN_9;
    }
    /**************change vpssgrp_clip to clip*************/    
    stVpssClip.bEnable = HI_TRUE;
    stVpssClip.enCropCoordinate = VPSS_CROP_RATIO_COOR;
    stVpssClip.stCropRect.s32X = 500;
    stVpssClip.stCropRect.s32Y = 500;
    stVpssClip.stCropRect.u32Width = 500;
    stVpssClip.stCropRect.u32Height = 500; 
    
    s32Ret = HI_MPI_VPSS_SetGrpCrop(VpssGrp_Clip, &stVpssClip);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_HDZOOMIN_10;
    } 

    while(1)
    {
        printf("press 'q' to exit this sample.\n");        
        ch = getchar();
        getchar();      
        if ('q' == ch)
        {
            break;
        }
        else
        {
            SAMPLE_PRT("the input is invaild! please try again.\n");
            continue;
        }

    }
    /******************************************
     step 6: exit process
    ******************************************/
    END_HDZOOMIN_10:
        SAMPLE_COMM_VDEC_UnBindVpss(0,VpssGrp_Full);
    END_HDZOOMIN_9:
        VoLayer = SAMPLE_VO_LAYER_VPIP;
        SAMPLE_COMM_VO_UnBindVpss(VoLayer, 0, VpssGrp_Full, 0);
    END_HDZOOMIN_8:        
        VoLayer = SAMPLE_VO_LAYER_VPIP;
        enVoMode = VO_MODE_1MUX;        
        SAMPLE_COMM_VO_StopChn(VoLayer,enVoMode);        
        SAMPLE_COMM_VO_StopLayer(VoLayer);        
    END_HDZOOMIN_7:        
        VoLayer = SAMPLE_VO_LAYER_VHD0;        
        u32WndNum = 1;
        for(i = 0;i < u32WndNum;i++)
        {
            SAMPLE_COMM_VO_UnBindVpss(VoLayer, i, VpssGrp_Clip, 0);
        }

    END_HDZOOMIN_6:        
        VoLayer = SAMPLE_VO_LAYER_VHD0;        
        enVoMode = VO_MODE_1MUX;        
        SAMPLE_COMM_VO_StopChn(VoLayer,enVoMode);
        SAMPLE_COMM_VO_StopLayer(VoLayer);
    END_HDZOOMIN_5:
        VoDev = SAMPLE_VO_DEV_DHD0;    
        SAMPLE_COMM_VO_StopDev(VoDev);
    END_HDZOOMIN_4:        
        for(i = 0;i < VdChn;i++)
        {
           SAMPLE_COMM_VDEC_UnBindVpss(i,VpssGrp_Clip);
        }
    END_HDZOOMIN_3:        
        SAMPLE_COMM_VPSS_Stop(s32VpssGrpCnt, VPSS_MAX_CHN_NUM);
    END_HDZOOMIN_2:
        SAMPLE_COMM_VDEC_Stop(VdChn); 
         SAMPLE_COMM_VDEC_StopSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
    END_HDZOOMIN_1:
        SAMPLE_COMM_SYS_Exit();
    return s32Ret;
}

/******************************************************************************
* function :  SD0 zoom in
******************************************************************************/
HI_S32 SAMPLE_VO_ZoomIn_SD0(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;    
    VB_CONF_S stVbConf;    
    HI_U32 u32BlkSize;
    VDEC_CHN VdChn = 1;
    PAYLOAD_TYPE_E enType;    
    SIZE_S stSize;    
    VO_DEV VoDev;    
    VO_LAYER VoLayer;
    VO_CHN VoChn_Clip = 0;    
    VO_CHN VoChn_Full = 1;
    VO_PUB_ATTR_S stVoPubAttr; 
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;     
    VO_CHN_ATTR_S stChnAttr;    
    VO_ZOOM_ATTR_S stZoomAttr;    
    HI_CHAR ch;    
	VDEC_CHN_ATTR_S stVdecChnAttr[VDEC_MAX_CHN_NUM];
	VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];    
	pthread_t   VdecThread[2 * VDEC_MAX_CHN_NUM];
    /******************************************
     step  1: init variable 
    ******************************************/    
    memset(&stVbConf,0,sizeof(VB_CONF_S));     
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(VIDEO_ENCODING_MODE_PAL,\
                PIC_HD1080, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.u32MaxPoolCnt = 128;
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt  = 8;
    
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(VIDEO_ENCODING_MODE_PAL, PIC_HD1080, &stSize);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return HI_FAILURE;
    }
    
    if (704 == stSize.u32Width)
    {
        stSize.u32Width = 720;
    }
    else if (352 == stSize.u32Width)
    {
        stSize.u32Width = 360;
    }
    else if (176 == stSize.u32Width)
    {
        stSize.u32Width = 180;
    }
    /******************************************
     step 2: mpp system init. 
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto END_SDZOOMIN_1;
    }
    
    /******************************************
     step 3: start vdec 
    ******************************************/    
    enType = PT_H264;
    memset(&stVbConf,0,sizeof(VB_CONF_S));        
    SAMPLE_COMM_VDEC_ModCommPoolConf(&stVbConf, enType, &stSize);    
    s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stVbConf);
    if(s32Ret != HI_SUCCESS)
    {           
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END_SDZOOMIN_1;
    }
    /**************create vdec chn****************************/    
    SAMPLE_COMM_VDEC_ChnAttr(VdChn, &stVdecChnAttr[0], enType, &stSize);
    s32Ret = SAMPLE_COMM_VDEC_Start(VdChn, &stVdecChnAttr[0]);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vdec failed!\n");
        goto END_SDZOOMIN_2;
    }    
    /**************send stream****************************/
    SAMPLE_COMM_VDEC_ThreadParam(VdChn, &stVdecSend[0], &stVdecChnAttr[0], SAMPLE_1080P_H264_PATH);  
    SAMPLE_COMM_VDEC_StartSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
    /******************************************
     step 4: start DSD0 
    ******************************************/    
    /**************start Dev DSD0****************************/
    VoDev = SAMPLE_VO_DEV_DSD0;    
    stVoPubAttr.enIntfSync = VO_OUTPUT_PAL;
    stVoPubAttr.enIntfType = VO_INTF_CVBS;
    stVoPubAttr.u32BgColor = 0x000000ff;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
        goto END_SDZOOMIN_2;
    }
    /**************start Layer VSD0 ****************************/
    VoLayer = SAMPLE_VO_LAYER_VSD0;
    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;    
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_SDZOOMIN_3;
    }
    stLayerAttr.stImageSize.u32Width = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
        goto END_SDZOOMIN_3;
    }    
    /**************start 1 Chn in  VSD0 Layer for base show************/
    stChnAttr.bDeflicker = HI_FALSE;
    stChnAttr.u32Priority = 0;    
    stChnAttr.stRect.s32X = 0;
    stChnAttr.stRect.s32Y = 0;
    stChnAttr.stRect.u32Width = stLayerAttr.stDispRect.u32Width;
    stChnAttr.stRect.u32Height = stLayerAttr.stDispRect.u32Height;
    
    s32Ret = HI_MPI_VO_SetChnAttr(VoLayer, VoChn_Clip, &stChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("%s(%d):failed with %#x!\n",\
               __FUNCTION__,__LINE__,  s32Ret);
        goto  END_SDZOOMIN_4;
    }
    
    s32Ret = HI_MPI_VO_EnableChn(VoLayer, VoChn_Clip);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_SDZOOMIN_4;
    }    
    /**************vo bind to Vdec 1 chn*************/
    s32Ret = SAMPLE_COMM_VDEC_BindVo(0, VoLayer, VoChn_Clip);
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
       goto END_SDZOOMIN_5;
    }     
    /******************************************
     step 5: Clip process
    ******************************************/    
    printf("press any key to show hd zoom.\n");
    getchar();    
    /**************start 1 Chn in  VSD0 Layer for Full show************/
    stChnAttr.bDeflicker = HI_FALSE;
    stChnAttr.u32Priority = 1; 
    
    stChnAttr.stRect.s32X = 360;
    stChnAttr.stRect.s32Y = 288;    
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(VIDEO_ENCODING_MODE_PAL, PIC_CIF, &stSize);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_SDZOOMIN_5;
    }
    stChnAttr.stRect.u32Width = stSize.u32Width;
    stChnAttr.stRect.u32Height = stSize.u32Height;
    
    s32Ret = HI_MPI_VO_SetChnAttr(VoLayer, VoChn_Full, &stChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("%s(%d):failed with %#x!\n",\
               __FUNCTION__,__LINE__,  s32Ret);
        goto  END_SDZOOMIN_6;
    }
    
    s32Ret = HI_MPI_VO_EnableChn(VoLayer, VoChn_Full);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_SDZOOMIN_6;
    }    
    /**************vo bind to Vdec 1 chn*************/
    s32Ret = SAMPLE_COMM_VDEC_BindVo(0, VoLayer, VoChn_Full);
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
       goto END_SDZOOMIN_7;
    }
    /**************zoom in for VoChn_Clip chn*************/
    stZoomAttr.enZoomType = VOU_ZOOM_IN_RATIO;
    stZoomAttr.stZoomRatio.u32XRatio = 500;
    stZoomAttr.stZoomRatio.u32YRatio = 500;
    stZoomAttr.stZoomRatio.u32WRatio = 500;
    stZoomAttr.stZoomRatio.u32HRatio = 500;
    
    s32Ret = HI_MPI_VO_SetZoomInWindow(VoLayer, VoChn_Clip, &stZoomAttr);
    if (HI_SUCCESS != s32Ret)
    {
       SAMPLE_PRT("HI_MPI_VO_SetZoomInWindow failed!\n");
       goto END_SDZOOMIN_8;
    } 
    while(1)
    {
        printf("press 'q' to exit this sample.\n");        
        ch = getchar();
        getchar();      
        if ('q' == ch)
        {
            break;
        }
        else
        {
            SAMPLE_PRT("the input is invaild! please try again.\n");
            continue;
        }

    }
    /******************************************
     step 6: exit process
    ******************************************/

    END_SDZOOMIN_8:        
        VoLayer = SAMPLE_VO_LAYER_VSD0;
        SAMPLE_COMM_VDEC_UnBindVo(0, VoLayer, VoChn_Full);
    END_SDZOOMIN_7:
        VoLayer = SAMPLE_VO_LAYER_VSD0;
        HI_MPI_VO_DisableChn(VoLayer,VoChn_Full);
    END_SDZOOMIN_6:        
        VoLayer = SAMPLE_VO_LAYER_VSD0;
        SAMPLE_COMM_VDEC_UnBindVo(0, VoLayer, VoChn_Clip);
    END_SDZOOMIN_5:        
        VoLayer = SAMPLE_VO_LAYER_VSD0;
        HI_MPI_VO_DisableChn(VoLayer,VoChn_Clip);
    END_SDZOOMIN_4:        
        VoLayer = SAMPLE_VO_LAYER_VSD0;
        SAMPLE_COMM_VO_StopLayer(VoLayer);
    END_SDZOOMIN_3:        
        VoDev = SAMPLE_VO_DEV_DSD0;    
        SAMPLE_COMM_VO_StopDev(VoDev);
    END_SDZOOMIN_2:
        SAMPLE_COMM_VDEC_Stop(VdChn); 
        SAMPLE_COMM_VDEC_StopSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
    END_SDZOOMIN_1:
        SAMPLE_COMM_SYS_Exit();
    return s32Ret;

    
}

/******************************************************************************
* function :  HD0 MutiArea Show
******************************************************************************/
HI_S32 SAMPLE_VO_MutiArea_HD0(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;    
    VB_CONF_S stVbConf;    
    HI_U32 u32BlkSize;
    VDEC_CHN VdChn = 1;
    PAYLOAD_TYPE_E enType;    
    SIZE_S stSize;    
    HI_S32 i;
    HI_U32 u32WndNum;
    VO_DEV VoDev;    
    VO_LAYER VoLayer;
    VO_CHN VoChn;
    VO_PUB_ATTR_S stVoPubAttr; 
    SAMPLE_VO_MODE_E enVoMode;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;    
    HI_CHAR ch;    
	VDEC_CHN_ATTR_S stVdecChnAttr[VDEC_MAX_CHN_NUM];
	VdecThreadParam stVdecSend[VDEC_MAX_CHN_NUM];    
	pthread_t   VdecThread[2 * VDEC_MAX_CHN_NUM];
    /******************************************
     step  1: init variable 
    ******************************************/    
    memset(&stVbConf,0,sizeof(VB_CONF_S));     
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(VIDEO_ENCODING_MODE_PAL,\
                PIC_CIF, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.u32MaxPoolCnt = 128;
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt  = 8;
    
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(VIDEO_ENCODING_MODE_PAL, PIC_CIF, &stSize);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return HI_FAILURE;
    }
    
    if (704 == stSize.u32Width)
    {
        stSize.u32Width = 720;
    }
    else if (352 == stSize.u32Width)
    {
        stSize.u32Width = 360;
    }
    else if (176 == stSize.u32Width)
    {
        stSize.u32Width = 180;
    }
    /******************************************
     step 2: mpp system init. 
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto END_MUTIAREA_1;
    }
    
    /******************************************
     step 3: start vdec 
    ******************************************/    
    enType = PT_H264;
    memset(&stVbConf,0,sizeof(VB_CONF_S));        
    SAMPLE_COMM_VDEC_ModCommPoolConf(&stVbConf, enType, &stSize);    
    s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stVbConf);
    if(s32Ret != HI_SUCCESS)
    {           
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END_MUTIAREA_1;
    }
    /**************create vdec chn****************************/    
    SAMPLE_COMM_VDEC_ChnAttr(VdChn, &stVdecChnAttr[0], enType, &stSize);
    s32Ret = SAMPLE_COMM_VDEC_Start(VdChn, &stVdecChnAttr[0]);    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vdec failed!\n");
        goto END_MUTIAREA_2;
    }    
    /**************send stream****************************/
	SAMPLE_COMM_VDEC_ThreadParam(VdChn, &stVdecSend[0], &stVdecChnAttr[0], SAMPLE_CIF_H264_PATH);	
	SAMPLE_COMM_VDEC_StartSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
       
    /******************************************
     step 5: start DHD0 for 64 chn show
    ******************************************/    
    /**************start Dev****************************/
    VoDev = SAMPLE_VO_DEV_DHD0;    
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;
    stVoPubAttr.enIntfType = VO_INTF_VGA|VO_INTF_HDMI;
    stVoPubAttr.u32BgColor = 0x000000ff;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);    
	if (HI_SUCCESS != s32Ret)
	{
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
        goto END_MUTIAREA_3;
	}
    
    if (HI_SUCCESS != SAMPLE_COMM_VO_HdmiStart(stVoPubAttr.enIntfSync))
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_HdmiStart failed!\n");
        goto END_MUTIAREA_3;
    }
    /**************start Layer****************************/
    VoLayer = SAMPLE_VO_LAYER_VHD0;
    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;  

    /**************set layer partiMode****************************/    
    s32Ret = HI_MPI_VO_SetVideoLayerPartitionMode(VoLayer,VO_PART_MODE_SINGLE);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VO_SetVideoLayerPartitionMode failed with %#x!\n", s32Ret);
        goto  END_MUTIAREA_3;
    }
    
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_MUTIAREA_3;
    }
    stLayerAttr.stImageSize.u32Width = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
	if (HI_SUCCESS != s32Ret)
	{
	   SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
	   goto END_MUTIAREA_3;
	}    
    /**************start Chn****************************/    
    enVoMode = VO_MODE_64MUX;
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer,enVoMode);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_MUTIAREA_4;
    }    
    /**************vo bind to vdec****************************/
    u32WndNum = 64;
    for(i = 0;i < u32WndNum;i++)
    {
    	s32Ret = SAMPLE_COMM_VDEC_BindVo(0,VoLayer, i);
    	if (HI_SUCCESS != s32Ret)
    	{
    	   SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
    	   goto END_MUTIAREA_5;
    	}    
    }

    while(1)
    {
        printf("press 'q' to exit this sample.\n");        
        ch = getchar();
        getchar();     
        if ('q' == ch)
        {
            break;
        }
        else
        {
            SAMPLE_PRT("the input is invaild! please try again.\n");
            continue;
        }

    }
    /******************************************
     step 7: exit process
    ******************************************/
    END_MUTIAREA_5:    
        VoLayer = SAMPLE_VO_LAYER_VHD0;        
        u32WndNum = 64;
        for(i = 0;i < u32WndNum;i++)
        {
            SAMPLE_COMM_VDEC_UnBindVo(0,VoLayer,i);
        }
    END_MUTIAREA_4:
        VoLayer = SAMPLE_VO_LAYER_VHD0;
        enVoMode = VO_MODE_64MUX;        
        SAMPLE_COMM_VO_StopChn(VoLayer,enVoMode);        
        SAMPLE_COMM_VO_StopLayer(VoLayer);
    END_MUTIAREA_3:        
        VoDev = SAMPLE_VO_DEV_DHD0;        
        SAMPLE_COMM_VO_HdmiStop();
        SAMPLE_COMM_VO_StopDev(VoDev);
    END_MUTIAREA_2:
        SAMPLE_COMM_VDEC_Stop(VdChn); 
         SAMPLE_COMM_VDEC_StopSendStream(VdChn, &stVdecSend[0], &VdecThread[0]);
    END_MUTIAREA_1:
        SAMPLE_COMM_SYS_Exit();
    
    return s32Ret;
}

int main(int argc, char *argv[])
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_CHAR ch;
    HI_BOOL bExit = HI_FALSE;

    signal(SIGINT, SAMPLE_VO_HandleSig);
    signal(SIGTERM, SAMPLE_VO_HandleSig);

    while (1)
    {
        SAMPLE_VO_Usage();         
        ch = getchar(); 
        getchar();
        switch (ch)
        {
            case '0':   
            {
                s32Ret = SAMPLE_VO_Preview_HD0_HD1_SD0();
                break;
            }
            case '1':   
            {
                s32Ret = SAMPLE_VO_Playback_HD0();
                break;
            }
            case '2':   
            {
                s32Ret = SAMPLE_VO_ZoomIn_HD0();
                break;
            }
            case '3':   
            {
                s32Ret = SAMPLE_VO_ZoomIn_SD0();
                break;
            }
            case '4':   
            {
                s32Ret = SAMPLE_VO_MutiArea_HD0();
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

