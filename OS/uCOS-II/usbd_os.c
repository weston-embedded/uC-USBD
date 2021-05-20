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
#include  <Source/ucos_ii.h>


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

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

                                                                /* -------------- USB EVENT QUEUE OBJECTS ------------- */
static  OS_EVENT  *USBD_OS_EventQPtr;
static  void      *USBD_OS_EventQ[USBD_CORE_EVENT_NBR_TOTAL];

static  OS_STK     USBD_OS_CoreTaskStk[USBD_OS_CFG_CORE_TASK_STK_SIZE];

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)
static  OS_EVENT  *USBD_OS_TraceSem;
static  OS_STK     USBD_OS_TraceTaskStk[USBD_OS_CFG_TRACE_TASK_STK_SIZE];
#endif

static  OS_EVENT  *USBD_OS_EP_SemTbl[USBD_CFG_MAX_NBR_DEV][USBD_CFG_MAX_NBR_EP_OPEN];
static  OS_EVENT  *USBD_OS_EP_LockTbl[USBD_CFG_MAX_NBR_DEV][USBD_CFG_MAX_NBR_EP_OPEN];


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
    INT8U  os_err;


                                                                /* Create USB events queue.                             */
    USBD_OS_EventQPtr = OSQCreate(&USBD_OS_EventQ[0], USBD_CORE_EVENT_NBR_TOTAL);

    if (USBD_OS_EventQPtr == (OS_EVENT *)0) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }
                                                                /* Create USB core task.                                */
#if (OS_TASK_CREATE_EXT_EN == 1u)

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(        USBD_OS_CoreTask,
                             (void *)0,
                                    &USBD_OS_CoreTaskStk[USBD_OS_CFG_CORE_TASK_STK_SIZE - 1u],
                                     USBD_OS_CFG_CORE_TASK_PRIO,
                                     USBD_OS_CFG_CORE_TASK_PRIO,
                                    &USBD_OS_CoreTaskStk[0],
                                     USBD_OS_CFG_CORE_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
    os_err = OSTaskCreateExt(        USBD_OS_CoreTask,
                             (void *)0,
                                    &USBD_OS_CoreTaskStk[0],
                                     USBD_OS_CFG_CORE_TASK_PRIO,
                                     USBD_OS_CFG_CORE_TASK_PRIO,
                                    &USBD_OS_CoreTaskStk[USBD_OS_CFG_CORE_TASK_STK_SIZE - 1u],
                                     USBD_OS_CFG_CORE_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#endif

#else

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreate(        USBD_OS_CoreTask,
                          (void *)0,
                                 &USBD_OS_CoreTaskStk[USBD_OS_CFG_CORE_TASK_STK_SIZE - 1u],
                                  USBD_OS_CFG_CORE_TASK_PRIO);
#else
    os_err = OSTaskCreate(        USBD_OS_CoreTask,
                          (void *)0,
                                 &USBD_OS_CoreTaskStk[0],
                                  USBD_OS_CFG_CORE_TASK_PRIO);
#endif

#endif

#if (OS_TASK_NAME_EN > 0)
    OSTaskNameSet(USBD_OS_CFG_CORE_TASK_PRIO, (INT8U *)"USB Core Task", &os_err);
#endif

    if (os_err !=  OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }

#if (USBD_CFG_DBG_TRACE_EN == DEF_ENABLED)

    USBD_OS_TraceSem = OSSemCreate(0u);
    if (USBD_OS_TraceSem == (OS_EVENT *)0) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }

#if (OS_TASK_CREATE_EXT_EN == 1u)

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(        USBD_OS_TraceTask,
                             (void *)0,
                                    &USBD_OS_TraceTaskStk[USBD_OS_CFG_TRACE_TASK_STK_SIZE - 1u],
                                     USBD_OS_CFG_TRACE_TASK_PRIO,
                                     USBD_OS_CFG_TRACE_TASK_PRIO,
                                    &USBD_OS_TraceTaskStk[0],
                                     USBD_OS_CFG_TRACE_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
    os_err = OSTaskCreateExt(        USBD_OS_TraceTask,
                             (void *)0,
                                    &USBD_OS_TraceTaskStk[0],
                                     USBD_OS_CFG_TRACE_TASK_PRIO,
                                     USBD_OS_CFG_TRACE_TASK_PRIO,
                                    &USBD_OS_TraceTaskStk[USBD_OS_CFG_TRACE_TASK_STK_SIZE - 1u],
                                     USBD_OS_CFG_TRACE_TASK_STK_SIZE,
                             (void *)0,
                                    (OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));
#endif

#else

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreate(        USBD_OS_TraceTask,
                          (void *)0,
                                 &USBD_OS_TraceTaskStk[USBD_OS_CFG_TRACE_TASK_STK_SIZE - 1u],
                                  USBD_OS_CFG_TRACE_TASK_PRIO);
#else
    os_err = OSTaskCreate(        USBD_OS_TraceTask,
                          (void *)0,
                                 &USBD_OS_TraceTaskStk[0],
                                  USBD_OS_CFG_TRACE_TASK_PRIO);
#endif

#endif

#if (OS_TASK_NAME_EN > 0)
    OSTaskNameSet(USBD_OS_CFG_TRACE_TASK_PRIO, (INT8U *)"USB Trace Task", &os_err);
#endif

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
    OS_EVENT  *p_event;


    p_event = OSSemCreate(0u);
    if (p_event == (OS_EVENT *)0) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
        return;
    }

    USBD_OS_EP_SemTbl[dev_nbr][ep_ix] = p_event;

   *p_err = USBD_ERR_NONE;
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
    OS_EVENT  *p_event;
    INT8U      os_err;


    p_event = USBD_OS_EP_SemTbl[dev_nbr][ep_ix];

    USBD_OS_EP_SemTbl[dev_nbr][ep_ix] = (OS_EVENT *)0;

    (void)OSSemDel(p_event,
                   OS_DEL_ALWAYS,
                  &os_err);
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
    OS_EVENT  *p_event;
    INT8U      os_err;
    INT32U     timeout_ticks;


    p_event       = USBD_OS_EP_SemTbl[dev_nbr][ep_ix];
    timeout_ticks = ((((INT32U)timeout_ms * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    OSSemPend(p_event,
              timeout_ticks,
             &os_err);
    switch (os_err) {
        case OS_ERR_NONE:
             p_event = USBD_OS_EP_SemTbl[dev_nbr][ep_ix];
             if (p_event == (OS_EVENT *)0) {
                *p_err = USBD_ERR_OS_DEL;
             } else {
                *p_err = USBD_ERR_NONE;
             }
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
    OS_EVENT  *p_event;
    INT8U      os_err;


    p_event = USBD_OS_EP_SemTbl[dev_nbr][ep_ix];
    (void)OSSemPendAbort(p_event,
                         OS_PEND_OPT_BROADCAST,
                        &os_err);
    switch (os_err) {
        case OS_ERR_NONE:
        case OS_ERR_PEND_ABORT:
            *p_err = USBD_ERR_NONE;
             break;


        case OS_ERR_SEM_OVF:
        case OS_ERR_EVENT_TYPE:
        case OS_ERR_PEVENT_NULL:
        default:
            *p_err = USBD_ERR_OS_FAIL;
             break;


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
    OS_EVENT  *p_event;
    INT8U      os_err;


    p_event = USBD_OS_EP_SemTbl[dev_nbr][ep_ix];
    os_err  = OSSemPost(p_event);
    if (os_err != OS_ERR_NONE) {
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
    OS_EVENT  *p_event;


    p_event = OSSemCreate(1u);
    if (p_event == (OS_EVENT *)0) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
        return;
    }

    USBD_OS_EP_LockTbl[dev_nbr][ep_ix] = p_event;

   *p_err = USBD_ERR_NONE;
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
    OS_EVENT  *p_event;
    INT8U      os_err;


    p_event = USBD_OS_EP_LockTbl[dev_nbr][ep_ix];

    USBD_OS_EP_LockTbl[dev_nbr][ep_ix] = (OS_EVENT *)0;

    (void)OSSemDel(p_event,
                   OS_DEL_ALWAYS,
                  &os_err);
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
    OS_EVENT  *p_event;
    INT8U      os_err;
    INT32U     timeout_ticks;


    p_event       = USBD_OS_EP_LockTbl[dev_nbr][ep_ix];
    timeout_ticks = ((((INT32U)timeout_ms * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    OSSemPend(p_event,
              timeout_ticks,
             &os_err);
    switch (os_err) {
        case OS_ERR_NONE:
             p_event = USBD_OS_EP_LockTbl[dev_nbr][ep_ix];
             if (p_event == (OS_EVENT *)0) {
                *p_err = USBD_ERR_OS_DEL;
             } else {
                *p_err = USBD_ERR_NONE;
             }
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
    OS_EVENT  *p_event;


    p_event = USBD_OS_EP_LockTbl[dev_nbr][ep_ix];
    (void)OSSemPost(p_event);
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
    (void)OSTimeDlyHMSM(0u, 0u, 0u, ms);
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
    void    *p_msg;
    INT32U   timeout_ticks;
    INT8U    os_err;


    timeout_ticks = (((timeout_ms * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    p_msg = OSQPend(USBD_OS_EventQPtr,
                    timeout_ticks,
                   &os_err);
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
    (void)OSQPost(USBD_OS_EventQPtr, p_event);
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
    (void)OSSemPost(USBD_OS_TraceSem);
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
    INT8U  os_err;


    OSSemPend(USBD_OS_TraceSem, 0u, &os_err);
}
#endif
