#include <dec.h>

HI_S32 hiSystemInit()
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