/*
*********************************************************************************************************
*                                            uC/USB-Device
*                                    The Embedded USB Device Stack
*
*                    Copyright 2004-2021 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                   USB DEVICE OPERATING SYSTEM LAYER
*                                          Micrium uC/OS-II
*
* Filename : usbd_msc_os.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  <app_cfg.h>
#include  "../../usbd_msc.h"
#include  "../../usbd_msc_os.h"
#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
#include  "../../Storage/uC-FS/V4/usbd_storage.h"
#endif
#include  <Source/ucos_ii.h>


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef USBD_MSC_OS_CFG_TASK_STK_SIZE
#error  "USBD_MSC_OS_CFG_TASK_STK_SIZE not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif

#ifndef USBD_MSC_OS_CFG_TASK_PRIO
#error  "USBD_MSC_OS_CFG_TASK_PRIO not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
#ifndef USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE
#error  "USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif

#ifndef USBD_MSC_OS_CFG_REFRESH_TASK_PRIO
#error  "USBD_MSC_OS_CFG_REFRESH_TASK_PRIO not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif

#ifndef  USBD_MSC_CFG_DEV_POLL_DLY_mS
#error  "USBD_MSC_CFG_DEV_POLL_DLY_mS not #defined'd in 'usbd_cfg.h' [MUST be > 0]"
#elif   (USBD_MSC_CFG_DEV_POLL_DLY_mS == 0)
#error  "USBD_MSC_CFG_DEV_POLL_DLY_mS illegally #define'd in 'usbd_cfg.h' [MUST be > 0]"
#endif
#endif


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


static  OS_STK     USBD_MSC_OS_TaskStk[USBD_MSC_OS_CFG_TASK_STK_SIZE];

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
static  OS_STK     USBD_MSC_OS_RefreshTaskStk[USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE];
#endif

static  OS_EVENT  *USBD_MSC_OS_TaskSemTbl[USBD_MSC_CFG_MAX_NBR_DEV];
static  OS_EVENT  *USBD_MSC_OS_EnumSignal;


/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  USBD_MSC_OS_Task        (void  *p_arg);

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
static  void  USBD_MSC_OS_RefreshTask(void  *p_arg);
#endif

/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           USBD_MSC_OS_Init()
*
* Description : Initialize MSC OS interface.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       OS initialization successful.
*                               USBD_ERR_OS_FAIL    OS objects NOT successfully initialized.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_MSC_OS_Init (USBD_ERR  *p_err)
{
    OS_EVENT    **p_comm_sem;
    OS_EVENT    **p_enum_sem;
    INT8U         os_err;
    CPU_INT08U    class_nbr;


                                                                /* Create sem for signal used for MSC comm.             */
    for (class_nbr = 0; class_nbr < USBD_MSC_CFG_MAX_NBR_DEV; class_nbr++) {
        p_comm_sem = &USBD_MSC_OS_TaskSemTbl[class_nbr];
       *p_comm_sem = OSSemCreate(0u);
        if (*p_comm_sem == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }
    }
                                                                /* Create sem for signal used for MSC enum.             */
    p_enum_sem = &USBD_MSC_OS_EnumSignal;
   *p_enum_sem = OSSemCreate(0u);
    if (*p_enum_sem == (OS_EVENT *)0) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
        return;
    }

#if (OS_TASK_CREATE_EXT_EN == 1u)
#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(        USBD_MSC_OS_Task,
                             (void *)0,
                                    &USBD_MSC_OS_TaskStk[USBD_MSC_OS_CFG_TASK_STK_SIZE - 1u],
                                     USBD_MSC_OS_CFG_TASK_PRIO,
                                     USBD_MSC_OS_CFG_TASK_PRIO,
                                    &USBD_MSC_OS_TaskStk[0],
                                     USBD_MSC_OS_CFG_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
    os_err = OSTaskCreateExt(        USBD_MSC_OS_Task,
                             (void *)0,
                                    &USBD_MSC_OS_TaskStk[0],
                                     USBD_MSC_OS_CFG_TASK_PRIO,
                                     USBD_MSC_OS_CFG_TASK_PRIO,
                                    &USBD_MSC_OS_TaskStk[USBD_MSC_OS_CFG_TASK_STK_SIZE - 1u],
                                     USBD_MSC_OS_CFG_TASK_STK_SIZE,
                             (void *)0,
                                    (OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));
#endif

#else

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreate(        USBD_MSC_OS_Task,
                          (void *)0,
                                 &USBD_MSC_OS_TaskStk[USBD_MSC_OS_CFG_TASK_STK_SIZE - 1u],
                                  USBD_MSC_OS_CFG_TASK_PRIO);
#else
    os_err = OSTaskCreate(        USBD_MSC_OS_Task,
                          (void *)0,
                                 &USBD_MSC_OS_TaskStk[0],
                                  USBD_MSC_OS_CFG_TASK_PRIO);
#endif

#endif
    if (os_err != OS_ERR_NONE) {
       *p_err   = USBD_ERR_OS_INIT_FAIL;
        return;
    }

#if (OS_TASK_STAT_EN > 0)
    OSTaskNameSet(USBD_MSC_OS_CFG_TASK_PRIO, (INT8U *)"USB MSC Task", &os_err);
#endif

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)

#if (OS_TASK_CREATE_EXT_EN == 1u)
#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(        USBD_MSC_OS_RefreshTask,
                             (void *)0,
                                    &USBD_MSC_OS_RefreshTaskStk[USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE - 1u],
                                     USBD_MSC_OS_CFG_REFRESH_TASK_PRIO,
                                     USBD_MSC_OS_CFG_REFRESH_TASK_PRIO,
                                    &USBD_MSC_OS_RefreshTaskStk[0],
                                     USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
    os_err = OSTaskCreateExt(        USBD_MSC_OS_RefreshTask,
                             (void *)0,
                                    &USBD_MSC_OS_RefreshTaskStk[0],
                                     USBD_MSC_OS_CFG_REFRESH_TASK_PRIO,
                                     USBD_MSC_OS_CFG_REFRESH_TASK_PRIO,
                                    &USBD_MSC_OS_RefreshTaskStk[USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE - 1u],
                                     USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE,
                             (void *)0,
                                    (OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));
#endif

#else

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreate(        USBD_MSC_OS_RefreshTask,
                          (void *)0,
                                 &USBD_MSC_OS_RefreshTaskStk[USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE - 1u],
                                  USBD_MSC_OS_CFG_REFRESH_TASK_PRIO);
#else
    os_err = OSTaskCreate(        USBD_MSC_OS_RefreshTask,
                          (void *)0,
                                 &USBD_MSC_OS_RefreshTaskStk[0],
                                  USBD_MSC_OS_CFG_REFRESH_TASK_PRIO);
#endif
#endif
    if (os_err != OS_ERR_NONE) {
       *p_err   = USBD_ERR_OS_INIT_FAIL;
        return;
    }

#if (OS_TASK_STAT_EN > 0)
    OSTaskNameSet(USBD_MSC_OS_CFG_REFRESH_TASK_PRIO, (INT8U *)"USB MSC Refresh Task", &os_err);
#endif

#endif


   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_MSC_OS_Task()
*
* Description : OS-dependent shell task to process MSC task
*
* Argument(s) : p_arg       Pointer to task initialization argument (required by uC/OS-II).
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_MSC_OS_Task (void  *p_arg)
{
    CPU_INT08U  class_nbr;


    class_nbr = (CPU_INT08U)(CPU_ADDR)p_arg;

    USBD_MSC_TaskHandler(class_nbr);
}


/*
*********************************************************************************************************
*                                         USBD_MSC_OS_RefreshTask()
*
* Description : OS-dependent shell task to process Device Refresh task
*
* Argument(s) : p_arg       Pointer to task initialization argument (required by uC/OS-II).
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
static  void  USBD_MSC_OS_RefreshTask (void  *p_arg)
{
    p_arg = p_arg;

    while (DEF_TRUE) {
        USBD_StorageRefreshTaskHandler(p_arg);

        OSTimeDlyHMSM(        0u,
                              0u,
                              0u,
                      (INT16U)USBD_MSC_CFG_DEV_POLL_DLY_mS);
    }
}
#endif


/*
*********************************************************************************************************
*                                          USBD_MSC_OS_CommSignalPost()
*
* Description : Post a semaphore used for MSC communication.
*
* Argument(s) : class_nbr   MSC instance class number
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       OS signal     successfully posted.
*                               USBD_ERR_OS_FAIL    OS signal NOT successfully posted.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_MSC_OS_CommSignalPost (CPU_INT08U   class_nbr,
                                  USBD_ERR    *p_err)
{
    OS_EVENT  *p_comm_sem;


    p_comm_sem = USBD_MSC_OS_TaskSemTbl[class_nbr];

    OSSemPost(p_comm_sem);

    *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_MSC_OS_CommSignalPend()
*
* Description : Wait on a semaphore to become available for MSC communication.
*
* Argument(s) : class_nbr   MSC instance class number
*
*               timeout     Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*                               USBD_ERR_NONE          The call was successful and your task owns the resource
*                                                       or, the event you are waiting for occurred.
*                               USBD_ERR_OS_TIMEOUT    The semaphore was not received within the specified timeout.
*                               USBD_ERR_OS_FAIL       otherwise.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_MSC_OS_CommSignalPend (CPU_INT08U   class_nbr,
                                  CPU_INT32U   timeout,
                                  USBD_ERR    *p_err)
{
    OS_EVENT  *p_comm_sem;
    INT8U      os_err;
    INT32U     timeout_ticks;


    p_comm_sem    = USBD_MSC_OS_TaskSemTbl[class_nbr];
    timeout_ticks = ((((INT32U)timeout * OS_TICKS_PER_SEC) + 1000u - 1u) / 1000u);

    OSSemPend(p_comm_sem, timeout_ticks, &os_err);

    switch (os_err) {
        case OS_ERR_NONE:
            *p_err = USBD_ERR_NONE;
             break;

        case OS_ERR_TIMEOUT:
            *p_err = USBD_ERR_OS_TIMEOUT;
             break;

        case OS_ERR_PEND_ABORT:
            *p_err = USBD_ERR_OS_ABORT;
             break;

        case OS_ERR_EVENT_TYPE:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEVENT_NULL:
        case OS_ERR_PEND_LOCKED:
        default:
            *p_err = USBD_ERR_OS_FAIL;
             break;
    }
}


/*
*********************************************************************************************************
*                                         USBD_MSC_OS_CommSignalDel()
*
* Description : Delete a semaphore if no tasks are waiting on it for MSC communication.
*
* Argument(s) : class_nbr   MSC instance class number
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*                               USBD_ERR_NONE          The call was successful and the semaphore was destroyed
*                               USBD_ERR_OS_FAIL       otherwise.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_MSC_OS_CommSignalDel (CPU_INT08U   class_nbr,
                                 USBD_ERR    *p_err)
{
    OS_EVENT  *p_comm_sem;
    INT8U      os_err;

    p_comm_sem = USBD_MSC_OS_TaskSemTbl[class_nbr];

    OSSemDel(p_comm_sem, OS_DEL_ALWAYS, &os_err);

    if (os_err == OS_ERR_NONE) {
       *p_err = USBD_ERR_NONE;
    } else {
       *p_err = USBD_ERR_OS_FAIL;
    }

}


/*
*********************************************************************************************************
*                                          USBD_MSC_OS_EnumSignalPost()
*
* Description : Post a semaphore for MSC enumeration process.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       OS signal     successfully posted.
*                               USBD_ERR_OS_FAIL    OS signal NOT successfully posted.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_MSC_OS_EnumSignalPost (USBD_ERR   *p_err)
{
    OS_EVENT  *p_enum_sem;


    p_enum_sem = USBD_MSC_OS_EnumSignal;

    OSSemPost(p_enum_sem);

    *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       USBD_MSC_OS_EnumSignalPend()
*
* Description : Wait on a semaphore to become available for MSC enumeration process.
*
* Argument(s) : timeout     Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*                               USBD_ERR_NONE          The call was successful and your task owns the resource
*                                                       or, the event you are waiting for occurred.
*                               USBD_ERR_OS_TIMEOUT    The semaphore was not received within the specified timeout.
*                               USBD_ERR_OS_FAIL       otherwise.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_MSC_OS_EnumSignalPend (CPU_INT32U  timeout,
                                  USBD_ERR   *p_err)
{
    OS_EVENT  *p_enum_sem;
    INT8U      os_err;
    INT32U     timeout_ticks;


    p_enum_sem    = USBD_MSC_OS_EnumSignal;
    timeout_ticks = ((((INT32U)timeout * OS_TICKS_PER_SEC) + 1000u - 1u) / 1000u);

    OSSemPend(p_enum_sem, timeout_ticks, &os_err);

    switch (os_err) {
        case OS_ERR_NONE:
            *p_err = USBD_ERR_NONE;
             break;

        case OS_ERR_TIMEOUT:
            *p_err = USBD_ERR_OS_TIMEOUT;
             break;

        case OS_ERR_PEND_ABORT:
            *p_err = USBD_ERR_OS_ABORT;
             break;

        case OS_ERR_EVENT_TYPE:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEVENT_NULL:
        case OS_ERR_PEND_LOCKED:
        default:
            *p_err = USBD_ERR_OS_FAIL;
             break;
    }
}



