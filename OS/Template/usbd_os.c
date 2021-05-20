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
*                                          Micrium Template
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
#include  "../../Source/usbd_core.h"
#include  "../../Source/usbd_internal.h"


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


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
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_OS_CoreTask()
*
* Description : OS-dependent shell task to process USB core events.
*
* Argument(s) : p_arg       Pointer to task initialization argument (can be removed, depending of the kernel).
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
* Argument(s) : p_arg       Pointer to task initialization argument (can be removed, depending of the kernel).
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
   *p_err = USBD_ERR_NONE;
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
   *p_err = USBD_ERR_NONE;
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
   *p_err = USBD_ERR_NONE;
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
   *p_err = USBD_ERR_NONE;
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
   *p_err = USBD_ERR_NONE;

    return ((void *)0);
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
}
#endif
