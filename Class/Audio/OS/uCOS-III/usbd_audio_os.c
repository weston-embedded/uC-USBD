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
*                                USB AUDIO DEVICE OPERATING SYSTEM LAYER
*                                          Micrium uC/OS-III
*
* Filename : usbd_audio_os.c
* Version  : V4.06.01
*********************************************************************************************************
*/

#include  <app_cfg.h>
#include  "../../usbd_audio_internal.h"
#include  "../../usbd_audio_os.h"
#include  <Source/os.h>
#include  <os_cfg_app.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
#ifndef  USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE
#error  "USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif

#ifndef  USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO
#error  "USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif
#endif

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
#ifndef  USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE
#error  "USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif

#ifndef  USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO
#error  "USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO not #define'd in 'app_cfg.h' [MUST be > 0]"
#endif
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
static  OS_TCB    USBD_Audio_OS_RecordTaskTCB;
static  CPU_STK   USBD_Audio_OS_RecordTaskStk[USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE];
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
static  OS_TCB    USBD_Audio_OS_PlaybackTaskTCB;
static  CPU_STK   USBD_Audio_OS_PlaybackTaskStk[USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE];
#endif

static  OS_MUTEX  USBD_Audio_OS_AS_IF_MutexTbl[USBD_AUDIO_MAX_NBR_AS_IF_EP];
static  OS_MUTEX  USBD_Audio_OS_RingBufQMutexTbl[USBD_AUDIO_MAX_NBR_AS_IF_SETTINGS];


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
static  void  USBD_Audio_OS_RecordTask  (void  *p_arg);
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
static  void  USBD_Audio_OS_PlaybackTask(void  *p_arg);
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         USBD_Audio_OS_Init()
*
* Description : Initialize the audio class OS layer.
*
* Argument(s) : msg_qty     Maximum quantity of messages for playback and record tasks' queues.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE               OS layer initialization successful.
*                           USBD_ERR_OS_INIT_FAIL       OS layer initialization failed.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_Audio_OS_Init (CPU_INT16U   msg_qty,
                          USBD_ERR    *p_err)
{
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
    OS_ERR  err_os;
#else
    (void)msg_qty;
#endif

                                                                /* -------------------- RECORD TASK ------------------- */
#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
    OSTaskCreate(&USBD_Audio_OS_RecordTaskTCB,
                 "USBD Audio Record Task",
                  USBD_Audio_OS_RecordTask,
                  DEF_NULL,
                  USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO,
                 &USBD_Audio_OS_RecordTaskStk[0u],
                  USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE / 10u,
                  USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE,
                  msg_qty,                              /* Record buf queue.                                    */
                  0u,
                  DEF_NULL,
                  OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err_os);
    if (err_os != OS_ERR_NONE) {
        *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }
#endif

                                                                /* ------------------- PLAYBACK TASK ------------------ */
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
    OSTaskCreate(&USBD_Audio_OS_PlaybackTaskTCB,
                 "USBD Audio Playback Task",
                  USBD_Audio_OS_PlaybackTask,
                  DEF_NULL,
                  USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO,
                 &USBD_Audio_OS_PlaybackTaskStk[0u],
                  USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE / 10u,
                  USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE,
                  msg_qty,                              /* Playback req queue.                                  */
                  0u,
                  DEF_NULL,
                  OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err_os);
    if (err_os != OS_ERR_NONE) {
        *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }
#endif

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   USBD_Audio_OS_AS_IF_LockCreate()
*
* Description : Create an OS resource to use as an AudioStreaming interface lock.
*
* Argument(s) : as_if_nbr   AudioStreaming interface index.
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

void   USBD_Audio_OS_AS_IF_LockCreate (CPU_INT08U   as_if_nbr,
                                       USBD_ERR    *p_err)
{
    OS_MUTEX  *p_mutex;
    OS_ERR     err_os;


    p_mutex = &USBD_Audio_OS_AS_IF_MutexTbl[as_if_nbr];

    OSMutexCreate(p_mutex,
                 "USBD AS IF Mutex",
                 &err_os);
    if (err_os != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
    } else {
       *p_err = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                   USBD_Audio_OS_AS_IF_LockAcquire()
*
* Description : Wait for an AudioStreaming interface to become available and acquire its lock.
*
* Argument(s) : as_if_nbr   AudioStreaming interface index.
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

void   USBD_Audio_OS_AS_IF_LockAcquire (CPU_INT08U   as_if_nbr,
                                        CPU_INT16U   timeout_ms,
                                        USBD_ERR    *p_err)
{
    OS_MUTEX   *p_mutex;
    OS_ERR      err_os;
    OS_TICK     timeout_ticks;


    p_mutex = &USBD_Audio_OS_AS_IF_MutexTbl[as_if_nbr];
    timeout_ticks = ((((OS_TICK)timeout_ms * OSCfg_TickRate_Hz)  + 1000u - 1u) / 1000u);

    (void)OSMutexPend(p_mutex,
                      timeout_ticks,
                      OS_OPT_PEND_BLOCKING,
                      DEF_NULL,
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
*                                   USBD_Audio_OS_AS_IF_LockRelease()
*
* Description : Release an AudioStreaming interface lock.
*
* Argument(s) : as_if_nbr   AudioStreaming interface index.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void   USBD_Audio_OS_AS_IF_LockRelease (CPU_INT08U  as_if_nbr)
{
    OS_MUTEX  *p_mutex;
    OS_ERR     err_os;


    p_mutex = &USBD_Audio_OS_AS_IF_MutexTbl[as_if_nbr];

    OSMutexPost(p_mutex,
                OS_OPT_POST_NONE,
               &err_os);
    (void)err_os;
}


/*
*********************************************************************************************************
*                                  USBD_Audio_OS_RingBufQLockCreate()
*
* Description : Create an OS resource to use as a stream ring buffer queue lock.
*
* Argument(s) : as_if_nbr   AudioStreaming interface settings index.
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

void   USBD_Audio_OS_RingBufQLockCreate (CPU_INT08U   as_if_settings_ix,
                                         USBD_ERR    *p_err)
{
    OS_MUTEX  *p_mutex;
    OS_ERR     err_os;


    p_mutex = &USBD_Audio_OS_RingBufQMutexTbl[as_if_settings_ix];

    OSMutexCreate(p_mutex,
                 "USBD Ring Buf Q Mutex",
                 &err_os);
    if (err_os != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
    } else {
       *p_err = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                  USBD_Audio_OS_RingBufQLockAcquire()
*
* Description : Wait for a stream ring buffer queue to become available and acquire its lock.
*
* Argument(s) : as_if_nbr   AudioStreaming interface settings index.
*
*               timeout_ms  Lock wait timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS lock     successfully acquired.
*                               USBD_ERR_OS_TIMEOUT     OS lock NOT successfully acquired in the time
*                                                       specified by 'timeout_ms'.
*                               USBD_ERR_OS_ABORT       OS lock aborted.
*                               USBD_ERR_OS_FAIL        OS lock not acquired because another error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void   USBD_Audio_OS_RingBufQLockAcquire (CPU_INT08U   as_if_settings_ix,
                                          CPU_INT16U   timeout_ms,
                                          USBD_ERR    *p_err)
{
    OS_MUTEX   *p_mutex;
    OS_ERR      err_os;
    OS_TICK     timeout_ticks;


    p_mutex       = &USBD_Audio_OS_RingBufQMutexTbl[as_if_settings_ix];
    timeout_ticks = ((((OS_TICK)timeout_ms * OSCfg_TickRate_Hz)  + 1000u - 1u) / 1000u);

    (void)OSMutexPend(p_mutex,
                      timeout_ticks,
                      OS_OPT_PEND_BLOCKING,
                      DEF_NULL,
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
*                                  USBD_Audio_OS_RingBufQLockRelease()
*
* Description : Release a stream ring buffer queue lock.
*
* Argument(s) : as_if_nbr   AudioStreaming interface settings index.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void   USBD_Audio_OS_RingBufQLockRelease (CPU_INT08U  as_if_settings_ix)
{
    OS_MUTEX  *p_mutex;
    OS_ERR     err_os;


    p_mutex = &USBD_Audio_OS_RingBufQMutexTbl[as_if_settings_ix];

    OSMutexPost(p_mutex,
                OS_OPT_POST_NONE,
               &err_os);
    (void)err_os;
}


/*
*********************************************************************************************************
*                                     USBD_Audio_OS_RecordReqPost()
*
* Description : Post a request into the record task's queue.
*
* Argument(s) : p_msg       Pointer to message.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE       Placing buffer in queue successful.
*                           USBD_ERR_OS_FAIL    Failed to place item into the Record buffer queue.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
void  USBD_Audio_OS_RecordReqPost (void      *p_msg,
                                   USBD_ERR  *p_err)
{
    OS_ERR  err_os;


    OSTaskQPost (            &USBD_Audio_OS_RecordTaskTCB,
                              p_msg,
                 (OS_MSG_SIZE)0u,
                              OS_OPT_POST_FIFO,
                             &err_os);
    if (err_os != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_FAIL;
        return;
    }

   *p_err = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_OS_RecordReqPend()
*
* Description : Pend on a request from the record task's queue.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE           Getting buffer in queue successful.
*                           USBD_ERR_OS_TIMEOUT     Timeout has elapsed.
*                           USBD_ERR_OS_ABORT       Getting buffer in queue aborted.
*                           USBD_ERR_OS_FAIL        Failed to get item from the record buffer queue.
*
* Return(s)   : Pointer to record request, if NO error(s).
*
*               Null pointer,              otherwise
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
void  *USBD_Audio_OS_RecordReqPend (USBD_ERR  *p_err)
{
    void         *p_msg;
    OS_ERR        err_os;
    OS_MSG_SIZE   len;


    p_msg = OSTaskQPend((OS_TICK )0,
                                  OS_OPT_PEND_BLOCKING,
                                 &len,
                                  DEF_NULL,
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


        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_WOULD_BLOCK:
        case OS_ERR_Q_EMPTY:
        case OS_ERR_SCHED_LOCKED:
        default:
            *p_err = USBD_ERR_OS_FAIL;
             break;
    }

    return (p_msg);
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_OS_PlaybackReqPost()
*
* Description : Post a request to the playback's task queue.
*
* Argument(s) : p_msg       Pointer to message.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE       Placing buffer in queue successful.
*                           USBD_ERR_OS_FAIL    Failed to place item into the playback buffer queue.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
void  USBD_Audio_OS_PlaybackReqPost (void      *p_msg,
                                     USBD_ERR  *p_err)
{
    OS_ERR  err_os;


    OSTaskQPost(            &USBD_Audio_OS_PlaybackTaskTCB,
                             p_msg,
                (OS_MSG_SIZE)0u,
                             OS_OPT_POST_FIFO,
                            &err_os);
    if (err_os != OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_FAIL;
        return;
    }

   *p_err = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_OS_PlaybackReqPend()
*
* Description : Pend on a request from the playback's task queue.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE           Getting buffer in queue successful.
*                           USBD_ERR_OS_TIMEOUT     Timeout has elapsed.
*                           USBD_ERR_OS_ABORT       Getting buffer in queue aborted.
*                           USBD_ERR_OS_FAIL        Failed to get item from the playback buffer queue.
*
* Return(s)   : Pointer to playback request, if NO error(s).
*
*               Null pointer,                otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
void  *USBD_Audio_OS_PlaybackReqPend (USBD_ERR  *p_err)
{
    void         *p_msg;
    OS_ERR        err_os;
    OS_MSG_SIZE   len;


    p_msg = OSTaskQPend((OS_TICK )0,
                                  OS_OPT_PEND_BLOCKING,
                                 &len,
                                  DEF_NULL,
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


        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_WOULD_BLOCK:
        case OS_ERR_Q_EMPTY:
        case OS_ERR_SCHED_LOCKED:
        default:
            *p_err = USBD_ERR_OS_FAIL;
             break;
    }

    return (p_msg);
}
#endif


/*
*********************************************************************************************************
*                                         USBD_Audio_OS_DlyMs()
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

void  USBD_Audio_OS_DlyMs (CPU_INT32U  ms)
{
    OS_ERR  err_os;


    OSTimeDlyHMSM(0u, 0u, 0u, ms,
                  OS_OPT_TIME_HMSM_NON_STRICT,
                 &err_os);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      USBD_Audio_OS_RecordTask()
*
* Description : OS-dependent shell task to process record data streams.
*
* Argument(s) : p_arg       Pointer to task initialization argument.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
static  void  USBD_Audio_OS_RecordTask (void  *p_arg)
{
    (void)p_arg;

    USBD_Audio_RecordTaskHandler();
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_OS_PlaybackTask()
*
* Description : OS-dependent shell task to process playback data streams.
*
* Argument(s) : p_arg       Pointer to task initialization argument.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
static  void  USBD_Audio_OS_PlaybackTask (void  *p_arg)
{
    (void)p_arg;

    USBD_Audio_PlaybackTaskHandler();
}
#endif
