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
*                                              Template
*
* Filename : usbd_audio_os.c
* Version  : V4.06.01
*********************************************************************************************************
*/

#include  "../../usbd_audio_internal.h"
#include  "../../usbd_audio_os.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


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
    (void)msg_qty;

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
    (void)as_if_nbr;

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
    (void)as_if_nbr;
    (void)timeout_ms;

   *p_err = USBD_ERR_NONE;
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
    (void)as_if_nbr;
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
    (void)as_if_settings_ix;

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
    (void)as_if_settings_ix;
    (void)timeout_ms;

   *p_err = USBD_ERR_NONE;
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
    (void)as_if_settings_ix;
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
    (void)p_msg;

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
   *p_err = USBD_ERR_NONE;

    return ((void *)0);
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
    (void)p_msg;

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
    *p_err = USBD_ERR_NONE;

     return ((void *)0);
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
    (void)ms;
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
