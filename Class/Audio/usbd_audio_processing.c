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
*                                        USB DEVICE AUDIO CLASS
*                                           PROCESSING LAYER
*
* Filename : usbd_audio_processing.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)       : (1) 'goto' statements were used in this software module. Their usage is restricted to
*                     cleanup purposes in exceptional program flow (e.g. error handling), in compliance
*                     with CERT MEM12-C and MISRA C:2012 rules 15.2, 15.3 and 15.4.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    USBD_AUDIO_PROCESSING_MODULE
#include  "usbd_audio_processing.h"
#include  "usbd_audio_internal.h"
#include  "usbd_audio_os.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  USBD_AUDIO_PLAYBACK_CORR_MIN_NBR_SAMPLES          4u   /* Min nbr of samples in buf to apply playback corr.    */
#define  USBD_AUDIO_OVERRUN_NBR_SAMPLES_FOR_AVERAGING      4u
#define  USBD_AUDIO_UNDERRUN_NBR_SAMPLES_FOR_AVERAGING     2u

#define  USBD_AUDIO_REQ_CTRL_SELECTOR_MASK            0xFF00u
#define  USBD_AUDIO_REQ_CH_NBR_MASK                   0x00FFu
#define  USBD_AUDIO_REQ_IN_CH_NBR_MASK                0xFF00u
#define  USBD_AUDIO_REQ_OUT_CH_NBR_MASK               0x00FFu


#define  USBD_AUDIO_LOCK_TIMEOUT_mS                     1000u


/*
*********************************************************************************************************
*                                      TERMINAL CONTROL SELECTOR
*
* Note(s):  (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 5.2.2.1.3 and appendix A.10.1 for more details about Terminal Controls.
*********************************************************************************************************
*/

#define  USBD_AUDIO_TERMINAL_CTRL_SEL_COPY_PROTECT      0x01u


/*
*********************************************************************************************************
*                                    FEATURE UNIT CONTROL SELECTOR
*
* Note(s):  (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 5.2.2.4.3 and appendix A.10.2 for more details about Feature Unit Controls.
*********************************************************************************************************
*/

#define  USBD_AUDIO_FU_CTRL_SEL_UNDEF                      0u
#define  USBD_AUDIO_FU_CTRL_SEL_MUTE                       1u
#define  USBD_AUDIO_FU_CTRL_SEL_VOL                        2u
#define  USBD_AUDIO_FU_CTRL_SEL_BASS                       3u
#define  USBD_AUDIO_FU_CTRL_SEL_MID                        4u
#define  USBD_AUDIO_FU_CTRL_SEL_TREBLE                     5u
#define  USBD_AUDIO_FU_CTRL_SEL_GRAPHIC_EQUALIZER          6u
#define  USBD_AUDIO_FU_CTRL_SEL_AUTO_GAIN                  7u
#define  USBD_AUDIO_FU_CTRL_SEL_DELAY                      8u
#define  USBD_AUDIO_FU_CTRL_SEL_BASS_BOOST                 9u
#define  USBD_AUDIO_FU_CTRL_SEL_LOUDNESS                  10u


/*
**********************************************************************************************************
*                                           TRACE MACROS
**********************************************************************************************************
*/

#define  USBD_DBG_AUDIO_PROC_MSG(msg)                       USBD_DBG_GENERIC    ((msg),              \
                                                                                  USBD_EP_ADDR_NONE, \
                                                                                  USBD_IF_NBR_NONE)

#define  USBD_DBG_AUDIO_PROC_ARG(msg, arg)                  USBD_DBG_GENERIC_ARG((msg),              \
                                                                                  USBD_EP_ADDR_NONE, \
                                                                                  USBD_IF_NBR_NONE,  \
                                                                                 (arg))

#define  USBD_DBG_AUDIO_PROC_ERR(msg, err)                  USBD_DBG_GENERIC_ERR((msg),              \
                                                                                  USBD_EP_ADDR_NONE, \
                                                                                  USBD_IF_NBR_NONE,  \
                                                                                 (err))


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

                                                                /* AudioStreaming IF tbl.                               */
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  USBD_AUDIO_AS_IF  USBD_Audio_AS_Tbl[USBD_AUDIO_MAX_NBR_AS_IF_EP];
static  CPU_INT08U        USBD_Audio_AS_NbrNext;
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
static  void                  USBD_Audio_RecordIsocCmpl                  (       CPU_INT08U                    dev_nbr,
                                                                                 CPU_INT08U                    ep_addr,
                                                                                 void                         *p_buf,
                                                                                 CPU_INT32U                    buf_len,
                                                                                 CPU_INT32U                    xfer_len,
                                                                                 void                         *p_arg,
                                                                                 USBD_ERR                      err);

static  void                  USBD_Audio_RecordPrime                     (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_RecordUsbBufSubmit              (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 CPU_INT16U                   *p_xfer_submitted_cnt);

static  CPU_INT16U            USBD_Audio_RecordDataRateAdj               (       USBD_AUDIO_AS_IF             *p_as_if);


#if (USBD_AUDIO_CFG_RECORD_CORR_EN == DEF_ENABLED)
static  void                  USBD_Audio_RecordCorrBuiltIn               (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 USBD_AUDIO_BUF_DESC          *p_buf_desc);
#endif
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN  == DEF_ENABLED)
static  void                  USBD_Audio_PlaybackIsocCmpl                (       CPU_INT08U                    dev_nbr,
                                                                                 CPU_INT08U                    ep_addr,
                                                                                 void                         *p_buf,
                                                                                 CPU_INT32U                    buf_len,
                                                                                 CPU_INT32U                    xfer_len,
                                                                                 void                         *p_arg,
                                                                                 USBD_ERR                      err);

static  void                  USBD_Audio_PlaybackPrime                   (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_PlaybackUsbBufSubmit            (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 CPU_INT16U                   *p_xfer_submitted_cnt);

static  void                  USBD_Audio_PlaybackBufSubmit               (       USBD_AUDIO_AS_IF             *p_as_if);

#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_PLAYBACK_CORR_EN     == DEF_ENABLED)
static  void                  USBD_Audio_PlaybackCorrExec                (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 USBD_AUDIO_BUF_DESC          *p_buf_desc,
                                                                                 USBD_ERR                     *p_err);
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED)
static  void                  USBD_Audio_PlaybackCorrBuiltIn             (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 USBD_AUDIO_BUF_DESC          *p_buf_desc,
                                                                                 USBD_ERR                     *p_err);
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
static  void                  USBD_Audio_PlaybackCorrSynchInit           (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 CPU_INT32U                    sampling_freq,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_PlaybackIsocSynchCmpl           (       CPU_INT08U                    dev_nbr,
                                                                                 CPU_INT08U                    ep_addr,
                                                                                 void                         *p_buf,
                                                                                 CPU_INT32U                    buf_len,
                                                                                 CPU_INT32U                    xfer_len,
                                                                                 void                         *p_arg,
                                                                                 USBD_ERR                      err);

static  void                  USBD_Audio_PlaybackCorrSynch               (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 CPU_INT16U                    frame_nbr,
                                                                                 USBD_ERR                     *p_err);

static  CPU_INT32U           *USBD_Audio_PlaybackSynchBufGet             (       USBD_AUDIO_AS_IF             *p_as_if);

static  void                  USBD_Audio_PlaybackSynchBufFree            (       USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 CPU_INT32U                   *p_buf);
#endif
#endif

static  void                  USBD_Audio_Terminal_CopyProtManage         (       CPU_INT08U                    terminal_id,
                                                                                 USBD_AUDIO_ENTITY_TYPE        recipient,
                                                                          const  void                         *p_recipient_info,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_FU_MuteManage                   (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_FU_VolManage                    (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);

#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
static  void                  USBD_Audio_FU_BassManage                   (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_FU_MidManage                    (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_FU_TrebleManage                 (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_FU_GraphicEqualizerManage       (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 CPU_INT16U                    req_len,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_FU_AutoGainManage               (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_FU_DlyManage                    (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_FU_BassBoostManage              (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);

static  void                  USBD_Audio_FU_LoudnessManage               (const  USBD_AUDIO_FU                *p_fu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);
#endif

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
static  void                  USBD_Audio_MU_CtrlManage                   (const  USBD_AUDIO_MU                *p_mu,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    log_in_ch_nbr,
                                                                                 CPU_INT08U                    log_out_ch_nbr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);
#endif

#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
static  void                  USBD_Audio_SU_CtrlManage                   (const  USBD_AUDIO_SU                *p_su,
                                                                                 CPU_INT08U                    unit_id,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void                  USBD_Audio_AS_SamplingFreqManage           (const  USBD_AUDIO_AS_IF             *p_as_if,
                                                                          const  USBD_AUDIO_AS_ALT_CFG        *p_as_cfg,
                                                                                 CPU_INT16U                    ep_addr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 CPU_INT16U                    req_len,
                                                                                       USBD_ERR               *p_err);

static  void                  USBD_Audio_AS_PitchManage                  (const  USBD_AUDIO_AS_IF             *p_as_if,
                                                                                 CPU_INT16U                    ep_addr,
                                                                                 CPU_INT08U                    b_req,
                                                                                 CPU_INT08U                   *p_buf,
                                                                                 USBD_ERR                     *p_err);
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void                  USBD_Audio_AS_IF_RingBufQInit              (       USBD_AUDIO_AS_IF             *p_as_if);

static  USBD_AUDIO_BUF_DESC  *USBD_Audio_AS_IF_RingBufQGet               (       USBD_AUDIO_AS_IF_RING_BUF_Q  *p_ring_buf_q,
                                                                                 CPU_INT16U                    ix);

static  CPU_INT16U            USBD_Audio_AS_IF_RingBufQProducerStartIxGet(       USBD_AUDIO_AS_IF_SETTINGS    *p_as_if_settings);

static  CPU_INT16U            USBD_Audio_AS_IF_RingBufQProducerEndIxGet  (       USBD_AUDIO_AS_IF_SETTINGS    *p_as_if_settings);

static  CPU_INT16U            USBD_Audio_AS_IF_RingBufQConsumerStartIxGet(       USBD_AUDIO_AS_IF_SETTINGS    *p_as_if_settings);

static  CPU_INT16U            USBD_Audio_AS_IF_RingBufQConsumerEndIxGet  (       USBD_AUDIO_AS_IF_SETTINGS    *p_as_if_settings);

static  void                  USBD_Audio_AS_IF_RingBufQIxUpdate          (       USBD_AUDIO_AS_IF_SETTINGS    *p_as_if_settings,
                                                                                 CPU_INT16U                   *p_ix);

#if ( (USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) &&   \
     ((USBD_AUDIO_CFG_PLAYBACK_CORR_EN     == DEF_ENABLED) ||   \
      (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED))) || \
    ( (USBD_AUDIO_CFG_RECORD_EN            == DEF_ENABLED) &&   \
      (USBD_AUDIO_CFG_RECORD_CORR_EN       == DEF_ENABLED) )
static  CPU_INT08S            USBD_Audio_BufDiffGet                      (       USBD_AUDIO_AS_IF_SETTINGS    *p_as_if_settings);
#endif

static  USBD_AUDIO_AS_IF     *USBD_Audio_AS_IF_Get                       (       USBD_AUDIO_AS_HANDLE          as_handle);
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     USBD_Audio_ProcessingInit()
*
* Description : Initialize the Audio Processing layer.
*
* Argument(s) : msg_qty     Maximum quantity of messages for playback and record tasks' queues.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE   Audio class successfully initialized.
*                           USBD_ERR_ALLOC  Buffer descriptors pool could not be allocated.
*
*                           -RETURNED BY USBD_Audio_OS_Init()-
*                           See USBD_Audio_OS_Init() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_Audio_ProcessingInit (CPU_INT16U   msg_qty,
                                 USBD_ERR    *p_err)
{
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
    Mem_Clr((void *)&USBD_Audio_AS_Tbl[0u],                     /* Init AudioStreaming IF tbl.                          */
                    (USBD_AUDIO_MAX_NBR_AS_IF_EP * sizeof(USBD_AUDIO_AS_IF)));

    USBD_Audio_AS_NbrNext = 0u;
#endif
    USBD_Audio_OS_Init(msg_qty, p_err);                         /* 2 tasks are used for audio streams comm.             */
}


/*
*********************************************************************************************************
*                                       USBD_Audio_AS_IF_Alloc()
*
* Description : Allocate an AudioStreaming interface.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE   AudioStreaming interface allocated.
*                           USBD_ERR_ALLOC  No AudioStreaming interface available.
*
* Return(s)   : Pointer to AudioStreaming interface, if NO error(s).
*
*               Null pointer,                        otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
USBD_AUDIO_AS_IF  *USBD_Audio_AS_IF_Alloc (USBD_ERR  *p_err)
{
    USBD_AUDIO_AS_IF  *p_as_if;
    CPU_INT08U         as_if_nbr;
    CPU_SR_ALLOC();


                                                                /* ----------- GET AUDIOSTREAMING IF STRUCT ----------- */
    CPU_CRITICAL_ENTER();
    as_if_nbr = USBD_Audio_AS_NbrNext;

    if (as_if_nbr >= USBD_AUDIO_MAX_NBR_AS_IF_EP) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_AS_IF_ALLOC;
        return (DEF_NULL);
    }

    USBD_Audio_AS_NbrNext++;
    CPU_CRITICAL_EXIT();

    p_as_if = &USBD_Audio_AS_Tbl[as_if_nbr];
    USBD_AUDIO_AS_IF_HANDLE_CREATE(p_as_if, as_if_nbr);         /* Store AudioStreaming IF ix.                          */

    return (p_as_if);
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_AC_TerminalCtrl()
*
* Description : Process Terminal's Control.
*
* Argument(s) : terminal_id         Input or Output Terminal ID.
*
*               recipient           Recipient Terminal Type.
*
*               p_recipient_info    Pointer to the Terminal Recipient info.
*
*               b_req               Received request type (SET_XXX or GET_XXX).
*
*               w_val               Received request control value.
*
*               p_buf               Pointer to the buffer space with the request attribute to SET or GET.
*
*               req_len             Request attribute length (in bytes).
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                     Terminal Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_CTRL   Invalid control.
*
*                           -RETURNED BY USBD_Audio_Terminal_CopyProtManage()-
*                           See USBD_Audio_Terminal_CopyProtManage() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_Audio_AC_TerminalCtrl (       CPU_INT08U               terminal_id,
                                         USBD_AUDIO_ENTITY_TYPE   recipient,
                                  const  void                    *p_recipient_info,
                                         CPU_INT08U               b_req,
                                         CPU_INT16U               w_val,
                                         CPU_INT08U              *p_buf,
                                         CPU_INT16U               req_len,
                                         USBD_ERR                *p_err)
{
    CPU_INT08U  ctrl_selector;


    (void)req_len;

    ctrl_selector = (w_val & USBD_AUDIO_REQ_CTRL_SELECTOR_MASK) >> 8u;

    switch (ctrl_selector) {
        case USBD_AUDIO_TERMINAL_CTRL_SEL_COPY_PROTECT:

             USBD_Audio_Terminal_CopyProtManage(terminal_id,
                                                recipient,
                                                p_recipient_info,
                                                b_req,
                                                p_buf,
                                                p_err);
             break;


        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_CTRL;           /* Ctrl Selector not supported. Ctrl EP will be stalled.*/
             break;
    }
}


/*
*********************************************************************************************************
*                                       USBD_Audio_AC_UnitCtrl()
*
* Description : Process Unit's Control.
*
* Argument(s) : unit_id             Feature Unit ID, Mixer Unit ID or Selector Unit ID.
*
*               recipient           Recipient Unit Type.
*
*               p_recipient_info    Pointer to the Unit Recipient info.
*
*               b_req               Received request type (SET_XXX or GET_XXX).
*
*               w_val               Received request control value.
*
*               p_buf               Pointer to the buffer space with the request attribute to SET or GET.
*
*               req_len             Request control attribute length (in bytes).
*
*               p_err               Pointer to variable that will receive the return error code from this function:
*
*                                   USBD_ERR_NONE                           Unit Control successfully processed.
*                                   USBD_ERR_AUDIO_REQ_INVALID_CTRL         Invalid control value passed to this
*                                                                           function.
*                                   USBD_ERR_AUDIO_REQ_INVALID_RECIPIENT    Recipient is not a Feature Unit, Mixer
*                                                                           Unit or Selector Unit ID.
*                                   USBD_ERR_AUDIO_REQ_FAIL                 Logical channel out of range or
*                                                                           unsupported 2nd form.
*
*                                   -RETURNED BY USBD_Audio_FU_TrebleManage()-
*                                   See USBD_Audio_FU_TrebleManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_FU_VolManage()-
*                                   See USBD_Audio_FU_VolManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_FU_AutoGainManage()-
*                                   See USBD_Audio_FU_AutoGainManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_FU_DlyManage()-
*                                   See USBD_Audio_FU_DlyManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_FU_GraphicEqualizerManage()-
*                                   See USBD_Audio_FU_GraphicEqualizerManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_FU_MuteManage()-
*                                   See USBD_Audio_FU_MuteManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_FU_LoudnessManage()-
*                                   See USBD_Audio_FU_LoudnessManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_FU_MidManage()-
*                                   See USBD_Audio_FU_MidManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_FU_BassBoostManage()-
*                                   See USBD_Audio_FU_BassBoostManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_FU_BassManage()-
*                                   See USBD_Audio_FU_BassManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_SU_CtrlManage()-
*                                   See USBD_Audio_SU_CtrlManage() for additional return error codes.
*
*                                   -RETURNED BY USBD_Audio_MU_CtrlManage()-
*                                   See USBD_Audio_MU_CtrlManage() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) When the channel number is set to 0xFF, one single Get or Set request is sent by the
*                   host to get or set an attribute for all available Controls in the Feature Unit.
*                   In that case, the Control parameter block is formatted according to the second
*                   form.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.1 and 5.2.2.4.2 for more details about the first form and second form
*                   of the Get/Set requests for the Feature Unit.
*
*               (2) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.2.1 and 5.2.2.2.2 for more details about the second form and third form
*                   of the Get/Set requests for the Mixer Unit.
*
*               (3) The Mixer Unit Descriptor reports which Controls are programmable in the bmControls
*                   bitmap field. This bitmap must be interpreted as a two-dimensional bit array that has a
*                   row for each logical input channel and a column for each logical output channel.
*                   If a bit at position [u, v] is set, this means that the Mixer Unit contains a
*                   programmable mixing Control that connects input channel u to output channel v.
*                   If bit [u, v] is clear, this indicates that the connection between input channel
*                   u and output channel v is non-programmable. The valid range for u is from 1 to n
*                   (n = number of logical input channels). The valid range for v is from 1 to m
*                   (m = number of logical output channels). For instance, let's consider a Mixer Unit
*                   with 2 stereo Input pins and 1 stereo Output pin:
*
*        u                    +---------+
*        1. Input 1 Left  --->|         |
*        2. Input 1 Right --->|         |     v
*                             |   MU    |---> 1. Output Left
*        3. Input 2 Left  --->|         |---> 2. Output Right
*        4. Input 2 Right --->|         |
*                             +---------+
*
*                   The 2-dimensional array will look like this one:
*
*          u \ v | 1 | 2 |                                  bit# reversed 0 1 2 3 4 5 6 7
*   Row 1: 1     |'1'|'0'|                                           bit# 7 6 5 4 3 2 1 0
*   Row 2: 2     |'0'|'1'| => bmControls |Row 1|Row 2|Row 3|Row 4| =     |1 0|0 1|1 0|0 1|
*   Row 3: 3     |'1'|'0'|
*   Row 4: 4     |'0'|'1'|
*
*                   In this example, bmControls has a size of 1 byte. If the host sends a SET_CUR request
*                   to [u;v] = [3;2], it wants to mix the Input 2 Left into the Output Right. The
*                   2-dimensional array at this position indicates that the Control is non-programmable.
*                   The request should be stalled. The computation does the following steps:
*
*                       (a) Compute bit number within bmControls field using the formula [(u-1) * m] + (v-1):
*                           Here, [(3-1) * 2] + (2-1) = 5th bit.
*                       (b) Compute the byte index in which the bit number is. Here, byte index = 0
*                       (c) Compute the bit shift to test the bit number position within the byte.
*********************************************************************************************************
*/

void  USBD_Audio_AC_UnitCtrl (       CPU_INT08U               unit_id,
                                     USBD_AUDIO_ENTITY_TYPE   recipient,
                              const  void                    *p_recipient_info,
                                     CPU_INT08U               b_req,
                                     CPU_INT16U               w_val,
                                     CPU_INT08U              *p_buf,
                                     CPU_INT16U               req_len,
                                     USBD_ERR                *p_err)
{
    USBD_AUDIO_FU       *p_fu;
    CPU_INT08U           ctrl_selector;
    CPU_INT08U           log_ch_nbr;
#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
    USBD_AUDIO_MU       *p_mu;
    CPU_INT08U           log_in_ch_nbr;
    CPU_INT08U           log_out_ch_nbr;
    CPU_INT32U           total_nbr_log_in_ch;
    CPU_INT08U           byte_ix;
    CPU_INT08U           bit_shift;
    CPU_INT32U           bit_nbr;
    CPU_BOOLEAN          programmable_ctrl;
    CPU_INT16U           i;
#endif
#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
    USBD_AUDIO_SU       *p_su;
#endif


#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_DISABLED)
    (void)req_len;
#endif

    switch (recipient) {
        case USBD_AUDIO_ENTITY_FU:                              /* ------------------- FEATURE UNIT ------------------- */
             p_fu = (USBD_AUDIO_FU *)p_recipient_info;

             if (unit_id != p_fu->ID) {                         /* Check if ID is known.                                */
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             ctrl_selector = (w_val & USBD_AUDIO_REQ_CTRL_SELECTOR_MASK) >> 8u;
             log_ch_nbr    = (w_val & USBD_AUDIO_REQ_CH_NBR_MASK);

             if ((log_ch_nbr == 0xFFu) ||                       /* 2nd form of Get/Set req not supported (see Note #1). */
                 (log_ch_nbr >  p_fu->FU_CfgPtr->LogChNbr)) {   /* log ch nbr out of range.                             */
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }
                                                                /* Check if ctrl selector is supported.                 */
             if (DEF_BIT_IS_CLR(p_fu->FU_CfgPtr->LogChCtrlPtr[log_ch_nbr], DEF_BIT(ctrl_selector - 1u)) == DEF_YES) {
                *p_err = USBD_ERR_AUDIO_REQ_INVALID_CTRL;
                 break;
             }

             switch (ctrl_selector) {
                 case USBD_AUDIO_FU_CTRL_SEL_MUTE:
                      USBD_Audio_FU_MuteManage(p_fu,
                                               unit_id,
                                               log_ch_nbr,
                                               b_req,
                                               p_buf,
                                               p_err);
                      break;


                 case USBD_AUDIO_FU_CTRL_SEL_VOL:
                      USBD_Audio_FU_VolManage(p_fu,
                                              unit_id,
                                              log_ch_nbr,
                                              b_req,
                                              p_buf,
                                              p_err);
                      break;


#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
                 case USBD_AUDIO_FU_CTRL_SEL_BASS:
                      USBD_Audio_FU_BassManage(p_fu,
                                               unit_id,
                                               log_ch_nbr,
                                               b_req,
                                               p_buf,
                                               p_err);
                      break;


                 case USBD_AUDIO_FU_CTRL_SEL_MID:
                      USBD_Audio_FU_MidManage(p_fu,
                                              unit_id,
                                              log_ch_nbr,
                                              b_req,
                                              p_buf,
                                              p_err);
                      break;


                 case USBD_AUDIO_FU_CTRL_SEL_TREBLE:
                      USBD_Audio_FU_TrebleManage(p_fu,
                                                 unit_id,
                                                 log_ch_nbr,
                                                 b_req,
                                                 p_buf,
                                                 p_err);
                      break;


                 case USBD_AUDIO_FU_CTRL_SEL_GRAPHIC_EQUALIZER:
                      USBD_Audio_FU_GraphicEqualizerManage(p_fu,
                                                           unit_id,
                                                           log_ch_nbr,
                                                           b_req,
                                                           p_buf,
                                                           req_len,
                                                           p_err);
                      break;


                 case USBD_AUDIO_FU_CTRL_SEL_AUTO_GAIN:
                      USBD_Audio_FU_AutoGainManage(p_fu,
                                                   unit_id,
                                                   log_ch_nbr,
                                                   b_req,
                                                   p_buf,
                                                   p_err);
                      break;


                 case USBD_AUDIO_FU_CTRL_SEL_DELAY:
                      USBD_Audio_FU_DlyManage(p_fu,
                                              unit_id,
                                              log_ch_nbr,
                                              b_req,
                                              p_buf,
                                              p_err);
                      break;


                 case USBD_AUDIO_FU_CTRL_SEL_BASS_BOOST:
                      USBD_Audio_FU_BassBoostManage(p_fu,
                                                    unit_id,
                                                    log_ch_nbr,
                                                    b_req,
                                                    p_buf,
                                                    p_err);
                      break;


                 case USBD_AUDIO_FU_CTRL_SEL_LOUDNESS:
                      USBD_Audio_FU_LoudnessManage(p_fu,
                                                   unit_id,
                                                   log_ch_nbr,
                                                   b_req,
                                                   p_buf,
                                                   p_err);
                      break;
#endif

                 default:                                       /* Ctrl Selector not supported. Ctrl EP will be stalled.*/
                     *p_err = USBD_ERR_AUDIO_REQ_INVALID_CTRL;
                      break;
             }
             break;


#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
        case USBD_AUDIO_ENTITY_MU:                              /* -------------------- MIXER UNIT -------------------- */
             p_mu = (USBD_AUDIO_MU *)p_recipient_info;

             if (unit_id != p_mu->ID) {                         /* Check if ID is known.                                */
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             log_in_ch_nbr  = (w_val & USBD_AUDIO_REQ_IN_CH_NBR_MASK) >> 8u;
             log_out_ch_nbr = (w_val & USBD_AUDIO_REQ_OUT_CH_NBR_MASK);
                                                                 /* 2nd and 3rd forms of Get/Set req not supported...   */
                                                                 /* ...(see Note #2).                                   */
             if (((log_in_ch_nbr == 0xFFu) || (log_in_ch_nbr == 0x00u)) &&
                 (log_in_ch_nbr == log_out_ch_nbr)) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             total_nbr_log_in_ch = (p_mu->MU_CfgPtr->LogOutChNbr * p_mu->MU_CfgPtr->NbrInPins);
             if (log_in_ch_nbr > total_nbr_log_in_ch) {         /* Log IN ch nbr exceeds total nbr of log IN ch.        */
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }
                                                                /* Log OUT ch nbr exceeds total nbr of log OUT ch.      */
             if (log_out_ch_nbr > p_mu->MU_CfgPtr->LogOutChNbr) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             if (b_req == USBD_AUDIO_REQ_SET_CUR) {
                                                                /* Verify if Mixer Ctrls are programmable.              */
                 programmable_ctrl = DEF_FALSE;
                 for (i = 0; i < p_mu->ControlsSize; i++) {
                     if (p_mu->ControlsTblPtr[i] != USBD_AUDIO_MU_CTRL_NONE) {
                         programmable_ctrl = DEF_TRUE;
                         break;
                     }
                 }
                 if (programmable_ctrl == DEF_FALSE) {
                    *p_err = USBD_ERR_AUDIO_REQ;                /* No programmable ctrl.                                */
                     break;
                 }
                                                                /* Verify if specified Ctrl is programmable...          */
                                                                /* ...(see Note #3).                                    */
                 bit_nbr   = ((log_in_ch_nbr - 1u) * p_mu->MU_CfgPtr->LogOutChNbr) + (log_out_ch_nbr - 1u);
                 byte_ix   =   bit_nbr / DEF_OCTET_NBR_BITS;
                 bit_shift =   bit_nbr % DEF_OCTET_NBR_BITS;

                 if (DEF_BIT_IS_CLR(p_mu->ControlsTblPtr[byte_ix], ((DEF_BIT_07) >> bit_shift)) == DEF_YES) {
                    *p_err = USBD_ERR_AUDIO_REQ;
                     break;
                 }
             }

             USBD_Audio_MU_CtrlManage(p_mu,
                                      unit_id,
                                      log_in_ch_nbr,
                                      log_out_ch_nbr,
                                      b_req,
                                      p_buf,
                                      p_err);
             break;
#endif

#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
        case USBD_AUDIO_ENTITY_SU:                              /* ------------------ SELECTOR UNIT ------------------- */
             if (w_val == 0u) {
                 p_su = (USBD_AUDIO_SU *)p_recipient_info;

                 if (unit_id != p_su->ID) {                     /* Check if ID is known.                                */
                    *p_err = USBD_ERR_AUDIO_REQ;
                     break;
                 }

                 USBD_Audio_SU_CtrlManage(p_su,
                                          unit_id,
                                          b_req,
                                          p_buf,
                                          p_err);
             } else {
                *p_err = USBD_ERR_AUDIO_REQ_INVALID_CTRL;
             }
             break;
#endif

        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_RECIPIENT;      /* Unit not supported. Ctrl EP will be stalled.         */
             break;
    }
}


/*
*********************************************************************************************************
*                                   USBD_Audio_AS_EP_CtrlProcess()
*
* Description : Process AudioStreaming endpoint's Control.
*
* Argument(s) : p_as_if     Pointer to AudioStreaming Interface.
*
*               p_as_cfg    Pointer to the AudioStreaming configuration.
*
*               ep_addr     Endpoint address.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               w_val       Received request control value.
*
*               p_buf       Pointer to the buffer space with the request attribute.
*
*               req_len     Request attribute length (in bytes).
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Endpoint Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_CTRL     Invalid or unsupported control value passed
*                                                               to this function.
*
*                           -RETURNED BY USBD_Audio_AS_IF_Start()-
*                           See USBD_Audio_AS_IF_Start() for additional return error codes.
*
*                           -RETURNED BY USBD_Audio_AS_SamplingFreqManage()-
*                           See USBD_Audio_AS_SamplingFreqManage() for additional return error codes.
*
*                           -RETURNED BY USBD_Audio_ProcPlaybackSynchCorrInit()-
*                           See USBD_Audio_ProcPlaybackSynchCorrInit() for additional return error codes.
*
*                           -RETURNED BY USBD_Audio_AS_PitchManage()-
*                           See USBD_Audio_AS_PitchManage() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) According to Audio 1.0 specification (section 5.2.3.2.3.1), the sampling frequency
*                   value holds on 3 bytes. Since the sampling frequency is referenced as a 32-bit word,
*                   clearing the most significant byte ensures that the correct value is read from
*                   memory.
*
*               (2) A general purpose OS (Windows, Mac OS, Linux) opens a record stream by sending a
*                   SET_INTERFACE request to select the operational interface of an AudioStreaming
*                   interface. The SET_INTERFACE request is always followed by a SET_CUR request to
*                   configure the sampling frequency. Getting audio record data must be started ONLY when
*                   the sampling frequency has been configured.
*
*               (3) For certain sampling frequencies (i.e. 11.025, 22.050 and 44.1 KHz), a data rate
*                   adjustment is necessary because the number of audio samples per ms is not an
*                   integer number. Partial audio samples are not possible. For those sampling
*                   frequencies, the table below gives the required adjustment:
*
*               Samples per frm/ms | Typical Packet Size | Adjustment
*               11.025             | 11 samples          | 12 samples every 40 packets (i.e. ms)
*               22.050             | 22 samples          | 23 samples every 20 packets (i.e. ms)
*               44.1               | 44 samples          | 45 samples every 10 packets (i.e. ms)
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
void  USBD_Audio_AS_EP_CtrlProcess (       USBD_AUDIO_AS_IF       *p_as_if,
                                    const  USBD_AUDIO_AS_ALT_CFG  *p_as_cfg,
                                           CPU_INT16U              ep_addr,
                                           CPU_INT08U              b_req,
                                           CPU_INT16U              w_val,
                                           CPU_INT08U             *p_buf,
                                           CPU_INT16U              req_len,
                                           USBD_ERR               *p_err)
{
    CPU_INT08U                  ctrl_selector;
    CPU_INT32U                  sampling_freq;
#if ((USBD_AUDIO_CFG_RECORD_EN             == DEF_ENABLED) || \
     ((USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) && \
      (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)))
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
#endif


#if ((USBD_AUDIO_CFG_RECORD_EN             == DEF_ENABLED) || \
     ((USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) && \
      (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)))
    p_as_if_settings =  p_as_if->AS_IF_SettingsPtr;
#endif
    ctrl_selector    = (w_val & USBD_AUDIO_REQ_CTRL_SELECTOR_MASK) >> 8u;

    switch (ctrl_selector) {
        case USBD_AUDIO_AS_EP_CTRL_SAMPLING_FREQ:
                                                                /* Verify if Sampling Freq Control supported by this EP.*/
             if (DEF_BIT_IS_CLR(p_as_cfg->EP_Attrib, USBD_AUDIO_AS_EP_CTRL_SAMPLING_FREQ) == DEF_YES) {
                *p_err = USBD_ERR_AUDIO_REQ_INVALID_CTRL;
                 break;
             }

             USBD_Audio_AS_SamplingFreqManage(p_as_if,
                                              p_as_cfg,
                                              ep_addr,
                                              b_req,
                                              p_buf,
                                              req_len,
                                              p_err);
             if (*p_err != USBD_ERR_NONE) {
                 break;
             }

             sampling_freq  = MEM_VAL_GET_INT32U_LITTLE(p_buf);
             sampling_freq &= 0x00FFFFFFu;                      /* Ensure correct sampling freq is read (see Note #1).  */

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
             if (p_as_if_settings->StreamDir == USBD_AUDIO_STREAM_IN) {
                                                                /* Compute buf len according to sample rate, bit...     */
                                                                /* ...resolution and nbr of channels.                   */
                 p_as_if_settings->RecordBufLen = (sampling_freq / DEF_TIME_NBR_mS_PER_SEC) * p_as_cfg->SubframeSize * p_as_cfg->NbrCh;

                                                                /* See Note #2.                                         */
                 if ((USBD_EP_IS_IN(ep_addr) == DEF_YES) &&
                     (b_req                  == USBD_AUDIO_REQ_SET_CUR)) {

                     if ((sampling_freq % 1000u) != 0u) {       /* Keep some info if data rate adjustment is needed...  */
                                                                /* ...See Note #3.                                      */
                         CPU_INT16U  temp;


                         temp                                   = sampling_freq % 1000u;
                         p_as_if_settings->RecordRateAdjMs      = 1000u / temp;
                         p_as_if_settings->RecordRateAdjXferCtr = 0u;
                     }

                     USBD_Audio_AS_IF_Start(p_as_if, p_err);
                 }
             }
#endif
#if (USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
             if ((p_as_if_settings->StreamDir             == USBD_AUDIO_STREAM_OUT) &&
                 (p_as_if->AS_IF_AltCurPtr->SynchIsocAddr != USBD_EP_ADDR_NONE)) {

                 USBD_Audio_PlaybackCorrSynchInit(p_as_if, sampling_freq, p_err);
             }
#endif
             break;


        case USBD_AUDIO_AS_EP_CTRL_PITCH:
                                                                /* Verify if Pitch Control supported by this EP.        */
             if (DEF_BIT_IS_CLR(p_as_cfg->EP_Attrib, USBD_AUDIO_AS_EP_CTRL_PITCH) == DEF_YES) {
                *p_err = USBD_ERR_AUDIO_REQ_INVALID_CTRL;
                 break;
             }

             USBD_Audio_AS_PitchManage(p_as_if,
                                       ep_addr,
                                       b_req,
                                       p_buf,
                                       p_err);
             break;


        case USBD_AUDIO_AS_EP_CTRL_MAX_PKT_ONLY:
        default:                                                /* Ctrl Selector not supported. Ctrl EP will be stalled.*/
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_CTRL;
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_RecordTaskHandler()
*
* Description : Process events sent by the codec driver when the audio transfers have finished.
*
* Argument(s) : None.
*
* Return(s)   : None.
*
* Note(s)     : (1) USBD_Audio_RecordPrime() is called in two distinct situations:
*
*                   (a) As soon as stream priming is done, that is a certain number of ready buffers
*                       have accumulated, the first USB transfer is started.
*
*                   (b) When there is no more ongoing isochronous transfers in the USB driver during
*                       an ongoing stream communication, that is the stream loop is broken. In that
*                       case, the Record task restarts the stream with a new USB transfer.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
void  USBD_Audio_RecordTaskHandler (void)
{
    USBD_AUDIO_AS_IF           *p_as_if;
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    USBD_AUDIO_BUF_DESC        *p_buf_desc;
    USBD_AUDIO_AS_HANDLE        as_if_handle;
    CPU_INT16U                  ix;
    CPU_INT16U                  isoc_tx_ongoing_cnt;
    CPU_BOOLEAN                 pre_buf_compl;
    USBD_ERR                    err_usbd;
    CPU_SR_ALLOC();


    while (DEF_TRUE) {
                                                                /* ------ WAIT FOR AUDIO XFER COMPLETION SIGNAL ------- */
        as_if_handle = (USBD_AUDIO_AS_HANDLE)(CPU_ADDR)USBD_Audio_OS_RecordReqPend(&err_usbd);

        p_as_if = USBD_Audio_AS_IF_Get(as_if_handle);
        if (p_as_if == DEF_NULL) {
            USBD_DBG_AUDIO_PROC_MSG("RecordTaskHandler(): Invalid AS IF handle\r\n");
            continue;
        }

        p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrReqPendRecordTask);

        USBD_Audio_OS_AS_IF_LockAcquire(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle),
                                        USBD_AUDIO_LOCK_TIMEOUT_mS,
                                       &err_usbd);
        if (err_usbd != USBD_ERR_NONE) {
            USBD_DBG_AUDIO_PROC_ERR("RecordTaskHandler(): lock acquire failed w/ err = %d\r\n", err_usbd);
            continue;
        }

        if (USBD_AUDIO_AS_IF_HANDLE_VALIDATE(p_as_if, as_if_handle) != DEF_OK) {
            USBD_DBG_AUDIO_PROC_MSG("RecordTaskHandler(): Invalid AS IF handle\r\n");
            goto end_lock_rel;
        }
                                                                /* -------------- GET RDY BUF FROM CODEC -------------- */
                                                                /* Get a buf desc from the Ring Buf Q.                  */
        ix = USBD_Audio_AS_IF_RingBufQProducerEndIxGet(p_as_if_settings);
        if (ix == USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {
            USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrErr);
            USBD_DBG_AUDIO_PROC_MSG("RecordTaskHandler(): no buffer descriptor\r\n");
            goto end_lock_rel;
        }
        p_buf_desc = USBD_Audio_AS_IF_RingBufQGet(&p_as_if_settings->StreamRingBufQ, ix);
        if (p_buf_desc == DEF_NULL) {
            USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrErr);
            USBD_DBG_AUDIO_PROC_MSG("RecordTaskHandler(): buf desc retrieved from Ring Buf Q should NOT be a NULL ptr\r\n");
            goto end_lock_rel;
        }

        p_as_if_settings->AS_API_Ptr->StreamRecordRx(p_as_if_settings->DrvInfoPtr,
                                                     p_as_if_settings->TerminalID,
                                                     p_buf_desc->BufPtr,
                                                    &p_buf_desc->BufLen,
                                                    &err_usbd);
        if (err_usbd != USBD_ERR_NONE) {
            USBD_DBG_AUDIO_PROC_ERR("RecordTaskHandler(): cannot get ready buf w/ err = %d\r\n", err_usbd);
            goto end_lock_rel;
        }

                                                                /* Update ix only after writing to buf desc.            */
        USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if_settings, &p_as_if_settings->StreamRingBufQ.ProducerEndIx);

                                                                /* --------------- PRIMING DONE OR NOT ---------------- */
        CPU_CRITICAL_ENTER();
        isoc_tx_ongoing_cnt = p_as_if_settings->RecordIsocTxOngoingCnt;
        CPU_CRITICAL_EXIT();
                                                                /* If priming done and isoc xfers in progress,...       */
                                                                /* ...wait for nxt audio xfer completion.               */
        if ((p_as_if_settings->StreamPrimingDone == DEF_YES) &&
            (isoc_tx_ongoing_cnt                  > 0u )) {
            goto end_lock_rel;
        }

                                                                /* If priming not done, wait for nxt audio xfer cmpl.   */
        pre_buf_compl = p_as_if_settings->StreamRingBufQ.ProducerEndIx >= p_as_if_settings->StreamPreBufMax ? DEF_YES : DEF_NO;
        if ((p_as_if_settings->StreamPrimingDone == DEF_NO) &&
            (pre_buf_compl                       == DEF_NO)) {
            goto end_lock_rel;
        }

        USBD_Audio_RecordPrime(p_as_if, &err_usbd);             /* Start 1st isoc IN xfer (see Note #1).                */
        if (err_usbd != USBD_ERR_NONE) {
            USBD_DBG_AUDIO_PROC_ERR("RecordTaskHandler(): starting first isoc IN transfer failed w/ err = %d\r\n", err_usbd);
            goto end_lock_rel;
        }
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxSubmitRecordTask);

        if (p_as_if_settings->StreamPrimingDone == DEF_NO) {
#if (USBD_AUDIO_CFG_RECORD_CORR_EN == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_RECORD_EN      == DEF_ENABLED)
                                                                /* Get initial frame nbr for corr period computation.   */
            p_as_if_settings->CorrFrameNbr = USBD_DevFrameNbrGet(p_as_if->DevNbr, &err_usbd);
            p_as_if_settings->CorrFrameNbr = USBD_FRAME_NBR_GET(p_as_if_settings->CorrFrameNbr);
#endif

            p_as_if_settings->StreamPrimingDone = DEF_YES;
        }

end_lock_rel:
        USBD_Audio_OS_AS_IF_LockRelease(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle));
    }
}
#endif


/*
*********************************************************************************************************
*                                   USBD_Audio_PlaybackTaskHandler()
*
* Description : Process events sent by the codec driver when the audio transfers have finished.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
void  USBD_Audio_PlaybackTaskHandler (void)
{
    USBD_AUDIO_AS_IF      *p_as_if;
    USBD_AUDIO_AS_HANDLE   as_if_handle;
    USBD_ERR               err_usbd;


    while (DEF_TRUE) {
                                                                /* Wait for an AS IF handle.                            */
        as_if_handle = (USBD_AUDIO_AS_HANDLE)(CPU_ADDR)USBD_Audio_OS_PlaybackReqPend(&err_usbd);

        p_as_if = USBD_Audio_AS_IF_Get(as_if_handle);
        if (p_as_if == DEF_NULL) {
            USBD_DBG_AUDIO_PROC_MSG("PlaybackTaskHandler(): Invalid AS IF handle\r\n");
            continue;
        }

        USBD_AUDIO_STAT_INC(p_as_if->AS_IF_SettingsPtr->StatPtr->AudioProc_Playback_NbrReqPendPlaybackTask);

        USBD_Audio_OS_AS_IF_LockAcquire(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle),
                                        USBD_AUDIO_LOCK_TIMEOUT_mS,
                                       &err_usbd);
        if (err_usbd != USBD_ERR_NONE) {
            USBD_DBG_AUDIO_PROC_ERR("PlaybackTaskHandler(): lock acquire failed w/ err = %d\r\n", err_usbd);
            continue;
        }

        if (USBD_AUDIO_AS_IF_HANDLE_VALIDATE(p_as_if, as_if_handle) != DEF_OK) {
            USBD_DBG_AUDIO_PROC_MSG("PlaybackTaskHandler(): Invalid AS IF handle\r\n");
            goto end_lock_rel;
        }

        USBD_Audio_PlaybackBufSubmit(p_as_if);                  /* Codec xfer finished, submit a new rdy buf to codec.  */

end_lock_rel:
        USBD_Audio_OS_AS_IF_LockRelease(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle));
    }
}
#endif


/*
*********************************************************************************************************
*                                       USBD_Audio_AS_IF_Start()
*
* Description : Start record or playback streaming.
*
* Argument(s) : p_as_if     Pointer to the AudioStreaming Interface structure.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE   Record or playback stream successfully started.
*                           USBD_ERR_FAIL   Audio driver failed to process the start request.
*
*                           -RETURNED BY USBD_Audio_OS_PlaybackEventPut()-
*                           See USBD_Audio_OS_PlaybackEventPut() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
void  USBD_Audio_AS_IF_Start (USBD_AUDIO_AS_IF  *p_as_if,
                              USBD_ERR          *p_err)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;


    USBD_Audio_OS_AS_IF_LockAcquire(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle),
                                    USBD_AUDIO_LOCK_TIMEOUT_mS,
                                    p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("AS_IF_Start(): lock acquire failed w/ err = %d\r\n", *p_err);
        return;
    }

                                                                /* ------------- START RECORD OR PLAYBACK ------------- */
    p_as_if_settings                = p_as_if->AS_IF_SettingsPtr;
    p_as_if_settings->StreamStarted = DEF_YES;
    USBD_AUDIO_STAT_RESET(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrBufDescInUse);
    USBD_AUDIO_STAT_RESET(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxOngoingCnt);

    USBD_Audio_AS_IF_RingBufQInit(p_as_if);                     /* Alloc buf and init ring buf q w/ it.                 */

    if (p_as_if_settings->StreamDir == USBD_AUDIO_STREAM_IN) {
#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
        CPU_BOOLEAN  valid;


                                                                /* Start record streaming on audio codec side.          */
        valid = p_as_if_settings->AS_API_Ptr->StreamStart(p_as_if_settings->DrvInfoPtr,
                                                          p_as_if->Handle,
                                                          p_as_if_settings->TerminalID);
        if (valid != DEF_OK) {
           *p_err = USBD_ERR_FAIL;
            goto end_lock_clean;
        }
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
    } else {
        USBD_Audio_PlaybackPrime(p_as_if, p_err);               /* Submit 1st isoc xfer to start priming.               */
        if (*p_err != USBD_ERR_NONE) {
            USBD_DBG_AUDIO_PROC_ERR("AS_IF_Start(): starting playback priming failed w/ err = %d\r\n", *p_err);
            goto end_lock_clean;
        }
#endif
    }
    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_NbrStreamOpen);

    USBD_Audio_OS_AS_IF_LockRelease(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle));
    return;

end_lock_clean:
    p_as_if_settings->StreamStarted = DEF_NO;
    USBD_AUDIO_AS_IF_HANDLE_INVALIDATE(p_as_if);
    USBD_Audio_OS_AS_IF_LockRelease(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle));
}
#endif


/*
*********************************************************************************************************
*                                        USBD_Audio_AS_IF_Stop()
*
* Description : Stop record or playback streaming.
*
* Argument(s) : p_as_if     Pointer to the AudioStreaming Interface structure.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE   Record or playback stream successfully stopped.
*                           USBD_ERR_FAIL   Audio driver failed to process the stop request.
*
*                           -RETURNED BY USBD_Audio_PlaybackBufQFlush()-
*                           See USBD_Audio_PlaybackBufQFlush() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) When closing the stream, any buffer currently processed by the codec driver, the
*                   USB driver or waiting to be processed inside the Audio Processing layer are freed
*                   implicitly by resetting the ring buffer queue. The buffers does not need to be
*                   explicitly returned in a pool or in a list. The buffers reside permanently in the
*                   stream memory segment. They will be re-allocated to the ring buffer queue at the next
*                   stream opening.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
void  USBD_Audio_AS_IF_Stop (USBD_AUDIO_AS_IF  *p_as_if,
                             USBD_ERR          *p_err)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    CPU_BOOLEAN                 valid;
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
    CPU_SR_ALLOC();
#endif

    USBD_Audio_OS_AS_IF_LockAcquire(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle),
                                    USBD_AUDIO_LOCK_TIMEOUT_mS,
                                    p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("AS_IF_Stop(): lock acquire failed w/ err = %d\r\n", *p_err);
        return;
    }

    p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    if (p_as_if_settings->StreamStarted == DEF_NO) {            /* If dev disconnects & stream not started, return.     */
       *p_err = USBD_ERR_NONE;
        goto end_lock_clean;
    }
                                                                /* ---------------- INVALIDATE HANDLE ----------------- */
    USBD_AUDIO_AS_IF_HANDLE_INVALIDATE(p_as_if);
                                                                /* ---------------- RESET AS IF STRUCT ---------------- */
    p_as_if_settings->StreamStarted     = DEF_NO;
    p_as_if_settings->StreamPrimingDone = DEF_NO;
#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_PLAYBACK_EN      == DEF_ENABLED)
    p_as_if_settings->CorrFrameNbr      = 0u;
#endif

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
    if (p_as_if_settings->StreamDir == USBD_AUDIO_STREAM_IN) {

        p_as_if_settings->RecordRateAdjMs        = 0u;
        p_as_if_settings->RecordRateAdjXferCtr   = 0u;
        p_as_if_settings->RecordBufLen           = 0u;
        p_as_if_settings->RecordIsocTxOngoingCnt = 0u;
    }
#endif
    USBD_AUDIO_STAT_PROT_INC(p_as_if_settings->StatPtr->AudioProc_NbrStreamClosed);

                                                                /* ------------------- STOP STREAM -------------------- */
    valid = p_as_if_settings->AS_API_Ptr->StreamStop(p_as_if_settings->DrvInfoPtr,
                                                     p_as_if_settings->TerminalID);
    if (valid != DEF_OK) {
       *p_err = USBD_ERR_FAIL;
        USBD_DBG_AUDIO_PROC_ERR("AS_IF_Stop(): codec playback stop failed w/ err = %d\r\n", *p_err);
        goto end_lock_clean;
    }

                                                                /* ----------------- RESET RING BUF Q ----------------- */
    p_as_if_settings->StreamRingBufQ.ProducerStartIx = 0u;
    p_as_if_settings->StreamRingBufQ.ProducerEndIx   = 0u;
    p_as_if_settings->StreamRingBufQ.ConsumerStartIx = 0u;
    p_as_if_settings->StreamRingBufQ.ConsumerEndIx   = 0u;
                                                                /* See Note #1.                                         */
    Mem_Clr((void *)p_as_if_settings->StreamRingBufQ.BufDescTblPtr,
                   (p_as_if_settings->BufTotalNbr * sizeof(USBD_AUDIO_BUF_DESC)));

end_lock_clean:
    USBD_Audio_OS_AS_IF_LockRelease(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle));
}
#endif


/*
*********************************************************************************************************
*                                       USBD_Audio_RecordBufGet()
*
* Description : Get a buffer from the stream ring buffer queue.
*
* Argument(s) : as_handle   AudioStreaming interface handle.
*
*               p_buf_len   Pointer to allocated buffer length (in bytes).
*
* Return(s)   : Pointer to record buffer, if NO error(s).
*
*               Null pointer,             otherwise.
*
* Note(s)     : (1) Record buffer length is computed according to sample rate, bit resolution and number
*                   of channels in USBD_Audio_AS_EP_CtrlProcess(). The buffer length takes into account
*                   the data rate adjustment and the built-in correction if this one has been enabled.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
void  *USBD_Audio_RecordBufGet (USBD_AUDIO_AS_HANDLE   as_handle,
                                CPU_INT16U            *p_buf_len)
{
    USBD_AUDIO_AS_IF     *p_as_if;
    USBD_AUDIO_BUF_DESC  *p_buf_desc;
    void                 *p_buf;
    CPU_INT16U            ix;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_buf_len == DEF_NULL) {                                /* Validate buf len ptr.                                */
        return (DEF_NULL);
    }
#endif

    p_as_if = USBD_Audio_AS_IF_Get(as_handle);
    if (p_as_if == DEF_NULL) {
        USBD_DBG_AUDIO_PROC_MSG("RecordBufGet(): Invalid AS IF handle\r\n");
        return (DEF_NULL);
    }

    if (USBD_AUDIO_AS_IF_HANDLE_VALIDATE(p_as_if, as_handle) != DEF_OK) {
        USBD_DBG_AUDIO_PROC_MSG("RecordBufGet(): Invalid AS IF handle\r\n");
        return (DEF_NULL);
    }
                                                                /* ----------------- RECORD BUF ALLOC ----------------- */
                                                                /* Get a buf desc from the Ring Buf Q.                  */
    ix = USBD_Audio_AS_IF_RingBufQProducerStartIxGet(p_as_if->AS_IF_SettingsPtr);
    if (ix == USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {
        return (DEF_NULL);
    }

    p_buf_desc = USBD_Audio_AS_IF_RingBufQGet(&p_as_if->AS_IF_SettingsPtr->StreamRingBufQ, ix);
    if (p_buf_desc == DEF_NULL) {
        USBD_AUDIO_STAT_INC(p_as_if->AS_IF_SettingsPtr->StatPtr->AudioProc_RingBufQ_NbrErr);
        USBD_DBG_AUDIO_PROC_MSG("RecordBufGet(): buf desc retrieved from Ring Buf Q should NOT be a NULL ptr\r\n");
        return (DEF_NULL);
    }

    p_buf     = p_buf_desc->BufPtr;
   *p_buf_len = p_buf_desc->BufLen;                             /* See Note #1.                                         */

   USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if->AS_IF_SettingsPtr, &p_as_if->AS_IF_SettingsPtr->StreamRingBufQ.ProducerStartIx);

    return (p_buf);
}
#endif


/*
*********************************************************************************************************
*                                       USBD_Audio_RecordRxCmpl()
*
* Description : Signal to the Record task that a buffer is ready.
*
* Argument(s) : as_handle   AudioStreaming interface handle.
*
* Return(s)   : none.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
void  USBD_Audio_RecordRxCmpl (USBD_AUDIO_AS_HANDLE  as_handle)
{
    USBD_ERR           err_usbd;
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
    USBD_AUDIO_AS_IF  *p_as_if;


    p_as_if = USBD_Audio_AS_IF_Get(as_handle);
    if (p_as_if == DEF_NULL) {
        USBD_DBG_AUDIO_PROC_MSG("RecordRxCmpl(): Invalid AS IF handle\r\n");
        return;
    }
#endif

    USBD_Audio_OS_RecordReqPost((void *)(CPU_ADDR)as_handle,    /* Signal audio xfer cmpl.                              */
                                                 &err_usbd);
    if (err_usbd != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("PlaybackTxCmpl(): signaling record task failed w/ err = %d\r\n", err_usbd);
    } else {
        USBD_AUDIO_STAT_INC(p_as_if->AS_IF_SettingsPtr->StatPtr->AudioProc_Record_NbrReqPostRecordTask);
    }
}
#endif


/*
*********************************************************************************************************
*                                      USBD_Audio_RecordBufFree()
*
* Description : Free aborted record buffer.
*
* Argument(s) : as_handle   AudioStreaming interface handle.
*
*               p_buf       Pointer to buffer to free.
*
* Return(s)   : None.
*
* Note(s)     : (1) This function is DEPRECATED and will be removed in a future version of this product.
*                   The new record path implementation does not require the codec driver to free
*                   explicitly a consumed buffer.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
void  USBD_Audio_RecordBufFree (USBD_AUDIO_AS_HANDLE   as_handle,
                                void                  *p_buf)
{
    (void)as_handle;
    (void)p_buf;
}
#endif


/*
*********************************************************************************************************
*                                      USBD_Audio_PlaybackTxCmpl()
*
* Description : Signal end of audio transfer to playback task.
*
* Argument(s) : as_handle   AudioStreaming interface handle.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
void  USBD_Audio_PlaybackTxCmpl (USBD_AUDIO_AS_HANDLE  as_handle)
{
    USBD_ERR           err_usbd;
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
    USBD_AUDIO_AS_IF  *p_as_if;


    p_as_if = USBD_Audio_AS_IF_Get(as_handle);
    if (p_as_if == DEF_NULL) {
        USBD_DBG_AUDIO_PROC_MSG("PlaybackTxCmpl(): Invalid AS IF handle\r\n");
        return;
    }
#endif

    USBD_Audio_OS_PlaybackReqPost((void *)(CPU_ADDR)as_handle,  /* Signal audio xfer cmpl.                              */
                                                   &err_usbd);
    if (err_usbd != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("PlaybackTxCmpl(): signaling playback task failed w/ err = %d\r\n", err_usbd);
    } else {
        USBD_AUDIO_STAT_INC(p_as_if->AS_IF_SettingsPtr->StatPtr->AudioProc_Playback_NbrReqPostPlaybackTask);
    }
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_PlaybackBufFree()
*
* Description : Update the Consumer End index in the stream ring buffer queue meaning that a buffer is
*               available for a new USB transfer.
*
* Argument(s) : as_handle   AudioStreaming interface handle.
*
*               p_buf       Pointer to buffer to free.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
void  USBD_Audio_PlaybackBufFree (USBD_AUDIO_AS_HANDLE   as_handle,
                                  void                  *p_buf)
{
    CPU_INT16U         ix;
    USBD_AUDIO_AS_IF  *p_as_if;


    (void)p_buf;

    p_as_if = USBD_Audio_AS_IF_Get(as_handle);
    if (p_as_if == DEF_NULL) {
        USBD_DBG_AUDIO_PROC_MSG("PlaybackBufFree(): Invalid AS IF handle\r\n");
        return;
    }

    if (USBD_AUDIO_AS_IF_HANDLE_VALIDATE(p_as_if, as_handle) != DEF_OK) {
        USBD_DBG_AUDIO_PROC_MSG("PlaybackBufFree(): Invalid AS IF handle\r\n");
        return;
    }

    ix = USBD_Audio_AS_IF_RingBufQConsumerEndIxGet(p_as_if->AS_IF_SettingsPtr);
    if (ix != USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {         /* Update ConsumerEndIx only if it did not catch up...  */
                                                                /* ...ConsumerStartIx or ProducerStartIx.               */
        USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if->AS_IF_SettingsPtr, &p_as_if->AS_IF_SettingsPtr->StreamRingBufQ.ConsumerEndIx);
    }
}
#endif


/*
*********************************************************************************************************
*                                        USBD_Audio_StatGet()
*
* Description : Get the audio statistics structure associated to the specified AudioStreaming interface.
*
* Argument(s) : as_handle   AudioStreaming handle.
*
* Return(s)   : Pointer to audio statistics.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
USBD_AUDIO_STAT  *USBD_Audio_StatGet (USBD_AUDIO_AS_HANDLE  as_handle)
{
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
    USBD_AUDIO_AS_IF  *p_as_if;
    CPU_INT08U         as_if_ix;


    as_if_ix = USBD_AUDIO_AS_IF_HANDLE_IX_GET(as_handle);

    p_as_if = &USBD_Audio_AS_Tbl[as_if_ix];

    return (p_as_if->AS_IF_SettingsPtr->StatPtr);
#else
    (void)as_handle;

    return (DEF_NULL);
#endif
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      USBD_Audio_RecordIsocCmpl()
*
* Description : Process isochronous IN transfer completion.
*
* Argument(s) : dev_nbr     Device Number.
*
*               ep_addr     Endpoint Address.
*
*               p_buf       Pointer to the buffer containing Record Data that was sent to the host.
*
*               buf_len     Buffer length (in bytes).
*
*               xfer_len    Number of bytes transferred (in bytes).
*
*               p_arg       Pointer to the AudioStreaming IF passed into argument to this function.
*
*               err         Error status.
*
* Return(s)   : None.
*
* Note(s)     : (1) If a transfer completes with an error code different from USBD_ERR_EP_ABORT, all
*                   the operations done in a normal situation, that is no error, are performed to reuse
*                   the freed buffer for a new buffer request.
*
*               (2) An isochronous IN transfer is aborted (error USBD_ERR_EP_ABORT) if the stream is
*                   closed by the host or if the device disconnects from the host.
*
*               (3) Ring buffer queue lock protects the 'ConsumerStartIx' when the record task must
*                   restart internally. When restarting a stream, the Record task starts an isochronous
*                   IN transfer. Record task may be preempted by the Core task because the isochronous
*                   IN transfer has finished but the Record task must update 'ConsumerStartIx' before
*                   'ConsumerEndIx' checks it. Stream restart occurs because there is no more ongoing
*                   isochronous transfer in the USB driver.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
static  void  USBD_Audio_RecordIsocCmpl (CPU_INT08U   dev_nbr,
                                         CPU_INT08U   ep_addr,
                                         void        *p_buf,
                                         CPU_INT32U   buf_len,
                                         CPU_INT32U   xfer_len,
                                         void        *p_arg,
                                         USBD_ERR     err)
{
    USBD_AUDIO_AS_IF           *p_as_if;
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    USBD_AUDIO_BUF_DESC        *p_buf_desc;
    CPU_INT16U                  nbr_xfer_submitted;
    CPU_INT16U                  ix;
    USBD_ERR                    err_usbd;
#if (USBD_AUDIO_CFG_RECORD_CORR_EN == DEF_ENABLED)
    CPU_INT16U                  frame_nbr_cur;
    CPU_INT16U                  frame_nbr_diff;
#endif
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)ep_addr;
    (void)p_buf;
    (void)buf_len;
    (void)xfer_len;

    p_as_if          = (USBD_AUDIO_AS_IF *)p_arg;
    p_as_if_settings =  p_as_if->AS_IF_SettingsPtr;

    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxCmpl);

    CPU_CRITICAL_ENTER();
    p_as_if_settings->RecordIsocTxOngoingCnt--;
    CPU_CRITICAL_EXIT();

                                                                /* -------- CHECK IF ERR UPON XFER COMPLETION --------- */
    if ((err != USBD_ERR_NONE) &&
        (err != USBD_ERR_EP_ABORT)) {                           /* See Note #1.                                         */

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxCmplErrOther);

    } else if (err == USBD_ERR_EP_ABORT) {                      /* See Note #2.                                         */
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxCmplErrAbort);
        return;
    }
                                                                /* --------------- PREPARE NXT BUF REQ ---------------- */
    USBD_Audio_OS_RingBufQLockAcquire(p_as_if_settings->Ix,     /* See Note #3.                                         */
                                      USBD_AUDIO_LOCK_TIMEOUT_mS,
                                     &err_usbd);
    if (err_usbd != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("RecordisocCmpl(): lock acquire failed w/ err = %d\r\n", err_usbd);
        return;
    }
                                                                /* Get a buf desc from the Ring Buf Q.                  */
    ix = USBD_Audio_AS_IF_RingBufQConsumerEndIxGet(p_as_if_settings);
    if (ix == USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {
        USBD_Audio_OS_RingBufQLockRelease(p_as_if_settings->Ix);
        return;
    }

    USBD_Audio_OS_RingBufQLockRelease(p_as_if_settings->Ix);

    p_buf_desc = USBD_Audio_AS_IF_RingBufQGet(&p_as_if_settings->StreamRingBufQ, ix);
    if (p_buf_desc == DEF_NULL) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrErr);
        USBD_DBG_AUDIO_PROC_MSG("RecordisocCmpl(): buf desc retrieved from Ring Buf Q should NOT be a NULL ptr\r\n");
        return;
    }

    p_buf_desc->BufLen = USBD_Audio_RecordDataRateAdj(p_as_if);
                                                                /* -------------- EVALUATE BUILT-IN CORR -------------- */
#if (USBD_AUDIO_CFG_RECORD_CORR_EN == DEF_ENABLED)
    frame_nbr_cur  = USBD_DevFrameNbrGet(p_as_if->DevNbr, &err_usbd);
    frame_nbr_cur  = USBD_FRAME_NBR_GET(frame_nbr_cur);
    frame_nbr_diff = USBD_FRAME_NBR_DIFF_GET(p_as_if_settings->CorrFrameNbr, frame_nbr_cur);
                                                                /* Check if we match or exceed the corr period.         */
    if (frame_nbr_diff >= p_as_if_settings->CorrPeriod) {

        USBD_Audio_RecordCorrBuiltIn(p_as_if, p_buf_desc);

        p_as_if_settings->CorrFrameNbr = frame_nbr_cur;         /* Keep frame nbr for next period computation.          */
    }
#endif
                                                                /* Update ix only after writing to buf desc.            */
    USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if_settings, &p_as_if_settings->StreamRingBufQ.ConsumerEndIx);

                                                                /* -------- SUBMIT ISOC XFER(S) TO USB DEV DRV -------- */
    USBD_Audio_RecordUsbBufSubmit(p_as_if, &nbr_xfer_submitted);
    USBD_AUDIO_STAT_ADD(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxSubmitCoreTask, nbr_xfer_submitted);
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_RecordPrime()
*
* Description : Start the first isochronous transfer that will prime the record stream.
*
* Argument(s) : p_as_if     Pointer to the audio streaming interface.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE               Stream priming successfully started.
*                           USBD_ERR_FAIL               No buffer descriptor obtained from ring buffer queue.
*
*                                                       - RETURNED BY USBD_Audio_OS_RingBufQLockAcquire() : -
*                           USBD_ERR_OS_TIMEOUT         OS lock NOT successfully acquired in the time
*                                                       specified by 'timeout_ms'.
*                           USBD_ERR_OS_ABORT           OS lock aborted.
*                           USBD_ERR_OS_FAIL            OS lock not acquired because another error.
*
*                                                       ------- RETURNED BY USBD_IsocTxAsync() : -------
*                           USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                           USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                       configured state.
*                           USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                           USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                           USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                                       See specific device driver(s) 'USBD_EP_Tx()' for
*                                                       additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Ring buffer queue lock protects the 'ConsumerStartIx' when the record task must restart
*                   internally. When restarting a stream, the Record task starts an isochronous IN
*                   transfer. Record task may be preempted by the Core task because the isochronous IN
*                   transfer has finished but the Record task must update 'ConsumerStartIx' before
*                   'ConsumerEndIx' checks it. Stream restart occurs because there is no more ongoing
*                   isochronous transfer in the USB driver.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
static  void  USBD_Audio_RecordPrime (USBD_AUDIO_AS_IF  *p_as_if,
                                      USBD_ERR          *p_err)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_AS_IF_ALT       *p_as_if_alt      = p_as_if->AS_IF_AltCurPtr;
    USBD_AUDIO_BUF_DESC        *p_buf_desc;
    CPU_INT16U                  ix;
    CPU_SR_ALLOC();


    USBD_Audio_OS_RingBufQLockAcquire(p_as_if_settings->Ix,     /* See Note #1.                                         */
                                      USBD_AUDIO_LOCK_TIMEOUT_mS,
                                      p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("RecordPrime(): lock acquire failed w/ err = %d\r\n", *p_err);
        return;
    }
                                                                /* Get a buf desc from the Ring Buf Q.                  */
    ix = USBD_Audio_AS_IF_RingBufQConsumerStartIxGet(p_as_if_settings);
    if (ix == USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxBufNotAvail);
        USBD_DBG_AUDIO_PROC_MSG("RecordPrime(): no buffer descriptor\r\n");
       *p_err = USBD_ERR_FAIL;
        goto end_lock_clean;
    }
    p_buf_desc = USBD_Audio_AS_IF_RingBufQGet(&p_as_if_settings->StreamRingBufQ, ix);
    if (p_buf_desc == DEF_NULL) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrErr);
        USBD_DBG_AUDIO_PROC_MSG("RecordPrime(): buf desc retrieved from Ring Buf Q should NOT be a NULL ptr\r\n");
       *p_err = USBD_ERR_FAIL;
        goto end_lock_clean;
    }
                                                                /* Submit buf to USB device driver.                     */
    USBD_IsocTxAsync(        p_as_if->DevNbr,
                             p_as_if_alt->DataIsocAddr,
                             p_buf_desc->BufPtr,
                             p_buf_desc->BufLen,
                             USBD_Audio_RecordIsocCmpl,
                     (void *)p_as_if,
                             p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("RecordPrime(): isochronous Tx failed w/ err = %d\r\n", *p_err);
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxSubmitErr);
        goto end_lock_clean;
    }

    USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if_settings, &p_as_if_settings->StreamRingBufQ.ConsumerStartIx);

    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxSubmitSuccess);
    CPU_CRITICAL_ENTER();
    p_as_if_settings->RecordIsocTxOngoingCnt++;
    CPU_CRITICAL_EXIT();


end_lock_clean:
    USBD_Audio_OS_RingBufQLockRelease(p_as_if_settings->Ix);
}
#endif


/*
*********************************************************************************************************
*                                   USBD_Audio_RecordUsbBufSubmit()
*
* Description : Submit all possible record buffers to the USB device driver.
*
* Argument(s) : p_as_if                 Pointer to the audio streaming interface.
*
*               p_xfer_submitted_cnt    Pointer to number of transfers successfully submitted.
*
* Return(s)   : none.
*
* Note(s)     : (1) The Core layer submits buffers as long as there are free buffers available and that
*                   the transfers can be queued by the USB device driver.
*
*               (2) The USB device driver can queue isochronous transfers. USBD_ERR_EP_QUEUING is returned
*                   when there is no room left to queue the current isochronous transfer. In that case,
*                   another isochronous transfer will be submitted next time an isochronous transfer
*                   completes.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
static  void  USBD_Audio_RecordUsbBufSubmit (USBD_AUDIO_AS_IF  *p_as_if,
                                             CPU_INT16U        *p_xfer_submitted_cnt)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_AS_IF_ALT       *p_as_if_alt      = p_as_if->AS_IF_AltCurPtr;
    USBD_AUDIO_BUF_DESC        *p_buf_desc;
    CPU_BOOLEAN                 loop_end         = DEF_NO;
    USBD_ERR                    err_usbd;
    CPU_INT16U                  ix;
    CPU_SR_ALLOC();


   *p_xfer_submitted_cnt = 0u;
    do {
                                                                /* Get a buf desc from the Ring Buf Q.                  */
        ix = USBD_Audio_AS_IF_RingBufQConsumerStartIxGet(p_as_if_settings);
        if (ix == USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {
            USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxBufNotAvail);
            break;
        }
        p_buf_desc = USBD_Audio_AS_IF_RingBufQGet(&p_as_if_settings->StreamRingBufQ, ix);
        if (p_buf_desc == DEF_NULL) {
            USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrErr);
            USBD_DBG_AUDIO_PROC_MSG("RecordUsbBufSubmit(): buf desc retrieved from Ring Buf Q should NOT be a NULL ptr\r\n");
            break;
        }

        USBD_IsocTxAsync(        p_as_if->DevNbr,
                                 p_as_if_alt->DataIsocAddr,
                                 p_buf_desc->BufPtr,
                                 p_buf_desc->BufLen,
                                 USBD_Audio_RecordIsocCmpl,
                         (void *)p_as_if,
                                &err_usbd);
        if (err_usbd == USBD_ERR_NONE) {                        /* See Note #1.                                         */
            USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if_settings, &p_as_if_settings->StreamRingBufQ.ConsumerStartIx);

            USBD_AUDIO_STAT_PROT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxSubmitSuccess);
           *p_xfer_submitted_cnt += 1u;

            CPU_CRITICAL_ENTER();
            p_as_if_settings->RecordIsocTxOngoingCnt++;
            CPU_CRITICAL_EXIT();
        } else {                                                /* See Note #2.                                         */
            USBD_AUDIO_STAT_PROT_INC(p_as_if_settings->StatPtr->AudioProc_Record_NbrIsocTxSubmitErr);
            loop_end = DEF_YES;
        }
    } while (loop_end != DEF_YES);
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_RecordDataRateAdj()
*
* Description : Adjusts buffer length if the sampling frequency (e.g. 11.025, 22.050, 44.1 KHz) produces
*               a non-integer number of audio samples per ms.
*
* Argument(s) : p_as_if     Pointer to the audio streaming interface.
*
* Return(s)   : Adjusted buffer length.
*
* Note(s)     : (1) Audio transfer for record adjusted by adding one sample frame for certain sampling
*                   frequencies (i.e. 11.025, 22.050 and 44.1 kHz) which produce a non-integer number
*                   of samples per ms. See function USBD_Audio_AS_EP_CtrlProcess() 'Note #3' for details
*                   about this data rate adjustment.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
static  CPU_INT16U  USBD_Audio_RecordDataRateAdj (USBD_AUDIO_AS_IF  *p_as_if)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_AS_IF_ALT       *p_as_alt         = p_as_if->AS_IF_AltCurPtr;
    USBD_AUDIO_AS_ALT_CFG      *p_as_cfg         = p_as_alt->AS_CfgPtr;
    CPU_INT08U                  sample_frame     = p_as_cfg->SubframeSize * p_as_cfg->NbrCh;
    CPU_INT16U                  buf_len          = p_as_if_settings->RecordBufLen;


    if (p_as_if_settings->RecordRateAdjMs != 0u) {          /* Adjust buf len if necessary (see Note #1).           */
        p_as_if_settings->RecordRateAdjXferCtr++;           /* Increment ctr tracking buf len adjustment.           */

        if (p_as_if_settings->RecordRateAdjXferCtr == p_as_if_settings->RecordRateAdjMs) {
           buf_len                                = p_as_if_settings->RecordBufLen + sample_frame;
           p_as_if_settings->RecordRateAdjXferCtr = 0u;
        }
    }

    return (buf_len);
}
#endif


/*
*********************************************************************************************************
*                                   USBD_Audio_RecordCorrBuiltIn()
*
* Description : Apply data rate error correction by inserting or removing audio sample frame (see Note #1).
*
* Argument(s) : p_as_if     Pointer to AudioStreaming interface.
*
*               p_buf_desc  Pointer to buffer descriptor.
*
* Return(s)   : none.
*
* Note(s)     : (1) Record correction is done by removing (overrun situation) or inserting (underrun) an
*                   audio sample frame. The correction is done implicitly by the Audio Peripheral
*                   hardware by directly getting the right number of audio samples (-1 sample frame or +1
*                   sample frame) to accommodate the overrun or underrun situation. The benefit of
*                   handling the correction here is to avoid using a software algorithm in the Record
*                   task to remove or add an audio sample frame.
*
*               (2) The overrun or underrun situation is detected by computing a buffers difference
*                   representing the difference between the ready buffers production by the codec and
*                   the consumption of these ready buffers by USB. This buffers' difference is obtained
*                   from the stream ring buffer queue. The ring buffer queue scheme is common to the
*                   playback and record streams. Within the audio class, the definition of overrun and
*                   underrun situation is "USB-centric". For record, a negative buffers' difference
*                   indicates an overrun situation, that is the USB consumes too quickly buffers and thus
*                   a sample is removed so that the codec production speeds up. Conversely, a positive
*                   buffers' difference indicates an underrun situation.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_RECORD_EN      == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_RECORD_CORR_EN == DEF_ENABLED)
static  void  USBD_Audio_RecordCorrBuiltIn (USBD_AUDIO_AS_IF     *p_as_if,
                                            USBD_AUDIO_BUF_DESC  *p_buf_desc)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_AS_IF_ALT       *p_as_if_alt;
    USBD_AUDIO_AS_ALT_CFG      *p_as_cfg;
    CPU_INT08S                  buf_diff;
    CPU_INT08U                  sample_frame;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    buf_diff = USBD_Audio_BufDiffGet(p_as_if_settings);         /* Get cur buf diff between USB & codec.                */
    CPU_CRITICAL_EXIT();

                                                                /* If safe zone, no correction applies.                 */
    if ((buf_diff > p_as_if_settings->CorrBoundaryHeavyNeg) &&
        (buf_diff < p_as_if_settings->CorrBoundaryHeavyPos)) {

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_CorrNbrSafeZone);
        return;
    }

    p_as_if_alt  = p_as_if->AS_IF_AltCurPtr;
    p_as_cfg     = p_as_if_alt->AS_CfgPtr;
    sample_frame = p_as_cfg->NbrCh * p_as_cfg->SubframeSize;    /* Compute audio frame size.                            */
                                                                /* -------------- OVERRUN: REMOVE SAMPLE -------------- */
    if (buf_diff <= p_as_if_settings->CorrBoundaryHeavyNeg) {   /* See Note 2.                                          */

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_CorrNbrOverrun);
        p_buf_desc->BufLen -= sample_frame;

    } else {                                                    /* ------------- UNDERRUN: INSERT SAMPLE -------------- */

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_CorrNbrUnderrun);
        p_buf_desc->BufLen += sample_frame;
    }
}
#endif

/*
*********************************************************************************************************
*                                     USBD_Audio_IsocPlaybackCmpl()
*
* Description : Process isochronous OUT transfer completion.
*
* Argument(s) : dev_nbr     Device Number.
*
*               ep_addr     Endpoint Address.
*
*               p_buf       Pointer to the buffer containing audio data received from the host.
*
*               buf_len     Buffer length (in bytes).
*
*               xfer_len    Number of bytes received (in bytes).
*
*               p_arg       Pointer to the AudioStreaming IF passed into argument to this function.
*
*               err         Error status.
*
* Return(s)   : None.
*
* Note(s)     : (1) An isochronous OUT transfer is aborted (error USBD_ERR_EP_ABORT) if the stream is
*                   closed by the host or if the device disconnects from the host.
*
*               (2) Ring buffer queue lock protects the 'ProducerStartIx' when the Playback task must
*                   restart internally. When restarting a stream, the Playback task starts an isochronous
*                   OUT transfer. Playback task may be preempted by the Core task because the isochronous
*                   IN transfer has finished but the Playback task must update 'ProducerStartIx' before
*                   'ProducerEndIx' checks it. Stream restart occurs because there is no more ongoing
*                   isochronous transfer in the USB driver.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
static  void  USBD_Audio_PlaybackIsocCmpl (CPU_INT08U   dev_nbr,
                                           CPU_INT08U   ep_addr,
                                           void        *p_buf,
                                           CPU_INT32U   buf_len,
                                           CPU_INT32U   xfer_len,
                                           void        *p_arg,
                                           USBD_ERR     err)
{
    USBD_AUDIO_AS_IF           *p_as_if;
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    USBD_AUDIO_BUF_DESC        *p_buf_desc;
#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED)
    USBD_AUDIO_AS_IF_ALT       *p_as_if_alt;
#endif
    CPU_INT16U                  ix;
    CPU_BOOLEAN                 valid;
    CPU_INT16U                  nbr_xfer_submitted;
    CPU_BOOLEAN                 pre_buf_compl;
    USBD_ERR                    err_usbd;
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
    CPU_SR_ALLOC();
#endif


    (void)dev_nbr;
    (void)ep_addr;
    (void)p_buf;
    (void)buf_len;

    p_as_if          = (USBD_AUDIO_AS_IF *)p_arg;
    p_as_if_settings =  p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_STAT_PROT_DEC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxOngoingCnt);
    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxCmpl);

                                                                /* -------- CHECK IF ERR UPON XFER COMPLETION --------- */
    if ((err != USBD_ERR_NONE) &&
        (err != USBD_ERR_EP_ABORT)) {

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxCmplErrOther);
        return;

    } else if (err == USBD_ERR_EP_ABORT) {                      /* See Note #1.                                         */
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxCmplErrAbort);
        return;
    }
                                                                /* ------------- STORE BUF IN RING BUF Q -------------- */
    USBD_Audio_OS_RingBufQLockAcquire(p_as_if_settings->Ix,     /* See note #2.                                         */
                                      USBD_AUDIO_LOCK_TIMEOUT_mS,
                                     &err_usbd);
    if (err_usbd != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("IsocPlaybackCmpl(): lock acquire failed w/ err = %d\r\n", err_usbd);
        return;
    }
                                                                /* Get a buf desc from the Ring Buf Q.                  */
    ix = USBD_Audio_AS_IF_RingBufQProducerEndIxGet(p_as_if_settings);
    if (ix == USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrErr);
        USBD_Audio_OS_RingBufQLockRelease(p_as_if_settings->Ix);
        return;
    }

    USBD_Audio_OS_RingBufQLockRelease(p_as_if_settings->Ix);

    p_buf_desc = USBD_Audio_AS_IF_RingBufQGet(&p_as_if_settings->StreamRingBufQ, ix);
    if (p_buf_desc == DEF_NULL) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrErr);
        USBD_DBG_AUDIO_PROC_MSG("IsocPlaybackCmpl(): buf desc retrieved from Ring Buf Q should NOT be a NULL ptr\r\n");
        return;
    }
                                                                /* Init buf desc.                                       */
    p_buf_desc->BufLen = xfer_len;
                                                                /* Update ix only after writing to buf desc.            */
    USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if_settings, &p_as_if_settings->StreamRingBufQ.ProducerEndIx);

                                                                /* -------- SUBMIT ISOC XFER(S) TO USB DEV DRV -------- */
    USBD_Audio_PlaybackUsbBufSubmit(p_as_if, &nbr_xfer_submitted);
    USBD_AUDIO_STAT_ADD(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxSubmitCoreTask, nbr_xfer_submitted);

                                                                /* ----- START PLAYBACK ON CODEC IF PRIMING DONE ------ */
    pre_buf_compl = p_as_if_settings->StreamRingBufQ.ProducerEndIx >= p_as_if_settings->StreamPreBufMax ? DEF_YES : DEF_NO;

    if ((p_as_if_settings->StreamPrimingDone == DEF_NO) &&
        (pre_buf_compl                       == DEF_YES)) {

        valid = p_as_if_settings->AS_API_Ptr->StreamStart(p_as_if_settings->DrvInfoPtr,
                                                          p_as_if->Handle,
                                                          p_as_if_settings->TerminalID);
        if (valid != DEF_OK) {
            USBD_AUDIO_AS_IF_HANDLE_INVALIDATE(p_as_if);        /* = stream marked as closed internally.                */
            USBD_AUDIO_STAT_PROT_INC(p_as_if_settings->StatPtr->AudioProc_NbrStreamClosed);

            USBD_DBG_AUDIO_PROC_MSG("IsocPlaybackCmpl(): playback not started\r\n");
            return;
        }
#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED)
                                                                /* Sample removal/insertion corr is en.                 */
        p_as_if_alt = p_as_if->AS_IF_AltCurPtr;
        if (p_as_if_alt->SynchIsocAddr == USBD_EP_ADDR_NONE) {
                                                                /* Get initial frame nbr for corr period computation.   */
            p_as_if_settings->CorrFrameNbr = USBD_DevFrameNbrGet(p_as_if->DevNbr, &err_usbd);
            p_as_if_settings->CorrFrameNbr = USBD_FRAME_NBR_GET(p_as_if_settings->CorrFrameNbr);
        }
#endif

        p_as_if_settings->StreamPrimingDone = DEF_YES;
    }
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_PlaybackPrime()
*
* Description : Start the first isochronous transfer that will prime the playback stream.
*
* Argument(s) : p_as_if     Pointer to the audio streaming interface.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE               Stream priming successfully started.
*                           USBD_ERR_FAIL               No buffer descriptor obtained from ring buffer queue.
*
*                                                       ------- RETURNED BY USBD_IsocRxAsync() : -------
*                           USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                           USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                       configured state.
*                           USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                           USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                           USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                                       See specific device driver(s) 'USBD_EP_Rx()' for
*                                                       additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN  == DEF_ENABLED)
static  void  USBD_Audio_PlaybackPrime (USBD_AUDIO_AS_IF  *p_as_if,
                                        USBD_ERR          *p_err)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_AS_IF_ALT       *p_as_if_alt      = p_as_if->AS_IF_AltCurPtr;
    USBD_AUDIO_BUF_DESC        *p_buf_desc;
    CPU_INT16U                  ix;
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
    CPU_SR_ALLOC();
#endif


                                                                /* Get a buf desc from the Ring Buf Q.                  */
    ix = USBD_Audio_AS_IF_RingBufQProducerStartIxGet(p_as_if_settings);
    if (ix == USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {
       *p_err = USBD_ERR_FAIL;
        return;
    }
    p_buf_desc = USBD_Audio_AS_IF_RingBufQGet(&p_as_if_settings->StreamRingBufQ, ix);
    if (p_buf_desc == DEF_NULL) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrErr);
        USBD_DBG_AUDIO_PROC_MSG("PlaybackPrime(): buf desc retrieved from Ring Buf Q should NOT be a NULL ptr\r\n");
       *p_err = USBD_ERR_FAIL;
        return;
    }

    USBD_IsocRxAsync(        p_as_if->DevNbr,
                             p_as_if_alt->DataIsocAddr,
                             p_buf_desc->BufPtr,
                             p_as_if_alt->MaxPktLen,
                             USBD_Audio_PlaybackIsocCmpl,
                     (void *)p_as_if,
                             p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("PlaybackPrime(): isochronous Rx failed w/ err = %d\r\n", *p_err);
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxSubmitErr);
        return;
    }

    USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if_settings, &p_as_if_settings->StreamRingBufQ.ProducerStartIx);

    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxSubmitSuccess);
    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxSubmitCoreTask);
    USBD_AUDIO_STAT_PROT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxOngoingCnt);
}
#endif


/*
*********************************************************************************************************
*                                  USBD_Audio_PlaybackUsbBufSubmit()
*
* Description : Submit all possible playback buffers to the USB device driver.
*
* Argument(s) : p_as_if                 Pointer to the audio streaming interface.
*
*               p_xfer_submitted_cnt    Pointer to number of transfers successfully submitted.
*
* Return(s)   : none.
*
* Note(s)     : (1) Playback task or Core layer submits buffers as long as there are free buffers available
*                   and that the transfers can be queued by the USB device driver.
*
*               (2) The USB device driver can queue isochronous transfers. USBD_ERR_EP_QUEUING is returned
*                   when there is no room left to queue the current isochronous transfer. In that case,
*                   another isochronous transfer will be submitted next time an isochronous transfer
*                   completes or the playback task processes an event.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
static  void  USBD_Audio_PlaybackUsbBufSubmit (USBD_AUDIO_AS_IF  *p_as_if,
                                               CPU_INT16U        *p_xfer_submitted_cnt)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_AS_IF_ALT       *p_as_if_alt      = p_as_if->AS_IF_AltCurPtr;
    USBD_AUDIO_BUF_DESC        *p_buf_desc;
    CPU_BOOLEAN                 loop_end         = DEF_NO;
    CPU_INT16U                  ix;
    USBD_ERR                    err_usbd;
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
    CPU_SR_ALLOC();
#endif


   *p_xfer_submitted_cnt = 0u;

    do {
                                                                    /* Get a buf desc from the Ring Buf Q.                  */
        ix = USBD_Audio_AS_IF_RingBufQProducerStartIxGet(p_as_if_settings);
        if (ix == USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {
            USBD_AUDIO_STAT_PROT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxBufNotAvail);
            break;
        }
        p_buf_desc = USBD_Audio_AS_IF_RingBufQGet(&p_as_if_settings->StreamRingBufQ, ix);
        if (p_buf_desc == DEF_NULL) {
            USBD_AUDIO_STAT_INC(p_as_if->AS_IF_SettingsPtr->StatPtr->AudioProc_RingBufQ_NbrErr);
            USBD_DBG_AUDIO_PROC_MSG("PlaybackUsbBufSubmit(): buf desc retrieved from Ring Buf Q should NOT be a NULL ptr\r\n");
            break;
        }

        USBD_IsocRxAsync(        p_as_if->DevNbr,
                                 p_as_if_alt->DataIsocAddr,
                                 p_buf_desc->BufPtr,
                                 p_as_if_alt->MaxPktLen,
                                 USBD_Audio_PlaybackIsocCmpl,
                         (void *)p_as_if,
                                &err_usbd);
        if (err_usbd == USBD_ERR_NONE) {
            USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if_settings, &p_as_if_settings->StreamRingBufQ.ProducerStartIx);

            USBD_AUDIO_STAT_PROT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxSubmitSuccess);
            USBD_AUDIO_STAT_PROT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxOngoingCnt);
           *p_xfer_submitted_cnt += 1u;
        } else {
            USBD_AUDIO_STAT_PROT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxSubmitErr);
            loop_end = DEF_YES;
        }
    } while (loop_end != DEF_YES);
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_PlaybackBufSubmit()
*
* Description : Submits empty buffer(s) to USB and a ready buffer to the codec.
*
* Argument(s) : p_as_if     Pointer to the audio streaming interface.
*
* Return(s)   : none.
*
* Note(s)     : (1) Ring buffer queue lock protects the index 'ProducerStartIx' from a specific race
*                   condition which can occur when the Playback task must restart internally.
*                   The race condition is more likely to happen when priority(core task) > priority
*                   (playback task). When restarting a stream, the Playback task starts an isochronous
*                   OUT transfer. Playback task may be preempted by the Core task because the isochronous
*                   OUT transfer has finished but the Playback task must update 'ProducerStartIx' before
*                   'ProducerEndIx' checks it. The lock allows the safe update of 'ProducerStartIx' by
*                   the Playback task.
*                   When priority(playback task) > priority(core task), the race condition won't happen.
*                   Stream restart occurs because there are no more ongoing isochronous transfers in the
*                   USB driver.
*
*               (2) USBD_Audio_PlaybackUsbBufSubmit() serves two purposes:
*
*                   (a) Restarting the stream if this one was broken because no more isochronous OUT
*                       transfers stored in the USB device driver
*
*                   (b) Simply submitting new USB transfer(s) to the USB driver besides the Core layer
*                       doing the same in USBD_Audio_PlaybackIsocCmpl().
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
static  void  USBD_Audio_PlaybackBufSubmit (USBD_AUDIO_AS_IF  *p_as_if)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_BUF_DESC        *p_buf_desc;
    CPU_INT16U                  ix;
    CPU_INT16U                  nbr_xfer_submitted;
    USBD_ERR                    err_usbd;


                                                                /* --------------------- USB SIDE --------------------- */
    USBD_Audio_OS_RingBufQLockAcquire(p_as_if_settings->Ix,     /* See Note #1.                                         */
                                      USBD_AUDIO_LOCK_TIMEOUT_mS,
                                     &err_usbd);
    if (err_usbd != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("PlaybackCodecBufSubmit(): lock acquire failed w/ err = %d\r\n", err_usbd);
        return;
    }
                                                                /* See Note #2.                                         */
    USBD_Audio_PlaybackUsbBufSubmit(p_as_if, &nbr_xfer_submitted);
    USBD_AUDIO_STAT_ADD(p_as_if_settings->StatPtr->AudioProc_Playback_NbrIsocRxSubmitPlaybackTask, nbr_xfer_submitted);

    USBD_Audio_OS_RingBufQLockRelease(p_as_if_settings->Ix);
                                                                /* -------------------- CODEC SIDE -------------------- */
                                                                /* Get a buf desc from the Ring Buf Q.                  */
    ix = USBD_Audio_AS_IF_RingBufQConsumerStartIxGet(p_as_if_settings);
    if (ix == USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX) {
        USBD_Audio_OS_DlyMs(1u);                                /* Wait 1ms to give chance to other tasks to execute.   */
        USBD_Audio_PlaybackTxCmpl(p_as_if->Handle);
        return;
    }
    p_buf_desc = USBD_Audio_AS_IF_RingBufQGet(&p_as_if_settings->StreamRingBufQ, ix);
    if (p_buf_desc == DEF_NULL) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrErr);
        USBD_DBG_AUDIO_PROC_MSG("RecordPrime(): buf desc retrieved from Ring Buf Q should NOT be a NULL ptr\r\n");
        return;
    }

#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_PLAYBACK_CORR_EN     == DEF_ENABLED)
                                                                /* Apply correction if necessary.                       */
    USBD_Audio_PlaybackCorrExec(p_as_if, p_buf_desc, &err_usbd);
    if (err_usbd != USBD_ERR_NONE) {
        return;
    }
#endif
                                                                /* Submit 1 rdy buf to codec.                           */
    p_as_if_settings->AS_API_Ptr->StreamPlaybackTx(p_as_if_settings->DrvInfoPtr,
                                                   p_as_if_settings->TerminalID,
                                                   p_buf_desc->BufPtr,
                                                   p_buf_desc->BufLen,
                                                  &err_usbd);
    if (err_usbd != USBD_ERR_NONE) {
        USBD_DBG_AUDIO_PROC_ERR("PlaybackCodecBufSubmit(): audio tx xfer not started w/ err = %d\r\n", err_usbd);
        return;
    }

    USBD_Audio_AS_IF_RingBufQIxUpdate(p_as_if_settings, &p_as_if_settings->StreamRingBufQ.ConsumerStartIx);
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_PlaybackCorrExec()
*
* Description : Executes playback correction alogrithm if enabled and required.
*
* Argument(s) : p_as_if     Pointer to the audio streaming interface.
*
*               p_buf_desc  Pointer to buffer to correct.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                       USBD_ERR_NONE               Synch (feedback) endpoint successfully processed.
*                       USBD_ERR_ALLOC              Synch Buffer Alloc failed.
*                       USBD_ERR_FAIL               USB driver failed to return the frame nbr.
*
*                                                   ------- RETURNED BY USBD_IsocTxAsync() : -------
*                       USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                       USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in configured
*                                                   state.
*                       USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                       USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                       USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                                   See specific device driver(s) 'USBD_EP_Tx()' for
*                                                   additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Playback built-in correction CANNOT be active when the endpoint uses the asynchronous
*                   synchronization as two different corrections method would coexist. Only one at a
*                   time can be applied on the stream.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN           == DEF_ENABLED) && \
    ((USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED) || \
     (USBD_AUDIO_CFG_PLAYBACK_CORR_EN     == DEF_ENABLED))
static  void  USBD_Audio_PlaybackCorrExec (USBD_AUDIO_AS_IF     *p_as_if,
                                           USBD_AUDIO_BUF_DESC  *p_buf_desc,
                                           USBD_ERR             *p_err)
{
    USBD_AUDIO_AS_IF_ALT  *p_as_if_alt;
    CPU_INT16U             frame_nbr;


#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_DISABLED)
    (void)p_buf_desc;
#endif

    frame_nbr = USBD_DevFrameNbrGet(p_as_if->DevNbr, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }
    frame_nbr = USBD_FRAME_NBR_GET(frame_nbr);

    p_as_if_alt = p_as_if->AS_IF_AltCurPtr;
    if (p_as_if_alt->SynchIsocAddr != USBD_EP_ADDR_NONE) {      /* See Note #1.                                         */
                                                                /* ------------ SYNCH FEEDBACK CORRECTION ------------- */
#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
        USBD_Audio_PlaybackCorrSynch(p_as_if, frame_nbr, p_err);
#endif

    } else {
#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED)
        USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
        CPU_INT16U                  frame_nbr_diff;


                                                                /* --------------- BUILT-IN CORRECTION ---------------- */
        p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
        frame_nbr_diff   = USBD_FRAME_NBR_DIFF_GET(p_as_if_settings->CorrFrameNbr, frame_nbr);

        if (frame_nbr_diff >= p_as_if_settings->CorrPeriod) {   /* Check if we match or exceed the corr period.         */

            USBD_Audio_PlaybackCorrBuiltIn(p_as_if,             /* Apply corr on the received buf.                      */
                                           p_buf_desc,
                                           p_err);
            if (*p_err != USBD_ERR_NONE) {
                return;
            }

            p_as_if_settings->CorrFrameNbr = frame_nbr;         /* Keep frame nbr for next period computation.          */
        }
#endif
    }
}
#endif


/*
*********************************************************************************************************
*                                  USBD_Audio_PlaybackCorrBuiltIn()
*
* Description : Apply data rate error correction by inserting or removing data samples (see Note #1).
*
* Argument(s) : p_as_if     Pointer to AudioStreaming Interface.
*
*               p_buf_desc  Pointer to buffer descriptor.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE   Correction successfully skipped or applied.
*                           USBD_ERR_FAIL   AudioStreaming interface not active.
*
*                           -RETURNED BY p_as_if->PlaybackCorrCallbackPtr()-
*                           See p_as_if->PlaybackCorrCallbackPtr() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) This stream correction is convenient for low-cost audio design. It will give good
*                   results as long as the incoming USB audio sampling frequency is very close to the DAC
*                   input clock frequency. However, if the difference between the two frequencies is too
*                   high, this will add audio distortion.
*
*                   The stream correction supports signed PCM and unsigned PCM8 format.
*
*               (2) A current buffers consumption difference between the USB and codec sides obtained
*                   from the playback ring buffer queue is monitored with lower and upper boundaries.
*                   Inside these boundaries, a safe zone is considered where no playback correction is
*                   required.
*
*               (3) A user can provide his own stream correction algorithm when an underrun or overrun
*                   occurs. If it is available, the user's correction should be used instead of the
*                   built-in stream correction.
*
*               (4) An overrun situation occurs when the USB part is faster than the audio part.
*                   In that case, an audio sample will be removed on each logical channel to
*                   re-synchronize the audio part so that this one can progressively catch up the USB
*                   rate. The audio sample removal algorithm is the following:
*
*           Sample  ^                                   Sample  ^
*           level   |    ___                            level   |    ___
*                   |___|   |                                   |___|   |
*                   |   |   |___                                |   |   |___
*                   |   |   |   |___                  ====>     |   |   |   |
*                   |   |   |   |   |___                        |   |   |   |___
*                   |   |   |   |   |   |___                    |   |   |   |   |___
*                  _|___|___|___|___|___|___|_____>            _|___|___|___|___|___|______>
*                   |N-5 N-4 N-3 N-2 N-1  N       t             |N-5 N-4 N-3 N-2 N-1  N    t
*
*                       (a) Sample N-2 is rebuilt and equal to the average of N, N-1, N-2 and N-3
*                       (b) Sample N is moved at N-1
*                       (c) The packet size is reduced of one sample
*
*               (5) An audio subframe holds a single PCM audio sample. An audio frame is a collection
*                   of audio subframes, each containing a PCM audio sample of a different physical
*                   audio channel, taken at the same moment in time.
*
*               (6) An underrun situation occurs when the audio part is faster than the USB part.
*                   In that case, an audio sample will be added on each logical channel to
*                   re-synchronize the audio part so that this one can progressively relapsed on the USB
*                   rate. The audio sample insertion algorithm is the following:
*
*           Sample  ^                                   Sample  ^
*           level   |    ___                            level   |    ___
*                   |___|   |                                   |___|   |
*                   |   |   |___                                |   |   |___
*                   |   |   |   |___                  ====>     |   |   |   |___
*                   |   |   |   |   |                           |   |   |   |   |___
*                   |   |   |   |   |___                        |   |   |   |   |   |___
*                  _|___|___|___|___|___|_____>                _|___|___|___|___|___|___|__________>
*                   |N-4 N-3 N-2 N-1  N       t                 |N-4 N-3 N-2 N-1  N  N+1   t
*
*                       (a) Sample N is moved at N+1
*                       (b) Sample N is rebuilt and equal to the average of N-1 and N+1
*                       (c) The packet size is increased of one sample
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_PLAYBACK_EN      == DEF_ENABLED)
static  void  USBD_Audio_PlaybackCorrBuiltIn (USBD_AUDIO_AS_IF     *p_as_if,
                                              USBD_AUDIO_BUF_DESC  *p_buf_desc,
                                              USBD_ERR             *p_err)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_AS_IF_ALT       *p_as_if_alt;
    USBD_AUDIO_AS_ALT_CFG      *p_as_cfg;
    CPU_INT08U                 *p_subframe;
    CPU_INT08U                 *p_frame;
    CPU_INT08U                 *p_buf_end;
    CPU_INT08U                 *p_frame_n_p1;
    CPU_INT08U                 *p_frame_n;
    CPU_INT08U                 *p_frame_n_m1;
    CPU_INT08U                 *p_frame_n_m2;
    CPU_INT32S                  buf_diff;
    CPU_INT08U                  subframe_len;
    CPU_INT08U                  frame_len;
    CPU_INT16U                  buf_len_min;
    CPU_INT08U                  ch_ix;
    CPU_INT08U                  i;
    CPU_INT32S                  sample_val;
    CPU_INT64S                  sum;
    CPU_INT32S                  average;
    CPU_INT16U                  new_buf_len;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    buf_diff = USBD_Audio_BufDiffGet(p_as_if_settings);         /* Get cur buf diff between USB & codec.                */
    CPU_CRITICAL_EXIT();

                                                                /* If safe zone, no correction applies (see Note #2).   */
    if ((buf_diff > p_as_if_settings->CorrBoundaryHeavyNeg) &&
        (buf_diff < p_as_if_settings->CorrBoundaryHeavyPos)) {

       *p_err = USBD_ERR_NONE;                                  /* No need of stream correction.                        */
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_CorrNbrSafeZone);
        return;
    }

    p_as_if_alt  = p_as_if->AS_IF_AltCurPtr;                    /* Get active alternate setting for given AS IF.        */
    p_as_cfg     = p_as_if_alt->AS_CfgPtr;                      /* Get user AS IF cfg.                                  */
                                                                /* Compute audio frame size.                            */
    subframe_len = p_as_cfg->SubframeSize;
    frame_len    = p_as_cfg->NbrCh * subframe_len;
                                                                /* Check if enough samples in buf to apply corr.        */
    buf_len_min  = USBD_AUDIO_PLAYBACK_CORR_MIN_NBR_SAMPLES * frame_len;
    if (p_buf_desc->BufLen < buf_len_min) {
       *p_err = USBD_ERR_FAIL;
        return;
    }
                                                                /* -------------- OVERRUN: REMOVE SAMPLE -------------- */
    if (buf_diff >= p_as_if_settings->CorrBoundaryHeavyPos) {

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_CorrNbrOverrun);

        if (p_as_if_settings->CorrCallbackPtr != (USBD_AUDIO_PLAYBACK_CORR_FNCT)0) {
                                                                /* See Note #3.                                         */
            new_buf_len = p_as_if_settings->CorrCallbackPtr(p_as_cfg,
                                                            DEF_NO,
                                                            p_buf_desc->BufPtr,
                                                            p_buf_desc->BufLen,
                                                            p_as_if_settings->BufTotalLen,
                                                            p_err);

            p_buf_desc->BufLen = new_buf_len;                   /* Buf size has been reduced of X samples by app...     */
                                                                /* ...corr algorithm.                                   */

        } else {                                                /* See Note #4.                                         */
                                                                /* Get ptr to different audio frames within buf.        */
            p_buf_end    = ((CPU_INT08U *)p_buf_desc->BufPtr) + p_buf_desc->BufLen;
            p_frame_n    =   p_buf_end    - frame_len;
            p_frame_n_m1 =   p_frame_n    - frame_len;
            p_frame_n_m2 =   p_frame_n_m1 - frame_len;

            for (ch_ix = 0u; ch_ix < p_as_cfg->NbrCh; ch_ix++) {/* Iterate through every log ch.                        */

                p_frame = p_buf_end;                            /* Point to buf end.                                    */
                sum     = 0u;
                for (i = 0u; i < USBD_AUDIO_OVERRUN_NBR_SAMPLES_FOR_AVERAGING; i++) {

                    p_frame    -=  frame_len;                   /* Point to sample N-x (with x = 0, 1, 2, 3).           */
                    p_subframe  =  p_frame;
                    p_subframe += (subframe_len * ch_ix);       /* Within frame, point to subframe associated to a...   */
                                                                /* ...given log ch (see Note #5).                       */
                                                                /* Get sample value.                                    */
                    sample_val  =  0u;
                    MEM_VAL_COPY_GET_INTU(&sample_val, p_subframe, subframe_len);
                    if ((subframe_len == USBD_AUDIO_FMT_TYPE_I_SUBFRAME_SIZE_2) ||
                        (subframe_len == USBD_AUDIO_FMT_TYPE_I_SUBFRAME_SIZE_3))  {
                                                                /* Ensure proper signed integer representation by...    */
                                                                /* ...testing bit sign.                                 */
                        if (DEF_BIT_IS_SET(sample_val, DEF_BIT((p_as_cfg->BitRes - 1u))) == DEF_YES) {
                            sample_val |= DEF_BIT_FIELD_32((32u - p_as_cfg->BitRes), p_as_cfg->BitRes);
                        }
                    }
                    sum += sample_val;
                }
                                                                /* ...sample N-2 is rebuilt and equal to the average... */
                                                                /* ...  of N, N-1, N-2 and N-3.                         */
                average     = (CPU_INT32S)(sum / USBD_AUDIO_OVERRUN_NBR_SAMPLES_FOR_AVERAGING);
                p_subframe  =  p_frame_n_m2;
                p_subframe += (subframe_len * ch_ix);
                MEM_VAL_COPY(p_subframe, &average, subframe_len);
            }

            Mem_Copy(p_frame_n_m1, p_frame_n, frame_len);       /* Sample N is moved at N-1.                            */

            p_buf_desc->BufLen -= frame_len;                    /* The packet size is reduced by one sample.            */
        }

    } else {                                                    /* ------------- UNDERRUN: INSERT SAMPLE -------------- */

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_CorrNbrUnderrun);
                                                                /* Call app corr callback.                              */
        if (p_as_if_settings->CorrCallbackPtr != (USBD_AUDIO_PLAYBACK_CORR_FNCT)0) {
                                                                /* See Note #3.                                         */
            new_buf_len = p_as_if_settings->CorrCallbackPtr(p_as_cfg,
                                                            DEF_YES,
                                                            p_buf_desc->BufPtr,
                                                            p_buf_desc->BufLen,
                                                            p_as_if_settings->BufTotalLen,
                                                            p_err);

            p_buf_desc->BufLen = new_buf_len;                   /* Buf size has been increased of X samples by app...   */
                                                                /* ...corr algorithm.                                   */

        } else {                                                /* See Note #6.                                         */
                                                                /* Get ptr to different frames within playback buf.     */
            p_buf_end    = ((CPU_INT08U *)p_buf_desc->BufPtr) + p_buf_desc->BufLen;
            p_frame_n_p1 =   p_buf_end;
            p_frame_n    =   p_buf_end - frame_len;
            p_frame_n_m1 =   p_frame_n - frame_len;

            Mem_Copy(p_frame_n_p1, p_frame_n, frame_len);       /* Sample N is moved at N+1                             */

            for (ch_ix = 0u; ch_ix < p_as_cfg->NbrCh; ch_ix++) {/* Iterate through every log ch.                        */

                sum = 0u;
                for (i = 0u; i < USBD_AUDIO_UNDERRUN_NBR_SAMPLES_FOR_AVERAGING; i++) {

                    if (i == 0u) {                              /* Point to sample N+1.                                 */
                        p_frame = p_frame_n_p1;
                    } else {
                        p_frame = p_frame_n_m1;                 /* Point to sample N-1.                                 */
                    }
                    p_subframe  =  p_frame;
                    p_subframe += (subframe_len * ch_ix);       /* Within frame, point to subframe associated to a...   */
                                                                /* ...given log ch (see Note #5).                       */
                    sample_val  =  0u;
                    MEM_VAL_COPY_GET_INTU(&sample_val, p_subframe, subframe_len);
                    if ((subframe_len == USBD_AUDIO_FMT_TYPE_I_SUBFRAME_SIZE_2) ||
                        (subframe_len == USBD_AUDIO_FMT_TYPE_I_SUBFRAME_SIZE_3))  {
                                                                /* Ensure proper signed integer representation by...    */
                                                                /* ...testing bit sign.                                 */
                        if (DEF_BIT_IS_SET(sample_val, DEF_BIT((p_as_cfg->BitRes - 1u))) == DEF_YES) {
                            sample_val |= DEF_BIT_FIELD_32((32u - p_as_cfg->BitRes), p_as_cfg->BitRes);
                        }
                    }
                    sum += sample_val;
                }

                average     = (CPU_INT32S)(sum / USBD_AUDIO_UNDERRUN_NBR_SAMPLES_FOR_AVERAGING);
                p_subframe  =  p_frame_n;
                p_subframe += (subframe_len * ch_ix);
                                                                /* ...sample N is rebuilt and equal to the average of...*/
                                                                /* ...N-1 and N+1.                                      */
                MEM_VAL_COPY(p_subframe, &average, subframe_len);
            }

            p_buf_desc->BufLen += frame_len;                    /* The packet size is increased by one sample.          */
        }
    }

   *p_err = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                 USBD_Audio_PlaybackCorrSynchInit()
*
* Description : Initializes playback synch correction.
*
* Argument(s) : p_as_if         Pointer to the AudioStreaming interface.
*
*               sampling_freq   Value of the current sampling frequency of this streaming interface.
*
*               p_err           Pointer to the variable that will receive the return error code from this function:
*
*                       USBD_ERR_NONE               Synch (feedback) endpoint successfully processed.
*                       USBD_ERR_DEV_INVALID_NBR    Invalid dev_nbr passed to USBD_DevSpdGet().
*                       USBD_ERR_FAIL               USB driver failed to return the frame nbr.
*                       USBD_ERR_ALLOC              Codec driver API not found.
*
*                                                   ------- RETURNED BY USBD_IsocTxAsync() : -------
*                       USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                       USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in configured
*                                                   state.
*                       USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                       USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                       USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                                   See specific device driver(s) 'USBD_EP_Tx()' for
*                                                   additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
static  void  USBD_Audio_PlaybackCorrSynchInit (USBD_AUDIO_AS_IF  *p_as_if,
                                                CPU_INT32U         sampling_freq,
                                                USBD_ERR          *p_err)
{
    USBD_DEV_SPD                spd;
    CPU_INT08U                  bit_shift;
    CPU_INT08U                  xfer_len;
    CPU_INT32U                 *p_feedback_buf;
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;


    spd = USBD_DevSpdGet(p_as_if->DevNbr, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    p_as_if_settings = p_as_if->AS_IF_SettingsPtr;

    p_as_if_settings->PlaybackSynch.SynchFrameNbr = USBD_DevFrameNbrGet(p_as_if->DevNbr, p_err);
    if (*p_err != USBD_ERR_NONE) {
       *p_err = USBD_ERR_FAIL;
        return;
    }

    if (spd == USBD_DEV_SPD_FULL) {
                                                                /* Value formatted in FS 10.14 format.                  */
        bit_shift = USBD_AUDIO_PLAYBACK_FS_BIT_SHIFT;
        xfer_len  = USBD_AUDIO_FS_SYNCH_EP_XFER_LEN;
    } else {
        bit_shift = USBD_AUDIO_PLAYBACK_HS_BIT_SHIFT;
        xfer_len  = USBD_AUDIO_HS_SYNCH_EP_XFER_LEN;
    }

                                                                /* Init frame nbr for synch EP.                         */
    p_as_if_settings->PlaybackSynch.SynchFrameNbr       =   USBD_FRAME_NBR_GET(p_as_if_settings->PlaybackSynch.SynchFrameNbr);
    p_as_if_settings->PlaybackSynch.PrevFrameNbr        =   p_as_if_settings->PlaybackSynch.SynchFrameNbr;
                                                                /* Get integer part of freq.                            */
    p_as_if_settings->PlaybackSynch.FeedbackNominalVal  =  (sampling_freq / 1000u) << bit_shift;
                                                                /* Get fractional part of feedback val.                 */
    p_as_if_settings->PlaybackSynch.FeedbackNominalVal += ((sampling_freq % 1000u) << bit_shift) / 1000u;
    p_as_if_settings->PlaybackSynch.FeedbackCurVal      =   p_as_if_settings->PlaybackSynch.FeedbackNominalVal;
    p_as_if_settings->PlaybackSynch.PrevBufDiff         =   0u;
    p_as_if_settings->PlaybackSynch.FeedbackValUpdate   =   DEF_NO;

    p_feedback_buf = USBD_Audio_PlaybackSynchBufGet(p_as_if);   /* Get synch buf.                                       */
    if (p_feedback_buf == DEF_NULL) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrBufNotAvail);
       *p_err = USBD_ERR_ALLOC;
        USBD_DBG_AUDIO_PROC_ERR("PlaybackCorrSynchInit(): cannot get a feedback buffer w/ err = %d\r\n", *p_err);
        return;
    }

    MEM_VAL_SET_INT32U_LITTLE(p_feedback_buf, p_as_if_settings->PlaybackSynch.FeedbackNominalVal);

    USBD_IsocTxAsync(        p_as_if->DevNbr,                   /* Start new xfer with nominal feedback val.            */
                             p_as_if->AS_IF_AltCurPtr->SynchIsocAddr,
                     (void *)p_feedback_buf,
                             xfer_len,
                             USBD_Audio_PlaybackIsocSynchCmpl,
                     (void *)p_as_if,
                             p_err);
    if (*p_err != USBD_ERR_NONE) {
        USBD_Audio_PlaybackSynchBufFree(p_as_if, p_feedback_buf);
        USBD_DBG_AUDIO_PROC_ERR("PlaybackCorrSynchInit(): isoc tx xfer not started w/ err = %d\r\n", *p_err);
        return;
    }
    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrIsocTxSubmitted);
}
#endif


/*
*********************************************************************************************************
*                                USBD_Audio_IsocPlaybackSynchCmpl()
*
* Description : Playback Synch Out transfer completion.
*
* Argument(s) : dev_nbr     Device Number.
*
*               ep_addr     Endpoint Address.
*
*               p_buf       Pointer to the buffer containing data that was sent to the host.
*
*               buf_len     Buffer length (in bytes).
*
*               xfer_len    Number of bytes received (in bytes).
*
*               p_arg       Pointer to the AudioStreaming IF passed into argument to this function.
*
*               err         Error status.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
static  void  USBD_Audio_PlaybackIsocSynchCmpl (CPU_INT08U   dev_nbr,
                                                CPU_INT08U   ep_addr,
                                                void        *p_buf,
                                                CPU_INT32U   buf_len,
                                                CPU_INT32U   xfer_len,
                                                void        *p_arg,
                                                USBD_ERR     err)
{
    USBD_AUDIO_AS_IF  *p_as_if;


    (void)dev_nbr;
    (void)ep_addr;
    (void)buf_len;
    (void)xfer_len;
    (void)p_arg;
    (void)err;

    p_as_if = (USBD_AUDIO_AS_IF *)p_arg;

    USBD_Audio_PlaybackSynchBufFree(p_as_if, p_buf);
    USBD_AUDIO_STAT_INC(p_as_if->AS_IF_SettingsPtr->StatPtr->AudioProc_Playback_SynchNbrIsocTxCmpl);
}
#endif


/*
*********************************************************************************************************
*                                   USBD_Audio_PlaybackCorrSynch()
*
* Description : Performs feedback correction.
*
* Argument(s) : p_as_if         Pointer to the AudioStreaming interface.
*
*               frame_nbr       Current frame number.
*
*               p_err           Pointer to the variable that will receive the return error code from this function:
*
*                               USBD_ERR_NONE       Synch (feedback) correction successfully.
*
*                                                   ------- RETURNED BY USBD_IsocTxAsync() : -------
*                               USBD_ERR_DEV_INVALID_NBR    Invalid device number.
*                               USBD_ERR_DEV_INVALID_STATE  Transfer type only available if device is in
*                                                           configured state.
*                               USBD_ERR_EP_INVALID_ADDR    Invalid endpoint address.
*                               USBD_ERR_EP_INVALID_STATE   Invalid endpoint state.
*                               USBD_ERR_EP_INVALID_TYPE    Invalid endpoint type.
*
*                                                           See specific device driver(s) 'USBD_EP_Tx()' for
*                                                           additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) Ff is expressed in number of samples per (micro)frame. for full-speed endpoints, the
*                   Ff value shall be encoded in an unsigned 10.10 (K=10) format which fits into three
*                   bytes. Because the maximum integer value is fixed to 1,023, the 10.10 number will be
*                   left-justified in the 24 bits, so that it has a 10.14 format.
*                   See 'Universal Serial Bus Specification Revision 2.0, Release 2.0, April 27, 2000',
*                   Section 5.12.4.2 for more information about Feedback.
*
*               (2) The feedback processing is done by monitoring the difference between the buffers
*                   produced by the USB side and the buffers consumed by the codec side. The following
*                   figure illustrates the processing principle:
*
*   Speed           USB << Codec            USB < Codec                         USB = Codec                         USB > Codec             USB >> Codec
*   Adjustment      MAX_ADJ     MED_ADJ     MIN_ADJ/MAX_ADJ         -           -           -           -    MIN_ADJ/MAX_ADJ    MED_ADJ     MAX_ADJ
*   Adj (sample)    +1          +1/2        +1 > adj > +1/2048      -           -           -           -    -1/2048 > adj> -1  -1/2        -1
*
*                 <-|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-->
*   Buffer diff    -5          -4          -3          -2          -1           0           1           2           3           4           5
*                   |_______________________|_______________________________________________________________________|_______________________|
*                   |       UNDERRUN        |                               SAFE ZONE                               |        OVERRUN        |
*                 heavy                   light                                                                   light                   heavy
*
*                   The underrun situation occurs when the USB side is slower than the codec side. In
*                   that case, depending how fast is the codec, the underrun situation could be light
*                   or heavy. The processing will adjust the feedback value by telling the host to
*                   add up to one sample per frame depending of the underrun degree.
*                   The overun situation occurs when the USB side is faster than the codec side. In
*                   that case, depending how slow is the codec, the overrun situation could be light
*                   or heavy. The processing will adjust the feedback value by telling the host to
*                   remove up to one sample per frame depending of the overrun degree.
*
*               (3) When coming from the safe zone, light underrun or overrun is corrected with a
*                   feedback value taking into account the variation of buffers during a certain
*                   number of elapsed frames. This allows to correct smoothly the stream deviation.
*                   The feedback value adjustment is between:
*
*                   MINIMIM ADJUSTMENT < adjustment < MAXIMUM ADJUSTMENT
*                   +1/2048 sample     < adjustment < +1      sample (underrun situation)
*                   -1      sample     < adjustment < -1/2048 sample (overrun situation)
*
*               (4) Practical tests with Windows has shown that Windows occasionally does NOT respect the
*                   bRefresh interval. Given that the feedback value uses one unique synch buffer, if a
*                   new feedback value is ready for the host, it may happen that the single buffer is not
*                   yet available because the host has not yet retrieved the current feedback value waiting
*                   in the USB driver. In that case, the new feedback value is lost and another feedback
*                   value may be sent to the host at the next bRefresh interval.
*
*               (5) The bRefresh field of the synch standard endpoint descriptor is used to report the
*                   exponent (10-P) to the Host. It can range from 9 down to 1. (512 ms down to 2 ms)
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   Section 3.7.2.2 for more details about Isochronous Synch Endpoint.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
static  void  USBD_Audio_PlaybackCorrSynch (USBD_AUDIO_AS_IF  *p_as_if,
                                            CPU_INT16U         frame_nbr,
                                            USBD_ERR          *p_err)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    CPU_INT16U                  frame_nbr_diff;
    USBD_AUDIO_AS_IF_ALT       *p_as_if_alt;
    CPU_INT32U                 *p_feedback_buf;
    CPU_INT32S                  buf_diff;
    CPU_INT32S                  prev_buf_diff;
    CPU_INT32U                  feedback_val_adj;
    CPU_INT16U                  prev_frame_nbr;
    CPU_INT16U                  frame_synch_refresh;
    CPU_BOOLEAN                 save_info;
    USBD_DEV_SPD                spd;
    CPU_INT08U                  feedback_val_bit_shift;
    CPU_INT08U                  feedback_len;
    CPU_SR_ALLOC();


   *p_err = USBD_ERR_NONE;

    p_as_if_settings = p_as_if->AS_IF_SettingsPtr;
    p_as_if_alt      = p_as_if->AS_IF_AltCurPtr;


    CPU_CRITICAL_ENTER();
    buf_diff = USBD_Audio_BufDiffGet(p_as_if_settings);         /* Get cur buf diff between USB & codec.                */
    CPU_CRITICAL_EXIT();
    prev_buf_diff  = p_as_if_settings->PlaybackSynch.PrevBufDiff;
    prev_frame_nbr = p_as_if_settings->PlaybackSynch.PrevFrameNbr;
    frame_nbr_diff = USBD_FRAME_NBR_DIFF_GET(prev_frame_nbr, frame_nbr);
    save_info      = DEF_NO;

    spd = USBD_DevSpdGet(p_as_if->DevNbr, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }

    if (spd == USBD_DEV_SPD_FULL) {                             /* See Note 1.                                          */
                                                                /* Value formatted in FS 10.14 format.                  */
        feedback_val_bit_shift = USBD_AUDIO_PLAYBACK_FS_BIT_SHIFT;
        feedback_len           = USBD_AUDIO_FS_SYNCH_EP_XFER_LEN;
    } else {
        feedback_val_bit_shift = USBD_AUDIO_PLAYBACK_HS_BIT_SHIFT;
        feedback_len           = USBD_AUDIO_HS_SYNCH_EP_XFER_LEN;
    }
                                                                /* Processing to monitor feedback needs (see Note #2).  */
    if (buf_diff == 0) {                                        /* No diff between received and consumed nb of buffs.   */
        save_info = DEF_YES;
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrSafeZone);

        if (prev_buf_diff != 0) {                               /* Coming from underrun or overrun.                     */
                                                                /* Restore feedback val to its nominal val.             */
            p_as_if_settings->PlaybackSynch.FeedbackValUpdate = DEF_YES;
            p_as_if_settings->PlaybackSynch.FeedbackCurVal    = p_as_if_settings->PlaybackSynch.FeedbackNominalVal;
        }
    } else if (buf_diff >= p_as_if_settings->CorrBoundaryHeavyPos) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrOverrun);
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrHeavyOverrun);

                                                                /* Heavy overrun. Do nothing if not the first time.     */
        if (prev_buf_diff < p_as_if_settings->CorrBoundaryHeavyPos) {
                                                                /* First time in heavy overrun.                         */
            save_info = DEF_YES;

            p_as_if_settings->PlaybackSynch.FeedbackValUpdate = DEF_YES;
                                                                /* Apply max adj to reduce nbr of samples rx'd.         */
            p_as_if_settings->PlaybackSynch.FeedbackCurVal   = p_as_if_settings->PlaybackSynch.FeedbackNominalVal -
                                                               USBD_AUDIO_PLAYBACK_SYNCH_MAX_ADJ(feedback_val_bit_shift);
        }
    } else if (buf_diff <= p_as_if_settings->CorrBoundaryHeavyNeg) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrUnderrun);
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrHeavyUnderrun);

                                                                /* Heavy underrun. Do nothing if not the first time.    */
        if (prev_buf_diff > p_as_if_settings->CorrBoundaryHeavyNeg) {
                                                                /* First time in heavy underrun.                        */
                                                                /* Apply max adj to increase nbr of samples rx'd.       */
            save_info = DEF_YES;

            p_as_if_settings->PlaybackSynch.FeedbackValUpdate = DEF_YES;
            p_as_if_settings->PlaybackSynch.FeedbackCurVal    = p_as_if_settings->PlaybackSynch.FeedbackNominalVal +
                                                                USBD_AUDIO_PLAYBACK_SYNCH_MAX_ADJ(feedback_val_bit_shift);
        }
    } else if (buf_diff >= p_as_if_settings->PlaybackSynch.SynchBoundaryLightPos) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrOverrun);
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrLightOverrun);

                                                                /* Light overrun. Do nothing if not the first time.     */
        if (prev_buf_diff >= p_as_if_settings->CorrBoundaryHeavyPos) {
                                                                /* First time in light overrun. Coming from heavy ...   */
                                                                /* ... overrun, reduce adj to avoid overshoot.          */
            save_info = DEF_YES;

            p_as_if_settings->PlaybackSynch.FeedbackValUpdate = DEF_YES;
            p_as_if_settings->PlaybackSynch.FeedbackCurVal    = p_as_if_settings->PlaybackSynch.FeedbackNominalVal -
                                                                USBD_AUDIO_PLAYBACK_SYNCH_MED_ADJ(feedback_val_bit_shift);
        } else if (prev_buf_diff < p_as_if_settings->PlaybackSynch.SynchBoundaryLightPos) {
                                                                /* First time in light overrun. Was 'OK' before.        */
            save_info = DEF_YES;

            p_as_if_settings->PlaybackSynch.FeedbackValUpdate = DEF_YES;
                                                                /* Calculate feedback val to send (see Note 3).         */
            feedback_val_adj = (p_as_if_settings->PlaybackSynch.SynchBoundaryLightPos << feedback_val_bit_shift) / frame_nbr_diff;
            feedback_val_adj = (feedback_val_adj < USBD_AUDIO_PLAYBACK_SYNCH_MIN_ADJ(feedback_val_bit_shift)) ?
                                                   USBD_AUDIO_PLAYBACK_SYNCH_MIN_ADJ(feedback_val_bit_shift) : feedback_val_adj;
            feedback_val_adj = (feedback_val_adj > USBD_AUDIO_PLAYBACK_SYNCH_MAX_ADJ(feedback_val_bit_shift)) ?
                                                   USBD_AUDIO_PLAYBACK_SYNCH_MAX_ADJ(feedback_val_bit_shift) : feedback_val_adj;
            p_as_if_settings->PlaybackSynch.FeedbackCurVal = p_as_if_settings->PlaybackSynch.FeedbackCurVal - feedback_val_adj;
        }
    } else if (buf_diff <= p_as_if_settings->PlaybackSynch.SynchBoundaryLightNeg) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrUnderrun);
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrLightUnderrun);
                                                                /* Light underrun. Do nothing if not the first time.    */
        if (prev_buf_diff <= p_as_if_settings->CorrBoundaryHeavyNeg) {
                                                                /* First time in light underrun. Coming from heavy ...  */
                                                                /* ... underrun, reduce adj to avoid overshoot.         */
            save_info = DEF_YES;

            p_as_if_settings->PlaybackSynch.FeedbackValUpdate = DEF_YES;
            p_as_if_settings->PlaybackSynch.FeedbackCurVal    = p_as_if_settings->PlaybackSynch.FeedbackNominalVal +
                                                                USBD_AUDIO_PLAYBACK_SYNCH_MED_ADJ(feedback_val_bit_shift);

        } else if (prev_buf_diff > p_as_if_settings->PlaybackSynch.SynchBoundaryLightNeg) {
                                                                /* First time in light underrun. Was 'OK' before.       */
            save_info = DEF_YES;


            p_as_if_settings->PlaybackSynch.FeedbackValUpdate = DEF_YES;
                                                                /* Calculate feedback val to send (see Note 3).         */
            feedback_val_adj = (p_as_if_settings->PlaybackSynch.SynchBoundaryLightPos << feedback_val_bit_shift) / frame_nbr_diff;
            feedback_val_adj = (feedback_val_adj < USBD_AUDIO_PLAYBACK_SYNCH_MIN_ADJ(feedback_val_bit_shift)) ?
                                                   USBD_AUDIO_PLAYBACK_SYNCH_MIN_ADJ(feedback_val_bit_shift) : feedback_val_adj;
            feedback_val_adj = (feedback_val_adj > USBD_AUDIO_PLAYBACK_SYNCH_MAX_ADJ(feedback_val_bit_shift)) ?
                                                   USBD_AUDIO_PLAYBACK_SYNCH_MAX_ADJ(feedback_val_bit_shift) : feedback_val_adj;
            p_as_if_settings->PlaybackSynch.FeedbackCurVal = p_as_if_settings->PlaybackSynch.FeedbackCurVal + feedback_val_adj;
        }
    }

    if (save_info == DEF_YES) {
        p_as_if_settings->PlaybackSynch.PrevBufDiff  = buf_diff;
        p_as_if_settings->PlaybackSynch.PrevFrameNbr = frame_nbr;
    }

    frame_synch_refresh = (1u << (p_as_if_alt->AS_CfgPtr->EP_SynchRefresh));
    frame_nbr_diff      =  USBD_FRAME_NBR_DIFF_GET(p_as_if_settings->PlaybackSynch.SynchFrameNbr, frame_nbr);

    if (frame_nbr_diff >= frame_synch_refresh) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrRefreshPeriodReached);
        p_as_if_settings->PlaybackSynch.SynchFrameNbr = frame_nbr; /* Update the frame nbr.                             */

                                                                /* Check if a transmit is required.                     */
        if (p_as_if_settings->PlaybackSynch.FeedbackValUpdate == DEF_YES) {
            p_as_if_settings->PlaybackSynch.FeedbackValUpdate = DEF_NO;

            p_feedback_buf = USBD_Audio_PlaybackSynchBufGet(p_as_if);
            if (p_feedback_buf == DEF_NULL) {                   /* See Note 4.                                          */
                USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrBufNotAvail);
                return;
            }
                                                                /* See Note #5                                          */
            MEM_VAL_SET_INT32U_LITTLE(p_feedback_buf, p_as_if_settings->PlaybackSynch.FeedbackCurVal);

            USBD_IsocTxAsync(        p_as_if->DevNbr,           /* Start new xfer with new feedback val.                */
                                     p_as_if_alt->SynchIsocAddr,
                             (void *)p_feedback_buf,
                                     feedback_len,
                                     USBD_Audio_PlaybackIsocSynchCmpl,
                             (void *)p_as_if,
                                     p_err);
            if (*p_err != USBD_ERR_NONE) {
                USBD_Audio_PlaybackSynchBufFree(p_as_if, p_feedback_buf);
                USBD_DBG_AUDIO_PROC_ERR("ProcPlaybackCorrExec(): isoc tx xfer not started w/ err = %d\r\n", *p_err);
                return;
            }
            USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrIsocTxSubmitted);
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                       USBD_Audio_SynchBufGet()
*
* Description : Get a buffer for the synch endpoint to use.
*
* Argument(s) : p_as_if     Pointer to the audio stream interface.
*
* Return(s)   : Pointer to a synch buffer, if NO error(s).
*
*               Null pointer,              otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
static  CPU_INT32U  *USBD_Audio_PlaybackSynchBufGet (USBD_AUDIO_AS_IF  *p_as_if)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    CPU_INT32U                 *p_buf;
    CPU_SR_ALLOC();


    p_as_if_settings = p_as_if->AS_IF_SettingsPtr;

    CPU_CRITICAL_ENTER();
    if (p_as_if_settings->PlaybackSynch.SynchBufFree == DEF_NO) {
        CPU_CRITICAL_EXIT();
        return (DEF_NULL);
    }

    p_as_if_settings->PlaybackSynch.SynchBufFree = DEF_NO;
    CPU_CRITICAL_EXIT();

    p_buf = p_as_if_settings->PlaybackSynch.SynchBufPtr;

    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrBufGet);

    return (p_buf);
}
#endif


/*
*********************************************************************************************************
*                                       USBD_Audio_SynchBufFree()
*
* Description : Return a synch buffer to the free list.
*
* Argument(s) : p_as_if     Pointer to the audio stream interface.
*
*               p_buf       Pointer to buffer to free.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
static  void  USBD_Audio_PlaybackSynchBufFree (USBD_AUDIO_AS_IF  *p_as_if,
                                               CPU_INT32U        *p_buf)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    CPU_SR_ALLOC();


    (void)p_buf;

    p_as_if_settings = p_as_if->AS_IF_SettingsPtr;

    CPU_CRITICAL_ENTER();
    p_as_if_settings->PlaybackSynch.SynchBufFree = DEF_YES;
    CPU_CRITICAL_EXIT();

    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_Playback_SynchNbrBufFree);
}
#endif


/*
*********************************************************************************************************
*                                 USBD_Audio_Terminal_CopyProtManage()
*
* Description : Process Copy Protect Control of a particular Terminal (see note #1).
*
* Argument(s) : terminal_id         Terminal id number.
*
*               recipient           Terminal Type.
*
*               p_recipient_info    Pointer to terminal info structure casted into (void *).
*
*               b_req               Received request type (SET_XXX or GET_XXX).
*
*               p_buf               Pointer to copy protection level attribute.
*
*               p_err           Pointer to the variable that will receive the return error code from this function:
*
*                               USBD_ERR_NONE                           Control successfully processed.
*                               USBD_ERR_AUDIO_REQ_INVALID_ATTRIB       Invalid request type or invalid copy
*                                                                       protection level attribute.
*                               USBD_ERR_AUDIO_REQ_FAIL                 Audio driver failed to process the request.
*                               USBD_ERR_AUDIO_REQ_INVALID_RECIPIENT    Invalid terminal recipient.
*
* Return(s)   : None.
*
* Note(s)     : (1) The Copy Protect Control is used to manipulate the Copy Protection Level (CPL)
*                   associated with a particular Terminal. Not all Terminals are required to support
*                   this Control. Only the Terminals that represent a connection to the audio function
*                   where audio streams enter or leave the audio function in a form that enables lossless
*                   duplication should consider Copy Protect Control.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.1.3.1 for more details about Copy Protect Control.
*********************************************************************************************************
*/

static  void  USBD_Audio_Terminal_CopyProtManage (       CPU_INT08U               terminal_id,
                                                         USBD_AUDIO_ENTITY_TYPE   recipient,
                                                  const  void                    *p_recipient_info,
                                                         CPU_INT08U               b_req,
                                                         CPU_INT08U              *p_buf,
                                                         USBD_ERR                *p_err)
{
    USBD_AUDIO_IT  *p_it;
    USBD_AUDIO_OT  *p_ot;
    CPU_INT08U      copy_prot_level;
    CPU_BOOLEAN     valid;


    switch (recipient) {
        case USBD_AUDIO_ENTITY_IT:
             switch (b_req) {
                 case USBD_AUDIO_REQ_GET_CUR:
                      p_it = (USBD_AUDIO_IT *)p_recipient_info;

                      if (terminal_id != p_it->ID) {            /* Check if ID is known.                                */
                         *p_err = USBD_ERR_AUDIO_REQ;
                          break;
                      }

                      if (p_it->IT_CfgPtr->CopyProtEn == DEF_ENABLED) {
                         *p_buf = p_it->IT_CfgPtr->CopyProtLevel;
                         *p_err = USBD_ERR_NONE;
                      } else {
                         *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;
                      }
                      break;


                 case USBD_AUDIO_REQ_GET_MIN:
                 case USBD_AUDIO_REQ_GET_MAX:
                 case USBD_AUDIO_REQ_GET_RES:
                 case USBD_AUDIO_REQ_SET_CUR:
                 case USBD_AUDIO_REQ_SET_MIN:
                 case USBD_AUDIO_REQ_SET_MAX:
                 case USBD_AUDIO_REQ_SET_RES:
                 default:
                     *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;
                      break;
             }
             break;

        case USBD_AUDIO_ENTITY_OT:
             switch (b_req) {
                 case USBD_AUDIO_REQ_SET_CUR:
                      p_ot = (USBD_AUDIO_OT *)p_recipient_info;

                      if (terminal_id != p_ot->ID) {            /* Check if ID is known.                                */
                         *p_err = USBD_ERR_AUDIO_REQ;
                          break;
                      }

                      if ((p_ot->OT_CfgPtr->CopyProtEn      == DEF_ENABLED) &&
                          (p_ot->OT_API_Ptr->OT_CopyProtSet != DEF_NULL)) {

                          copy_prot_level = *p_buf;

                          valid = p_ot->OT_API_Ptr->OT_CopyProtSet(p_ot->DrvInfoPtr,
                                                                   terminal_id,
                                                                   copy_prot_level);
                          if (valid == DEF_OK) {
                             *p_err = USBD_ERR_NONE;
                          } else {
                             *p_err = USBD_ERR_AUDIO_REQ;
                          }
                      } else {
                         *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;
                      }
                      break;


                 case USBD_AUDIO_REQ_GET_CUR:
                 case USBD_AUDIO_REQ_GET_MIN:
                 case USBD_AUDIO_REQ_GET_MAX:
                 case USBD_AUDIO_REQ_GET_RES:
                 case USBD_AUDIO_REQ_SET_MIN:
                 case USBD_AUDIO_REQ_SET_MAX:
                 case USBD_AUDIO_REQ_SET_RES:
                 default:
                     *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;
                      break;
             }
             break;

        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_RECIPIENT;
             break;
    }
}


/*
*********************************************************************************************************
*                                      USBD_Audio_FU_MuteManage()
*
* Description : Mute or unmute one logical or several channels inside a cluster (see note #1).
*
* Argument(s) : p_fu        Pointer to Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical Channel Number to mute or unmute.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer containing the mute state.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) A Mute Control can have only the current setting attribute (CUR). The position of a
*                   Mute Control CUR attribute can be either TRUE or FALSE.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.1 for more details about Mute Control.
*********************************************************************************************************
*/

static  void  USBD_Audio_FU_MuteManage (const  USBD_AUDIO_FU  *p_fu,
                                               CPU_INT08U      unit_id,
                                               CPU_INT08U      log_ch_nbr,
                                               CPU_INT08U      b_req,
                                               CPU_INT08U     *p_buf,
                                               USBD_ERR       *p_err)
{
    CPU_BOOLEAN  mute;
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
             if (p_fu->FU_API_Ptr->FU_MuteManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_fu->FU_API_Ptr->FU_MuteManage(p_fu->DrvInfoPtr,
                                                     unit_id,
                                                     log_ch_nbr,
                                                     DEF_FALSE,
                                                    &mute);
             if (valid == DEF_OK) {
                 p_buf[0u] = (mute == DEF_OFF) ? 0u : 1u;
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_MuteManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             mute  = (p_buf[0u] == 0u) ? DEF_OFF : DEF_ON;

             valid = p_fu->FU_API_Ptr->FU_MuteManage(p_fu->DrvInfoPtr,
                                                     unit_id,
                                                     log_ch_nbr,
                                                     DEF_TRUE,
                                                    &mute);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Mute Ctrl.                */
             break;
    }
}


/*
*********************************************************************************************************
*                                       USBD_Audio_FU_VolManage()
*
* Description : Handle volume control for one or several logical channels inside a cluster (see note #1).
*
* Argument(s) : p_fu        Pointer to Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical channel Number to apply volume control.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer containing the volume.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) A Volume Control can support all possible Control attributes (CUR, MIN, MAX, and
*                   RES). The settings for the CUR, MIN, and MAX attributes can range from +127.9961 dB
*                   (0x7FFF) down to -127.9961 dB (0x8001) in steps of 1/256 dB or 0.00390625 dB
*                   (0x0001). The range for the CUR attribute is extended by code 0x8000, representing
*                   silence, i.e., -infinit dB. The settings for the RES attribute can only take positive
*                   values and range from 1/256 dB (0x0001) to +127.9961 dB (0x7FFF).
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.2 for more details about Volume Control.
*
*               (2) The audio 1.0 indicates: "The MIN, MAX, and RES attributes are usually not supported
*                   for the Set request" (section 5.2.2.4.1). Thus SET_MIN/MAX/RES are rejected.
*********************************************************************************************************
*/

static  void  USBD_Audio_FU_VolManage (const  USBD_AUDIO_FU  *p_fu,
                                              CPU_INT08U      unit_id,
                                              CPU_INT08U      log_ch_nbr,
                                              CPU_INT08U      b_req,
                                              CPU_INT08U     *p_buf,
                                              USBD_ERR       *p_err)
{
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_VolManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_fu->FU_API_Ptr->FU_VolManage(              p_fu->DrvInfoPtr,
                                                                  b_req,
                                                                  unit_id,
                                                                  log_ch_nbr,
                                                    (CPU_INT16U *)p_buf);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_MIN:                            /* See Note #2.                                         */
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Vol Ctrl.                 */
             break;
    }
}


/*
*********************************************************************************************************
*                                      USBD_Audio_FU_BassManage()
*
* Description : Handle bass control for one or several logical channels inside a cluster (see note #1).
*
* Argument(s) : p_fu        Pointer to the Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical Channel Number to apply bass control.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer containing the bass attribute.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) The Bass Control influences the general Bass behavior of the Feature Unit. A Bass
*                   Control can support all possible Control attributes (CUR, MIN, MAX, and RES). The
*                   settings for the CUR, MIN, and MAX attributes can range from +31.75 dB (0x7F) down
*                   to    32.00 dB (0x80) in steps of 0.25 dB (0x01). The settings for the RES attribute
*                   can only take positive values and range from 0.25 dB (0x01) to +31.75 dB (0x7F).
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.3 for more details about Bass Control.
*
*               (2) The audio 1.0 indicates: "The MIN, MAX, and RES attributes are usually not supported
*                   for the Set request" (section 5.2.2.4.1). Thus SET_MIN/MAX/RES are rejected.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
static  void  USBD_Audio_FU_BassManage (const  USBD_AUDIO_FU  *p_fu,
                                               CPU_INT08U      unit_id,
                                               CPU_INT08U      log_ch_nbr,
                                               CPU_INT08U      b_req,
                                               CPU_INT08U     *p_buf,
                                               USBD_ERR       *p_err)
{
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_BassManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_fu->FU_API_Ptr->FU_BassManage(p_fu->DrvInfoPtr,
                                                     b_req,
                                                     unit_id,
                                                     log_ch_nbr,
                                                     p_buf);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_MIN:                            /* See Note #2.                                         */
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Bass Ctrl.                */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                       USBD_Audio_FU_MidManage()
*
* Description : Handle middle control for one or several logical channels inside a cluster (see note #1).
*
* Argument(s) : p_fu        Pointer to the Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical Channel Number to apply mid control.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer containing the mid band attribute.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) The Mid Control influences the general Mid behavior of the Feature Unit. A Mid
*                   Control can support all possible Control attributes (CUR, MIN, MAX, and RES). The
*                   settings for the CUR, MIN, and MAX attributes can range from +31.75 dB (0x7F) down
*                   to    32.00 dB (0x80) in steps of 0.25 dB (0x01). The settings for the RES attribute
*                   can only take positive values and range from 0.25 dB (0x01) to +31.75 dB (0x7F).
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.4 for more details about Mid Control.
*
*               (2) The audio 1.0 indicates: "The MIN, MAX, and RES attributes are usually not supported
*                   for the Set request" (section 5.2.2.4.1). Thus SET_MIN/MAX/RES are rejected.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
static  void  USBD_Audio_FU_MidManage (const  USBD_AUDIO_FU  *p_fu,
                                              CPU_INT08U      unit_id,
                                              CPU_INT08U      log_ch_nbr,
                                              CPU_INT08U      b_req,
                                              CPU_INT08U     *p_buf,
                                              USBD_ERR       *p_err)
{
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_MidManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_fu->FU_API_Ptr->FU_MidManage(p_fu->DrvInfoPtr,
                                                    b_req,
                                                    unit_id,
                                                    log_ch_nbr,
                                                    p_buf);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_MIN:                            /* See Note #2.                                         */
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Mid Ctrl.                 */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_FU_TrebleManage()
*
* Description : Handle treble control for one or several logical channels inside a cluster (see note #1).
*
* Argument(s) : p_fu        Pointer to the Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical Channel Number to apply treble control.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer containing the treble attribute.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) The Treble Control influences the general Treble behavior of the Feature Unit. A
*                   Treble Control can support all possible Control attributes (CUR, MIN, MAX, and RES).
*                   The settings for the CUR, MIN, and MAX attributes can range from +31.75 dB (0x7F)
*                   down to    32.00 dB (0x80) in steps of 0.25 dB (0x01). The settings for the RES
*                   attribute can only take positive values and range from 0.25 dB (0x01) to +31.75 dB
*                   (0x7F).
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.5 for more details about Treble Control.
*
*               (2) The audio 1.0 indicates: "The MIN, MAX, and RES attributes are usually not supported
*                   for the Set request" (section 5.2.2.4.1). Thus SET_MIN/MAX/RES are rejected.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
static  void  USBD_Audio_FU_TrebleManage (const  USBD_AUDIO_FU  *p_fu,
                                                 CPU_INT08U      unit_id,
                                                 CPU_INT08U      log_ch_nbr,
                                                 CPU_INT08U      b_req,
                                                 CPU_INT08U     *p_buf,
                                                 USBD_ERR       *p_err)
{
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_TrebleManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_fu->FU_API_Ptr->FU_TrebleManage(p_fu->DrvInfoPtr,
                                                       b_req,
                                                       unit_id,
                                                       log_ch_nbr,
                                                       p_buf);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_MIN:                            /* See Note #2.                                         */
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Treble Ctrl.              */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                USBD_Audio_FU_GraphicEqualizerManage()
*
* Description : Handle graphic analyzer control for one or several logical channels inside a cluster
*               (see note #1).
*
* Argument(s) : p_fu        Pointer to the Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical Channel Number to apply graphic analyzer control.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer containing the graphic equalizer attribute.
*
*               req_len     Length of the buffer in octets.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) The Audio Device Class definition provides for standard support of a third octave
*                   graphic equalizer. The bands are defined according to the ANSI S1.11-1986 standard.
*                   Bands are numbered from 14 (center frequency of 25 Hz) up to 43 (center frequency of
*                   20,000 Hz), making a total of 30 possible bands.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.6 for more details about Graphic Equalizer Control.
*
*               (2) 'req_len' is the value of wLength from the SETUP packet. It is equal to
*                   4 + (number of bits set in bmBandsPresent : NrBits).
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   Table 5-27 for more details.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
static  void  USBD_Audio_FU_GraphicEqualizerManage (const  USBD_AUDIO_FU  *p_fu,
                                                           CPU_INT08U      unit_id,
                                                           CPU_INT08U      log_ch_nbr,
                                                           CPU_INT08U      b_req,
                                                           CPU_INT08U     *p_buf,
                                                           CPU_INT16U      req_len,
                                                           USBD_ERR       *p_err)
{
    CPU_BOOLEAN  valid;
    CPU_INT08U   nbr_bands_present;
    CPU_INT32U   bm_bands_present;
    CPU_INT08U   nbr_bits_set;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
             if (p_fu->FU_API_Ptr->FU_GraphicEqualizerManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             bm_bands_present = 0u;

             valid = p_fu->FU_API_Ptr->FU_GraphicEqualizerManage(p_fu->DrvInfoPtr,
                                                                 b_req,
                                                                 unit_id,
                                                                 log_ch_nbr,
                                                                 0u,
                                                                &bm_bands_present,
                                                                (p_buf + 4u));   /* bBand fields start after bmBandsPresent.*/
             if (valid == DEF_OK) {
                 MEM_VAL_SET_INT32U_LITTLE(&p_buf[0u], bm_bands_present);
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_GraphicEqualizerManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             nbr_bands_present = (CPU_INT08U)req_len - 4u;      /* See Note #2.                                         */
             if (nbr_bands_present < 1u) {                      /* At least one frequency band must be specified.       */
                *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;
                 return;
             }

             bm_bands_present = (*(CPU_INT32U *)p_buf);         /* Get bitmap of bands effectively implemented.         */

                                                                /* Verify if nbr of bits set in 'bmBandsPresent'...     */
                                                                /* ...matches nbr of param specified in Param Block.    */
             USBD_AUDIO_BIT_SET_CNT(nbr_bits_set, bm_bands_present);
             if (nbr_bits_set != nbr_bands_present) {
                *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;
                 return;
             }

             valid = p_fu->FU_API_Ptr->FU_GraphicEqualizerManage(p_fu->DrvInfoPtr,
                                                                 b_req,
                                                                 unit_id,
                                                                 log_ch_nbr,
                                                                 nbr_bands_present,
                                                                &bm_bands_present,
                                                                (p_buf + 4u));
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Graphic Equalizer Ctrl.   */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_FU_AutoGainManage()
*
* Description : Enable or disable auto gain for one or several logical channels inside a cluster
*               (see note #1).
*
* Argument(s) : p_fu        Pointer to the Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical Channel Number to enable or disable auto gain.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer containing the autogain state.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) An Automatic Gain Control can have only the current setting attribute (CUR). The
*                   position of an Automatic Gain Control CUR attribute can be either TRUE or FALSE.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.7 for more details about Automatic Gain Control.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
static  void  USBD_Audio_FU_AutoGainManage (const  USBD_AUDIO_FU  *p_fu,
                                                   CPU_INT08U      unit_id,
                                                   CPU_INT08U      log_ch_nbr,
                                                   CPU_INT08U      b_req,
                                                   CPU_INT08U     *p_buf,
                                                   USBD_ERR       *p_err)
{
    CPU_BOOLEAN  auto_gain;
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
             if (p_fu->FU_API_Ptr->FU_AutoGainManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_fu->FU_API_Ptr->FU_AutoGainManage(p_fu->DrvInfoPtr,
                                                         unit_id,
                                                         log_ch_nbr,
                                                         DEF_FALSE,
                                                        &auto_gain);
             if (valid == DEF_OK) {
                 p_buf[0u] = (auto_gain == DEF_OFF) ? 0u : 1u;
                *p_err     =  USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_AutoGainManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             auto_gain = (p_buf[0u] == 0u) ? DEF_OFF : DEF_ON;

             valid = p_fu->FU_API_Ptr->FU_AutoGainManage(p_fu->DrvInfoPtr,
                                                         unit_id,
                                                         log_ch_nbr,
                                                         DEF_TRUE,
                                                        &auto_gain);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Auto Gain Ctrl.           */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                       USBD_Audio_FU_DlyManage()
*
* Description : Handle delay control for one or several logical channels inside a cluster (see note #1).
*
* Argument(s) : p_fu        Pointer to Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical Channel Number to apply delay control.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer that contains the delay.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) A Delay Control can support all possible Control attributes (CUR, MIN, MAX, and RES).
*                   The settings for the CUR, MIN, MAX, and RES attributes can range from zero (0x0000)
*                   to 1023.9844ms (0xFFFF) in steps of 1/64 ms or 0.015625 ms (0x0001).
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.8 for more details about Delay Control.
*
*               (2) The audio 1.0 indicates: "The MIN, MAX, and RES attributes are usually not supported
*                   for the Set request" (section 5.2.2.4.1). Thus SET_MIN/MAX/RES are rejected.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
static  void  USBD_Audio_FU_DlyManage (const  USBD_AUDIO_FU  *p_fu,
                                              CPU_INT08U      unit_id,
                                              CPU_INT08U      log_ch_nbr,
                                              CPU_INT08U      b_req,
                                              CPU_INT08U     *p_buf,
                                              USBD_ERR       *p_err)
{
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_DlyManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_fu->FU_API_Ptr->FU_DlyManage(              p_fu->DrvInfoPtr,
                                                                  b_req,
                                                                  unit_id,
                                                                  log_ch_nbr,
                                                    (CPU_INT16U *)p_buf);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_MIN:                            /* See Note #2.                                         */
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Dly Ctrl.                 */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_FU_BassBoostManage()
*
* Description : Enable or disable bass boost for one or several logical channels inside a cluster
*               (see note #1).
*
* Argument(s) : p_fu        Pointer to Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical Channel Number to enable or disable bass boost.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer that contains the bass boost state.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) A Bass Boost Control can have only the current setting attribute (CUR). The position
*                   of a Bass Boost Control CUR attribute can be either TRUE or FALSE.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.9 for more details about Bass Boost Control.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
static  void  USBD_Audio_FU_BassBoostManage (const  USBD_AUDIO_FU  *p_fu,
                                                    CPU_INT08U      unit_id,
                                                    CPU_INT08U      log_ch_nbr,
                                                    CPU_INT08U      b_req,
                                                    CPU_INT08U     *p_buf,
                                                    USBD_ERR       *p_err)
{
    CPU_BOOLEAN  bass_boost;
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
             if (p_fu->FU_API_Ptr->FU_BassBoostManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_fu->FU_API_Ptr->FU_BassBoostManage(p_fu->DrvInfoPtr,
                                                          unit_id,
                                                          log_ch_nbr,
                                                          DEF_FALSE,
                                                         &bass_boost);
             if (valid == DEF_OK) {
                 p_buf[0u] = (bass_boost == DEF_OFF) ? 0u : 1u;
                *p_err     =  USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_BassBoostManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             bass_boost = (p_buf[0u] == 0) ? DEF_OFF : DEF_ON;

             valid = p_fu->FU_API_Ptr->FU_BassBoostManage(p_fu->DrvInfoPtr,
                                                          unit_id,
                                                          log_ch_nbr,
                                                          DEF_TRUE,
                                                         &bass_boost);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Auto Gain Ctrl.           */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_FU_LoudnessManage()
*
* Description : Enable or disable loudness for one or several logical channels inside a cluster
*               (see note #1).
*
* Argument(s) : p_fu        Pointer to Feature Unit structure.
*
*               unit_id     Feature Unit ID number.
*
*               log_ch_nbr  Logical Channel Number to enable or disable bass boost.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer that contains the loudness state.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) A Loudness Control can have only the current setting attribute (CUR). The position
*                   of a Loudness Control CUR attribute can be either TRUE or FALSE.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.4.3.10 for more details about Loudness Control.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_FU_MAX_CTRL == DEF_ENABLED)
static  void  USBD_Audio_FU_LoudnessManage (const  USBD_AUDIO_FU  *p_fu,
                                                   CPU_INT08U      unit_id,
                                                   CPU_INT08U      log_ch_nbr,
                                                   CPU_INT08U      b_req,
                                                   CPU_INT08U     *p_buf,
                                                   USBD_ERR       *p_err)
{
    CPU_BOOLEAN  loudness;
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
             if (p_fu->FU_API_Ptr->FU_LoudnessManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_fu->FU_API_Ptr->FU_LoudnessManage(p_fu->DrvInfoPtr,
                                                         unit_id,
                                                         log_ch_nbr,
                                                         DEF_FALSE,
                                                        &loudness);
             if (valid == DEF_OK) {
                 p_buf[0u] = (loudness == DEF_OFF) ? 0u : 1u;
                *p_err     =  USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             if (p_fu->FU_API_Ptr->FU_LoudnessManage == DEF_NULL) {
                 break;
             }

             loudness = (p_buf[0u] == 0u) ? DEF_OFF : DEF_ON;

             valid = p_fu->FU_API_Ptr->FU_LoudnessManage(p_fu->DrvInfoPtr,
                                                         unit_id,
                                                         log_ch_nbr,
                                                         DEF_TRUE,
                                                        &loudness);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Auto Gain Ctrl.           */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                      USBD_Audio_MU_CtrlManage()
*
* Description : Process Mixer Unit Control.
*
* Argument(s) : p_mu            Pointer to Mixer Unit structure.
*
*               unit_id         Feature Unit ID number.
*
*               log_in_ch_nbr   Logical channel number input.
*
*               log_out_ch_nbr  Logical channel number output.
*
*               b_req           Received request type (SET_XXX or GET_XXX).
*
*               p_buf           Pointer to the buffer that contains the mixer control attribute.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) The Set Mixer Unit Control request can have 2 forms and the Get Mixer Unit Control
*                   request can have 3 forms. However the 2nd and 3nd are optional and are not
*                   supported by this implementation.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.2 for more details about Mixer Unit Control Requests.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
static  void  USBD_Audio_MU_CtrlManage (const  USBD_AUDIO_MU  *p_mu,
                                               CPU_INT08U      unit_id,
                                               CPU_INT08U      log_in_ch_nbr,
                                               CPU_INT08U      log_out_ch_nbr,
                                               CPU_INT08U      b_req,
                                               CPU_INT08U     *p_buf,
                                               USBD_ERR       *p_err)
{
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_CUR:
             if ((p_mu->MU_API_Ptr                == DEF_NULL) ||
                 (p_mu->MU_API_Ptr->MU_CtrlManage == DEF_NULL)) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_mu->MU_API_Ptr->MU_CtrlManage(          p_mu->DrvInfoPtr,
                                                               b_req,
                                                               unit_id,
                                                               log_in_ch_nbr,
                                                               log_out_ch_nbr,
                                                 (CPU_INT16U *)p_buf);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Mixer Unit Ctrl.          */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                      USBD_Audio_SU_CtrlManage()
*
* Description : Process Selector Unit Control.
*
* Argument(s) : p_su        Pointer to Selector Unit structure.
*
*               p_su        Pointer to selector unit info.
*
*               unit_id     Feature Unit ID number.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer that contains the input pin number.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) Set request does not support the MIN, MAX, and RES. Also the Get request does not
*                   support RES.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2.3 for more details about Mixer Unit Control Requests.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
static  void  USBD_Audio_SU_CtrlManage (const  USBD_AUDIO_SU  *p_su,
                                               CPU_INT08U      unit_id,
                                               CPU_INT08U      b_req,
                                               CPU_INT08U     *p_buf,
                                               USBD_ERR       *p_err)
{
    CPU_BOOLEAN  valid;


    switch (b_req) {
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_RES:
             p_buf[0u] = 1u;                                    /* Min is always 1 input pin.                           */
            *p_err     = USBD_ERR_NONE;
             break;


        case USBD_AUDIO_REQ_GET_MAX:
             p_buf[0u] = p_su->SU_CfgPtr->NbrInPins;
            *p_err     = USBD_ERR_NONE;
             break;


        case USBD_AUDIO_REQ_GET_CUR:
             if (p_su->SU_API_Ptr->SU_InPinManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_su->SU_API_Ptr->SU_InPinManage(p_su->DrvInfoPtr,
                                                      unit_id,
                                                      DEF_FALSE,
                                                      p_buf);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             if (p_su->SU_API_Ptr->SU_InPinManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_su->SU_API_Ptr->SU_InPinManage(p_su->DrvInfoPtr,
                                                  unit_id,
                                                  DEF_TRUE,
                                                  p_buf);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Auto Gain Ctrl.           */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                  USBD_Audio_AS_SamplingFreqManage()
*
* Description : Set/Get initial sampling frequency for an isochronous audio data endpoint.
*
* Argument(s) : p_as_if     Pointer to AudioStreaming interface.
*
*               p_as_cfg    Pointer to the AudioStreaming configuration
*
*               ep_addr     Endpoint address.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer that contains the sampling frequency.
*
*               req_len     Request control attribute length (in bytes).
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) If the sampling frequency is continuous, the resolution in 1 Hz.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.3.2.3.1 for more details.
*
*               (2) If the endpoint supports a discrete number of sampling frequencies, a fixed
*                   resolution is not available between the different discrete sampling frequencies.
*
*               (3) According to Audio 1.0 specification (section 5.2.3.2.3.1), the sampling frequency
*                   value holds on 3 bytes. Since the sampling frequency is referenced as a 32-bit word,
*                   clearing the most significant byte ensures that the correct value is read from
*                   memory.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_AS_SamplingFreqManage (const  USBD_AUDIO_AS_IF       *p_as_if,
                                                const  USBD_AUDIO_AS_ALT_CFG  *p_as_cfg,
                                                       CPU_INT16U              ep_addr,
                                                       CPU_INT08U              b_req,
                                                       CPU_INT08U             *p_buf,
                                                       CPU_INT16U              req_len,
                                                       USBD_ERR               *p_err)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    CPU_BOOLEAN                 valid;
    CPU_INT32U                  sampling_freq = 0u;
    CPU_BOOLEAN                 freq_match;
    CPU_INT08U                  ix;
    CPU_INT32U                 *p_sampling_freq;


    (void)ep_addr;
    (void)req_len;

    p_as_if_settings = p_as_if->AS_IF_SettingsPtr;

    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
             if (p_as_if_settings->AS_API_Ptr->AS_SamplingFreqManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_as_if_settings->AS_API_Ptr->AS_SamplingFreqManage(              p_as_if_settings->DrvInfoPtr,
                                                                                       p_as_if_settings->TerminalID,
                                                                                       DEF_FALSE,
                                                                         (CPU_INT32U *)p_buf);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_GET_MIN:
             if (p_as_cfg->NbrSamplingFreq == 0u) {             /* Continuous sampling freq.                            */
                 MEM_VAL_SET_INT32U_LITTLE(p_buf, p_as_cfg->LowerSamplingFreq);
             } else {                                           /* Discrete sampling frequencies.                       */
                                                                /* Find the minimum frequency.                          */
                 p_sampling_freq = &p_as_cfg->SamplingFreqTblPtr[0u];
                 for (ix = 1u; ix < p_as_cfg->NbrSamplingFreq; ix++) {
                     if (*p_sampling_freq > p_as_cfg->SamplingFreqTblPtr[ix]) {
                         p_sampling_freq = &p_as_cfg->SamplingFreqTblPtr[ix];
                     }
                 }

                 MEM_VAL_SET_INT32U_LITTLE(p_buf, *p_sampling_freq);
             }

            *p_err = USBD_ERR_NONE;
             break;


        case USBD_AUDIO_REQ_GET_MAX:
             if (p_as_cfg->NbrSamplingFreq == 0u) {             /* Continuous sampling freq.                            */
                 MEM_VAL_SET_INT32U_LITTLE(p_buf, p_as_cfg->UpperSamplingFreq);
             } else {                                           /* Discrete sampling frequencies.                       */
                                                                /* Find the maximum frequency.                          */
                 p_sampling_freq = &p_as_cfg->SamplingFreqTblPtr[0u];
                 for (ix = 1u; ix < p_as_cfg->NbrSamplingFreq; ix++) {
                     if (*p_sampling_freq < p_as_cfg->SamplingFreqTblPtr[ix]) {
                         p_sampling_freq = &p_as_cfg->SamplingFreqTblPtr[ix];
                     }
                 }

                 MEM_VAL_SET_INT32U_LITTLE(p_buf, *p_sampling_freq);
             }

            *p_err = USBD_ERR_NONE;
             break;


        case USBD_AUDIO_REQ_GET_RES:
             if (p_as_cfg->NbrSamplingFreq == 0u) {             /* Continuous sampling freq.                            */
                 p_buf[0u] = 1u;                                 /* See Note #1.                                         */

                *p_err = USBD_ERR_NONE;
             } else {                                           /* Discrete sampling frequencies.                       */
                *p_err = USBD_ERR_AUDIO_REQ;                    /* See Note #2.                                         */
             }
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             if (p_as_if_settings->AS_API_Ptr->AS_SamplingFreqManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             sampling_freq  = MEM_VAL_GET_INT32U_LITTLE(p_buf);
             sampling_freq &= 0x00FFFFFFu;                      /* Ensure correct sampling freq is read (see Note #3).  */

                                                                /* Check if sampling freq is supported.                 */
             if (p_as_cfg->NbrSamplingFreq == 0u) {             /* Continuous sampling freq.                            */

                 if ((sampling_freq < p_as_cfg->LowerSamplingFreq) ||
                     (sampling_freq > p_as_cfg->UpperSamplingFreq)) {

                    *p_err = USBD_ERR_AUDIO_REQ;
                     break;
                 }
             } else {                                           /* Discrete sampling frequencies.                       */
                 freq_match = DEF_FALSE;
                 for (ix = 0u; ix < p_as_cfg->NbrSamplingFreq; ix++) {

                     if (sampling_freq == p_as_cfg->SamplingFreqTblPtr[ix]) {
                         freq_match = DEF_TRUE;
                         break;
                     }
                 }

                 if (freq_match == DEF_FALSE) {
                    *p_err = USBD_ERR_AUDIO_REQ;
                     break;
                 }
             }

             valid = p_as_if_settings->AS_API_Ptr->AS_SamplingFreqManage(p_as_if_settings->DrvInfoPtr,
                                                                         p_as_if_settings->TerminalID,
                                                                         DEF_TRUE,
                                                                        &sampling_freq);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Sampling Freq Ctrl.       */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                      USBD_Audio_AS_PitchManage()
*
* Description : Enable or disable ability of an adaptive endpoint to dynamically track its sampling
*               frequency.
*
* Argument(s) : p_as_if     Pointer to AudioStreaming interface.
*
*               ep_addr     Endpoint address.
*
*               b_req       Received request type (SET_XXX or GET_XXX).
*
*               p_buf       Pointer to the buffer that contains the pitch state.
*
*               p_err       Pointer to the variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE                       Control successfully processed.
*                           USBD_ERR_AUDIO_REQ_INVALID_ATTRIB   Invalid request type.
*                           USBD_ERR_AUDIO_REQ_FAIL             Audio driver failed to process the request.
*
* Return(s)   : None.
*
* Note(s)     : (1) Pitch Control enables or disables the ability of an adaptive endpoint to
*                   dynamically track its sampling frequency. The Control is necessary because the clock
*                   recovery circuitry must be informed whether it should allow for relatively large
*                   swings in the sampling frequency. A Pitch Control can have only the current setting
*                   attribute (CUR). The position of a Pitch Control CUR attribute can be either TRUE or
*                   FALSE.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.3.2.3.2 for more details on Pitch Control.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_AS_PitchManage (const  USBD_AUDIO_AS_IF  *p_as_if,
                                                CPU_INT16U         ep_addr,
                                                CPU_INT08U         b_req,
                                                CPU_INT08U        *p_buf,
                                                USBD_ERR          *p_err)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    CPU_BOOLEAN                 valid;
    CPU_BOOLEAN                 pitch;


    (void)ep_addr;

    p_as_if_settings = p_as_if->AS_IF_SettingsPtr;

    switch (b_req) {
        case USBD_AUDIO_REQ_GET_CUR:
             if (p_as_if_settings->AS_API_Ptr->AS_PitchManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             valid = p_as_if_settings->AS_API_Ptr->AS_PitchManage(p_as_if_settings->DrvInfoPtr,
                                                                  p_as_if_settings->TerminalID,
                                                                  DEF_FALSE,
                                                                 &pitch);
             if (valid == DEF_OK) {
                *p_buf = (pitch != DEF_OFF) ? 1u : 0u;
                *p_err =  USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             if (p_as_if_settings->AS_API_Ptr->AS_PitchManage == DEF_NULL) {
                *p_err = USBD_ERR_AUDIO_REQ;
                 break;
             }

             pitch = (*p_buf == 0u) ? DEF_OFF : DEF_ON;

             valid = p_as_if_settings->AS_API_Ptr->AS_PitchManage(p_as_if_settings->DrvInfoPtr,
                                                                  p_as_if_settings->TerminalID,
                                                                  DEF_TRUE,
                                                                 &pitch);
             if (valid == DEF_OK) {
                *p_err = USBD_ERR_NONE;
             } else {
                *p_err = USBD_ERR_AUDIO_REQ;
             }
             break;


        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
        default:
            *p_err = USBD_ERR_AUDIO_REQ_INVALID_ATTRIB;         /* Stall req not supported by Pitch Ctrl.               */
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_AS_IF_RingBufQInit()
*
* Description : Initialize the record ring buffer queue.
*
* Argument(s) : p_as_if     Pointer to the audio streaming interface.
*
* Return(s)   : none.
*
* Note(s)     : (1) The ring buffer queue contains buffer descriptors. Each buffer descriptor stores
*                   information about a buffer (pointer to buffer and buffer size). Buffers are taken
*                   from a memory segment allocated at the stream initialization. The memory segment
*                   is split into several buffers whose alignment takes into account any DMA and/or cache
*                   alignment requirement specified by the user via USBD_AUDIO_CFG_BUF_ALIGN_OCTETS.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_AS_IF_RingBufQInit (USBD_AUDIO_AS_IF  *p_as_if)
{
    USBD_AUDIO_AS_IF_SETTINGS    *p_as_if_settings  =  p_as_if->AS_IF_SettingsPtr;
    USBD_AUDIO_AS_IF_RING_BUF_Q  *p_ring_buf_q      = &p_as_if_settings->StreamRingBufQ;
    void                         *p_buf_new;
    USBD_AUDIO_BUF_DESC          *p_buf_desc;
    CPU_INT16U                    mem_blk_len_worst = MATH_ROUND_INC_UP(p_as_if_settings->BufTotalLen, USBD_AUDIO_CFG_BUF_ALIGN_OCTETS);
    CPU_INT08U                    ix;


    for (ix = 0u; ix < p_as_if_settings->BufTotalNbr; ix++) {
                                                                /* Alloc a buf (see Note #1).                           */
        p_buf_new = (void *)(p_as_if_settings->BufMemPtr + (ix * mem_blk_len_worst));

                                                                /* Init buf desc.                                       */
        p_buf_desc = &p_ring_buf_q->BufDescTblPtr[ix];
        if (p_as_if_settings->StreamDir == USBD_AUDIO_STREAM_IN) {
#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
            p_buf_desc->BufLen = USBD_Audio_RecordDataRateAdj(p_as_if);
#endif
        } else {
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
            p_buf_desc->BufLen = 0u;
#endif
        }
        p_buf_desc->BufPtr = p_buf_new;
    }
}
#endif


/*
*********************************************************************************************************
*                                    USBD_Audio_AS_IF_RingBufQGet()
*
* Description : Get a buffer descriptor at the specified index.
*
* Argument(s) : p_ring_buf_q    Pointer to ring buffer queue.
*
*               ix              Index.
*
* Return(s)   : Pointer to buffer descriptor.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  USBD_AUDIO_BUF_DESC  *USBD_Audio_AS_IF_RingBufQGet (USBD_AUDIO_AS_IF_RING_BUF_Q  *p_ring_buf_q,
                                                            CPU_INT16U                    ix)
{
    USBD_AUDIO_BUF_DESC  *p_buf_desc;


    p_buf_desc = &p_ring_buf_q->BufDescTblPtr[ix];

    return (p_buf_desc);
}
#endif


/*
*********************************************************************************************************
*                             USBD_Audio_AS_IF_RingBufQProducerStartIxGet()
*
* Description : Get the current Producer Start index.
*
* Argument(s) : p_as_if_settings    Pointer to AudioStreaming interface settings.
*
* Return(s)   : Current Producer Start index.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_INT16U  USBD_Audio_AS_IF_RingBufQProducerStartIxGet (USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings)
{
    USBD_AUDIO_AS_IF_RING_BUF_Q  *p_ring_buf_q = &p_as_if_settings->StreamRingBufQ;
    CPU_INT16U                    nxt_ix;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    nxt_ix = p_ring_buf_q->ProducerStartIx + 1;
    if (nxt_ix == p_as_if_settings->BufTotalNbr) {
        nxt_ix = 0u;
    }

    if ((nxt_ix == p_ring_buf_q->ConsumerEndIx) ||
        (nxt_ix == p_ring_buf_q->ProducerEndIx)) {

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrProducerStartIxCatchUp);
        CPU_CRITICAL_EXIT();
        return (USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX);
    }
    CPU_CRITICAL_EXIT();

    USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrBufDescInUse);

    return (p_ring_buf_q->ProducerStartIx);
}
#endif


/*
*********************************************************************************************************
*                              USBD_Audio_AS_IF_RingBufQProducerEndIxGet()
*
* Description : Get the current Producer End index.
*
* Argument(s) : p_as_if_settings    Pointer to AudioStreaming interface settings.
*
* Return(s)   : Current Producer End index.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_INT16U  USBD_Audio_AS_IF_RingBufQProducerEndIxGet (USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings)
{
    USBD_AUDIO_AS_IF_RING_BUF_Q  *p_ring_buf_q = &p_as_if_settings->StreamRingBufQ;
    CPU_INT16U                    cur_ix;
    CPU_INT16U                    nxt_ix;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    cur_ix = p_ring_buf_q->ProducerEndIx;
    nxt_ix = p_ring_buf_q->ProducerEndIx + 1;
    if (nxt_ix == p_as_if_settings->BufTotalNbr) {
        nxt_ix = 0u;
    }

    if ((cur_ix == p_ring_buf_q->ProducerStartIx) ||
        (nxt_ix == p_ring_buf_q->ConsumerStartIx)) {

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrProducerEndIxCatchUp);
        CPU_CRITICAL_EXIT();
        return (USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX);
    }
    CPU_CRITICAL_EXIT();

    return (cur_ix);
}
#endif


/*
*********************************************************************************************************
*                             USBD_Audio_AS_IF_RingBufQConsumerStartIxGet()
*
* Description : Get the current Consumer Start index.
*
* Argument(s) : p_as_if_settings    Pointer to AudioStreaming interface settings.
*
* Return(s)   : Current Consumer Start index.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_INT16U  USBD_Audio_AS_IF_RingBufQConsumerStartIxGet (USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings)
{
    USBD_AUDIO_AS_IF_RING_BUF_Q  *p_ring_buf_q = &p_as_if_settings->StreamRingBufQ;
    CPU_INT16U                    cur_ix;
    CPU_INT16U                    nxt_ix;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    cur_ix = p_ring_buf_q->ConsumerStartIx;
    nxt_ix = p_ring_buf_q->ConsumerStartIx + 1;
    if (nxt_ix == p_as_if_settings->BufTotalNbr) {
        nxt_ix = 0u;
    }

    if ((cur_ix == p_ring_buf_q->ProducerEndIx) ||
        (nxt_ix == p_ring_buf_q->ConsumerEndIx)) {
        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrConsumerStartIxCatchUp);
        CPU_CRITICAL_EXIT();
        return (USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX);
    }
    CPU_CRITICAL_EXIT();

    return (cur_ix);
}
#endif


/*
*********************************************************************************************************
*                              USBD_Audio_AS_IF_RingBufQConsumerEndIxGet()
*
* Description : Get the current Consumer End index.
*
* Argument(s) : p_as_if_settings    Pointer to AudioStreaming interface settings.
*
* Return(s)   : Current Consumer End index.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_INT16U  USBD_Audio_AS_IF_RingBufQConsumerEndIxGet (USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings)
{
    USBD_AUDIO_AS_IF_RING_BUF_Q  *p_ring_buf_q     = &p_as_if_settings->StreamRingBufQ;
    CPU_INT16U                    cur_ix;
    CPU_INT16U                    nxt_ix;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    cur_ix = p_ring_buf_q->ConsumerEndIx;
    nxt_ix = p_ring_buf_q->ConsumerEndIx + 1;
    if (nxt_ix == p_as_if_settings->BufTotalNbr) {
        nxt_ix = 0u;
    }

    if ((cur_ix == p_ring_buf_q->ConsumerStartIx) ||
        (nxt_ix == p_ring_buf_q->ProducerStartIx)) {

        USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrConsumerEndIxCatchUp);
        CPU_CRITICAL_EXIT();
        return (USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX);
    }
    CPU_CRITICAL_EXIT();

    USBD_AUDIO_STAT_DEC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrBufDescInUse);

    return (cur_ix);
}
#endif


/*
*********************************************************************************************************
*                                  USBD_Audio_AS_IF_RingBufQIxUpdate()
*
* Description : Update index.
*
* Argument(s) : p_as_if_settings    Pointer to AudioStreaming interface settings.
*
*               p_ix                Pointer to index to increment.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_AS_IF_RingBufQIxUpdate (USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings,
                                                 CPU_INT16U                 *p_ix)
{
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
    USBD_AUDIO_AS_IF_RING_BUF_Q  *p_ring_buf_q = &p_as_if_settings->StreamRingBufQ;
#endif
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    (*p_ix)++;                                                  /* Nxt avail ix.                                        */
    if (*p_ix == p_as_if_settings->BufTotalNbr) {               /* Reset ix if end of ring q reached.                   */
       *p_ix = 0u;
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
       if (p_ix == &p_ring_buf_q->ProducerStartIx) {
           USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrProducerStartIxWrapAround);
       } else if (p_ix == &p_ring_buf_q->ProducerEndIx) {
           USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrProducerEndIxWrapAround);
       } else if (p_ix == &p_ring_buf_q->ConsumerStartIx) {
           USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrConsumerStartIxWrapAround);
       } else {
           USBD_AUDIO_STAT_INC(p_as_if_settings->StatPtr->AudioProc_RingBufQ_NbrConsumerEndIxWrapAround);
       }
#endif
    }
    CPU_CRITICAL_EXIT();
}
#endif


/*
*********************************************************************************************************
*                                        USBD_Audio_BufDiffGet()
*
* Description : Compute current difference of buffers between Producer End index and Consumer End index.
*
* Argument(s) : p_as_if_settings    Pointer to AudioStreaming interface settings.
*
* Return(s)   : Difference of buffers.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if ( (USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) &&   \
     ((USBD_AUDIO_CFG_PLAYBACK_CORR_EN     == DEF_ENABLED) ||   \
      (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED))) || \
    ( (USBD_AUDIO_CFG_RECORD_EN            == DEF_ENABLED) &&   \
      (USBD_AUDIO_CFG_RECORD_CORR_EN       == DEF_ENABLED) )
static  CPU_INT08S  USBD_Audio_BufDiffGet (USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings)
{
    USBD_AUDIO_AS_IF_RING_BUF_Q  *p_ring_buf_q = &p_as_if_settings->StreamRingBufQ;
    CPU_INT08S                    buf_diff;
    CPU_INT16U                    circular_distance;

    if (p_ring_buf_q->ProducerEndIx >= p_ring_buf_q->ConsumerEndIx) {
        circular_distance = p_ring_buf_q->ProducerEndIx - p_ring_buf_q->ConsumerEndIx;
    } else {
        circular_distance = (p_as_if_settings->BufTotalNbr + 1 + p_ring_buf_q->ProducerEndIx) - p_ring_buf_q->ConsumerEndIx;
    }

    buf_diff = (CPU_INT08S)(circular_distance - p_as_if_settings->StreamPreBufMax);

    return (buf_diff);
}
#endif


/*
*********************************************************************************************************
*                                        USBD_Audio_AS_IF_Get()
*
* Description : Retrieve an AudioStreaming interface structure.
*
* Argument(s) : as_handle   AudioStreaming interface handle.
*
* Return(s)   : Pointer to AudioStreaming interface structure.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  USBD_AUDIO_AS_IF  *USBD_Audio_AS_IF_Get (USBD_AUDIO_AS_HANDLE  as_handle)
{
    USBD_AUDIO_AS_IF  *p_as_if;
    CPU_INT08U         as_if_ix;


    as_if_ix = USBD_AUDIO_AS_IF_HANDLE_IX_GET(as_handle);

    p_as_if = &USBD_Audio_AS_Tbl[as_if_ix];

    return (p_as_if);
}
#endif

