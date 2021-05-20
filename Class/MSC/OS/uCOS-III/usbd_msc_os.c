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
*                                          Micrium uC/OS-III
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
#include  <Source/os.h>


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

static  OS_TCB   USBD_MSC_OS_TaskTCB;
static  CPU_STK  USBD_MSC_OS_TaskStk[USBD_MSC_OS_CFG_TASK_STK_SIZE];

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
static  OS_TCB   USBD_MSC_OS_RefreshTaskTCB;
static  CPU_STK  USBD_MSC_OS_RefreshTaskStk[USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE];
#endif

static  OS_SEM   USBD_MSC_OS_TASK_SemTbl[USBD_MSC_CFG_MAX_NBR_DEV];

static  OS_SEM   USBD_MSC_OS_EnumSignal;


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
    OS_ERR        kernel_err;
    CPU_INT08U    class_nbr;
    OS_SEM       *p_comm_sem;
    OS_SEM       *p_enum_sem;


                                                                /* Create sem for signal used for MSC comm.             */
    for (class_nbr = 0u; class_nbr < USBD_MSC_CFG_MAX_NBR_DEV; class_nbr++) {
        p_comm_sem = &USBD_MSC_OS_TASK_SemTbl[class_nbr];
        OSSemCreate(p_comm_sem,
                   "USB-Device MSC Task Sem",
                    0u,
                   &kernel_err);
        if (kernel_err != OS_ERR_NONE) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }
    }
                                                                /* Create sem for signal used for MSC enum.             */
    p_enum_sem = &USBD_MSC_OS_EnumSignal;
    OSSemCreate(p_enum_sem,
               "USB-Device MSC Connect Sem",
                0u,
               &kernel_err);
    if (kernel_err != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
        return;
    }

    OSTaskCreate(        &USBD_MSC_OS_TaskTCB,
                         "USB MSC Task",
                          USBD_MSC_OS_Task,
                 (void *) 0,
                          USBD_MSC_OS_CFG_TASK_PRIO,
                         &USBD_MSC_OS_TaskStk[0],
                          USBD_MSC_OS_CFG_TASK_STK_SIZE / 10u,
                          USBD_MSC_OS_CFG_TASK_STK_SIZE,
                          0u,
                          0u,
                 (void *) 0,
                          OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                         &kernel_err);
    if (kernel_err != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
    OSTaskCreate(        &USBD_MSC_OS_RefreshTaskTCB,           /* Create the refresh task                              */
                         "Storage Refresh",
                          USBD_MSC_OS_RefreshTask,
                (void *)  0,
                          USBD_MSC_OS_CFG_REFRESH_TASK_PRIO,
                         &USBD_MSC_OS_RefreshTaskStk[0],
                          USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE / 10,
                          USBD_MSC_OS_CFG_REFRESH_TASK_STK_SIZE,
                          5u,
                          0u,
                (void  *) 0,
                          OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                         &kernel_err);
    if (kernel_err != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }
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
    OS_ERR  kernel_err;


    p_arg = p_arg;

    while (DEF_TRUE) {
        USBD_StorageRefreshTaskHandler(p_arg);

        OSTimeDlyHMSM(            0u,
                                  0u,
                                  0u,
                      (CPU_INT32U)USBD_MSC_CFG_DEV_POLL_DLY_mS,
                                  OS_OPT_TIME_HMSM_STRICT,
                                 &kernel_err);
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
    OS_SEM  *p_comm_sem;
    OS_ERR   kernel_err;


    p_comm_sem = &USBD_MSC_OS_TASK_SemTbl[class_nbr];

    OSSemPost(p_comm_sem,
              OS_OPT_POST_1,
             &kernel_err);
    if(kernel_err == OS_ERR_NONE) {
       *p_err = USBD_ERR_NONE;
    } else {
       *p_err = USBD_ERR_OS_FAIL;
    }
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
    OS_SEM  *p_comm_sem;
    OS_ERR   kernel_err;
    OS_TICK  timeout_ticks;


    p_comm_sem    = &USBD_MSC_OS_TASK_SemTbl[class_nbr];
    timeout_ticks = ((((OS_TICK)timeout * OSCfg_TickRate_Hz) + 1000u - 1u) / 1000u);

    OSSemPend(          p_comm_sem,
                        timeout_ticks,
                        OS_OPT_PEND_BLOCKING,
              (CPU_TS *)0,
                       &kernel_err);

    switch (kernel_err) {
        case OS_ERR_NONE:
            *p_err = USBD_ERR_NONE;
             break;


        case OS_ERR_TIMEOUT:
            *p_err = USBD_ERR_OS_TIMEOUT;
             break;


        case OS_ERR_PEND_ABORT:
            *p_err = USBD_ERR_OS_ABORT;
             break;


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
    OS_SEM  *p_comm_sem;
    OS_ERR   kernel_err;


    p_comm_sem = &USBD_MSC_OS_TASK_SemTbl[class_nbr];

    OSSemDel(p_comm_sem,
             OS_OPT_DEL_NO_PEND,
            &kernel_err);
    if (kernel_err != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_FAIL;
    } else {

       *p_err = USBD_ERR_NONE;
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

void  USBD_MSC_OS_EnumSignalPost (USBD_ERR  *p_err)
{
    OS_SEM  *p_enum_sem;
    OS_ERR   kernel_err;


    p_enum_sem = &USBD_MSC_OS_EnumSignal;

    OSSemPost(p_enum_sem,
              OS_OPT_POST_1,
             &kernel_err);
    if(kernel_err == OS_ERR_NONE) {
       *p_err = USBD_ERR_NONE;
    } else {
       *p_err = USBD_ERR_OS_FAIL;
    }
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
    OS_SEM  *p_enum_sem;
    OS_ERR   kernel_err;
    OS_TICK  timeout_ticks;


    p_enum_sem    = &USBD_MSC_OS_EnumSignal;
    timeout_ticks = ((((OS_TICK)timeout * OSCfg_TickRate_Hz) + 1000u - 1u) / 1000u);

    OSSemPend(          p_enum_sem,
                        timeout_ticks,
                        OS_OPT_PEND_BLOCKING,
              (CPU_TS *)0,
                       &kernel_err);

    switch (kernel_err) {
        case OS_ERR_NONE:
            *p_err = USBD_ERR_NONE;
             break;


        case OS_ERR_TIMEOUT:
            *p_err = USBD_ERR_OS_TIMEOUT;
             break;


        case OS_ERR_PEND_ABORT:
            *p_err = USBD_ERR_OS_ABORT;
             break;


        default:
            *p_err = USBD_ERR_OS_FAIL;
             break;
    }
}




