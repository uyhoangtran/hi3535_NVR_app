#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>

#if 0
#include "hi_common.h"
#include "hi_comm_vi.h"
#include "hi_comm_video.h"
#include "hi_comm_sys.h"

#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_vi.h"
#endif

#include "mpi_ive.h"
#include "hi_comm_ive.h"

#include "sample_comm.h"

#define SAMPLE_D1_H264_PATH "../common/D1chn0.h264"

#define CLIP(a, maxv, minv)      (((a)>(maxv)) ? (maxv) : (((a) < (minv)) ? (minv) : (a)))


#define IVECHARCALH 8
#define IVECHARCALW 8
#define IVECHARNUM IVECHARCALW*IVECHARCALH

VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;

HI_S32   SAMPLE_IVE_MST_INIT_MPI()
{
    HI_S32 sRet = HI_FAILURE;
    VB_CONF_S       stVbConf;
    MPP_SYS_CONF_S   stSysConf;
	HI_BOOL bMpiInit=HI_FALSE;
    HI_S32 i =0;
    if(HI_TRUE == bMpiInit)
    {
        printf("MPI has been inited \n ");
        return sRet;
    }
    /*初始化之前先确定系统已退出*/
    HI_MPI_SYS_Exit();
    for(i=0;i<VB_MAX_USER;i++)
    {
         HI_MPI_VB_ExitModCommPool(i);
    }	
    HI_MPI_VB_Exit();

    /*VB初始化之前先配置VB*/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

    /*1080p*/
    stVbConf.u32MaxPoolCnt = 64;
    stVbConf.astCommPool[0].u32BlkSize   = 1920*1088*2;
    stVbConf.astCommPool[0].u32BlkCnt    = 0;

     /*720p*/

    stVbConf.astCommPool[1].u32BlkSize   = 1280*720*2;
    stVbConf.astCommPool[1].u32BlkCnt    = 0;


     /*D1*/

    stVbConf.astCommPool[2].u32BlkSize   = 768*576*2;
    stVbConf.astCommPool[2].u32BlkCnt    = 0;


     /*Cif*/

    stVbConf.astCommPool[3].u32BlkSize   = 384*288*2;
    stVbConf.astCommPool[3].u32BlkCnt    = 0;


  #if 1
    stVbConf.astCommPool[4].u32BlkSize   = 176*144*2;
    stVbConf.astCommPool[4].u32BlkCnt    = 0;

   #endif
    sRet = HI_MPI_VB_SetConf(&stVbConf);
    if(HI_SUCCESS != sRet)
    {
        printf("Config VB fail!\n");
        return sRet;
    }


    sRet = HI_MPI_VB_Init();
    if(HI_SUCCESS != sRet)
    {
        printf("Init VB fail!\n");
        return sRet;
    }


    memset(&stSysConf,0,sizeof(MPP_SYS_CONF_S));
    stSysConf.u32AlignWidth = 16;

    sRet = HI_MPI_SYS_SetConf(&stSysConf);
    if(HI_SUCCESS != sRet)
    {
        printf("Config sys fail!\n");
        HI_MPI_VB_Exit();
        return sRet;
    }


    sRet = HI_MPI_SYS_Init();
     if(HI_SUCCESS != sRet)
    {
        printf("Init sys fail!\n");
        HI_MPI_VB_Exit();
        return sRet;
    }

    bMpiInit = HI_TRUE;

    return sRet;
}


HI_BOOL g_bStopSignal;

typedef struct hiIVE_LINEAR_DATA_S
{
	HI_S32 s32LinearNum;
	HI_S32 s32ThreshNum;
	POINT_S *pstLinearPoint;
}IVE_LINEAR_DATA_S;


HI_S32 SAMPLE_IVE_Linear2DClassifer(POINT_S *pstChar, HI_S32 s32CharNum, 
                                            POINT_S *pstLinearPoint, HI_S32 s32Linearnum )
{
	HI_S32 s32ResultNum;
	HI_S32 i,j;
	HI_BOOL bTestFlag;
	POINT_S *pstNextLinearPoint;
	
	s32ResultNum=0;
	pstNextLinearPoint=&pstLinearPoint[1];	
	for(i=0;i<s32CharNum;i++)
	{
		bTestFlag=HI_TRUE;
		for(j=0;j<(s32Linearnum-1);j++)
		{

			if(   ( (pstChar[i].s32Y-pstLinearPoint[j].s32Y)*(pstNextLinearPoint[j].s32X-pstLinearPoint[j].s32X)>
				  (pstChar[i].s32X-pstLinearPoint[j].s32X)*(pstNextLinearPoint[j].s32Y-pstLinearPoint[j].s32Y) 
				   && (pstNextLinearPoint[j].s32X!=pstLinearPoint[j].s32X))
			   || ( (pstChar[i].s32X>pstLinearPoint[j].s32X) && (pstNextLinearPoint[j].s32X==pstLinearPoint[j].s32X) ))	
			{
				bTestFlag=HI_FALSE;
				break;
			}
		}
		if(bTestFlag==HI_TRUE)
		{
			s32ResultNum++;
		}
	}
	return s32ResultNum;
}

HI_VOID * SAMPLE_IVE_BlockDetect(HI_VOID *pArgs)
{
	VIDEO_FRAME_INFO_S stFrameInfo;
	HI_S32 s32Ret,s32LinearNum;
	HI_S32 s32ThreshNum;
	IVE_MEM_INFO_S stDst;
	IVE_LINEAR_DATA_S *pstIveLinerData;
	POINT_S *pstLinearPoint;

    HI_U32 tic=0;
	
	pstIveLinerData=(IVE_LINEAR_DATA_S * )pArgs;
	s32LinearNum=pstIveLinerData->s32LinearNum;
	pstLinearPoint=pstIveLinerData->pstLinearPoint;
	s32ThreshNum=pstIveLinerData->s32ThreshNum;
	
	stDst.u32PhyAddr=0;

		while(1)
		{
			IVE_SRC_INFO_S stSrc;
			IVE_HANDLE hIveHandle;
			HI_U64 *pu64VirData;
			int i,j;
			POINT_S stChar[IVECHARNUM];
			int w,h;
           

            

            s32Ret = HI_MPI_VDEC_GetImage(1, &stFrameInfo, -1);
            
			if(s32Ret!=HI_SUCCESS)
			{
				 printf("can't get Vdec frame for %x\n",s32Ret);
				 continue;
			}
            
            stSrc.u32Width = stFrameInfo.stVFrame.u32Width;
			stSrc.u32Height = stFrameInfo.stVFrame.u32Height;
			stSrc.stSrcMem.u32PhyAddr = stFrameInfo.stVFrame.u32PhyAddr[0];
			stSrc.stSrcMem.u32Stride = stFrameInfo.stVFrame.u32Stride[0];
			stSrc.enSrcFmt = IVE_SRC_FMT_SINGLE;

			w = stFrameInfo.stVFrame.u32Width/IVECHARCALW;
			h = stSrc.u32Height/IVECHARCALH;
            
			if(stDst.u32PhyAddr==0)
			{
				s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&stDst.u32PhyAddr, (HI_VOID *)&pu64VirData, 
					"User", HI_NULL, stSrc.u32Height * stSrc.u32Width*8);
				if(s32Ret!=HI_SUCCESS)
				{
					 printf("can't alloc intergal memory for %x\n",s32Ret);
					 return HI_NULL;
				}			
				stDst.u32Stride = stFrameInfo.stVFrame.u32Width;
				
			}	
			else if(stDst.u32Stride!=stSrc.u32Width)
			{
				HI_MPI_SYS_MmzFree(stDst.u32PhyAddr, pu64VirData);
				s32Ret = HI_MPI_SYS_MmzAlloc_Cached(&stDst.u32PhyAddr,(HI_VOID *)&pu64VirData, 
					"User", HI_NULL, stSrc.u32Height * stSrc.u32Width*8);
				if(s32Ret!=HI_SUCCESS)
				{
					 printf("can't alloc intergal memory for %x\n",s32Ret);
					 return HI_NULL;
				}			
				stDst.u32Stride = stFrameInfo.stVFrame.u32Width;				
			}
			s32Ret = HI_MPI_IVE_INTEG(&hIveHandle, &stSrc, &stDst, HI_TRUE);
            tic++;
			if(s32Ret != HI_SUCCESS)
			{
				HI_MPI_SYS_MmzFree(stDst.u32PhyAddr, pu64VirData);
                HI_MPI_VDEC_ReleaseImage(1, &stFrameInfo);
				printf(" ive integal function can't submmit for %x\n",s32Ret);
				return HI_NULL;
			}
            
			for(i=0;i<IVECHARCALW;i++)
			{
			    HI_U64 u64TopLeft, u64TopRight, u64BtmLeft, u64BtmRight;
                HI_U64 *u64TopRow, *u64BtmRow;

                u64TopRow = (0 == i) ? (pu64VirData) : ( pu64VirData + (i * h -1) * stDst.u32Stride);
                u64BtmRow = pu64VirData + ((i + 1) * h - 1) * stDst.u32Stride;
                
				for(j=0;j<IVECHARCALH;j++)
				{
					HI_U64 u64BlockSum,u64BlockSq;

                    u64TopLeft  = (0 == i) ? (0) : ((0 == j) ? (0) : (u64TopRow[j * w-1]));
                    u64TopRight = (0 == i) ? (0) : (u64TopRow[(j + 1) * w - 1]);
                    u64BtmLeft  = (0 == j) ? (0) : (u64BtmRow[j * w - 1]);
                    u64BtmRight = u64BtmRow[(j + 1) * w -1];
                                  
                    u64BlockSum = (u64TopLeft & 0xfffffffLL) + (u64BtmRight & 0xfffffffLL)
                                - (u64BtmLeft & 0xfffffffLL) - (u64TopRight & 0xfffffffLL);

                    u64BlockSq  = (u64TopLeft >> 28) + (u64BtmRight >> 28)
                                - (u64BtmLeft >> 28) - (u64TopRight >> 28);
                   // mean
    				stChar[i * IVECHARCALW + j].s32X = u64BlockSum/(w*h);
    				stChar[i * IVECHARCALW + j].s32Y = sqrt(u64BlockSq/(w*h) - stChar[i * IVECHARCALW + j].s32X * stChar[i * IVECHARCALW + j].s32X);
                    //printf(" area = %llu; mean=%d, var=%d; w=%d, h=%d\n", u64BlockSum, stChar[i*IVECHARCALW+j].s32X, stChar[i*IVECHARCALW+j].s32Y, w, h);
                    //printf("mean=%d, var=%d; ", stChar[i * IVECHARCALW + j].s32X, stChar[i * IVECHARCALW + j].s32Y);
                
                    							
				}	
			}
			s32Ret=SAMPLE_IVE_Linear2DClassifer(&stChar[0],IVECHARNUM,pstLinearPoint,s32LinearNum);

            if(s32Ret>s32ThreshNum)
    		{
    			printf("\n\033[0;31mOcclusion detected in the %dth frame!\033[0;39m Please input 'e' to stop sample ...... \n", tic);
    		}
            else
            {
                printf("The %dth frame's occlusion blocks is %d. Please input 'e' to stop sample ...... \n", tic, s32Ret);
            }       	
			if(g_bStopSignal == HI_TRUE)
			{
				HI_MPI_SYS_MmzFree(stDst.u32PhyAddr, pu64VirData);
                HI_MPI_VDEC_ReleaseImage(1, &stFrameInfo);
                printf(".........\n");
				break;
			}

            s32Ret=HI_MPI_SYS_MmzFlushCache(stDst.u32PhyAddr , pu64VirData , stSrc.u32Height * stSrc.u32Width*8);
			if(s32Ret!=HI_SUCCESS)
			{
				HI_MPI_SYS_MmzFree(stDst.u32PhyAddr, pu64VirData);
                HI_MPI_VDEC_ReleaseImage(1, &stFrameInfo);
				printf(" ive integal function can't flush cache for %x\n",s32Ret);
				return HI_NULL;
			}

            HI_MPI_VDEC_ReleaseImage(1, &stFrameInfo);
				
		}
		return HI_NULL;
}

#define SAMPLE_IVE_ExitMpp()\
do{\
    HI_S32 i;\
    if (HI_MPI_SYS_Exit())\
    {\
        printf("sys exit fail\n");\
        return -1;\
    }\
    for(i=0;i<VB_MAX_USER;i++)\
    {\
         HI_MPI_VB_ExitModCommPool(i);\
    }\
    if (HI_MPI_VB_Exit())\
    {\
        printf("vb exit fail\n");\
        return -1;\
    }\
    return 0;\
}while(0)

#define SAMPLE_IVE_NOT_PASS(err)\
	do\
	{\
		printf("\033[0;31mtest case <%s>not pass at line:%d.\033[0;39m\n",__FUNCTION__,__LINE__);\
	}while(0)


#define SAMPLE_IVE_CHECK_RET(express,name)\
do{\
    HI_S32 s32Ret;\
    s32Ret = express;\
    if (HI_SUCCESS != s32Ret)\
    {\
        printf("%s failed at %s: LINE: %d with %#x!\n", name, __FUNCTION__, __LINE__, s32Ret);\
		SAMPLE_IVE_NOT_PASS(err);\
	    SAMPLE_IVE_ExitMpp();\
	    return HI_FAILURE;\
    }\
}while(0)


void SAMPLE_IVE_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(0);
}


HI_S32 main(int argc, char *argv[])
{
	pthread_t hIveThread;
	IVE_LINEAR_DATA_S stIveLinerData;

    HI_U32 u32ChnCnt = 2;
    HI_S32 s32VpssGrpCnt = 1;
    VO_DEV VoDev = SAMPLE_VO_DEV_DHD0;
    VO_LAYER VoLayer;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;

    VPSS_GRP_ATTR_S stGrpAttr;
    VO_PUB_ATTR_S stVoPubAttr; 
    SAMPLE_VO_MODE_E enVoMode;

    HI_S32 i;
    HI_U32 u32WndNum;
    
    PIC_SIZE_E enSize = PIC_D1;
    PAYLOAD_TYPE_E enPtH264Type = PT_H264;
    
    VB_CONF_S       stVbConf,stModVbConf;
    SIZE_S  stSize;
    VDEC_CHN_ATTR_S stVdecChnAttr[VDEC_MAX_CHN_NUM];
    VdecThreadParam stSampleVdecSendParam[VDEC_MAX_CHN_NUM];
    pthread_t SampleVdecSendStream[VDEC_MAX_CHN_NUM];
    
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32BlkSize;

    signal(SIGINT, SAMPLE_IVE_HandleSig);
    signal(SIGTERM, SAMPLE_IVE_HandleSig);
    
	
	g_bStopSignal = HI_FALSE;

    /******************************************
     step  1: init variable 
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                PIC_D1, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.u32MaxPoolCnt = 64;


    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 1 * 8;

    s32Ret=SAMPLE_COMM_SYS_GetPicSize(gs_enNorm,enSize,&stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get pic size failed with %d!\n", s32Ret);
        goto END_IVE_0;
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
        goto END_IVE_0;
    }

    /******************************************
    step 3: vdec mode vb init. 
    *****************************************/
    memset(&stModVbConf,0,sizeof(VB_CONF_S));
	SAMPLE_COMM_VDEC_ModCommPoolConf(&stModVbConf, enPtH264Type,&stSize);

    s32Ret = SAMPLE_COMM_VDEC_InitModCommVb(&stModVbConf);
	if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Vdec mode vb init failed with %d!\n", s32Ret);
        goto END_IVE_0;
    }

    /******************************************
     step 4: start vdec chn and send stream
    ******************************************/
    SAMPLE_COMM_VDEC_ChnAttr(u32ChnCnt,&stVdecChnAttr[0],enPtH264Type,&stSize);
    s32Ret = SAMPLE_COMM_VDEC_Start(u32ChnCnt,&stVdecChnAttr[0]);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start vdec failed with %d!\n", s32Ret);
        goto END_IVE_1;
    }

    SAMPLE_COMM_VDEC_ThreadParam(u32ChnCnt, stSampleVdecSendParam, &stVdecChnAttr[0], SAMPLE_D1_H264_PATH);
    SAMPLE_COMM_VDEC_StartSendStream(u32ChnCnt,stSampleVdecSendParam,SampleVdecSendStream);
	
    /******************************************
     step 5: start vpss with vdec bind vpss 
    ******************************************/
    stGrpAttr.u32MaxW = stSize.u32Width;
    stGrpAttr.u32MaxH = stSize.u32Height;
    stGrpAttr.bIeEn = HI_FALSE;
    stGrpAttr.bNrEn = HI_TRUE;
    stGrpAttr.bHistEn = HI_FALSE;
    stGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;
    stGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;

    s32Ret = SAMPLE_COMM_VPSS_Start(s32VpssGrpCnt, &stSize, 1,NULL);
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vpss failed!\n");
        goto END_IVE_2;
    }

    /******************************************
     step 6: bind vdec to vpss
    ******************************************/
    for(i = 0;i < u32ChnCnt-1;i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_BindVpss(i,i);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VDEC_BindVpss failed!\n");
            goto END_IVE_2;
        }
    }

    /******************************************
     step 7: start VO to preview
    ******************************************/
    printf("start vo hd0\n");
    u32WndNum = 1;
    enVoMode = VO_MODE_1MUX;
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;
    stVoPubAttr.enIntfType = VO_INTF_VGA;
    stVoPubAttr.u32BgColor = 0x000000ff;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);    
	if (HI_SUCCESS != s32Ret)
	{
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
        goto END_IVE_3;
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
        goto  END_IVE_4;
    }
    stLayerAttr.stImageSize.u32Width = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);    
	if (HI_SUCCESS != s32Ret)
	{
	   SAMPLE_PRT("SAMPLE_COMM_VO_StartLayer failed!\n");
	   goto END_IVE_5;
	}    
    /**************start Chn****************************/    
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer,enVoMode);    
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        goto  END_IVE_6;
    }      
    /**************vo bind to vpss****************************/
    for(i = 0;i < u32WndNum;i++)
    {
    	s32Ret = SAMPLE_COMM_VO_BindVpss(VoLayer, i, i, 0);
    	if (HI_SUCCESS != s32Ret)
    	{
    	   SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
    	   goto END_IVE_6;
    	}    
    }
    
	stIveLinerData.pstLinearPoint = malloc(sizeof(POINT_S)*10);
	stIveLinerData.s32LinearNum = 2;
	stIveLinerData.s32ThreshNum = IVECHARNUM/2;
	stIveLinerData.pstLinearPoint[0].s32X = 95;
	stIveLinerData.pstLinearPoint[0].s32Y = 0;
	stIveLinerData.pstLinearPoint[1].s32X = 95;
	stIveLinerData.pstLinearPoint[1].s32Y = 256;


	pthread_create(&hIveThread, 0, SAMPLE_IVE_BlockDetect, (HI_VOID *)&stIveLinerData);

    
	printf("press 'e' to exit\n");
	while(1)
	{
		char c;
		c=getchar();

		if(c=='e')            
			break;
	}
        
    g_bStopSignal = HI_TRUE;
	pthread_join(hIveThread,HI_NULL);
    SAMPLE_COMM_VDEC_StopSendStream(u32ChnCnt,stSampleVdecSendParam,SampleVdecSendStream);
	free(stIveLinerData.pstLinearPoint);

    for(i = 0;i < u32WndNum;i++)
    {
      SAMPLE_COMM_VO_UnBindVpss(VoLayer, i, i, 0);
    }

  END_IVE_6:       
    SAMPLE_COMM_VO_StopChn(VoLayer,enVoMode);  
  END_IVE_5:
    VoLayer = SAMPLE_VO_LAYER_VHD0;     
    SAMPLE_COMM_VO_StopLayer(VoLayer);

  END_IVE_4:
    VoDev = SAMPLE_VO_DEV_DHD0;        
    SAMPLE_COMM_VO_StopDev(VoDev);
  END_IVE_3:
    for(i = 0;i < u32ChnCnt-1;i++)
    {
         SAMPLE_COMM_VDEC_UnBindVpss(i,i);    
    }
  END_IVE_2:
    SAMPLE_COMM_VPSS_Stop(s32VpssGrpCnt, VPSS_MAX_CHN_NUM);
  END_IVE_1:
	SAMPLE_COMM_VDEC_Stop(u32ChnCnt);
    
  END_IVE_0:  
    SAMPLE_COMM_SYS_Exit();

	return 0;
}



