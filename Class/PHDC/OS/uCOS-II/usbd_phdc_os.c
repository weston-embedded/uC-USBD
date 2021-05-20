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
*                                 USB PHDC CLASS OPERATING SYSTEM LAYER
*                                          Micrium uC/OS-II
*
* Filename : usbd_phdc_os.c
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
#include  "../../usbd_phdc.h"
#include  "../../usbd_phdc_os.h"
#include  <Source/ucos_ii.h>


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
#ifndef USBD_PHDC_OS_CFG_SCHED_TASK_STK_SIZE
#error  "USBD_PHDC_OS_CFG_SCHED_TASK_STK_SIZE not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif

#ifndef USBD_PHDC_OS_CFG_SCHED_TASK_PRIO
#error  USBD_PHDC_OS_CFG_SCHED_TASK_PRIO not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif
#endif


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
#define  USBD_PHDC_OS_BULK_WR_PRIO_MAX                     5u
#else
#define  USBD_PHDC_OS_BULK_WR_PRIO_MAX                     1u
#endif


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
*                                          PHDC OS CTRL INFO
*********************************************************************************************************
*/

typedef struct usbd_phdc_os_ctrl {
    OS_EVENT     *WrIntrSem;                                    /* Lock that protect wr intr EP.                        */
    OS_EVENT     *RdSem;                                        /* Lock that protect rd bulk EP.                        */
    OS_EVENT     *WrBulkSem[USBD_PHDC_OS_BULK_WR_PRIO_MAX];     /* Sem that unlock bulk write of given prio.            */

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
    CPU_INT08U    WrBulkSemCtr[USBD_PHDC_OS_BULK_WR_PRIO_MAX];  /* Count of lock performed on given prio.               */
    CPU_BOOLEAN   ReleasedSched;                                /* Indicate if sched is released.                       */
    CPU_INT08U    WrBulkLockCnt;                                /* Count of call to WrBulkLock.                         */
#endif
} USBD_PHDC_OS_CTRL;


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

static  USBD_PHDC_OS_CTRL  USBD_PHDC_OS_CtrlTbl[USBD_PHDC_CFG_MAX_NBR_DEV];

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
static  OS_STK             USBD_PHDC_OS_WrBulkSchedTaskStk[USBD_PHDC_OS_CFG_SCHED_TASK_STK_SIZE];
                                                                /* Lock and release sem to control scheduler.           */
static  OS_EVENT          *USBD_PHDC_OS_EventsPendPtr[(USBD_PHDC_CFG_MAX_NBR_DEV * 2) + 1];
                                                                /* Array of ptr of posted sem.                          */
static  OS_EVENT          *USBD_PHDC_OS_EventsRdyPtr[(USBD_PHDC_CFG_MAX_NBR_DEV * 2) + 1];
static  void              *USBD_PHDC_OS_MsgRdyPtr[USBD_PHDC_CFG_MAX_NBR_DEV * 2];
#endif


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

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
static  void  USBD_PHDC_OS_WrBulkSchedTask(void  *p_arg);
#endif


/*
*********************************************************************************************************
*                                         USBD_PHDC_OS_Init()
*
* Description : Initialize PHDC OS interface.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               OS initialization successful.
*                               USBD_ERR_OS_SIGNAL_CREATE   OS semaphore NOT successfully initialized.
*                               USBD_ERR_OS_INIT_FAIL       OS task      NOT successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_Init (USBD_ERR  *p_err)
{
    USBD_PHDC_OS_CTRL   *p_os_ctrl;
    CPU_INT08U           ix;
    CPU_INT08U           cnt;
#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
    INT8U                os_err;
    OS_EVENT           **p_sched_lock_sem;
    OS_EVENT           **p_sched_release_sem;


    p_sched_lock_sem    = (OS_EVENT **)&USBD_PHDC_OS_EventsPendPtr[0];
    p_sched_release_sem = (OS_EVENT **)&USBD_PHDC_OS_EventsPendPtr[USBD_PHDC_CFG_MAX_NBR_DEV];
#endif


    for (cnt = 0; cnt < USBD_PHDC_CFG_MAX_NBR_DEV; cnt ++) {
        p_os_ctrl = &USBD_PHDC_OS_CtrlTbl[cnt];

        p_os_ctrl->WrIntrSem = OSSemCreate(1u);

        if (p_os_ctrl->WrIntrSem == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }

        p_os_ctrl->RdSem = OSSemCreate(1u);

        if (p_os_ctrl->RdSem == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }

        for (ix = 0u; ix < USBD_PHDC_OS_BULK_WR_PRIO_MAX; ix++) {
#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
            p_os_ctrl->WrBulkSem[ix] = OSSemCreate(0u);
#else
            p_os_ctrl->WrBulkSem[ix] = OSSemCreate(1u);
#endif

            if (p_os_ctrl->WrBulkSem[ix] == (OS_EVENT *)0) {
               *p_err = USBD_ERR_OS_SIGNAL_CREATE;
                return;
            }
#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
            p_os_ctrl->WrBulkSemCtr[ix] = 0u;
#endif
        }

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED

        p_sched_lock_sem[cnt] = OSSemCreate(0u);                /* Create sched lock sem.                               */

        if (p_sched_lock_sem[cnt] == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }


        p_sched_release_sem[cnt] = OSSemCreate(0u);             /* Create sched release sem.                            */

        if (p_sched_release_sem[cnt] == (OS_EVENT *)0) {
           *p_err = USBD_ERR_OS_SIGNAL_CREATE;
            return;
        }

        p_os_ctrl->WrBulkLockCnt = 0;
        p_os_ctrl->ReleasedSched = DEF_YES;                     /* Sched is released by dflt.                           */
#endif
    }

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
                                                                /* OSPendMulti needs a NULL terminated array.           */
    USBD_PHDC_OS_EventsPendPtr[USBD_PHDC_CFG_MAX_NBR_DEV * 2] = (OS_EVENT *)0;

#if (OS_TASK_CREATE_EXT_EN == 1u)

#if (OS_STK_GROWTH == 1u)                                       /* Create sched task.                                   */
    os_err = OSTaskCreateExt(        USBD_PHDC_OS_WrBulkSchedTask,
                             (void *)0,
                                    &USBD_PHDC_OS_WrBulkSchedTaskStk[USBD_PHDC_OS_CFG_SCHED_TASK_STK_SIZE - 1u],
                                     USBD_PHDC_OS_CFG_SCHED_TASK_PRIO,
                                     USBD_PHDC_OS_CFG_SCHED_TASK_PRIO,
                                    &USBD_PHDC_OS_WrBulkSchedTaskStk[0],
                                     USBD_PHDC_OS_CFG_SCHED_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
    os_err = OSTaskCreateExt(        USBD_PHDC_OS_WrBulkSchedTask,
                             (void *)0,
                                    &USBD_PHDC_OS_WrBulkSchedTaskStk[0],
                                     USBD_PHDC_OS_CFG_SCHED_TASK_PRIO,
                                     USBD_PHDC_OS_CFG_SCHED_TASK_PRIO,
                                    &USBD_PHDC_OS_WrBulkSchedTaskStk[USBD_PHDC_OS_CFG_SCHED_TASK_STK_SIZE - 1u],
                                     USBD_PHDC_OS_CFG_SCHED_TASK_STK_SIZE,
                             (void *)0,
                                     OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#endif

#else

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreate(        USBD_PHDC_OS_WrBulkSchedTask,
                          (void *)0,
                                 &USBD_PHDC_OS_WrBulkSchedTaskStk[USBD_OS_CFG_ASYNC_TASK_STK_SIZE - 1u],
                                  USBD_PHDC_OS_CFG_SCHED_TASK_PRIO);
#else
    os_err = OSTaskCreate(        USBD_PHDC_OS_WrBulkSchedTask,
                          (void *)0,
                                 &USBD_PHDC_OS_WrBulkSchedTaskStk[0],
                                  USBD_PHDC_OS_CFG_SCHED_TASK_PRIO);
#endif

#endif

#if (OS_TASK_NAME_EN > 0)
    OSTaskNameSet(USBD_PHDC_OS_CFG_SCHED_TASK_PRIO, (INT8U *)"USB PHDC Scheduler", &os_err);
#endif

    if (os_err !=  OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }
#endif

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_PHDC_OS_RdLock()
*
* Description : Lock PHDC read pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
*               timeout     Timeout, in ms.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS signal     successfully acquired.
*                               USBD_OS_ERR_TIMEOUT     OS signal NOT successfully acquired in the time
*                                                         specified by 'timeout'.
*                               USBD_OS_ERR_ABORT       OS signal aborted.
*                               USBD_OS_ERR_FAIL        OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_RdLock (CPU_INT08U   class_nbr,
                           CPU_INT16U   timeout,
                           USBD_ERR    *p_err)
{
    OS_EVENT           *p_event;
    USBD_PHDC_OS_CTRL  *p_os_ctrl;
    INT8U               os_err;
    INT32U              timeout_ticks;


    p_os_ctrl     = &USBD_PHDC_OS_CtrlTbl[class_nbr];
    p_event       =  p_os_ctrl->RdSem;
    timeout_ticks = ((((INT32U)timeout * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    OSSemPend(p_event, timeout_ticks, &os_err);

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
*                                       USBD_PHDC_OS_RdUnlock()
*
* Description : Unlock PHDC read pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_RdUnlock (CPU_INT08U  class_nbr)
{
    OS_EVENT           *p_event;
    USBD_PHDC_OS_CTRL  *p_os_ctrl;


    p_os_ctrl = &USBD_PHDC_OS_CtrlTbl[class_nbr];
    p_event   =  p_os_ctrl->RdSem;

    OSSemPost(p_event);
}


/*
*********************************************************************************************************
*                                      USBD_PHDC_OS_WrIntrLock()
*
* Description : Lock PHDC write interrupt pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
*               timeout     Timeout, in ms.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS signal     successfully acquired.
*                               USBD_OS_ERR_TIMEOUT     OS signal NOT successfully acquired in the time
*                                                       specified by 'timeout'.
*                               USBD_OS_ERR_ABORT       OS signal aborted.
*                               USBD_OS_ERR_FAIL        OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_WrIntrLock (CPU_INT08U   class_nbr,
                               CPU_INT16U   timeout,
                               USBD_ERR    *p_err)
{
    OS_EVENT           *p_event;
    USBD_PHDC_OS_CTRL  *p_os_ctrl;
    INT8U               os_err;
    INT32U              timeout_ticks;


    p_os_ctrl     = &USBD_PHDC_OS_CtrlTbl[class_nbr];
    p_event       =  p_os_ctrl->WrIntrSem;
    timeout_ticks = ((((INT32U)timeout * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    OSSemPend(p_event, timeout_ticks, &os_err);

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
*                                     USBD_PHDC_OS_WrIntrUnlock()
*
* Description : Unlock PHDC write interrupt pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_WrIntrUnlock (CPU_INT08U  class_nbr)
{
    OS_EVENT           *p_event;
    USBD_PHDC_OS_CTRL  *p_os_ctrl;


    p_os_ctrl = &USBD_PHDC_OS_CtrlTbl[class_nbr];
    p_event   =  p_os_ctrl->WrIntrSem;

    OSSemPost(p_event);
}


/*
*********************************************************************************************************
*                                      USBD_PHDC_OS_WrBulkLock()
*
* Description : Lock PHDC write bulk pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
*               timeout     Timeout, in ms.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS signal     successfully acquired.
*                               USBD_OS_ERR_TIMEOUT     OS signal NOT successfully acquired in the time
*                                                         specified by 'timeout'.
*                               USBD_OS_ERR_ABORT       OS signal aborted.
*                               USBD_OS_ERR_FAIL        OS signal not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_WrBulkLock (CPU_INT08U   class_nbr,
                               CPU_INT08U   prio,
                               CPU_INT16U   timeout,
                               USBD_ERR    *p_err)
{
    OS_EVENT            *p_event;
    USBD_PHDC_OS_CTRL   *p_os_ctrl;
    INT8U                os_err;
    INT32U               timeout_ticks;
#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
    OS_EVENT           **p_sched_lock_sem;
    CPU_SR_ALLOC();


    p_sched_lock_sem = (OS_EVENT **)&USBD_PHDC_OS_EventsPendPtr[0];
#else
    (void)prio;
#endif


    p_os_ctrl     = &USBD_PHDC_OS_CtrlTbl[class_nbr];
    timeout_ticks = ((((INT32U)timeout * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
    p_event =  p_os_ctrl->WrBulkSem[prio];

    CPU_CRITICAL_ENTER();
    p_os_ctrl->WrBulkSemCtr[prio]++;
    CPU_CRITICAL_EXIT();

    OSSemPost(p_sched_lock_sem[class_nbr]);
#else
    p_event = p_os_ctrl->WrBulkSem[0];
#endif

    OSSemPend(p_event, timeout_ticks, &os_err);

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
    CPU_CRITICAL_ENTER();
    if (p_os_ctrl->WrBulkSemCtr[prio] > 0) {
        p_os_ctrl->WrBulkSemCtr[prio]--;
    }
    CPU_CRITICAL_EXIT();
#endif

    switch (os_err) {
        case OS_ERR_NONE:
            *p_err = USBD_ERR_NONE;
             break;

        case OS_ERR_TIMEOUT:
#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
             CPU_CRITICAL_ENTER();
             if (p_os_ctrl->WrBulkLockCnt > 0) {
                 p_os_ctrl->WrBulkLockCnt--;
             }
             CPU_CRITICAL_EXIT();
#endif
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
*                                     USBD_PHDC_OS_WrBulkUnlock()
*
* Description : Unlock PHDC write bulk pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_WrBulkUnlock (CPU_INT08U  class_nbr)
{
    OS_EVENT            *p_event;
#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
    OS_EVENT           **p_sched_release_sem;


    p_sched_release_sem = (OS_EVENT **)&USBD_PHDC_OS_EventsPendPtr[USBD_PHDC_CFG_MAX_NBR_DEV];
    p_event             = p_sched_release_sem[class_nbr];
#else
    USBD_PHDC_OS_CTRL   *p_os_ctrl;


    p_os_ctrl = &USBD_PHDC_OS_CtrlTbl[class_nbr];
    p_event   = p_os_ctrl->WrBulkSem[0];
#endif

    OSSemPost(p_event);
}


/*
*********************************************************************************************************
*                                        USBD_PHDC_OS_Reset()
*
* Description : Reset PHDC OS layer for given instance.
*
* Argument(s) : class_nbr   PHDC instance number;
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_Reset (CPU_INT08U  class_nbr)
{
    USBD_PHDC_OS_CTRL        *p_os_ctrl;
    INT8U                     err;
    CPU_INT08U                cnt;
#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
    CPU_SR_ALLOC();
#endif


    p_os_ctrl = &USBD_PHDC_OS_CtrlTbl[class_nbr];

    (void)OSSemPendAbort(p_os_ctrl->WrIntrSem,                  /* Resume all task pending on sem.                      */
                         OS_PEND_OPT_BROADCAST,
                        &err);

    (void)OSSemPendAbort(p_os_ctrl->RdSem,
                         OS_PEND_OPT_BROADCAST,
                        &err);

    for (cnt = 0; cnt < USBD_PHDC_OS_BULK_WR_PRIO_MAX; cnt++) {
        (void)OSSemPendAbort(p_os_ctrl->WrBulkSem[cnt],
                             OS_PEND_OPT_BROADCAST,
                            &err);
    }

#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
    CPU_CRITICAL_ENTER();
    p_os_ctrl->WrBulkLockCnt = 0;
    p_os_ctrl->ReleasedSched = DEF_YES;

    for (cnt = 0; cnt < USBD_PHDC_OS_BULK_WR_PRIO_MAX; cnt++) {
        p_os_ctrl->WrBulkSemCtr[cnt] = 0;
    }
    CPU_CRITICAL_EXIT();
#endif
}


/*
*********************************************************************************************************
*                                   USBD_PHDC_OS_WrBulkSchedTask()
*
* Description : OS-dependent shell task to schedule bulk transfers in function of their priority.
*
* Argument(s) : p_arg       Pointer to task initialization argument (required by uC/OS-II).
*
* Return(s)   : none.
*
* Note(s)     : (1) Only one task handle all class instances bulk write scheduling.
*********************************************************************************************************
*/
#if USBD_PHDC_OS_CFG_SCHED_EN == DEF_ENABLED
static  void  USBD_PHDC_OS_WrBulkSchedTask (void *p_arg)
{
    INT8U                os_err;
    USBD_PHDC_OS_CTRL   *p_os_ctrl;
    OS_EVENT            *p_event_rdy;
    CPU_INT08U           prio;
    CPU_INT08U           cnt;
    CPU_INT16U           nbr_rdy;
    CPU_INT16U           rdy_cnt;
    CPU_BOOLEAN          found;
    OS_EVENT           **p_sched_lock_sem;
    OS_EVENT           **p_sched_release_sem;
    CPU_SR_ALLOC();


    (void)p_arg;

    p_sched_lock_sem    = (OS_EVENT **)&USBD_PHDC_OS_EventsPendPtr[0];
    p_sched_release_sem = (OS_EVENT **)&USBD_PHDC_OS_EventsPendPtr[USBD_PHDC_CFG_MAX_NBR_DEV];

    while (DEF_ON) {
        nbr_rdy = OSEventPendMulti(&USBD_PHDC_OS_EventsPendPtr[0],
                                   &USBD_PHDC_OS_EventsRdyPtr[0],
                                   &USBD_PHDC_OS_MsgRdyPtr[0],
                                    0,
                                   &os_err);
        if (os_err == OS_ERR_NONE) {

            for (rdy_cnt = 0; rdy_cnt < nbr_rdy; rdy_cnt++) {
                p_event_rdy = USBD_PHDC_OS_EventsRdyPtr[rdy_cnt];

                found = DEF_FALSE;                                  /* Loop to find class instance that posted sem.         */
                for (cnt = 0; cnt < USBD_PHDC_CFG_MAX_NBR_DEV; cnt++) {
                    p_os_ctrl = &USBD_PHDC_OS_CtrlTbl[cnt];

                    if (p_event_rdy == p_sched_lock_sem[cnt]) {
                        p_os_ctrl->WrBulkLockCnt++;
                        found = DEF_TRUE;
                    } else if (p_event_rdy == p_sched_release_sem[cnt]) {
                        p_os_ctrl->ReleasedSched = DEF_YES;
                        found                    = DEF_TRUE;
                    }

                    if ((p_os_ctrl->WrBulkLockCnt  > 0      ) &&
                        (p_os_ctrl->ReleasedSched == DEF_YES)) {
                        prio = 0u;

                        CPU_CRITICAL_ENTER();
                        p_os_ctrl->WrBulkLockCnt--;
                        p_os_ctrl->ReleasedSched = DEF_NO;

                        while (prio < USBD_PHDC_OS_BULK_WR_PRIO_MAX) {
                            if (p_os_ctrl->WrBulkSemCtr[prio] > 0) {
                                break;
                            }
                            prio++;
                        }
                        CPU_CRITICAL_EXIT();

                        if (prio < USBD_PHDC_OS_BULK_WR_PRIO_MAX) {
                            OSSemPost(p_os_ctrl->WrBulkSem[prio]);
                        }
                    }

                    if (found == DEF_TRUE) {
                        break;
                    }
                }
            }
        }
    }
}
#endif
