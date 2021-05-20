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
*                                          Micrium uC/OS-II
*
* Filename : usbd_audio_os.c
* Version  : V4.06.01
*********************************************************************************************************
*/

#include  <app_cfg.h>
#include  "../../usbd_audio_internal.h"
#include  "../../usbd_audio_os.h"
#include  <Source/ucos_ii.h>


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
static  OS_STK     USBD_Audio_OS_RecordTaskStk[USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE];
static  OS_EVENT  *USBD_Audio_OS_RecordMsgQPtr;                 /* Ptr to record msg Q.                                 */
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
static  OS_STK     USBD_Audio_OS_PlaybackTaskStk[USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE];
static  OS_EVENT  *USBD_Audio_OS_PlaybackMsgQPtr;               /* Ptr to playback msg Q.                               */
#endif

static  OS_EVENT  *USBD_Audio_OS_AS_IF_MutexTbl[USBD_AUDIO_MAX_NBR_AS_IF_EP];
static  OS_EVENT  *USBD_Audio_OS_RingBufQMutexTbl[USBD_AUDIO_MAX_NBR_AS_IF_SETTINGS];


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
    INT8U     os_err;
    void     *p_record_msg_q_storage;
    void     *p_playback_msg_q_storage;
    LIB_ERR   err_lib;


                                                                /* ---------------- CREATE REQUIRED QS ---------------- */
                                                                /* Alloc storage space for record msg Q.                */
#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
    p_record_msg_q_storage = Mem_HeapAlloc(             (msg_qty * sizeof(void *)),
                                                         sizeof(CPU_ALIGN),
                                           (CPU_SIZE_T *)0,
                                                        &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }
#endif
                                                                /* Alloc storage space for playback msg Q.              */
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
    p_playback_msg_q_storage = Mem_HeapAlloc(             (msg_qty * sizeof(void *)),
                                                           sizeof(CPU_ALIGN),
                                             (CPU_SIZE_T *)0,
                                                          &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }
#endif

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
    USBD_Audio_OS_RecordMsgQPtr = OSQCreate(p_record_msg_q_storage,
                                            msg_qty);
    if (USBD_Audio_OS_RecordMsgQPtr == (OS_EVENT *)0) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
        return;
    }
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
    USBD_Audio_OS_PlaybackMsgQPtr = OSQCreate(p_playback_msg_q_storage,
                                              msg_qty);
    if (USBD_Audio_OS_PlaybackMsgQPtr == (OS_EVENT *)0) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
        return;
    }
#endif

                                                                /* ------------------- RECORD TASK -------------------- */
#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
#if (OS_TASK_CREATE_EXT_EN == 1u)

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(USBD_Audio_OS_RecordTask,
                             DEF_NULL,
                            &USBD_Audio_OS_RecordTaskStk[USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE - 1u],
                             USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO,
                             USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO,
                            &USBD_Audio_OS_RecordTaskStk[0u],
                             USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE,
                             DEF_NULL,
                             OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
    os_err = OSTaskCreateExt(USBD_Audio_OS_RecordTask,
                             DEF_NULL,
                            &USBD_Audio_OS_RecordTaskStk[0u],
                             USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO,
                             USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO,
                            &USBD_Audio_OS_RecordTaskStk[USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE - 1u],
                             USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE,
                             DEF_NULL,
                             OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#endif

#else

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreate(USBD_Audio_OS_RecordTask,
                          DEF_NULL,
                         &USBD_Audio_OS_RecordTaskStk[USBD_AUDIO_CFG_OS_RECORD_TASK_STK_SIZE - 1u],
                          USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO);
#else
    os_err = OSTaskCreate(USBD_Audio_OS_RecordTask,
                          DEF_NULL,
                         &USBD_Audio_OS_RecordTaskStk[0u],
                          USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO);
#endif

#endif
    if (os_err !=  OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }

#if (OS_TASK_STAT_EN > 0)
    OSTaskNameSet(USBD_AUDIO_CFG_OS_RECORD_TASK_PRIO, (INT8U *)"USBD Audio Record Task", &os_err);
#endif
#endif

                                                                /* ------------------ PLAYBACK TASK ------------------- */
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
#if (OS_TASK_CREATE_EXT_EN == 1u)

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreateExt(USBD_Audio_OS_PlaybackTask,
                             DEF_NULL,
                            &USBD_Audio_OS_PlaybackTaskStk[USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE - 1u],
                             USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO,
                             USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO,
                            &USBD_Audio_OS_PlaybackTaskStk[0u],
                             USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE,
                             DEF_NULL,
                             OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK);
#else
    os_err = OSTaskCreateExt(USBD_Audio_OS_PlaybackTask,
                             DEF_NULL,
                            &USBD_Audio_OS_PlaybackTaskStk[0u],
                             USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO,
                             USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO,
                            &USBD_Audio_OS_PlaybackTaskStk[USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE - 1u],
                             USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE,
                             DEF_NULL,
                            (OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));
#endif

#else

#if (OS_STK_GROWTH == 1u)
    os_err = OSTaskCreate(USBD_Audio_OS_PlaybackTask,
                          DEF_NULL,
                         &USBD_Audio_OS_PlaybackTaskStk[USBD_AUDIO_CFG_OS_PLAYBACK_TASK_STK_SIZE - 1u],
                          USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO);
#else
    os_err = OSTaskCreate(USBD_Audio_OS_PlaybackTask,
                          DEF_NULL,
                         &USBD_Audio_OS_PlaybackTaskStk[0u],
                          USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO);
#endif

#endif
    if (os_err !=  OS_ERR_NONE) {
       *p_err = USBD_ERR_OS_INIT_FAIL;
        return;
    }

#if (OS_TASK_STAT_EN > 0)
    OSTaskNameSet(USBD_AUDIO_CFG_OS_PLAYBACK_TASK_PRIO, (INT8U *)"USBD Audio Playback Task", &os_err);
#endif
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
    OS_EVENT  *p_event;


    p_event = OSSemCreate(1u);
    if (p_event == (OS_EVENT *)0) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
        return;
    }

    USBD_Audio_OS_AS_IF_MutexTbl[as_if_nbr] = p_event;

   *p_err = USBD_ERR_NONE;
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
    OS_EVENT  *p_event;
    INT8U      err_os;
    INT32U     timeout_ticks;


    p_event       = USBD_Audio_OS_AS_IF_MutexTbl[as_if_nbr];
    timeout_ticks = ((((INT32U)timeout_ms * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    OSSemPend(p_event,
              timeout_ticks,
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
    OS_EVENT  *p_event;


    p_event = USBD_Audio_OS_AS_IF_MutexTbl[as_if_nbr];
    (void)OSSemPost(p_event);
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
    OS_EVENT  *p_event;


    p_event = OSSemCreate(1u);
    if (p_event == (OS_EVENT *)0) {
       *p_err = USBD_ERR_OS_SIGNAL_CREATE;
        return;
    }

    USBD_Audio_OS_RingBufQMutexTbl[as_if_settings_ix] = p_event;

   *p_err = USBD_ERR_NONE;
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
    OS_EVENT  *p_event;
    INT8U      err_os;
    INT32U     timeout_ticks;


    p_event       = USBD_Audio_OS_RingBufQMutexTbl[as_if_settings_ix];
    timeout_ticks = ((((INT32U)timeout_ms * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    OSSemPend(p_event,
              timeout_ticks,
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
    OS_EVENT  *p_event;


    p_event = USBD_Audio_OS_RingBufQMutexTbl[as_if_settings_ix];
    (void)OSSemPost(p_event);
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
    INT8U  os_err;


    os_err = OSQPost(USBD_Audio_OS_RecordMsgQPtr, p_msg);
    if (os_err != OS_ERR_NONE) {
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
    void   *p_msg;
    INT8U   os_err;


    p_msg = OSQPend(USBD_Audio_OS_RecordMsgQPtr,
                    0u,
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
        case OS_ERR_PEVENT_NULL:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_LOCKED:
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
    INT8U  os_err;


    os_err = OSQPost(USBD_Audio_OS_PlaybackMsgQPtr, p_msg);
    if (os_err != OS_ERR_NONE) {
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
    void   *p_msg;
    INT8U   os_err;


    p_msg = OSQPend(USBD_Audio_OS_PlaybackMsgQPtr,
                    0u,
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
        case OS_ERR_PEVENT_NULL:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_LOCKED:
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
    (void)OSTimeDlyHMSM(0u, 0u, 0u, ms);
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
