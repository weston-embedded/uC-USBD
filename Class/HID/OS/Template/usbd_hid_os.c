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
*                                              TEMPLATE
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

/* $$$$ Include required header file(s) for the proprietary RTOS services. */
#include  "../../../../Source/usbd_core.h"
#include  "../../usbd_hid_report.h"
#include  <app_cfg.h>


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*
* $$$$ The periodic input reports task has a priority and a stack size defined in app_cfg.h. These
*      configuration errors ensure that the priority and the stack size are defined. You may want to reuse
*      the same constants for your task creation function.
*********************************************************************************************************
*/

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
*
* $$$$ Semaphores are used for several locking and synchronization mechanisms. 5 tables of semaphores are
*      declared below whose size is specified by USBD_HID_CFG_MAX_NBR_DEV defined in usbd_cfg.h.
*      Each HID class instance uses 5 sempahores:
*          One for HID Tx            Lock
*          One for HID Output Report Lock
*          One for HID Output Report Data Signal
*          One for HID Input  Report Lock
*          One for HID Input  Report Data Signal
*      Replace <semaphore data type> by your semaphore data type.
*
* $$$$ A stack area is created for the HID periodic input reports task whose size is specified by
*      USBD_HID_OS_CFG_TMR_TASK_STK_SIZE defined in app_cfg.h
*********************************************************************************************************
*/

                                                                /* ---------------- USB HID SEM OBJECTS --------------- */
static  <semaphore data type>  USBD_HID_OS_InputDataSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];

static  <semaphore data type>  USBD_HID_OS_InputLockSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];

static  <semaphore data type>  USBD_HID_OS_TxSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];

static  <semaphore data type>  USBD_HID_OS_OutputLockSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];
static  <semaphore data type>  USBD_HID_OS_OutputDataSem_Tbl[USBD_HID_CFG_MAX_NBR_DEV];
                                                                /* ---------------- USB HID TASK OBJECTS -------------- */
static  CPU_STK  USBD_HID_OS_TmrTaskStack[USBD_HID_OS_CFG_TMR_TASK_STK_SIZE];


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
    CPU_INT08U   class_nbr;

    /* $$$$ For each class instance... */
    for (class_nbr = 0; class_nbr < USBD_HID_CFG_MAX_NBR_DEV; class_nbr++) {
        /* $$$$ ... insert code to create the 5 required semaphores. If a semaphore creation fails, USBD_ERR_OS_SIGNAL_CREATE should be returned. */
    }

    /* $$$$ Insert code to create the HID periodic input reports task. If the task creation fails, USBD_ERR_OS_INIT_FAIL should be returned. */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_HID_OS_TmrTask()
*
* Description : OS-dependent shell task to process periodic HID input reports.
*
* Argument(s) : p_arg       Pointer to task initialization argument.
*
* Return(s)   : none.
*
* Note(s)     : (1) Timer task MUST delay without failure.
*
*                   (a) Failure to delay timer task will prevent some HID report task(s)/operation(s) from
*                       functioning correctly.
*********************************************************************************************************
*/

static  void  USBD_HID_OS_TmrTask (void  *p_arg)
{
   (void)p_arg;                                                /* Prevent 'variable unused' compiler warning.          */

    while (DEF_ON) {
        /* $$$$ Insert code to delay the task for 4 ms. */

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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_InputLockSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to pend on a semaphore used for Input Report Lock. */

   *p_err = USBD_ERR_NONE;
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_InputLockSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to post a semaphore used for Input Report Lock. */
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_OutputDataSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to abort the semaphore wait used for Output Report Data Signal. */
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_OutputDataSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to pend on a semaphore used for Output Report Data Signal. */

   *p_err = USBD_ERR_NONE;
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_OutputDataSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to post a semaphore used for Output Report Data Signal. */
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_OutputLockSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to pend on a semaphore used for Output Report Lock. */

   *p_err = USBD_ERR_NONE;
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_OutputLockSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to post a semaphore used for Output Report Lock. */
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_TxSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to pend on a semaphore used for Tx Lock. */

   *p_err = USBD_ERR_NONE;
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_TxSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to post a semaphore used for Tx Lock. */
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_InputDataSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to pend on a semaphore used for Input Report Data Signal. */
   *p_err = USBD_ERR_NONE;
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_InputDataSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to abort the semaphore wait used for Input Report Data Signal. */
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
    /* $$$$ 'class_nbr' argument can be used to retrieve the associated semaphore for the given HID class instance number. The semaphore may be gotten from USBD_HID_OS_InputDataSem_Tbl[] table. */
    (void)class_nbr;

    /* $$$$ Insert code to post a semaphore used for Input Report Data Signal. */
}
