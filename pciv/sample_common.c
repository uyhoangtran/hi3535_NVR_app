/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

******************************************************************************
  File Name     : sample_common.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2009/07/04
  Description   : common functions for sample.
                  In default,
                  VI capture TV signals.
                  VO display on TV screen.
  History       :
  1.Date        : 2009/07/04
    Author      : Hi3520MPP
    Modification: Created file.
  2.Date        : 2010/02/12
    Author      : Hi3520MPP
    Modification: Add video loss detect demo
******************************************************************************/
#ifdef __cplusplus
 #if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>

#include "sample_common.h"
#include "sample_comm.h"

/************!!!!attention:********
The test code havn't check whether vi dev is out of  range(0-3)
*********************************/
#define G_VIDEV_START 0

/* video input into VI.
** Only valid when input from ADC, such as TW2864, TW2815, etc.
** VI samples that run for PAL will change to NTSC if modify it
** to VIDEO_ENCODING_MODE_NTSC.
*/
#if 1
    VIDEO_NORM_E   gs_enViNorm   = VIDEO_ENCODING_MODE_PAL;
    VO_INTF_SYNC_E gs_enSDTvMode = VO_OUTPUT_PAL;
#else
    VIDEO_NORM_E   gs_enViNorm   = VIDEO_ENCODING_MODE_NTSC;
    VO_INTF_SYNC_E gs_enSDTvMode = VO_OUTPUT_NTSC;
#endif
#if 1
VI_DEV_ATTR_S DEV_ATTR_BT656D1_4MUX =
{
    /*接口模式*/
    VI_MODE_BT656,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_4Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},
    /*逐行or隔行输入*/
    VI_SCAN_INTERLACED,
    /*AdChnId*/
    {-1, -1, -1, -1}
};

VI_DEV_ATTR_S DEV_ATTR_BT656D1_2MUX =
{
    /*接口模式*/
    VI_MODE_BT656,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_2Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},
    /*逐行or隔行输入*/
    VI_SCAN_INTERLACED,
    /*AdChnId*/
    {-1, -1, -1, -1}
};

VI_DEV_ATTR_S DEV_ATTR_BT656D1_1MUX =
{
    /*接口模式*/
    VI_MODE_BT656,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_1Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},
    /*逐行or隔行输入*/
    VI_SCAN_INTERLACED,
    /*AdChnId*/
    {-1, -1, -1, -1}
};

VI_DEV_ATTR_S DEV_ATTR_7441_BT1120_1080P =
/* 典型时序3:7441 BT1120 1080P@60fps典型时序 (对接时序: 时序)*/
{
    /*接口模式*/
    VI_MODE_BT1120_STANDARD,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_1Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0xFF0000},
    /*逐行or隔行输入*/
    VI_SCAN_PROGRESSIVE,
    /*AdChnId*/
    {-1, -1, -1, -1},
    /*enDataSeq, 仅支持YUV格式*/
    VI_INPUT_DATA_UVUV,

    /*同步信息，对应reg手册的如下配置, --bt1120时序无效*/
    {
    /*port_vsync   port_vsync_neg     port_hsync        port_hsync_neg        */
    VI_VSYNC_PULSE, VI_VSYNC_NEG_HIGH, VI_HSYNC_VALID_SINGNAL,VI_HSYNC_NEG_HIGH,VI_VSYNC_NORM_PULSE,VI_VSYNC_VALID_NEG_HIGH,

    /*timing信息，对应reg手册的如下配置*/
    /*hsync_hfb    hsync_act    hsync_hhb*/
    {0,            1920,        0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,            1080,        0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,            0}
    }
};


VI_DEV_ATTR_S DEV_ATTR_7441_BT1120_720P =
/* 典型时序3:7441 BT1120 720P@60fps典型时序 (对接时序: 时序)*/
{
    /*接口模式*/
    VI_MODE_BT1120_STANDARD,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_1Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF00,    0xFF},
    /*逐行or隔行输入*/
    VI_SCAN_PROGRESSIVE,
    /*AdChnId*/
    {-1, -1, -1, -1},
    /*enDataSeq, 仅支持YUV格式*/
    VI_INPUT_DATA_UVUV,

    /*同步信息，对应reg手册的如下配置, --bt1120时序无效*/
    {
    /*port_vsync   port_vsync_neg     port_hsync        port_hsync_neg        */
    VI_VSYNC_PULSE, VI_VSYNC_NEG_HIGH, VI_HSYNC_VALID_SINGNAL,VI_HSYNC_NEG_HIGH,VI_VSYNC_NORM_PULSE,VI_VSYNC_VALID_NEG_HIGH,

    /*timing信息，对应reg手册的如下配置*/
    /*hsync_hfb    hsync_act    hsync_hhb*/
    {0,            1280,        0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,            720,        0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,            0}
    }
};
#endif
#if 1
VI_DEV_ATTR_S DEV_ATTR_7441_INTERLEAVED_720P =
/* 典型时序3:7441 BT1120 720P@60fps典型时序 (对接时序: 时序)*/
{
    /*接口模式*/
    VI_MODE_BT1120_INTERLEAVED,
    /*1、2、4路工作模式*/
    VI_WORK_MODE_1Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},
    /*逐行or隔行输入*/
    VI_SCAN_PROGRESSIVE,
    /*AdChnId*/
    {-1, -1, -1, -1},
    /*enDataSeq, 仅支持YUV格式*/
    VI_INPUT_DATA_UVUV,

    /*同步信息，对应reg手册的如下配置, --bt1120时序无效*/
    {
    /*port_vsync   port_vsync_neg     port_hsync        port_hsync_neg        */
    VI_VSYNC_PULSE, VI_VSYNC_NEG_HIGH, VI_HSYNC_VALID_SINGNAL,VI_HSYNC_NEG_HIGH,VI_VSYNC_NORM_PULSE,VI_VSYNC_VALID_NEG_HIGH,

    /*timing信息，对应reg手册的如下配置*/
    /*hsync_hfb    hsync_act    hsync_hhb*/
    {0,            1280,        0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,            720,        0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,            0}
    }
};
#endif

/************************************************************************************/
const HI_U8 g_SOI[2] = {0xFF, 0xD8};
const HI_U8 g_EOI[2] = {0xFF, 0xD9};
/*****************************************************************************
 Prototype       : SAMPLE_InitMpp
 Description     : Init Mpp
 Input           : pstVbConf  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2009/7/4
    Author       : c55300
    Modification : Created function

*****************************************************************************/
HI_S32 SAMPLE_InitMPP(VB_CONF_S *pstVbConf, VB_CONF_S *pstVdecVbConf)
{
    HI_S32         s32Ret;
    MPP_SYS_CONF_S stSysConf = {0};

    HI_MPI_VB_ExitModCommPool(VB_UID_VDEC);
    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();

    s32Ret = HI_MPI_VB_SetConf(pstVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VB_SetConf failed!\n");
        return -1;
    }

    s32Ret = HI_MPI_VB_Init();
    if (HI_SUCCESS != s32Ret)    
    {
        printf("HI_MPI_VB_Init failed!\n");
        return -1;
    }

    stSysConf.u32AlignWidth = 16;
    s32Ret = HI_MPI_SYS_SetConf(&stSysConf);
    if (HI_SUCCESS != s32Ret)
    {
        printf("conf : system config failed!\n");
        return -1;
    }

    s32Ret = HI_MPI_SYS_Init();
    if (HI_SUCCESS != s32Ret)    
    {
        printf("sys init failed!\n");
        return -1;
    }

    s32Ret = HI_MPI_VB_ExitModCommPool(VB_UID_VDEC);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VB_ExitModCommPool fail! s32Ret:0x%x\n", s32Ret);
        (HI_VOID)HI_MPI_VB_Exit();
        return s32Ret;
    }
        
    s32Ret = HI_MPI_VB_SetModPoolConf(VB_UID_VDEC, pstVdecVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VB_SetModPoolConf fail! s32Ret:0x%x\n", s32Ret);
        (HI_VOID)HI_MPI_VB_Exit();
        return s32Ret;
    }
    
    s32Ret = HI_MPI_VB_InitModCommPool(VB_UID_VDEC);		
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VB_InitModCommPool fail! s32Ret:0x%x\n", s32Ret);
        (HI_VOID)HI_MPI_VB_Exit();
        return s32Ret;
    }

    return 0;
}

/*****************************************************************************
 Prototype       : SAMPLE_ExitMpp
 Description     : Exit Mpp
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
HI_S32 SAMPLE_ExitMPP(HI_VOID)
{
    if (HI_MPI_VB_ExitModCommPool(VB_UID_VDEC))
    {
        printf("Vdec vb sys exit fail\n");
        return -1;
    }
    
    if (HI_MPI_SYS_Exit())
    {
        printf("sys exit fail\n");
        return -1;
    }

    if (HI_MPI_VB_Exit())
    {
        printf("vb exit fail\n");
        return -1;
    }

    return 0;
}


#if 1
#endif

HI_S32 SAMPLE_PicSize2WH(PIC_SIZE_E enPicSize, HI_S32* s32Width, HI_S32* s32Height)
{
    switch ( enPicSize )
    {
        case PIC_QCIF:
            *s32Width = 176;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 144 : 120;
            break;
        case PIC_CIF:
            *s32Width = 352;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 288 : 240;
            break;
        case PIC_2CIF:
            *s32Width = 352;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 576 : 480;
            break;
        case PIC_HD1:
            *s32Width = 704;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 288 : 240;
            break;
        case PIC_D1:
            *s32Width = 704;
            *s32Height = (VIDEO_ENCODING_MODE_PAL == gs_enViNorm) ? 576 : 480;
            break;
        case PIC_QVGA:
            *s32Width = 320;
            *s32Height = 240;
            break;
        case PIC_VGA:
            *s32Width = 640;
            *s32Height = 480;
            break;
        case PIC_XGA:
            *s32Width = 1024;
            *s32Height = 768;
            break;
        case PIC_SXGA:
            *s32Width = 1400;
            *s32Height = 1050;
            break;
        case PIC_UXGA:
            *s32Width = 1600;
            *s32Height = 1200;
            break;
        case PIC_WSXGA:
            *s32Width = 1680;
            *s32Height = 1050;
            break;
        default:
            printf("err pic size\n");
            return HI_FAILURE;
    }
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SampleParseVoOutput
 Description     : parse vo output, to decide dispaly widht,height and rate.
 Input           : enVoOutput     **
 Output          : s32Width       **
                   s32Height      **
                   s32Rate        **
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2009/7/27
    Author       : c55300
    Modification : Created function

*****************************************************************************/
HI_S32 SampleParseVoOutput(VO_INTF_SYNC_E enVoOutput,
    HI_S32* s32Width, HI_S32* s32Height, HI_S32* s32DisplayRate)
{
    switch ( enVoOutput )
    {
        case VO_OUTPUT_PAL:
            *s32Width = 720;
            *s32Height = 576;
            *s32DisplayRate = 25;
            break;
        case VO_OUTPUT_NTSC:
            *s32Width = 720;
            *s32Height = 480;
            *s32DisplayRate = 30;
            break;
        case VO_OUTPUT_720P60:
            *s32Width = 1280;
            *s32Height = 720;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1080I60:
            *s32Width = 1920;
            *s32Height = 1080;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1080P30:
            *s32Width = 1920;
            *s32Height = 1080;
            *s32DisplayRate = 30;
            break;
        case VO_OUTPUT_800x600_60:
            *s32Width = 800;
            *s32Height = 600;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1024x768_60:
            *s32Width = 1024;
            *s32Height = 768;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1280x1024_60:
            *s32Width = 1280;
            *s32Height = 1024;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1366x768_60:
            *s32Width = 1366;
            *s32Height = 768;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_1440x900_60:
            *s32Width = 1440;
            *s32Height = 900;
            *s32DisplayRate = 60;
            break;
        case VO_OUTPUT_USER:
            printf("Why call me? You should set display size in VO_OUTPUT_USER.\n");
            break;
        default:
            *s32Width = 720;
            *s32Height = 576;
            *s32DisplayRate = 25;
            printf(" vo display size is (720, 576).\n");
            break;
    }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartVoDevice
 Description     : Start vo device as you specified.
 Input           : VoDev       **
                   pstDevAttr  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
HI_S32 SAMPLE_StartVoDevice(VO_DEV VoDev, VO_PUB_ATTR_S* pstDevAttr)
{
    HI_S32 ret;

	/*because we will change vo device attribution,
	  so we diable vo device first*/
	ret = HI_MPI_VO_Disable(VoDev);
    if (HI_SUCCESS != ret)
    {
        printf("HI_MPI_VO_Disable fail 0x%08x.\n", ret);
        return HI_FAILURE;
    }

    ret = HI_MPI_VO_SetPubAttr(VoDev, pstDevAttr);
    if (HI_SUCCESS != ret)
    {
        printf("HI_MPI_VO_SetPubAttr fail 0x%08x.\n", ret);
        return HI_FAILURE;
    }

    ret = HI_MPI_VO_Enable(VoDev);
    if (HI_SUCCESS != ret)
    {
        printf("HI_MPI_VO_Enable fail 0x%08x.\n", ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_StartVoVideoLayer(VO_DEV VoDev, VO_VIDEO_LAYER_ATTR_S* pstVideoLayerAttr)
{
    HI_S32 ret;

    /* set public attr of VO*/
    ret = HI_MPI_VO_SetVideoLayerAttr(VoDev, pstVideoLayerAttr);
    if (HI_SUCCESS != ret)
    {
        printf("set video layer of dev %u failed %#x!\n", VoDev, ret);
        return HI_FAILURE;
    }

    /* enable VO device*/
    ret = HI_MPI_VO_EnableVideoLayer(VoDev);
    if (HI_SUCCESS != ret)
    {
        printf("enable video layer of dev %d failed with %#x !\n", VoDev, ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_SetVoChnMScreen(VO_DEV VoDev, HI_U32 u32ChnCnt, HI_U32 u32Width, HI_U32 u32Height)
{
    HI_U32 i, div, w, h, ret;
    VO_CHN_ATTR_S stChnAttr;

    /* If display 32 vo channels, should use 36-screen split. */
    u32ChnCnt = (u32ChnCnt == VO_MAX_CHN_NUM) ? 36 : u32ChnCnt;

    div = sqrt(u32ChnCnt);
    w = (u32Width / div);
    h = (u32Height / div);
    
    for (i = 0; i < u32ChnCnt; i++)
    {
        if (i >= VO_MAX_CHN_NUM)
        {
            break;
        }

        stChnAttr.u32Priority = 0;
        stChnAttr.bDeflicker  = HI_FALSE;
        stChnAttr.stRect.s32X = w * (i % div);
        stChnAttr.stRect.s32Y = h * (i / div);
        stChnAttr.stRect.u32Width  = w;
        stChnAttr.stRect.u32Height = h;

        if (stChnAttr.stRect.s32X % 2 != 0)
        {
            stChnAttr.stRect.s32X++;
        }

        if (stChnAttr.stRect.s32Y % 2 != 0)
        {
            stChnAttr.stRect.s32Y++;
        }

        if (stChnAttr.stRect.u32Width % 2 != 0)
        {
            stChnAttr.stRect.u32Width++;
        }

        if (stChnAttr.stRect.u32Height % 2 != 0)
        {
            stChnAttr.stRect.u32Height++;
        }

        ret = HI_MPI_VO_SetChnAttr(VoDev, i, &stChnAttr);
        if (ret != HI_SUCCESS)
        {
            printf("In %s set channel %d attr failed with %#x!\n", __FUNCTION__, i, ret);
            return ret;
        }
    }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartVo
 Description     : Start VO to output CVBS signal
 Input           : VoDev       **  SD, AD or HD
                   s32ChnTotal **  how many channel display on screen.
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
*****************************************************************************/
HI_S32 SAMPLE_StartVo(HI_S32                 s32ChnTotal,
                            VO_DEV                 VoDev,
                            VO_PUB_ATTR_S*         pstVoDevAttr,
                            VO_VIDEO_LAYER_ATTR_S* pstVideoLayerAttr)
{
    HI_S32 s32Ret;
    HI_U32 u32Width;
    HI_U32 u32Height;
    VO_CHN VoChn;

    s32Ret = SAMPLE_StartVoDevice(VoDev, pstVoDevAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    /* set vo video layer attribute and start video layer. */
    s32Ret = SAMPLE_StartVoVideoLayer(VoDev, pstVideoLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    /* set vo channel attribute and start each channel. */
    u32Width  = pstVideoLayerAttr->stImageSize.u32Width;
    u32Height = pstVideoLayerAttr->stImageSize.u32Height;

    if (s32ChnTotal == 1)
    {
        ;
    }
    else if (s32ChnTotal <= 4)
    {
        s32ChnTotal = 4;
    }
    else if (s32ChnTotal <= 9)
    {
        s32ChnTotal = 9;
    }
    else if (s32ChnTotal <= 16)
    {
        s32ChnTotal = 16;
    }
    else if (s32ChnTotal <= 36)
    {
        /* 36-screen split is support. But only 32 vo channels display. */
        s32ChnTotal = VO_MAX_CHN_NUM;
    }
    else
    {
        printf("too many vo channels!\n");
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_SetVoChnMScreen(VoDev, s32ChnTotal, u32Width, u32Height);
    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    for (VoChn = 0; VoChn < s32ChnTotal; VoChn++)
    {
        s32Ret = HI_MPI_VO_EnableChn(VoDev, VoChn);
        if (HI_SUCCESS != s32Ret)
        {
            printf("HI_MPI_VO_EnableChn(%d, %d) failed, err code:0x%08x\n\n",
                VoDev, VoChn, s32Ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_StopVo(HI_S32 s32ChnTotal, VO_DEV VoDev)
{
    VO_CHN VoChn;
    HI_S32 s32Ret;

    for (VoChn = 0; VoChn < s32ChnTotal; VoChn++)
    {
        s32Ret = HI_MPI_VO_DisableChn(VoDev, VoChn);
        if (HI_SUCCESS != s32Ret)
        {
            printf("HI_MPI_VO_DisableChn(%d, %d) fail, err code: 0x%08x.\n",
                VoDev, VoChn, s32Ret);
            return HI_FAILURE;
        }
    }
    s32Ret = HI_MPI_VO_DisableVideoLayer(VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_DisableVideoLayer(%d) fail, err code: 0x%08x.\n",
            VoDev, s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VO_Disable(VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_Disable(%d) fail, err code: 0x%08x.\n",
            VoDev, s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartVo_SD
 Description     : start vo to display standard-definition video.
 Input           : s32VoChnTotal  **
                   VoDev          **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
 Remark          : Using different vo device, you can display video on SDTV or VGA.
*****************************************************************************/
HI_S32 SAMPLE_StartVo_SD(HI_S32 s32VoChnTotal, VO_DEV VoDev)
{
    VO_PUB_ATTR_S stVoDevAttr;
    VO_VIDEO_LAYER_ATTR_S stVideoLayerAttr;
    HI_U32 u32Width;
    HI_U32 u32Height;
    HI_U32 u32DisplayRate = -1;
    HI_S32 s32Ret;

    switch (VoDev)
    {
        case VO_DEV_HD:
            stVoDevAttr.enIntfType = VO_INTF_VGA;
            stVoDevAttr.enIntfSync = VO_OUTPUT_800x600_60;
            stVoDevAttr.u32BgColor = VO_BKGRD_BLACK;
            u32DisplayRate = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 25 : 30;
            break;
        case VO_DEV_AD:
            stVoDevAttr.enIntfType = VO_INTF_CVBS;
            stVoDevAttr.enIntfSync = gs_enSDTvMode;
            stVoDevAttr.u32BgColor = VO_BKGRD_BLACK;
            u32DisplayRate = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 25 : 30;
            break;
        case VO_DEV_SD:
            stVoDevAttr.enIntfType = VO_INTF_CVBS;
            stVoDevAttr.enIntfSync = gs_enSDTvMode;
            stVoDevAttr.u32BgColor = VO_BKGRD_BLACK;
            u32DisplayRate = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 25 : 30;
            break;
        default:
            return HI_FAILURE;
    }

    u32Width = 720;
    u32Height = (VO_OUTPUT_PAL == gs_enSDTvMode) ? 576 : 480;
    stVideoLayerAttr.stDispRect.s32X = 0;
    stVideoLayerAttr.stDispRect.s32Y = 0;
    stVideoLayerAttr.stDispRect.u32Width   = u32Width;
    stVideoLayerAttr.stDispRect.u32Height  = u32Height;
    stVideoLayerAttr.stImageSize.u32Width  = u32Width;
    stVideoLayerAttr.stImageSize.u32Height = u32Height;
    stVideoLayerAttr.u32DispFrmRt = u32DisplayRate;
    stVideoLayerAttr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    //stVideoLayerAttr.s32PiPChn = VO_DEFAULT_CHN;     //add by Mike

    s32Ret = SAMPLE_StartVo(s32VoChnTotal, VoDev, &stVoDevAttr, &stVideoLayerAttr);
    if(HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : SAMPLE_StartViVo_SD
 Description     : start vio (i.e. preview) . input and output are both standart-
                   definition.
 Input           : s32ChnTotal  **
                   enPicSize    **
                   VoDev        **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :

*****************************************************************************/
HI_S32 SAMPLE_StartViVo_SD(HI_S32 s32ChnTotal, PIC_SIZE_E enViSize, VO_DEV VoDev)
{
    VI_DEV ViDev;
    VI_CHN ViChn;
    VO_CHN VoChn;
    HI_S32 s32ChnCnt;
    HI_S32 s32ViChnPerDev;
    //HI_S32 s32Ret;

    s32ViChnPerDev = 4;
    ViDev = G_VIDEV_START;
    ViChn = 0;
    VoChn = 0;

    //SAMPLE_StartVi_SD(s32ChnTotal, enViSize);
    SAMPLE_StartVo_SD(s32ChnTotal, VoDev);

    s32ChnCnt = 0;
    while(s32ChnTotal--)
    {
        //s32Ret = HI_MPI_VI_BindOutput(ViDev, ViChn, VoDev, VoChn);    //add by Mike
        //if (HI_SUCCESS != s32Ret)
        //{
        //    printf("bind Vi(%d,%d) to Vo(%d,%d) fail! \n",
        //        ViDev, ViChn, VoDev, VoChn);
        //    return HI_FAILURE;
        //}
        ViChn++;
        VoChn++;
        if (++s32ChnCnt == s32ViChnPerDev)
        {
            s32ChnCnt = 0;
            ViChn = 0;
            #ifdef hi3515
            ViDev+=2;
            #else
            ViDev++;
            #endif
        }
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_StopViVo_SD(HI_S32 s32ChnTotal, VO_DEV VoDev)
{
    VI_DEV ViDev;
    VI_CHN ViChn;
    VO_CHN VoChn;
    HI_S32 s32ChnCnt;
    HI_S32 s32ViChnPerDev;
    //HI_S32 s32Ret;

    s32ViChnPerDev = 4;
    ViDev = G_VIDEV_START;
    ViChn = 0;
    VoChn = 0;

    SAMPLE_StopVo(s32ChnTotal, VoDev);

    s32ChnCnt = 0;
    while(s32ChnTotal--)
    {
        //s32Ret = HI_MPI_VI_UnBindOutput(ViDev, ViChn, VoDev, VoChn);  //add by Mike
        //if (HI_SUCCESS != s32Ret)
        //{
        //    printf("bind Vi(%d,%d) to Vo(%d,%d) fail! \n",
        //        ViDev, ViChn, VoDev, VoChn);
        //    return HI_FAILURE;
        //}
        ViChn++;
        VoChn++;
        if (++s32ChnCnt == s32ViChnPerDev)
        {
            s32ChnCnt = 0;
            ViChn = 0;
            ViDev++;
        }
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_StopAllVo(HI_VOID)
{
    VO_DEV VoDev;
    VO_CHN VoChn;

    for (VoDev = 0; VoDev < VO_MAX_DEV_NUM; VoDev++)
    {
        for (VoChn = 0; VoChn < VO_MAX_CHN_NUM; VoChn++)
        {
            HI_MPI_VO_DisableChn(VoDev, VoChn);
        }

        HI_MPI_VO_DisableVideoLayer(VoDev);
        HI_MPI_VO_Disable(VoDev);
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_VoPicSwitch(VO_DEV VoDev, HI_U32 u32VoPicDiv)
{
    VO_CHN VoChn;
    VO_CHN_ATTR_S VoChnAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    HI_S32 s32Ret;
    HI_S32 i, div, u32ScreemDiv, u32PicWidth, u32PicHeight;

    u32ScreemDiv = u32VoPicDiv;

    s32Ret = HI_MPI_VO_SetAttrBegin(VoDev);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_SetAttrBegin(%d) errcode: 0x%08x\n",
            VoDev, s32Ret);
        return HI_FAILURE;
    }

    div = sqrt(u32ScreemDiv);

    s32Ret = HI_MPI_VO_GetVideoLayerAttr(VoDev, &stLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO_GetVideoLayerAttr(%d) errcode: 0x%08x\n",
            VoDev, s32Ret);
        return HI_FAILURE;
    }
    u32PicWidth  = stLayerAttr.stDispRect.u32Width / div;
    u32PicHeight = stLayerAttr.stDispRect.u32Height / div;

    for (i = 0; i < u32ScreemDiv; i++)
    {
        VoChn = i;
        VoChnAttr.stRect.s32X = (i % div) * u32PicWidth;
        VoChnAttr.stRect.s32Y = (i / div) * u32PicHeight;
        VoChnAttr.stRect.u32Width  = u32PicWidth;
        VoChnAttr.stRect.u32Height = u32PicHeight;
        VoChnAttr.u32Priority = 1;
        //VoChnAttr.bZoomEnable = HI_TRUE;  //add by Mike
        VoChnAttr.bDeflicker  = HI_FALSE;
        if (0 != HI_MPI_VO_SetChnAttr(VoDev, VoChn, &VoChnAttr))
        {
            printf("set VO Chn %d attribute(%d,%d,%d,%d) failed !\n",
                   VoChn, VoChnAttr.stRect.s32X, VoChnAttr.stRect.s32Y,
                   VoChnAttr.stRect.u32Width, VoChnAttr.stRect.u32Height);
            return -1;
        }

    }

    if (0 != HI_MPI_VO_SetAttrEnd(VoDev))
    {
        return -1;
    }

    return 0;
}

HI_S32 SAMPLE_GetJpegeCfg(PIC_SIZE_E enPicSize, VENC_ATTR_JPEG_S *pstJpegeAttr)
{
    VENC_ATTR_JPEG_S stJpegAttr;

    if (PIC_D1 == enPicSize)
    {
        stJpegAttr.u32PicWidth          = 704;
        stJpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?576:480;
    }
    else if (PIC_CIF == enPicSize)
    {
        stJpegAttr.u32PicWidth          = 352;
        stJpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?288:240;
    }
    else
    {
        printf("%s: not support this payload type\n", __FUNCTION__);
        return -1;
    }
    stJpegAttr.u32MaxPicWidth = stJpegAttr.u32PicWidth;
    stJpegAttr.u32MaxPicHeight= stJpegAttr.u32PicHeight;

    stJpegAttr.u32BufSize   = stJpegAttr.u32PicWidth * stJpegAttr.u32PicHeight * 2;
    //stJpegAttr.bVIField     = HI_TRUE;
    stJpegAttr.bByFrame     = HI_TRUE;
    //stJpegAttr.u32MCUPerECS = 0;             //add by Mike
    //stJpegAttr.u32Priority  = 0;
    //stJpegAttr.u32ImageQuality = 3;

    memcpy(pstJpegeAttr, &stJpegAttr, sizeof(VENC_ATTR_JPEG_S));

    return 0;
}

HI_S32 SAMPLE_GetMjpegeCfg(PIC_SIZE_E enPicSize, HI_BOOL bMainStream,
        VENC_ATTR_MJPEG_S *pstMjpegeAttr)
{
    VENC_ATTR_MJPEG_S stMjpegAttr;

    if (PIC_D1 == enPicSize)
    {
        stMjpegAttr.u32PicWidth          = 704;
        stMjpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?576:480;
        //stMjpegAttr.u32TargetBitrate     = 8192;   //add by Mike
    }
    else if (PIC_CIF == enPicSize)
    {
        stMjpegAttr.u32PicWidth          = 352;
        stMjpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?288:240;
        //stMjpegAttr.u32TargetBitrate     = 4096;   //add by Mike
    }
    else if (PIC_QCIF == enPicSize)
    {
        stMjpegAttr.u32PicWidth          = 176;
        stMjpegAttr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?144:120;;
        //stMjpegAttr.u32TargetBitrate     = 2048;   //add by Mike
    }
    else
    {
        printf("%s: not support this payload type\n", __FUNCTION__);
        return -1;
    }

    //stMjpegAttr.bMainStream             = bMainStream;
    stMjpegAttr.bByFrame                = HI_TRUE;
    //stMjpegAttr.bVIField                = HI_TRUE;
    //stMjpegAttr.u32ViFramerate          = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?25:30;    //add by Mike
    //stMjpegAttr.u32TargetFramerate      = stMjpegAttr.u32ViFramerate;
    //stMjpegAttr.u32MCUPerECS            = 0;
    stMjpegAttr.u32BufSize              = stMjpegAttr.u32PicWidth * stMjpegAttr.u32PicHeight * 2;

    memcpy(pstMjpegeAttr, &stMjpegAttr, sizeof(VENC_ATTR_MJPEG_S));

    return 0;
}

HI_S32 SAMPLE_GetH264eCfg(PIC_SIZE_E enPicSize, HI_BOOL bMainStream,
        VENC_ATTR_H264_S *pstH264eAttr)
{
    VENC_ATTR_H264_S stH264Attr;

    if (PIC_D1 == enPicSize)
    {
        stH264Attr.u32PicWidth          = 704;
        stH264Attr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?576:480;
        //stH264Attr.u32Bitrate           = 1024;             //add by Mike
    }
    else if (PIC_HD1 == enPicSize)
    {
        stH264Attr.u32PicWidth          = 704;
        stH264Attr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?288:240;
        //stH264Attr.u32Bitrate           = 1024;            //add by Mike
    }
    else if (PIC_CIF == enPicSize)
    {
        stH264Attr.u32PicWidth          = 352;
        stH264Attr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?288:240;
        //stH264Attr.u32Bitrate           = 512;             //add by Mike
    }
    else if (PIC_QCIF == enPicSize)
    {
        stH264Attr.u32PicWidth          = 176;
        stH264Attr.u32PicHeight         = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?144:120;
        //stH264Attr.u32Bitrate           = 256;                //add by Mike
    }
    else if (PIC_VGA == enPicSize)
    {
        stH264Attr.u32PicWidth          = 640;
        stH264Attr.u32PicHeight         = 480;
        //stH264Attr.u32Bitrate           = 1024;             //add by Mike
    }
    else if (PIC_QVGA == enPicSize)
    {
        stH264Attr.u32PicWidth          = 320;
        stH264Attr.u32PicHeight         = 240;
        //stH264Attr.u32Bitrate           = 512;                 //add by Mike
    }
    else if (PIC_HD720 == enPicSize)
    {
        stH264Attr.u32PicWidth          = 1280;
        stH264Attr.u32PicHeight         = 720;
        //stH264Attr.u32Bitrate           = 2048;                //add by Mike
    }
    else if (PIC_HD1080 == enPicSize)
    {
        stH264Attr.u32PicWidth          = 1920;
        stH264Attr.u32PicHeight         = 1072;
        //stH264Attr.u32Bitrate           = 4000;                //add by Mike
    }
    else
    {
        printf("%s: not support this payload type\n", __FUNCTION__);
        return -1;
    }

    //stH264Attr.bMainStream          = bMainStream;
    stH264Attr.bByFrame             = HI_TRUE;
    //stH264Attr.enRcMode             = RC_MODE_CBR;             //add by Mike
    //stH264Attr.bField               = HI_FALSE;
    //stH264Attr.bVIField             = HI_TRUE;
    //stH264Attr.u32ViFramerate       = (VIDEO_ENCODING_MODE_PAL==gs_enViNorm)?25:30;
    //stH264Attr.u32TargetFramerate   = stH264Attr.u32ViFramerate;
    //stH264Attr.u32Gop               = 100;
    //stH264Attr.u32MaxDelay          = 100;
    //stH264Attr.u32PicLevel          = 0;
    //stH264Attr.u32Priority          = 0;
    stH264Attr.u32BufSize           = stH264Attr.u32PicWidth * stH264Attr.u32PicHeight * 2;

    memcpy(pstH264eAttr, &stH264Attr, sizeof(VENC_ATTR_H264_S));

    return 0;
}

HI_S32 SAMPLE_StartOneVenc(VENC_GRP VeGrp, VI_DEV ViDev, VI_CHN ViChn,
    PAYLOAD_TYPE_E enType, PIC_SIZE_E enSize, HI_S32 s32FrmRate)
{
    HI_S32 s32Ret;
    VENC_CHN VeChn;
    VENC_ATTR_H264_S stH264eAttr;
    VENC_ATTR_MJPEG_S stMjpegAttr;
    VENC_CHN_ATTR_S stVencAttr;

    if (PT_H264 == enType)
    {
        /* you should get config by your method */
        SAMPLE_GetH264eCfg(enSize, HI_TRUE, &stH264eAttr);
        //stH264eAttr.u32TargetFramerate = s32FrmRate;             //add by Mike
    }
    else if (PT_MJPEG == enType)
    {
        SAMPLE_GetMjpegeCfg(enSize, HI_TRUE, &stMjpegAttr);
        //stMjpegAttr.u32TargetFramerate = s32FrmRate;              //add by Mike
    }
    else
    {
        printf("venc not support this payload type :%d\n", enType);
        return -1;
    }

    //s32Ret = HI_MPI_VENC_BindInput(VeGrp, ViDev, ViChn);  //add by Mike
    //if (s32Ret != HI_SUCCESS)
    //{
    //    printf("HI_MPI_VENC_BindInput err 0x%x\n", s32Ret);
    //    return HI_FAILURE;
    //}
    //printf("grp %d bind vi(%d,%d) ok\n", VeGrp, ViDev, ViChn);

    VeChn = VeGrp;
    //stVencAttr.enType = enType;                //add by Mike
    //if (PT_H264 == stVencAttr.enType)
    //{
    //    stVencAttr.pValue = &stH264eAttr;
    //}
    //else
    //{
    //    stVencAttr.pValue = &stMjpegAttr;
    //}                                        //add by Mike
    s32Ret = HI_MPI_VENC_CreateChn(VeChn, &stVencAttr);        //add by Mike
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_CreateChn(%d) err 0x%x\n", VeChn, s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_StartRecvPic(VeChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_StartRecvPic err 0x%x\n", s32Ret);
        return HI_FAILURE;
    }
    printf("venc chn%d start ok\n", VeChn);

    return HI_SUCCESS;
}

HI_S32 SAMPLE_CreateJpegChn(VENC_GRP VeGroup, VENC_CHN SnapChn, PIC_SIZE_E enPicSize)
{
    HI_S32 s32Ret = HI_SUCCESS;
    VENC_CHN_ATTR_S stVencAttr;
    VENC_ATTR_JPEG_S stJpegeAttr;

    //stVencAttr.enType = PT_JPEG;          //add by Mike
    //stVencAttr.pValue = &stJpegeAttr;

    SAMPLE_GetJpegeCfg(enPicSize, &stJpegeAttr);

    //s32Ret = HI_MPI_VENC_CreateGroup(VeGroup);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_CreateGroup err 0x%x\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_CreateChn(SnapChn, &stVencAttr);   //add by Mike
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_CreateChn err 0x%x\n", s32Ret);
        return HI_FAILURE;
    }

    /* This is recommended if you snap one picture each time. */
    s32Ret = HI_MPI_VENC_SetMaxStreamCnt(SnapChn, 1);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_MPI_VENC_SetMaxStreamCnt(%d) err 0x%x\n", SnapChn, s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 Payload2FilePostfix(PAYLOAD_TYPE_E enPayload, HI_U8* szFilePostfix)
{
    if (PT_H264 == enPayload)
    {
        strcpy((char*)szFilePostfix, ".h264");
    }
    else if (PT_JPEG == enPayload)
    {
        strcpy((char*)szFilePostfix, ".jpg");
    }
    else if (PT_MJPEG == enPayload)
    {
        strcpy((char*)szFilePostfix, ".mjp");
    }
    else
    {
        printf("payload type err!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

/* save jpeg stream */
HI_S32 SampleSaveJpegStream(FILE* fpJpegFile, VENC_STREAM_S *pstStream)
{
    HI_U8 * p;
    VENC_PACK_S*  pstData;
    HI_U32 i;
    HI_U32 u32Len;
    
    fwrite(g_SOI, 1, sizeof(g_SOI), fpJpegFile);

    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        pstData = &pstStream->pstPack[i];

        p = (HI_U8 *) pstData->pu8Addr+pstData->u32Offset;
        u32Len = pstData->u32Len-pstData->u32Offset;

        fwrite(p, u32Len, sizeof(HI_U8), fpJpegFile);

        fflush(fpJpegFile);
    }

    return HI_SUCCESS;
}

/* save h264 stream */
HI_S32 SampleSaveH264Stream(FILE* fpH264File, VENC_STREAM_S *pstStream)
{
    HI_S32 i;

    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        fwrite(pstStream->pstPack[i].pu8Addr,
               pstStream->pstPack[i].u32Len, 1, fpH264File);

        fflush(fpH264File);        
    }

    return HI_SUCCESS;
}

HI_S32 SampleSaveVencStream(PAYLOAD_TYPE_E enType,FILE *pFd, VENC_STREAM_S *pstStream)
{
    HI_S32 s32Ret;

    if (PT_H264 == enType)
    {
        s32Ret = SampleSaveH264Stream(pFd, pstStream);
    }
    else if (PT_MJPEG == enType)
    {
        s32Ret = SampleSaveJpegStream(pFd, pstStream);
    }
    else
    {
        return HI_FAILURE;
    }
    return s32Ret;
}

/* get stream from each channels and save them..
 * only for H264/MJPEG, not for JPEG.  */
HI_VOID* SampleGetVencStreamProc(HI_VOID *p)
{
    HI_S32 i;
    HI_S32 s32ChnTotal;
    PAYLOAD_TYPE_E enPayload;
    VENC_CHN VeChnStart;
    GET_STREAM_S* pstGet;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE *pFile[VENC_MAX_CHN_NUM];
    HI_U8 szFilePostfix[10];
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32ret;

    pstGet = (GET_STREAM_S*)p;
    VeChnStart = pstGet->VeChnStart;
    s32ChnTotal = pstGet->s32ChnTotal;
    enPayload = pstGet->enPayload;

    printf("VeChnStart:%d, s32ChnTotal:%d\n",VeChnStart,s32ChnTotal);

    /* Prepare for all channel. */
    Payload2FilePostfix(enPayload, szFilePostfix);

    for (i = VeChnStart; i < VeChnStart + s32ChnTotal; i++)
    {
        /* decide the stream file name, and open file to save stream */
        sprintf(aszFileName[i], "stream_%s%d%s", "chn", i, szFilePostfix);
        pFile[i] = fopen(aszFileName[i], "wb");
        if (!pFile[i])
        {
            printf("open file err!\n");
            return NULL;
        }
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] <= 0)
        {
            return NULL;
        }
        if (maxfd <= VencFd[i])
        {
            maxfd = VencFd[i];
        }
    }

    /* Start to get streams of each channel. */
    while (HI_TRUE == pstGet->bThreadStart)
    {
        FD_ZERO(&read_fds);
        for (i = VeChnStart; i < VeChnStart + s32ChnTotal; i++)
        {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        s32ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32ret < 0)
        {
            printf("select err\n");
            break;
        }
        else if (s32ret == 0)
        {
            printf("get venc stream time out, exit thread\n");
            break;
        }
        else
        {
            for (i = VeChnStart; i < VeChnStart + s32ChnTotal; i++)
            {
                if (FD_ISSET(VencFd[i], &read_fds))
                {
                    /* step 1: query how many packs in one-frame stream. */
                    memset(&stStream, 0, sizeof(stStream));
                    s32ret = HI_MPI_VENC_Query(i, &stStat);
                    if (s32ret != HI_SUCCESS)
                    {
                        printf("HI_MPI_VENC_Query:0x%x, chn:%d\n", s32ret, i);
                        pstGet->bThreadStart = HI_FALSE;
                        return NULL;
                    }

                    /* step 2: malloc corresponding number of pack nodes. */
                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack)
                    {
                        printf("malloc stream pack err!\n");
                        pstGet->bThreadStart = HI_FALSE;
                        return NULL;
                    }

                    /* step 3: call mpi to get one-frame stream. */
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32ret = HI_MPI_VENC_GetStream(i, &stStream, HI_IO_BLOCK);
                    if (s32ret != HI_SUCCESS)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        printf("HI_MPI_VENC_GetStream err 0x%x\n", s32ret);
                        pstGet->bThreadStart = HI_FALSE;
                        return NULL;
                    }

                    /* step 4: save stream. */
                    SampleSaveVencStream(pstGet->enPayload, pFile[i], &stStream);

                    /* step 5: release these stream */
                    s32ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (s32ret != HI_SUCCESS)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        pstGet->bThreadStart = HI_FALSE;
                        return NULL;
                    }

                    /* step 6: free pack nodes */
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
    };

    for (i = VeChnStart; i < VeChnStart + s32ChnTotal; i++)
    {
        fclose(pFile[i]);
    }
    pstGet->bThreadStart = HI_FALSE;
    return NULL;
}


static GET_STREAM_S s_stGetVeStream;

HI_S32 SAMPLE_StartVencGetStream(GET_STREAM_S *pstGetVeStream)
{
    memcpy(&s_stGetVeStream, pstGetVeStream, sizeof(GET_STREAM_S));

    s_stGetVeStream.bThreadStart = HI_TRUE;
    return pthread_create(&s_stGetVeStream.pid, 0, SampleGetVencStreamProc, (HI_VOID*)&s_stGetVeStream);
}

HI_S32 SAMPLE_StopVencGetStream()
{
    if (HI_TRUE == s_stGetVeStream.bThreadStart)
    {
        s_stGetVeStream.bThreadStart = HI_FALSE;
        pthread_join(s_stGetVeStream.pid, 0);
    }
    return HI_SUCCESS;
}

HI_VOID HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_ExitMPP();
        printf("MPP exit\n");
    }
    exit(0);
}

HI_S32 SamplePcivGetVoDisplayNum(HI_U32 u32VoChnNum)
{
    HI_S32 s32DispNum;

    if (1 == u32VoChnNum)
    {
        s32DispNum = 1;
    }
    else if (4 == u32VoChnNum)
    {
        s32DispNum = 2;
    }
    else if (9 == u32VoChnNum)
    {
        s32DispNum = 3;
    }
    else if (16 == u32VoChnNum)
    {
        s32DispNum = 4;
    }
    else
    {
        return -1;
    }

    return s32DispNum;
}


HI_S32 SamplePcivGetVoLayer(VO_DEV VoDev)
{
    HI_S32 s32LayerNum;
    
    if (0 == VoDev)
    {
        s32LayerNum = 0;
    }
    else if (1 == VoDev)
    {
        s32LayerNum = 1;
    }
    else if ((2 <= VoDev) && (VoDev <= 6))
    {
        s32LayerNum = VoDev + 1;
    }
    else
    {
        return -1;
    }

    return s32LayerNum;
}


HI_S32 SamplePcivGetVoAttr(VO_DEV VoDev, VO_INTF_SYNC_E enIntfSync, VO_PUB_ATTR_S *pstPubAttr,
        VO_VIDEO_LAYER_ATTR_S *pstLayerAttr, HI_S32 s32SquareSort, VO_CHN_ATTR_S *astChnAttr)
{    
    VO_INTF_TYPE_E enIntfType;
    HI_U32 u32Frmt, u32Width, u32Height, j;
 
    switch (VoDev)
    {
        case 1: enIntfType = VO_INTF_VGA; break;
        case 0: enIntfType = VO_INTF_BT1120; break;
        case 2: enIntfType = VO_INTF_CVBS; break;
        case 3: enIntfType = VO_INTF_VGA; break;
    }

    switch (enIntfSync)
    {
        case VO_OUTPUT_PAL      :    u32Width = 720;  u32Height = 576;  u32Frmt = 25; break;
        case VO_OUTPUT_NTSC     :    u32Width = 720;  u32Height = 480;  u32Frmt = 30; break;
        case VO_OUTPUT_1080P24  :    u32Width = 1920; u32Height = 1080; u32Frmt = 24; break;
        case VO_OUTPUT_1080P25  :    u32Width = 1920; u32Height = 1080; u32Frmt = 25; break;
        case VO_OUTPUT_1080P30  :    u32Width = 1920; u32Height = 1080; u32Frmt = 30; break;
        case VO_OUTPUT_720P50   :    u32Width = 1280; u32Height = 720;  u32Frmt = 50; break;
        case VO_OUTPUT_720P60   :    u32Width = 1280; u32Height = 720;  u32Frmt = 60; break;
        case VO_OUTPUT_1080I50  :    u32Width = 1920; u32Height = 1080; u32Frmt = 50; break;
        case VO_OUTPUT_1080I60  :    u32Width = 1920; u32Height = 1080; u32Frmt = 60; break;
        case VO_OUTPUT_1080P50  :    u32Width = 1920; u32Height = 1080; u32Frmt = 50; break;
        case VO_OUTPUT_1080P60  :    u32Width = 1920; u32Height = 1080; u32Frmt = 60; break;
        case VO_OUTPUT_576P50   :    u32Width = 720;  u32Height = 576;  u32Frmt = 50; break;
        case VO_OUTPUT_480P60   :    u32Width = 720;  u32Height = 480;  u32Frmt = 60; break;
        case VO_OUTPUT_800x600_60:   u32Width = 800;  u32Height = 600;  u32Frmt = 60; break;
        case VO_OUTPUT_1024x768_60:  u32Width = 1024; u32Height = 768;  u32Frmt = 60; break;
        case VO_OUTPUT_1280x1024_60: u32Width = 1280; u32Height = 1024; u32Frmt = 60; break;
        case VO_OUTPUT_1366x768_60:  u32Width = 1366; u32Height = 768;  u32Frmt = 60; break;
        case VO_OUTPUT_1440x900_60:  u32Width = 1440; u32Height = 900;  u32Frmt = 60; break;
        case VO_OUTPUT_1280x800_60:  u32Width = 1280; u32Height = 800;  u32Frmt = 60; break;

        default: return HI_FAILURE;
    }

    if (NULL != pstPubAttr)
    {
        pstPubAttr->enIntfSync = enIntfSync;
        pstPubAttr->u32BgColor = 0; //0xFF; //BLUE
        pstPubAttr->enIntfType = enIntfType;
    }

    if (NULL != pstLayerAttr)
    {
        pstLayerAttr->stDispRect.s32X       = 0;
        pstLayerAttr->stDispRect.s32Y       = 0;
        pstLayerAttr->stDispRect.u32Width   = u32Width;
        pstLayerAttr->stDispRect.u32Height  = u32Height;
        pstLayerAttr->stImageSize.u32Width  = u32Width;
        pstLayerAttr->stImageSize.u32Height = u32Height;
        pstLayerAttr->bDoubleFrame          = HI_FALSE;
        pstLayerAttr->bClusterMode          = HI_FALSE;
        pstLayerAttr->u32DispFrmRt          = 25;
        pstLayerAttr->enPixFormat           = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }

    if (NULL != astChnAttr)
    {
        for (j=0; j<(s32SquareSort * s32SquareSort); j++)
        {
            astChnAttr[j].stRect.s32X       = ALIGN_BACK((u32Width / s32SquareSort) * (j % s32SquareSort), 4);
            astChnAttr[j].stRect.s32Y       = ALIGN_BACK((u32Height / s32SquareSort) * (j / s32SquareSort), 4);
            astChnAttr[j].stRect.u32Width   = ALIGN_BACK(u32Width / s32SquareSort, 4);
            astChnAttr[j].stRect.u32Height  = ALIGN_BACK(u32Height / s32SquareSort, 4);
            astChnAttr[j].u32Priority       = 0;
            astChnAttr[j].bDeflicker        = HI_FALSE;
        }
    }
    
    return HI_SUCCESS;
}


void VDEC_MST_DefaultH264HD_Attr(VDEC_CHN_ATTR_S *pstAttr)
{
    pstAttr->enType=PT_H264;
    pstAttr->u32BufSize=2*1920*1080;
    pstAttr->u32Priority=5;
    pstAttr->u32PicWidth=1920;
    pstAttr->u32PicHeight=1080;
    pstAttr->stVdecVideoAttr.enMode=VIDEO_MODE_STREAM;
    pstAttr->stVdecVideoAttr.u32RefFrameNum=1;
    pstAttr->stVdecVideoAttr.s32SupportBFrame=0;
}
void VDEC_MST_DefaultH264D1_Attr(VDEC_CHN_ATTR_S *pstAttr)
{
	pstAttr->enType=PT_H264;
	pstAttr->u32BufSize=2*720*576;
	pstAttr->u32Priority=5;
	pstAttr->u32PicWidth=720;
	pstAttr->u32PicHeight=576;
	pstAttr->stVdecVideoAttr.enMode=VIDEO_MODE_FRAME;
	pstAttr->stVdecVideoAttr.u32RefFrameNum=1;
	pstAttr->stVdecVideoAttr.s32SupportBFrame=0;
}
void VDEC_MST_DefaultH264960H_Attr(VDEC_CHN_ATTR_S *pstAttr)
{
	pstAttr->enType=PT_H264;
	pstAttr->u32BufSize=2*960*540;
	pstAttr->u32Priority=5;
	pstAttr->u32PicWidth=960;
	pstAttr->u32PicHeight=540;
	pstAttr->stVdecVideoAttr.enMode=VIDEO_MODE_STREAM;
	pstAttr->stVdecVideoAttr.u32RefFrameNum=1;
	pstAttr->stVdecVideoAttr.s32SupportBFrame=0;
}


#if 1
HI_VOID * SamplePcivStartVdecSendStreamThread(HI_VOID *pArgs)
{
	VdecThreadParam *pstVdecThreadParam =(VdecThreadParam *)pArgs;
	FILE *fpStrm=NULL;
	HI_U8 *pu8Buf = NULL;
	VDEC_STREAM_S stStream;
	HI_BOOL bFindStart, bFindEnd;
	HI_S32 s32Ret,  i,  start = 0;
    HI_S32 s32UsedBytes = 0, s32ReadLen = 0;
    HI_U64 u64pts = 0;
    HI_S32 len;
    
	if(pstVdecThreadParam->cFileName != 0)
	{
		fpStrm = fopen(pstVdecThreadParam->cFileName, "rb");
		if(fpStrm == NULL)
		{
			printf("SAMPLE_TEST:can't open file %s in send stream thread:%d\n",pstVdecThreadParam->cFileName, pstVdecThreadParam->s32ChnId);
			return (HI_VOID *)(HI_FAILURE);
		}
	}
    printf("SAMPLE_TEST:chn %d, stream file:%s, bufsize: %d\n", 
		   pstVdecThreadParam->s32ChnId, pstVdecThreadParam->cFileName, pstVdecThreadParam->s32MinBufSize);

	pu8Buf = malloc(pstVdecThreadParam->s32MinBufSize);
	if(pu8Buf == NULL)
	{
		printf("SAMPLE_TEST:can't alloc %d in send stream thread:%d\n", pstVdecThreadParam->s32MinBufSize, pstVdecThreadParam->s32ChnId);
		fclose(fpStrm);
		return (HI_VOID *)(HI_FAILURE);
	}     
	fflush(stdout);
	
	u64pts = pstVdecThreadParam->u64PtsInit;
	while (1)
    {
        if (pstVdecThreadParam->eCtrlSinal == VDEC_CTRL_STOP)
        {
            break;
        }
        else if (pstVdecThreadParam->eCtrlSinal == VDEC_CTRL_PAUSE)
        {
			sleep(MIN2(pstVdecThreadParam->s32IntervalTime,1000));
            continue;
        }

        if ( (pstVdecThreadParam->s32StreamMode==VIDEO_MODE_FRAME) && (pstVdecThreadParam->enType == PT_MP4VIDEO) )
        {
            bFindStart = HI_FALSE;  
            bFindEnd   = HI_FALSE;
            fseek(fpStrm, s32UsedBytes, SEEK_SET);
            s32ReadLen = fread(pu8Buf, 1, pstVdecThreadParam->s32MinBufSize, fpStrm);
            if (s32ReadLen == 0)
            {
                if (pstVdecThreadParam->bLoopSend)
                {
                    s32UsedBytes = 0;
                    fseek(fpStrm, 0, SEEK_SET);
                    s32ReadLen = fread(pu8Buf, 1, pstVdecThreadParam->s32MinBufSize, fpStrm);
                }
                else
                {
                    break;
                }
            }

            for (i=0; i<s32ReadLen-4; i++)
            {
                if (pu8Buf[i] == 0 && pu8Buf[i+1] == 0 && pu8Buf[i+2] == 1 && pu8Buf[i+3] == 0xB6)
                {
                    bFindStart = HI_TRUE;
                    i += 4;
                    break;
                }
            }

            for (; i<s32ReadLen-4; i++)
            {
                if (pu8Buf[i  ] == 0 && pu8Buf[i+1] == 0 && pu8Buf[i+2] == 1 && pu8Buf[i+3] == 0xB6)
                {
                    bFindEnd = HI_TRUE;
                    break;
                }
            }

            s32ReadLen = i;
            if (bFindStart == HI_FALSE)
            {
                printf("SAMPLE_TEST: chn %d can not find start code! s32ReadLen %d, s32UsedBytes %d. \n", 
					                        pstVdecThreadParam->s32ChnId, s32ReadLen, s32UsedBytes);
            }
            else if (bFindEnd == HI_FALSE)
            {
                s32ReadLen = i+4;
            }
            
        }
        else if ( (pstVdecThreadParam->s32StreamMode==VIDEO_MODE_FRAME) && (pstVdecThreadParam->enType == PT_H264) )
        {
            bFindStart = HI_FALSE;  
            bFindEnd   = HI_FALSE;
            fseek(fpStrm, s32UsedBytes, SEEK_SET);
            s32ReadLen = fread(pu8Buf, 1, pstVdecThreadParam->s32MinBufSize, fpStrm);
            if (s32ReadLen == 0)
            {
                if (pstVdecThreadParam->bLoopSend)
                {
                    s32UsedBytes = 0;
                    fseek(fpStrm, 0, SEEK_SET);
                    s32ReadLen = fread(pu8Buf, 1, pstVdecThreadParam->s32MinBufSize, fpStrm);
                }
                else
                {
                    break;
                }
            }
         
            for (i=0; i<s32ReadLen-5; i++)
            {
                if (  pu8Buf[i  ] == 0 && pu8Buf[i+1] == 0 && pu8Buf[i+2] == 1 && 
                     ( (pu8Buf[i+3]&0x1F) == 0x5 || (pu8Buf[i+3]&0x1F) == 0x1 ) &&
                     ( (pu8Buf[i+4]&0x80) == 0x80)
                   )                 
                {
                    bFindStart = HI_TRUE;
                    i += 4;
                    break;
                }
            }

            for (; i<s32ReadLen-5; i++)
            {
                if (  pu8Buf[i] == 0 && pu8Buf[i+1] == 0 && pu8Buf[i+2] == 1 && 
                     ( (pu8Buf[i+3]&0x1F) == 0x5 || (pu8Buf[i+3]&0x1F) == 0x1 ) &&
                     ( (pu8Buf[i+4]&0x80) == 0x80 )
                   )
                {
                    bFindEnd = HI_TRUE;
                    break;
                }
            }

            s32ReadLen = i;
            if (bFindStart == HI_FALSE)
            {
                printf("SAMPLE_TEST: chn %d can not find start code!s32ReadLen %d, s32UsedBytes %d. \n", 
					                        pstVdecThreadParam->s32ChnId, s32ReadLen, s32UsedBytes);
            }
            else if (bFindEnd == HI_FALSE)
            {
                s32ReadLen = i+5;
            }
            
        }
        else if ( (pstVdecThreadParam->enType == PT_MJPEG) || (pstVdecThreadParam->enType == PT_JPEG) )
        {
            bFindStart = HI_FALSE;  
            bFindEnd   = HI_FALSE;          
            fseek(fpStrm, s32UsedBytes, SEEK_SET);
            s32ReadLen = fread(pu8Buf, 1, pstVdecThreadParam->s32MinBufSize, fpStrm);
            if (s32ReadLen == 0)
            {
                if (pstVdecThreadParam->bLoopSend)
                {
                    s32UsedBytes = 0;
                    fseek(fpStrm, 0, SEEK_SET);
                    s32ReadLen = fread(pu8Buf, 1, pstVdecThreadParam->s32MinBufSize, fpStrm);
                }
                else
                {
                    break;
                }
            }

           
            for (i=0; i<s32ReadLen-2; i++)
            {
                if (pu8Buf[i] == 0xFF && pu8Buf[i+1] == 0xD8) 
                {  
                    start = i;
                    bFindStart = HI_TRUE;
                    i = i + 2;
                    break;
                }  
            }

            for (; i<s32ReadLen-4; i++)
            {
                if ( (pu8Buf[i] == 0xFF) && (pu8Buf[i+1]& 0xF0) == 0xE0 )
                {   
                     len = (pu8Buf[i+2]<<8) + pu8Buf[i+3];                    
                     i += 1 + len;                  
                }
                else
                {
                    break;
                }
            }

            for (; i<s32ReadLen-2; i++)
            {
                if (pu8Buf[i] == 0xFF && pu8Buf[i+1] == 0xD8)
                {
                    bFindEnd = HI_TRUE;
                    break;
                } 
            }                    
            s32ReadLen = i;
            if (bFindStart == HI_FALSE)
            {
                printf("SAMPLE_TEST: chn %d can not find start code! s32ReadLen %d, s32UsedBytes %d. \n", 
					                        pstVdecThreadParam->s32ChnId, s32ReadLen, s32UsedBytes);
            }
            else if (bFindEnd == HI_FALSE)
            {
                s32ReadLen = i+2;
            }
        }
        else
        {
            fseek(fpStrm, s32UsedBytes, SEEK_SET);
            s32ReadLen = fread(pu8Buf, 1, pstVdecThreadParam->s32MinBufSize, fpStrm);
            if (s32ReadLen == 0)
            {
                if (pstVdecThreadParam->bLoopSend)
                {
                    s32UsedBytes = 0;
                    fseek(fpStrm, 0, SEEK_SET);
                    s32ReadLen = fread(pu8Buf, 1, pstVdecThreadParam->s32MinBufSize, fpStrm);
                }
                else
                {
                    break;
                }
            }
        }
        
        stStream.u64PTS  = u64pts;
		stStream.pu8Addr = pu8Buf + start;
		stStream.u32Len  = s32ReadLen; 
		stStream.bEndOfFrame  = (pstVdecThreadParam->s32StreamMode==VIDEO_MODE_FRAME)? HI_TRUE: HI_FALSE;
		stStream.bEndOfStream = HI_FALSE;       
        s32Ret=HI_MPI_VDEC_SendStream(pstVdecThreadParam->s32ChnId, &stStream, pstVdecThreadParam->s32MilliSec);
        if (HI_SUCCESS != s32Ret)
        {
			usleep(100);
        }
        else
        {
            s32UsedBytes = s32UsedBytes +s32ReadLen + start;			
            u64pts += pstVdecThreadParam->u64PtsIncrease;            
        }
		usleep(20000);
	}

    /* send the flag of stream end */
    memset(&stStream, 0, sizeof(VDEC_STREAM_S) );
    stStream.bEndOfStream = HI_TRUE;
    HI_MPI_VDEC_SendStream(pstVdecThreadParam->s32ChnId, &stStream, -1);
    
	//printf("SAMPLE_TEST:send steam thread %d return ...\n", pstVdecThreadParam->s32ChnId);
	fflush(stdout);
	if (pu8Buf != HI_NULL)
	{
        free(pu8Buf);
	}
	fclose(fpStrm);
	
	return (HI_VOID *)HI_SUCCESS;
}
#endif

HI_VOID SamplePcivSetVdecParam(HI_S32 s32ChnNum, VdecThreadParam *pstVdecSend, VDEC_CHN_ATTR_S *pstVdecChnAttr, char *pStreamFileName)
{
	int i;
	
	for(i=0; i<s32ChnNum; i++)
	{
	    sprintf(pstVdecSend[i].cFileName, pStreamFileName, i);
		pstVdecSend[i].s32MilliSec     = 0;
		pstVdecSend[i].s32ChnId        = i;
		pstVdecSend[i].s32IntervalTime = 1;
		pstVdecSend[i].u64PtsInit      = 0;
	    pstVdecSend[i].u64PtsIncrease  = 0;
		pstVdecSend[i].eCtrlSinal      = VDEC_CTRL_START;
        pstVdecSend[i].bLoopSend       = HI_TRUE;
        pstVdecSend[i].enType          = pstVdecChnAttr->enType;
		pstVdecSend[i].s32MinBufSize   = pstVdecChnAttr->u32PicWidth * pstVdecChnAttr->u32PicHeight;
		if (PT_H264 == pstVdecChnAttr->enType  || PT_MP4VIDEO == pstVdecChnAttr->enType)
		{
			pstVdecSend[i].s32StreamMode  = pstVdecChnAttr->stVdecVideoAttr.enMode;
		}
		else
		{
		    pstVdecSend[i].s32StreamMode = VIDEO_MODE_FRAME;
		}
	}
}

HI_VOID SamplePcivStartVdecSendStream(HI_S32 s32ChnNum, VdecThreadParam *pstVdecSend, pthread_t *pVdecThread)
{
	HI_S32  i;
	
	for(i=0; i<s32ChnNum; i++)
	{
	    pthread_create(&pVdecThread[i], 0, SamplePcivStartVdecSendStreamThread, (HI_VOID *)&pstVdecSend[i]);
	}
}

HI_VOID SamplePcivStopVdecReadStream(HI_S32 s32ChnNum, VdecThreadParam *pstVdecSend, pthread_t *pVdecThread)
{
	HI_S32  i;
	
	for(i=0; i<s32ChnNum; i++)
	{	
		pstVdecSend[i].eCtrlSinal=VDEC_CTRL_STOP;
        pthread_cancel(pVdecThread[i]);
		pthread_join(pVdecThread[i], 0);

        printf("Vdec read stream file thread has stopped successful! Vdec ChnNum: %d.\n", i);
        
	}
} 



#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

