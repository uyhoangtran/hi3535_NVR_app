/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : sample_common_new.h
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2009/07/07
  Description   : sample_common.c header file
  History       :
  1.Date        : 2009/07/07
    Author      : Hi3520MPP
    Modification: Created file

******************************************************************************/

#ifndef __SAMPLE_COMMON_H__
#define __SAMPLE_COMMON_H__

#include "hi_common.h"
#include "hi_comm_sys.h"
#include "hi_comm_vb.h"
#include "hi_comm_vi.h"
#include "hi_comm_vo.h"
#include "hi_comm_venc.h"
#include "hi_comm_vdec.h"

#include "mpi_sys.h"
#include "mpi_vb.h"
#include "mpi_vi.h"
#include "mpi_vo.h"
#include "mpi_venc.h"
#include "mpi_vdec.h"

#include "sample_comm.h"


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */


/* RGB format is 1888. */
#define VO_BKGRD_RED      0xFF0000    /* red back groud color */
#define VO_BKGRD_GREEN    0x00FF00    /* green back groud color */
#define VO_BKGRD_BLUE     0x0000FF    /* blue back groud color */
#define VO_BKGRD_BLACK    0x000000    /* black back groud color */
//#define Board_Test_B

#define CHECK_MPI_CALL(ret, mpi_name) \
do\
{\
    if (HI_SUCCESS != ret)\
    {\
        printf("%s fail! return 0x%08x\n", mpi_name, ret);\
        return HI_FAILURE;\
    }\
}while(0)

#define RET_IF_FAIL(ret) \
do\
{\
    if (HI_SUCCESS != ret)\
    {\
        return HI_FAILURE;\
    }\
}while(0)

/* vou device enumeration */
typedef enum hiVO_DEV_E
{
    VO_DEV_HD  = 0,                 /* high definition device */
    VO_DEV_AD  = 1,                 /* assistant device */
    VO_DEV_SD  = 2,                 /* spot device */
    VO_DEV_BUTT
} VO_DEV_E;

/***************************** venc ************************************/
typedef struct hiGET_STREAM_S
{
    HI_BOOL         bThreadStart;
    pthread_t       pid;
    PAYLOAD_TYPE_E enPayload;   /* ven channel is H.264? MJPEG? or JPEG? */
    VENC_CHN VeChnStart;        /* From this channel to get stream. */
    HI_S32 s32ChnTotal;         /* how many channels to get stream from.
                                         * channel index is VeChnStart, VeChnStart+1, ..., VeChnStart+(s32ChnTotal-1).
                                         * Used for SampleGetVencStreamProc. */
    HI_S32 s32SaveTotal;        /* how many frames will be get each channel. */
                                /* Note: JPEG snap, it must be 1. */
    HI_U8 aszFileNoPostfix[32]; /* complete file name will add postfix automaticly: (not finish yet)
                                         * _chnX.h264.  -- for h.264
                                         * _chnX.mjp.   -- for mjpeg
                                         * _chnX_Y.jpg  -- for jpeg
                                         * */
} GET_STREAM_S;

typedef struct vdec_sendparam
{
    pthread_t Pid;
    HI_BOOL bRun;
    VDEC_CHN VdChn;    
    PAYLOAD_TYPE_E enPayload;
    HI_S32 s32MinBufSize;
    VIDEO_MODE_E enVideoMode;
}VDEC_SENDPARAM_S;


HI_S32 SAMPLE_InitMPP(VB_CONF_S *pstVbConf, VB_CONF_S *pstVdecVbConf);
HI_S32 SAMPLE_ExitMPP(HI_VOID);

HI_S32 SAMPLE_StartVenc(HI_U32 u32GrpCnt, HI_BOOL bHaveMinor,
                                PAYLOAD_TYPE_E aenType[2], PIC_SIZE_E aenSize[2]);
HI_S32 SAMPLE_StopVenc(HI_U32 u32GrpCnt, HI_BOOL bHaveMinor);

HI_S32 SAMPLE_StartOneVenc(VENC_GRP VeGrp, VI_DEV ViDev, VI_CHN ViChn,
    PAYLOAD_TYPE_E enType, PIC_SIZE_E enSize, HI_S32 s32FrmRate);

HI_S32 SampleSaveH264Stream(FILE* fpH264File, VENC_STREAM_S *pstStream);
HI_S32 SampleSaveJpegStream(FILE* fpJpegFile, VENC_STREAM_S *pstStream);
HI_VOID* SampleGetVencStreamProc(HI_VOID *p);
HI_S32 SAMPLE_StartVencGetStream(GET_STREAM_S *pstGetVeStream);
HI_S32 SAMPLE_StopVencGetStream();
HI_S32 SAMPLE_CreateJpegChn(VENC_GRP VeGroup, VENC_CHN SnapChn, PIC_SIZE_E enPicSize);
HI_S32 SAMPLE_StartVo_SD(HI_S32 s32VoChnTotal, VO_DEV VoDev);

HI_S32 SAMPLE_SetVoChnMScreen(VO_DEV VoDev, HI_U32 u32ChnCnt, HI_U32 u32Width, HI_U32 u32Height);
HI_VOID SamplePcivStopVdecReadStream(HI_S32 s32ChnNum, VdecThreadParam *pstVdecSend, pthread_t *pVdecThread);
HI_VOID SamplePcivStartVdecSendStream(HI_S32 s32ChnNum, VdecThreadParam *pstVdecSend, pthread_t *pVdecThread);
HI_VOID SamplePcivSetVdecParam(HI_S32 s32ChnNum, VdecThreadParam *pstVdecSend, VDEC_CHN_ATTR_S *pstVdecChnAttr, char *pStreamFileName);
void VDEC_MST_DefaultH264HD_Attr(VDEC_CHN_ATTR_S *pstAttr);
void VDEC_MST_DefaultH264D1_Attr(VDEC_CHN_ATTR_S *pstAttr);
void VDEC_MST_DefaultH264960H_Attr(VDEC_CHN_ATTR_S *pstAttr);
HI_S32 SamplePcivGetVoDisplayNum(HI_U32 u32VoChnNum);
HI_S32 SamplePcivGetVoLayer(VO_DEV VoDev);
HI_S32 SamplePcivGetVoAttr(VO_DEV VoDev, VO_INTF_SYNC_E enIntfSync, VO_PUB_ATTR_S *pstPubAttr, 
    VO_VIDEO_LAYER_ATTR_S *pstLayerAttr, HI_S32 s32SquareSort, VO_CHN_ATTR_S *astChnAttr);

HI_VOID HandleSig(HI_S32 signo);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif /* End of #ifndef __SAMPLE_COMMON_H__ */
