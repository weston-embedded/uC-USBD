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
*                                USB HID CLASS OPERATING SYSTEM LAYER
*                                          Micrium uC/OS-II
*
* Filename : usbd_hid_os.c
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
#include  "../../../../Source/usbd_core.h"
#include  "../../usbd_hid_report.h"
#include  "../../usbd_hid_os.h"
#include  <Source/ucos_ii.h>


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if     (OS_TICKS_PER_SEC < 250u)
#error  "OS_TICKS_PER_SEC illegally #define'd in 'os_cfg.h' [MUST be >= 250]"
#endif

#ifndef  USBD_HID_OS_CFG_TMR_TASK_STK_SIZE
#error  "USBD_HID_OS_CFG_TMR_TASK_STK_SIZE not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif


#ifndef  USBD_HID_OS_CFG_TMR_TASK_PRIO
#error  "USBD_HID_OS_CFG_TMR_TASK_PRIO not #define'd in 'app_cfg.h' [MUST be > 0]"
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

                                                                /* ---------------- USB HID SEM OBJECTS --------------- */
static  OS_EVENT  *USBD_HID_OS_InputDataSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];

static  OS_EVENT  *USBD_HID_OS_InputLockSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];

static  OS_EVENT  *USBD_HID_OS_TxSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];

static  OS_EVENT  *USBD_HID_OS_OutputLockSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];
static  OS_EVENT  *USBD_HID_OS_OutputDataSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];

static  OS_STK     USBD_HID_OS_TmrTaskStack[USBD_HID_OS_CFG_TMR_TASK_STK_SIZE];


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

static  void  USBD_HID_OS_TmrTask(void  *p_arg);


/*
*********************************************************************************************************
*                                         USBD_HID_OS_Init()
*
* Description : Initialize HID OS interface.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               HID OS initialization successful.
*                               USBD_ERR_OS_SIGNAL_CREATE   HID OS objects NOT successfully initialized.
*                               USBD_ERR_OS_INIT_FAIL       HID OS task    NOT successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_Init (USBD_ERR  *p_err)
{
    CPU_INT08U  class_nbr;
    INT8U       os_err;


    for (class_nbr = 0; class_nbr < USBD_HID_CFG_MAX_NBR_DEV; class_nbr++) {
        USBD_HID_OS_TxSem_Tbl[class_nbr] = OSSemCreate(1u);
        if (USBD_HID_OS_TxSem_Tbl[class_nbr] == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }

        USBD_HID_OS_OutputLockSem_Tbl[class_nbr] = OSSemCreate(1u);
        if (USBD_HID_OS_OutputLockSem_Tbl[class_nbr] == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }

        USBD_HID_OS_OutputDataSem_Tbl[class_nbr] = OSSemCreate(0u);
        if (USBD_HID_OS_OutputDataSem_Tbl[class_nbr] == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }

        USBD_HID_OS_InputLockSem_Tbl[class_nbr] = OSSemCreate(1u);
        if (USBD_HID_OS_InputLockSem_Tbl[class_nbr] == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }

        USBD_HID_OS_InputDataSem_Tbl[class_nbr] = OSSemCreate(0u);
        if (USBD_HID_OS_InputDataSem_Tbl[class_nbr] == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }
    }

#if (OS_TASK_CREATE_EXT_EN == 1u)

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(        USBD_HID_OS_TmrTask,
                             (void *)0,
                                    &USBD_HID_OS_TmrTaskStack[USBD_HID_OS_CFG_TMR_TASK_STK_SIZE - 1u],
                                     USBD_HID_OS_CFG_TMR_TASK_PRIO,
                                     USBD_HID_OS_CFG_TMR_TASK_PRIO,
                                    &USBD_HID_OS_TmrTaskStack[0],
                                     USBD_HID_OS_CFG_TMR_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
    os_err = OSTaskCreateExt(        USBD_HID_OS_TmrTask,
                             (void *)0,
                                    &USBD_HID_OS_TmrTaskStack[0],
                                     USBD_HID_OS_CFG_TMR_TASK_PRIO,
                                     USBD_HID_OS_CFG_TMR_TASK_PRIO,
                                    &USBD_HID_OS_TmrTaskStack[USBD_HID_OS_CFG_TMR_TASK_STK_SIZE - 1u],
                                     USBD_HID_OS_CFG_TMR_TASK_STK_SIZE,
                             (void *)0,
                                    (OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));
#endif

#else

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreate(        USBD_HID_OS_TmrTask,
                          (void *)0,
                                 &USBD_HID_OS_TmrTaskStack[USBD_HID_OS_CFG_TMR_TASK_STK_SIZE - 1u],
                                  USBD_HID_OS_CFG_TMR_TASK_PRIO);
#else
    os_err = OSTaskCreate(        USBD_HID_OS_TmrTask,
                          (void *)0,
                                 &USBD_HID_OS_TmrTaskStack[0],
                                  USBD_HID_OS_CFG_TMR_TASK_PRIO);
#endif

#endif

    if (os_err != OS_ERR_NONE) {
       *p_err   = USBD_ERR_OS_INIT_FAIL;
        return;
    }

#if (OS_TASK_STAT_EN > 0)
    OSTaskNameSet(USBD_HID_OS_CFG_TMR_TASK_PRIO, (INT8U *)"USB-Device HID Timer Task", &os_err);
#endif

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_HID_OS_TmrTask()
*
* Description : OS-dependent shell task to process periodic HID input reports.
*
* Argument(s) : p_arg       Pointer to task initialization argument (required by uC/OS-II).
*
* Return(s)   : none.
*
* Note(s)     : (1) Assumes OS_TICKS_PER_SEC frequency is greater than or equal to 250 Hz.  Otherwise,
*                   timer task scheduling rate will NOT be correct.
*********************************************************************************************************
*/

static  void  USBD_HID_OS_TmrTask (void  *p_arg)
{
    INT32U  dly;


   (void)p_arg;                                                /* Prevent 'variable unused' compiler warning.          */

    dly = OS_TICKS_PER_SEC / 250;                               /* Delay task at 4 milliseconds rate (see Note #1).     */

    while (DEF_ON) {
        OSTimeDly(dly);

        USBD_HID_Report_TmrTaskHandler();
    }
}


/*
*********************************************************************************************************
*                                       USBD_HID_OS_InputLock()
*
* Description : Lock class input report.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       Class input report successfully locked.
*                               USBD_ERR_OS_ABORT   Class input report aborted.
*                               USBD_ERR_OS_FAIL    OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_InputLock (CPU_INT08U   class_nbr,
                             USBD_ERR    *p_err)
{
    INT8U  os_err;


    OSSemPend(USBD_HID_OS_InputLockSem_Tbl[class_nbr],
              0,
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
}


/*
*********************************************************************************************************
*                                      USBD_HID_OS_InputUnlock()
*
* Description : Unlock class input report.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_InputUnlock (CPU_INT08U  class_nbr)
{
   (void)OSSemPost(USBD_HID_OS_InputLockSem_Tbl[class_nbr]);
}


/*
*********************************************************************************************************
*                                    USBD_HID_OS_OutputDataPendAbort()
*
* Description : Abort class output report.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_OutputDataPendAbort (CPU_INT08U  class_nbr)
{
    INT8U  os_err;


    (void)OSSemPendAbort(USBD_HID_OS_OutputDataSem_Tbl[class_nbr],
                         OS_PEND_OPT_BROADCAST,
                        &os_err);
}


/*
*********************************************************************************************************
*                                     USBD_HID_OS_OutputDataPend()
*
* Description : Wait for output report data transfer to complete.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
*               timeout_ms  Signal wait timeout, in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       Class output successfully locked.
*                               USBD_ERR_OS_ABORT   Class output aborted.
*                               USBD_ERR_OS_FAIL    OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_OutputDataPend (CPU_INT08U   class_nbr,
                                  CPU_INT16U   timeout_ms,
                                  USBD_ERR    *p_err)
{
    INT32U  timeout_ticks;
    INT8U   os_err;


    timeout_ticks = ((((INT32U)timeout_ms * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    OSSemPend(USBD_HID_OS_OutputDataSem_Tbl[class_nbr],
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
}


/*
*********************************************************************************************************
*                                     USBD_HID_OS_OutputDataPost()
*
* Description : Signal that output report data is available.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_OutputDataPost (CPU_INT08U  class_nbr)
{
   (void)OSSemPost(USBD_HID_OS_OutputDataSem_Tbl[class_nbr]);
}


/*
*********************************************************************************************************
*                                      USBD_HID_OS_OutputLock()
*
* Description : Lock class output report.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       Class output successfully locked.
*                               USBD_ERR_OS_ABORT   Class output aborted.
*                               USBD_ERR_OS_FAIL    OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_OutputLock (CPU_INT08U   class_nbr,
                              USBD_ERR    *p_err)
{
    INT8U  os_err;


    OSSemPend(USBD_HID_OS_OutputLockSem_Tbl[class_nbr],
              0,
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
}


/*
*********************************************************************************************************
*                                     USBD_HID_OS_OutputUnlock()
*
* Description : Unlock class output report.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_OutputUnlock (CPU_INT08U  class_nbr)
{
   (void)OSSemPost(USBD_HID_OS_OutputLockSem_Tbl[class_nbr]);
}


/*
*********************************************************************************************************
*                                        USBD_HID_OS_TxLock()
*
* Description : Lock class transmit.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE       Class feature report successfully locked.
*                               USBD_ERR_OS_ABORT   Class feature report aborted.
*                               USBD_ERR_OS_FAIL    OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_TxLock (CPU_INT08U   class_nbr,
                          USBD_ERR    *p_err)
{
    INT8U  os_err;


    OSSemPend(USBD_HID_OS_TxSem_Tbl[class_nbr],
              0,
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
}


/*
*********************************************************************************************************
*                                       USBD_HID_OS_TxUnlock()
*
* Description : Unlock class transmit.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_TxUnlock (CPU_INT08U  class_nbr)
{
   (void)OSSemPost(USBD_HID_OS_TxSem_Tbl[class_nbr]);
}


/*
*********************************************************************************************************
*                                     USBD_HID_OS_InputDataPend()
*
* Description : Wait for input report data transfer to complete.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
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

void  USBD_HID_OS_InputDataPend (CPU_INT08U   class_nbr,
                                 CPU_INT16U   timeout_ms,
                                 USBD_ERR    *p_err)
{
    INT32U  timeout_ticks;
    INT8U   os_err;


    timeout_ticks = ((((INT32U)timeout_ms * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    OSSemPend(USBD_HID_OS_InputDataSem_Tbl[class_nbr],
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
}


/*
*********************************************************************************************************
*                                  USBD_HID_OS_InputDataPendAbort()
*
* Description : Abort any operation on input report.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_InputDataPendAbort (CPU_INT08U  class_nbr)
{
    INT8U  os_err;


    (void)OSSemPendAbort(USBD_HID_OS_InputDataSem_Tbl[class_nbr],
                         OS_PEND_OPT_BROADCAST,
                        &os_err);
}


/*
*********************************************************************************************************
*                                     USBD_HID_OS_InputDataPost()
*
* Description : Signal that input report data transfer has completed.
*
* Argument(s) : class_nbr   Class instance number.
*               ---------   Argument validated by the caller(s).
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_OS_InputDataPost (CPU_INT08U  class_nbr)
{
   (void)OSSemPost(USBD_HID_OS_InputDataSem_Tbl[class_nbr]);
}
