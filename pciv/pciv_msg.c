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

#include "hi_mcc_usrdev.h"
#include "pciv_msg.h"

/* we use msg port from PCIV_MSG_BASE_PORT to (PCIV_MSG_BASE_PORT + PCIV_MSG_MAX_PORT_NUM) */
static int g_nMsgFd[PCIV_MAX_CHIPNUM][PCIV_MSG_MAX_PORT_NUM+1];

static HI_S32 g_s32CurMsgPort = PCIV_MSG_BASE_PORT+1;

HI_S32 PCIV_AllocMsgPort(HI_S32 *ps32MsgPort)
{
    if (g_s32CurMsgPort > PCIV_MSG_MAX_PORT)
    {
        return HI_FAILURE;
    }
    *ps32MsgPort = g_s32CurMsgPort ++;
    return HI_SUCCESS;
}

HI_S32 PCIV_WaitConnect(HI_S32 s32TgtId)
{
    HI_S32 s32MsgFd, s32Ret;
    struct hi_mcc_handle_attr attr;
                
    s32MsgFd = open("/dev/mcc_userdev", O_RDWR);
    if (s32MsgFd <= 0)
    {
        printf("open pci msg dev fail!\n");
        return HI_FAILURE;
    }
    printf("open msg dev ok, fd:%d\n", s32MsgFd);

    if (ioctl(s32MsgFd, HI_MCC_IOC_ATTR_INIT, &attr))
    {
	    printf("initialization for attr failed!\n");
	    return -1;
    }

    attr.target_id = s32TgtId;
    printf("start check pci target id:%d  ... ... ... \n", s32TgtId);
    while (ioctl(s32MsgFd, HI_MCC_IOC_CHECK, &attr))
    {
        usleep(10000);
    }
    printf("have checked pci target id:%d ok ! \n", s32TgtId);

    attr.port      = 1000;
    attr.priority  = 0;
    s32Ret = ioctl(s32MsgFd, HI_MCC_IOC_CONNECT, &attr);
    HI_ASSERT((HI_SUCCESS == s32Ret));

    /* check target chip whether is start up, */
    /* PCI主从片通讯的握手处理:
        调用mcc的check接口检查对端是否启动，同时对端也必须调用check接口完成握手过程 */

    close(s32MsgFd);
    return HI_SUCCESS;
}

HI_S32 PCIV_OpenMsgPort(HI_S32 s32TgtId, HI_S32 s32Port)
{
    HI_S32 s32Ret = HI_SUCCESS;
    struct hi_mcc_handle_attr attr;
    HI_S32 s32MsgFd;

    if (s32TgtId >= PCIV_MAX_CHIPNUM || s32Port >= PCIV_MSG_MAX_PORT)
    {
        printf("invalid pci msg port(%d,%d)!\n", s32TgtId, s32Port);
        return HI_FAILURE;
    }
    
    if (g_nMsgFd[s32TgtId][s32Port-PCIV_MSG_BASE_PORT] > 0)
    {
        printf("pci msg port(%d,%d) have open!\n", s32TgtId, s32Port);
        return HI_FAILURE;
    }
        
    s32MsgFd = open("/dev/mcc_userdev", O_RDWR);
    if (s32MsgFd <= 0)
    {
        printf("open pci msg dev fail!\n");
        return HI_FAILURE;
    }

    attr.target_id = s32TgtId;
    attr.port      = s32Port;
    attr.priority  = 2;
    s32Ret = ioctl(s32MsgFd, HI_MCC_IOC_CONNECT, &attr);
    if (s32Ret)
    {
        printf("HI_MCC_IOC_CONNECT err, target:%d, port:%d\n",s32TgtId,s32Port);
        return -1;
    }
    
    g_nMsgFd[s32TgtId][s32Port-PCIV_MSG_BASE_PORT] = s32MsgFd;
    return HI_SUCCESS;    
}

HI_S32 PCIV_CloseMsgPort(HI_S32 s32TgtId, HI_S32 s32Port)
{
    HI_S32 s32MsgFd;
    
    if (s32TgtId >= PCIV_MAX_CHIPNUM || s32Port >= PCIV_MSG_MAX_PORT)
    {
        printf("invalid pci msg port(%d,%d)!\n", s32TgtId, s32Port);
        return HI_FAILURE;
    }
    
    s32MsgFd = g_nMsgFd[s32TgtId][s32Port-PCIV_MSG_BASE_PORT];
    
    if (s32MsgFd <= 0)
    {
        return HI_SUCCESS;
    }
    //printf("===================close port %d!\n", s32Port);
    close(s32MsgFd);
    g_nMsgFd[s32TgtId][s32Port-PCIV_MSG_BASE_PORT] = -1;
    
    return HI_SUCCESS;
}


HI_S32 PCIV_SendMsg(HI_S32 s32TgtId, HI_S32 s32Port, SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_S32 s32MsgFd;
    
    if (s32TgtId >= PCIV_MAX_CHIPNUM || s32Port >= PCIV_MSG_MAX_PORT)
    {
        printf("invalid pci msg port(%d,%d)!\n", s32TgtId, s32Port);
        return HI_FAILURE;
    }
        
    if (g_nMsgFd[s32TgtId][s32Port-PCIV_MSG_BASE_PORT] <= 0) 
    {
        printf("you should open msg port before send message !\n");
        return HI_FAILURE;
    }
    s32MsgFd = g_nMsgFd[s32TgtId][s32Port-PCIV_MSG_BASE_PORT];
    
    HI_ASSERT(pMsg->stMsgHead.u32MsgLen < SAMPLE_PCIV_MSG_MAXLEN);

    s32Ret = write(s32MsgFd, pMsg, pMsg->stMsgHead.u32MsgLen + sizeof(SAMPLE_PCIV_MSGHEAD_S));    
    if (s32Ret != pMsg->stMsgHead.u32MsgLen + sizeof(SAMPLE_PCIV_MSGHEAD_S))
    {
        printf("PCIV_SendMsg write_len err:%d\n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 PCIV_ReadMsg(HI_S32 s32TgtId, HI_S32 s32Port, SAMPLE_PCIV_MSG_S *pMsg)
{
    HI_S32 s32Ret, s32PortIndex;
    HI_U32 u32MsgLen = sizeof(SAMPLE_PCIV_MSGHEAD_S) + SAMPLE_PCIV_MSG_MAXLEN;
    
    if (s32TgtId >= PCIV_MAX_CHIPNUM || s32Port >= PCIV_MSG_MAX_PORT)
    {
        printf("invalid pci msg port(%d,%d)!\n", s32TgtId, s32Port);
        return HI_FAILURE;
    }
    s32PortIndex = s32Port - PCIV_MSG_BASE_PORT;
    
    if (g_nMsgFd[s32TgtId][s32PortIndex] <= 0) 
    {
        printf("you should open msg port before read message!\n");
        return HI_FAILURE;
    }
   
    s32Ret = read(g_nMsgFd[s32TgtId][s32PortIndex], pMsg, u32MsgLen);
    if (s32Ret <= 0)
    {
        return HI_FAILURE;
    }
    else if (s32Ret < sizeof(SAMPLE_PCIV_MSGHEAD_S))
    {
        printf("%s -> read len err:%d\n", __FUNCTION__, s32Ret);
        return HI_FAILURE;
    }
    
    pMsg->stMsgHead.u32MsgLen = s32Ret - sizeof(SAMPLE_PCIV_MSGHEAD_S);
    return HI_SUCCESS;
}


