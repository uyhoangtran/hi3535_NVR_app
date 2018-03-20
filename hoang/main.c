/******************************************************************************
  Hi3535 NVR main program
  Hoang Tran 
 ******************************************************************************
    Init. date: 2018/03/18
    Reference: Hi3535 SDK V100
******************************************************************************/

#include "main.h"
 
/******************************************************************************
* function : Signal Handler
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
* function : main function
******************************************************************************/
int main(int argc, char *argv[])
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_CHAR ch;
    HI_BOOL bExit = HI_FALSE;

    signal(SIGINT, SAMPLE_VO_HandleSig);
    signal(SIGTERM, SAMPLE_VO_HandleSig);
    printf("sample program \n");
    s32Ret = hiSystemInit();
    return s32Ret;
}