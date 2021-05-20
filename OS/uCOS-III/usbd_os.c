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
* Filename : usbd_os.c
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
#include  "../../Source/usbd_core.h"
#include  "../../Source/usbd_internal.h"
#include  <Source/os.h>


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef  OS_CFG_TASK_Q_EN
#error  "OS_CFG_TASK_Q_EN not #define'd in 'os_cfg.h' [MUST be > 0]"

#elif   (OS_CFG_TASK_Q_EN < 1u)
#error  "OS_CFG_TASK_Q_EN illegally #define'd in 'os_cfg.h' [MUST be > 0]"
#endif


#ifndef  OS_CFG_MUTEX_EN
#error  "OS_CFG_MUTEX_EN not #define'd in 'os_cfg.h' [MUST be > 0]"

#elif   (OS_CFG_MUTEX_EN < 1u)
#error  "OS_CFG_MUTEX_EN illegally #define'd in 'os_cfg.h' [MUST be > 0]"
#endif


#ifndef  OS_CFG_MUTEX_DEL_EN
#error  "OS_CFG_MUTEX_DEL_EN not #define'd in 'os_cfg.h' [MUST be > 0]"

#elif   (OS_CFG_MUTEX_DEL_EN < 1u)
#error  "OS_CFG_MUTEX_DEL_EN illegally #define'd in 'os_cfg.h' [MUST be > 0]"
#endif


#ifndef  USBD_OS_CFG_CORE_TASK_STK_SIZE
#error  "USBD_OS_CFG_CORE_TASK_STK_SIZE not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif


#ifndef  USBD_OS_CFG_CORE_TASK_PRIO
#error  "USBD_OS_CFG_CORE_TASK_PRIO not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif

#if     (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
#ifndef  USBD_OS_CFG_TRACE_TASK_STK_SIZE
#error  "USBD_OS_CFG_TRACE_TASK_STK_SIZE not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif


#ifndef  USBD_OS_CFG_TRACE_TASK_PRIO
#error  "USBD_OS_CFG_TRACE_TASK_PRIO not #define'd in 'app_cfg.h' [MUST be > 0]"
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

static  OS_TCB    USBD_OS_CoreTaskTCB;
static  CPU_STK   USBD_OS_CoreTaskStk[USBD_OS_CFG_CORE_TASK_STK_SIZE];

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
static  OS_TCB    USBD_OS_TraceTaskTCB;
static  CPU_STK   USBD_OS_TraceTaskStk[USBD_OS_CFG_TRACE_TASK_STK_SIZE];

#endif

static  OS_SEM    USBD_OS_EP_SemTbl[USBD_CFG_MAX_NBR_DEV][USBD_CFG_MAX_NBR_EP_OPEN];
static  OS_MUTEX  USBD_OS_EP_MutexTbl[USBD_CFG_MAX_NBR_DEV][USBD_CFG_MAX_NBR_EP_OPEN];


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

static  void  USBD_OS_CoreTask (void  *p_arg);

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
static  void  USBD_OS_TraceTask(void  *p_arg);
#endif


/*
*********************************************************************************************************
*                                           USBD_OS_Init()
*
* Description : Initialize OS interface.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS initialization successful.
*                               USBD_ERR_OS_INIT_FAIL   OS objects NOT successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_OS_Init (USBD_ERR  *p_err)
{
    OS_ERR  err_os;


    OSTaskCreate(        &USBD_OS_CoreTaskTCB,
                         "USB Core Task",
                          USBD_OS_CoreTask,
                  (void *)0,
                          USBD_OS_CFG_CORE_TASK_PRIO,
                         &USBD_OS_CoreTaskStk[0],
                          USBD_OS_CFG_CORE_TASK_STK_SIZE / 10u,
                          USBD_OS_CFG_CORE_TASK_STK_SIZE,
                          USBD_CORE_EVENT_NBR_TOTAL,
                          0u,
                  (void *)0,
                          OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                         &err_os);

    if (err_os !=  OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
    OSTaskCreate(        &USBD_OS_TraceTaskTCB,
                         "USB Trace Task",
                          USBD_OS_TraceTask,
                  (void *)0,
                          USBD_OS_CFG_TRACE_TASK_PRIO,
                         &USBD_OS_TraceTaskStk[0],
                          USBD_OS_CFG_TRACE_TASK_STK_SIZE / 10u,
                          USBD_OS_CFG_TRACE_TASK_STK_SIZE,
                          0u,
                          0u,
                  (void *)0,
                          OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                         &err_os);

    if (err_os !=  OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }
#endif

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_OS_CoreTask()
*
* Description : OS-dependent shell task to process USB core events.
*
* Argument(s) : p_arg       Pointer to task initialization argument (required by uC/OS-II).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_OS_CoreTask (void  *p_arg)
{
    (void)p_arg;

    while (DEF_ON) {
        USBD_CoreTaskHandler();
    }
}


/*
*********************************************************************************************************
*                                         USBD_OS_TraceTask)
*
* Description : OS-dependent shell task to process debug events.
*
* Argument(s) : p_arg       Pointer to task initialization argument (required by uC/OS-II).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
static  void  USBD_OS_TraceTask (void  *p_arg)
{
    (void)p_arg;

    while (DEF_ON) {
        USBD_DbgTaskHandler();
    }
}
#endif


/*
*********************************************************************************************************
*                                       USBD_OS_SignalCreate()
*
* Description : Create an OS signal.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument validated by the caller(s).
*
*               ep_ix       Endpoint index.
*               -----       Argument validated by the caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               OS signal     successfully created.
*                               USBD_ERR_OS_SIGNAL_CREATE   OS signal NOT successfully created.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_OS_EP_SignalCreate (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix,
                               USBD_ERR    *p_err)
{
    OS_SEM  *p_sem;
    OS_ERR   err;


    p_sem = &USBD_OS_EP_SemTbl[dev_nbr][ep_ix];

    OSSemCreate(p_sem,
               "USB-Device EP Signal",
                0u,
               &err);
    if (err != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
    } else {
       *p_err = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                         USBD_OS_SignalDel()
*
* Description : Delete an OS signal.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument validated by the caller(s).
*
*               ep_ix       Endpoint index.
*               -----       Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_OS_EP_SignalDel (CPU_INT08U  dev_nbr,
                            CPU_INT08U  ep_ix)
{
    OS_SEM  *p_sem;
    OS_ERR   err_os;


    p_sem = &USBD_OS_EP_SemTbl[dev_nbr][ep_ix];

    OSSemDel(p_sem,
             OS_OPT_DEL_ALWAYS,
            &err_os);
}


/*
*********************************************************************************************************
*                                        USBD_OS_SignalPend()
*
* Description : Wait for a signal to become available.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument validated by the caller(s).
*
*               ep_ix       Endpoint index.
*               -----       Argument validated by the caller(s).
*
*               timeout_ms  Signal wait timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS signal     successfully acquired.
*                               USBD_ERR_OS_TIMEOUT     OS signal NOT successfully acquired in the time
*                                                           specified by 'timeout_ms'.
*                               USBD_ERR_OS_ABORT       OS signal aborted.
*                               USBD_ERR_OS_FAIL        OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_OS_EP_SignalPend (CPU_INT08U   dev_nbr,
                             CPU_INT08U   ep_ix,
                             CPU_INT16U   timeout_ms,
                             USBD_ERR    *p_err)
{
    OS_SEM   *p_sem;
    OS_ERR    err;
    OS_TICK   timeout_ticks;


    p_sem         = &USBD_OS_EP_SemTbl[dev_nbr][ep_ix];
    timeout_ticks = ((((OS_TICK)timeout_ms * OSCfg_TickRate_Hz)  + 1000u - 1u) / 1000u);

    (void)OSSemPend(          p_sem,
                              timeout_ticks,
                              OS_OPT_PEND_BLOCKING,
                    (CPU_TS *)0,
                             &err);
    switch (err) {
        case OS_ERR_NONE:
            *p_err = USBD_ERR_NONE;
             break;


        case OS_ERR_TIMEOUT:
            *p_err = USBD_ERR_OS_TIMEOUT;
             break;


        case OS_ERR_PEND_ABORT:
            *p_err = USBD_ERR_OS_ABORT;
             break;


        case OS_ERR_OBJ_DEL:
        case OS_ERR_OBJ_PTR_NULL:
        case OS_ERR_OBJ_TYPE:
        case OS_ERR_OPT_INVALID:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_WOULD_BLOCK:
        case OS_ERR_SCHED_LOCKED:
        case OS_ERR_STATUS_INVALID:
        default:
            *p_err = USBD_ERR_OS_FAIL;
             break;


    }
}


/*
*********************************************************************************************************
*                                        USBD_OS_SignalAbort()
*
* Description : Abort any wait operation on signal.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument validated by the caller(s).
*
*               ep_ix       Endpoint index.
*               -----       Argument validated by the caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       OS signal     successfully aborted.
*                               USBD_ERR_OS_FAIL    OS signal NOT successfully aborted.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_OS_EP_SignalAbort (CPU_INT08U   dev_nbr,
                              CPU_INT08U   ep_ix,
                              USBD_ERR    *p_err)
{
    OS_SEM  *p_sem;
    OS_ERR   err;


    p_sem = &USBD_OS_EP_SemTbl[dev_nbr][ep_ix];

    OSSemPendAbort(p_sem,
                   OS_OPT_PEND_ABORT_ALL,
                  &err);
    if (err != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_FAIL;
    } else {
       *p_err = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                        USBD_OS_SignalPost()
*
* Description : Make a signal available.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument validated by the caller(s).
*
*               ep_ix       Endpoint index.
*               -----       Argument validated by the caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       OS signal     successfully readied.
*                               USBD_ERR_OS_FAIL    OS signal NOT successfully readied.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_OS_EP_SignalPost (CPU_INT08U   dev_nbr,
                             CPU_INT08U   ep_ix,
                             USBD_ERR    *p_err)
{
    OS_SEM  *p_sem;
    OS_ERR   err;


    p_sem = &USBD_OS_EP_SemTbl[dev_nbr][ep_ix];

    OSSemPost(p_sem,
              OS_OPT_POST_ALL,
             &err);
    if (err != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_FAIL;
    } else {
       *p_err = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                       USBD_OS_EP_LockCreate()
*
* Description : Create an OS resource to use as an endpoint lock.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument validated by the caller(s).
*
*               ep_ix       Endpoint index.
*               -----       Argument validated by the caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               OS lock     successfully created.
*                               USBD_ERR_OS_SIGNAL_CREATE   OS lock NOT successfully created.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void   USBD_OS_EP_LockCreate (CPU_INT08U   dev_nbr,
                              CPU_INT08U   ep_ix,
                              USBD_ERR    *p_err)
{
    OS_MUTEX  *p_mutex;
    OS_ERR     err_os;


    p_mutex = &USBD_OS_EP_MutexTbl[dev_nbr][ep_ix];

    OSMutexCreate(p_mutex,
                 "USB-Device EP Mutex",
                 &err_os);
    if (err_os != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
    } else {
       *p_err = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                         USBD_OS_EP_LockDel()
*
* Description : Delete the OS resource used as an endpoint lock.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument validated by the caller(s).
*
*               ep_ix       Endpoint index.
*               -----       Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void   USBD_OS_EP_LockDel (CPU_INT08U  dev_nbr,
                           CPU_INT08U  ep_ix)
{
    OS_MUTEX  *p_mutex;
    OS_ERR     err_os;


    p_mutex = &USBD_OS_EP_MutexTbl[dev_nbr][ep_ix];

    OSMutexDel(p_mutex,
               OS_OPT_DEL_ALWAYS,
              &err_os);
}


/*
*********************************************************************************************************
*                                       USBD_OS_EP_LockAcquire()
*
* Description : Wait for an endpoint to become available and acquire its lock.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument validated by the caller(s).
*
*               ep_ix       Endpoint index.
*               -----       Argument validated by the caller(s).
*
*               timeout_ms  Lock wait timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS lock     successfully acquired.
*                               USBD_ERR_OS_TIMEOUT     OS lock NOT successfully acquired in the time
*                                                           specified by 'timeout_ms'.
*                               USBD_ERR_OS_ABORT       OS lock aborted.
*                               USBD_ERR_OS_FAIL        OS lock not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void   USBD_OS_EP_LockAcquire (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix,
                               CPU_INT16U   timeout_ms,
                               USBD_ERR    *p_err)
{
    OS_MUTEX   *p_mutex;
    OS_ERR      err_os;
    OS_TICK     timeout_ticks;


    p_mutex       = &USBD_OS_EP_MutexTbl[dev_nbr][ep_ix];
    timeout_ticks = ((((OS_TICK)timeout_ms * OSCfg_TickRate_Hz)  + 1000u - 1u) / 1000u);

    (void)OSMutexPend(          p_mutex,
                                timeout_ticks,
                                OS_OPT_PEND_BLOCKING,
                      (CPU_TS *)0,
                               &err_os);
    switch (err_os) {
        case OS_ERR_NONE:
            *p_err = USBD_ERR_NONE;
             break;


        case OS_ERR_TIMEOUT:
            *p_err = USBD_ERR_OS_TIMEOUT;
             break;


        case OS_ERR_PEND_ABORT:
            *p_err = USBD_ERR_OS_ABORT;
             break;


        case OS_ERR_MUTEX_OWNER:
        case OS_ERR_OBJ_DEL:
        case OS_ERR_OBJ_PTR_NULL:
        case OS_ERR_OBJ_TYPE:
        case OS_ERR_OPT_INVALID:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_WOULD_BLOCK:
        case OS_ERR_SCHED_LOCKED:
        case OS_ERR_STATE_INVALID:
        case OS_ERR_STATUS_INVALID:
        default:
            *p_err = USBD_ERR_OS_FAIL;
             break;


    }
}


/*
*********************************************************************************************************
*                                      USBD_OS_EP_LockRelease()
*
* Description : Release an endpoint lock.
*
* Argument(s) : dev_nbr     Device number.
*               -------     Argument validated by the caller(s).
*
*               ep_ix       Endpoint index.
*               -----       Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void   USBD_OS_EP_LockRelease (CPU_INT08U  dev_nbr,
                               CPU_INT08U  ep_ix)
{
    OS_MUTEX  *p_mutex;
    OS_ERR     err_os;


    p_mutex = &USBD_OS_EP_MutexTbl[dev_nbr][ep_ix];

    OSMutexPost(p_mutex,
                OS_OPT_POST_NONE,
               &err_os);
    (void)err_os;
}


/*
*********************************************************************************************************
*                                        USBD_OS_DlyMs()
*
* Description : Delay a task for a certain time.
*
* Argument(s) : ms          Delay in milliseconds.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_OS_DlyMs (CPU_INT32U  ms)
{
    OS_ERR  err_os;


    OSTimeDlyHMSM(0u, 0u, 0u, ms,
                  OS_OPT_TIME_HMSM_NON_STRICT,
                 &err_os);
    (void)err_os;
}


/*
*********************************************************************************************************
*                                       USBD_OS_CoreEventGet()
*
* Description : Wait until a core event is ready.
*
* Argument(s) : timeout_ms  Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           Core event successfully obtained.
*                               USBD_ERR_OS_TIMEOUT     Core event NOT ready and a timeout occurred.
*                               USBD_ERR_OS_ABORT       Core event was aborted.
*                               USBD_ERR_OS_FAIL        Core event NOT ready because of another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  *USBD_OS_CoreEventGet (CPU_INT32U   timeout_ms,
                             USBD_ERR    *p_err)
{
    void         *p_msg;
    OS_TICK       timeout_ticks;
    OS_MSG_SIZE   msg_size;
    OS_ERR        err;


    timeout_ticks = (((timeout_ms * OSCfg_TickRate_Hz)  + 1000u - 1u) / 1000u);

    p_msg         = OSTaskQPend(          timeout_ticks,
                                          OS_OPT_PEND_BLOCKING,
                                         &msg_size,
                                (CPU_TS *)0,
                                         &err);
    switch (err) {
        case OS_ERR_NONE:
            *p_err = USBD_ERR_NONE;
             break;


        case OS_ERR_TIMEOUT:
            *p_err = USBD_ERR_OS_TIMEOUT;
             break;


        case OS_ERR_PEND_ABORT:
            *p_err = USBD_ERR_OS_ABORT;
             break;


        case OS_ERR_PEND_WOULD_BLOCK:
        case OS_ERR_Q_EMPTY:
        case OS_ERR_SCHED_LOCKED:
        default:
            *p_err = USBD_ERR_OS_FAIL;
             break;


    }

    return (p_msg);
}


/*
*********************************************************************************************************
*                                       USBD_OS_CoreEventPut()
*
* Description : Queues core event.
*
* Argument(s) : p_event     Pointer to core event.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_OS_CoreEventPut (void  *p_event)
{
    OS_ERR  err;


    OSTaskQPost(&USBD_OS_CoreTaskTCB,
                 p_event,
                 sizeof(void *),
                 OS_OPT_POST_FIFO,
                &err);
}


/*
*********************************************************************************************************
*                                        USBD_OS_DbgEventRdy()
*
* Description : Signals that a trace event is ready for processing.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
void  USBD_OS_DbgEventRdy (void)
{
    OS_ERR  err;


    (void)OSTaskSemPost(&USBD_OS_TraceTaskTCB,
                         OS_OPT_POST_NONE,
                        &err);
}
#endif


/*
*********************************************************************************************************
*                                       USBD_OS_DbgEventWait()
*
* Description : Waits until a trace event is available.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
void  USBD_OS_DbgEventWait (void)
{
    OS_ERR  err;


    (void)OSTaskSemPend(          0,
                                  OS_OPT_PEND_BLOCKING,
                        (CPU_TS *)0,
                                 &err);
}
#endif
