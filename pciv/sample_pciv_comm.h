#ifndef __SAMPLE_PCIV_COMM__
#define __SAMPLE_PCIV_COMM__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#include "hi_mcc_usrdev.h"
#include "sample_comm.h"
#include "hi_comm_venc.h"
#include "mpi_venc.h"


#define SAMPLE_PCIV_DEF_VO_DEV  2

#define SAMPLE_PCIV_MAX_CHN_CNT 32  /* Pciv chn max count for each slave chip (we use 16 SD + 16 HD) */

#define SAMPLE_PCIV_SEQ_DEBUG   2001

#define SAMPLE_PCIV_VENC_STREAM_BUF_LEN 768*576*10
/* ≤‚ ‘ */
//#define SAMPLE_PCIV_VENC_STREAM_BUF_LEN 10240000

#define SAMPLE_PCIV_MAXVO_BIND      4

#define SAMPLE_PCIV_SEND_VDEC_LEN   512*1024
#define SAMPLE_PCIV_VDEC_STREAM_BUF_LEN (SAMPLE_PCIV_SEND_VDEC_LEN*3)

#define SAMPLE_PCIV_GET_STRIDE(u32Width, u32Align)\
    (u32Align * ((u32Width + u32Align - 1) / u32Align))

typedef enum hiSAMPLE_PCIV_MSG_TYPE_E
{
    SAMPLE_PCIV_MSG_CREATE_PCIV,
    SAMPLE_PCIV_MSG_DESTROY_PCIV,

    SAMPLE_PCIV_MSG_START_VDEC,
    SAMPLE_PCIV_MSG_STOP_VDEC,

    SAMPLE_PCIV_MSG_START_VPSS,
    SAMPLE_PCIV_MSG_STOP_VPSS,

    SAMPLE_PCIV_MSG_START_VO,
    SAMPLE_PCIV_MSG_STOP_VO,

    SAMPLE_PCIV_MSG_MALLOC,
    SAMPLE_PCIV_MSG_FREE,

    SAMPLE_PCIV_MSG_INIT_STREAM_VDEC,
    SAMPLE_PCIV_MSG_EXIT_STREAM_VDEC,

    SAMPLE_PCIV_MSG_INIT_WIN_VB,
    SAMPLE_PCIV_MSG_EXIT_WIN_VB,

    SAMPLE_PCIV_MSG_INIT_MSG_PORG,
    SAMPLE_PCIV_MSG_EXIT_MSG_PORG,

    SAMPLE_PCIV_MSG_ECHO,
    SAMPLE_PCIV_MSG_BUTT
} SAMPLE_PCIV_MSG_TYPE_E;

static inline HI_CHAR *PCIV_MSG_PRINT_TYPE(SAMPLE_PCIV_MSG_TYPE_E enType)
{
    switch (enType)
    {
        case SAMPLE_PCIV_MSG_CREATE_PCIV:        return "start pciv";      break;
        case SAMPLE_PCIV_MSG_DESTROY_PCIV:       return "stop pciv";       break;
        case SAMPLE_PCIV_MSG_START_VO:           return "start vo";        break;
        case SAMPLE_PCIV_MSG_STOP_VO:            return "stop vo";         break;
        case SAMPLE_PCIV_MSG_START_VPSS:         return "start vpss";      break;
        case SAMPLE_PCIV_MSG_STOP_VPSS:          return "stop vpss";       break;
        case SAMPLE_PCIV_MSG_START_VDEC:         return "start vdec";      break;
        case SAMPLE_PCIV_MSG_STOP_VDEC:          return "stop vdec";       break;
        case SAMPLE_PCIV_MSG_MALLOC:             return "malloc";          break;
        case SAMPLE_PCIV_MSG_INIT_STREAM_VDEC:   return "init vdstream";   break;
        case SAMPLE_PCIV_MSG_EXIT_STREAM_VDEC:   return "exit vdstream";   break;
        case SAMPLE_PCIV_MSG_INIT_WIN_VB:        return "init winvb";      break;
        case SAMPLE_PCIV_MSG_EXIT_WIN_VB:        return "exit winvb";      break;
        case SAMPLE_PCIV_MSG_INIT_MSG_PORG:      return "init port";       break;
        case SAMPLE_PCIV_MSG_EXIT_MSG_PORG:      return "exit port";       break;
        default: return "invalid type";
    }
    return "invalid type";
}

typedef struct hiSAMPLE_PCIV_MSG_VDEC_S
{
    HI_U32     u32VdecChnNum;
    PIC_SIZE_E enPicSize;
    HI_BOOL    bReadStream;
} SAMPLE_PCIV_MSG_VDEC_S;

typedef struct hiSAMPLE_PCIV_MSG_VPSS_S
{
    VPSS_GRP vpssGrp;
    PIC_SIZE_E enPicSize;
    MPP_CHN_S stBInd;
    HI_BOOL  bBindVdec;
    HI_BOOL vpssChnStart[VPSS_MAX_CHN_NUM];
} SAMPLE_PCIV_MSG_VPSS_S;

typedef struct hiSAMPLE_PCIV_MSG_VO_S
{
    VO_DEV     VirtualVo;
    PIC_SIZE_E enPicSize;
    MPP_CHN_S  stBInd;
    HI_S32     s32VoChnNum;
    VO_INTF_SYNC_E enIntfSync;
} SAMPLE_PCIV_MSG_VO_S;

typedef struct hiSAMPLE_PCIV_MSG_WINVB_S
{
    PCIV_WINVBCFG_S stPciWinVbCfg;
} SAMPLE_PCIV_MSG_WINVB_S;

typedef struct hiPCIV_PCIVCMD_MALLOC_S
{
    HI_S32 s32Ret;
    HI_S32 s32BufChip;
    HI_U32 u32BlkSize;
    HI_U32 u32BlkCount;
    HI_U32 u32PhyAddr[PCIV_MAX_BUF_NUM];
} PCIV_PCIVCMD_MALLOC_S;

typedef struct hiPCIV_PCIVCMD_CREATE_S
{
    HI_BOOL         bAddOsd;
    PCIV_CHN        pcivChn;
    PCIV_ATTR_S     stDevAttr; /* The attribute should set */
    PCIV_BIND_OBJ_S stBindObj[SAMPLE_PCIV_MAXVO_BIND];
} PCIV_PCIVCMD_CREATE_S;

typedef struct hiPCIV_PCIVCMD_DESTROY_S
{
	HI_BOOL  bAddOsd;
    PCIV_CHN pcivChn;
    PCIV_BIND_OBJ_S stBindObj[SAMPLE_PCIV_MAXVO_BIND];
} PCIV_PCIVCMD_DESTROY_S;

typedef struct hiPCIV_VENCCMD_INIT_S
{
    HI_U32 u32GrpCnt;
    HI_BOOL bHaveMinor;
    HI_BOOL bUseVpss;
    PAYLOAD_TYPE_E aenType[2];
    PIC_SIZE_E aenSize[2];
    PCIV_TRANS_ATTR_S stStreamArgs;
} PCIV_VENCCMD_INIT_S;

typedef struct hiPCIV_VENCCMD_EXIT_S
{
    HI_U32 u32GrpCnt;
    HI_BOOL bHaveMinor;
    HI_BOOL bUseVpss;
} PCIV_VENCCMD_EXIT_S;

typedef struct hiPCIV_MSGPORT_INIT_S
{
    HI_S32 s32VencMsgPortW;
    HI_S32 s32VencMsgPortR;
    HI_S32 s32VdecMsgPortW[VDEC_MAX_CHN_NUM];
    HI_S32 s32VdecMsgPortR[VDEC_MAX_CHN_NUM];
} PCIV_MSGPORT_INIT_S;

#define PCIV_STREAM_MAGIC 0x55555555

typedef struct hiPCIV_STREAM_HEAD_S
{
    HI_U32              u32Magic;
    PAYLOAD_TYPE_E      enPayLoad;
    HI_U32              u32DMADataLen;
    HI_U32              u32StreamDataLen;
    HI_U32              u32Seq;
    HI_U64              u64PTS;     /*PTS*/
    HI_S32              s32ChnID;
    HI_BOOL             bFieldEnd;  /*field end */
    HI_BOOL             bFrameEnd;  /*frame end */
    VENC_DATA_TYPE_U    enDataType;   /*the type of stream*/
} PCIV_STREAM_HEAD_S;

typedef struct hiSAMPLE_PCIV_VDEC_CTX_S
{
    VDEC_CHN    VdecChn;
    pthread_t   pid;
    HI_BOOL     bThreadStart;
    HI_CHAR     aszFileName[64];
    HI_VOID     *pTransHandle;
    HI_S32      s32MsgPortWrite;
    HI_S32      s32MsgPortRead;
} SAMPLE_PCIV_VDEC_CTX_S;

typedef struct hiSAMPLE_PCIV_VENC_CTX_S
{
    HI_S32      s32VencCnt;
    pthread_t   pid;
    HI_BOOL     bThreadStart;
    HI_VOID     *pTransHandle;
    HI_S32      s32MsgPortWrite;
    HI_S32      s32MsgPortRead;
    HI_U32      u32Seq;
} SAMPLE_PCIV_VENC_CTX_S;

#define PCIV_CHECK_ERR(err)\
do{\
    if(err!=0)\
    {\
        printf("\033[0;31mSample Pciv err:%x,Func:%s,Line:%d\033[0;39m\n",err,__FUNCTION__,__LINE__);\
        return err;\
    }\
}while(0)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif

