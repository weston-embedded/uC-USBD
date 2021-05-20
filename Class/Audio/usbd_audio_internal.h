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
*
* Filename : usbd_audio_internal.h
* Version  : V4.06.01
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  USBD_AUDIO_INTERNAL_MODULE_PRESENT
#define  USBD_AUDIO_INTERNAL_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "usbd_audio.h"
#include  <lib_math.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             RING BUF Q
*********************************************************************************************************
*/

#define  USBD_AUDIO_AS_IF_RING_BUF_Q_INVALID_IX             0xFFFF


/*
*********************************************************************************************************
*                                         AUDIO CLASS SYNCH EP
*
* Note(s):  (1) The information carried over the synch path consists of a 3-byte data packet.
*               See 'USB Device Class Definition for Audio Data Formats, Release 1.0, March 18, 1998',
*               section 3.7.2.2 for more details about Isochronous Synch Endpoint.
*
*           (2) Tests on Windows showed that the transfer length should be the same, no matter the speed
*               of the device.
*********************************************************************************************************
*/

                                                                /* See Note #1.                                         */
#define  USBD_AUDIO_FS_SYNCH_EP_XFER_LEN                   3u   /* Full-speed synch xfer len.                           */
#define  USBD_AUDIO_HS_SYNCH_EP_XFER_LEN                   3u   /* High-speed synch xfer len. See Note #2.              */

#define  USBD_AUDIO_PLAYBACK_SYNCH_BUF_NBR                 1u   /* Nbr of synch buf per AudioStreaming IF.              */


/*
*********************************************************************************************************
*                                         AUDIO STREAMING IF
*
* Note(s):  (1) The audio class uses 2 internal important types of structure for its functioning:
*               AudioStreaming Interface (AS IF) and AudioStreaming Interface Settings (AS IF Settings).
*
*               The following figure shows the device/configuration/interface/endpoint topology and how
*               the AudioStreaming Interface and AudioStreaming Interface Settings fit in this topology.
*               Let's consider the following high-speed capable device topology:
*
*               Device HS
*                   Configuration HS
*                      +--------------------------------------+
*                      |                                      |
*                      | AudioControl Interface               |
*                      | AudioStreaming Interface             | -> AS IF #1     -> AS IF Settings #1
*                      |     Isochronous OUT EP               |
*                      | AudioStreaming Interface             | -> AS IF #2     -> AS IF Settings #2
*                      |     Isochronous IN EP                |
*                      |                                      |
*                      +---- AUDIO INTERFACE COLLECTION 0 ----+ = class instance #0
*                   Configuration FS
*                      +--------------------------------------+
*                      |                                      |
*                      | AudioControl Interface               |
*                      | AudioStreaming Interface             | -> AS IF #3     -> AS IF Settings #1
*                      |     Isochronous OUT EP               |
*                      | AudioStreaming Interface             | -> AS IF #4     -> AS IF Settings #2
*                      |     Isochronous IN EP                |
*                      |                                      |
*                      +---- AUDIO INTERFACE COLLECTION 0 ----+ = class instance #0
*
*               2 configurations are needed, one high-speed and the other full-speed. Each configuration
*               contains the same playback and record AudioStreaming interfaces. To handle this topology,
*               the audio class will use 4 different AS IF structures and 2 different AS IF Settings
*               structures.
*               The AS IF structure is associated to the ischronous endpoint. Since an endpoint can have
*               different characteristics depending of the speed, each time an AudioStreaming interface
*               is added to a configuration, a new AS IF structure is allocated.
*               Since the AS IF Settings contains information that is more stream-specific and the same
*               stream can be added to different configurations, thus the same AS IF Settings structure
*               can also be added to different configurations.
*********************************************************************************************************
*/

                                                                /* Max nbr of comm struct.                              */
#define  USBD_AUDIO_MAX_NBR_COMM                           (USBD_AUDIO_CFG_MAX_NBR_AIC * \
                                                            USBD_AUDIO_CFG_MAX_NBR_CFG)

                                                                /* Max nbr of alt setting struct.                       */
#define  USBD_AUDIO_MAX_NBR_IF_ALT                         (USBD_AUDIO_CFG_MAX_NBR_AIC * \
                                                            USBD_AUDIO_CFG_MAX_NBR_CFG * \
                                                            USBD_AUDIO_CFG_MAX_NBR_IF_ALT)

                                                                /* Max nbr of AudioStreaming IF EP.                     */
#define  USBD_AUDIO_MAX_NBR_AS_IF_EP                       ( USBD_AUDIO_CFG_MAX_NBR_AIC * \
                                                             USBD_AUDIO_CFG_MAX_NBR_CFG * \
                                                            (USBD_AUDIO_CFG_MAX_NBR_AS_IF_PLAYBACK + USBD_AUDIO_CFG_MAX_NBR_AS_IF_RECORD))

                                                                /* Maximum Number of AudioStreaming IF Settings.        */
#define  USBD_AUDIO_MAX_NBR_AS_IF_SETTINGS                 ( USBD_AUDIO_CFG_MAX_NBR_AIC * \
                                                            (USBD_AUDIO_CFG_MAX_NBR_AS_IF_PLAYBACK + USBD_AUDIO_CFG_MAX_NBR_AS_IF_RECORD))


/*
*********************************************************************************************************
*                                           SYNCH ENDPOINT
*
* Note(s):  (1) Synch endpoint was known as feedback endpoint in the audio 1.0 specification. Synch
*               endpoint is the official name now.
*
*           (2) Tests on Windows showed that the bit shift amount should be the same, no matter the speed
*               of the device.
*
*           (3) The maximum adjustment allows to add (underrun) or remove (overrun) 1 sample.
*               The medium  adjustment allows to add (underrun) or remove (overrun) 1/2 sample.
*               The min     adjustment allows to add (underrun) or remove (overrun) 1/2048 sample.
*********************************************************************************************************
*/

#define  USBD_AUDIO_STREAM_CORR_BOUNDARY_INTERVAL          6u

#define  USBD_AUDIO_PLAYBACK_FS_BIT_SHIFT                 14u   /* Full-speed synch bit shift amount.                   */
#define  USBD_AUDIO_PLAYBACK_HS_BIT_SHIFT                 14u   /* High-speed synch bit shift amount. See Note #2.      */

                                                                /* Min, med and max cst for feedback value (see Note #3)*/
#define  USBD_AUDIO_PLAYBACK_SYNCH_MIN_ADJ(bit_shift)   (1u << (bit_shift - 11u))
#define  USBD_AUDIO_PLAYBACK_SYNCH_MED_ADJ(bit_shift)   (1u << (bit_shift -  1u))
#define  USBD_AUDIO_PLAYBACK_SYNCH_MAX_ADJ(bit_shift)   (1u <<  bit_shift)


/*
*********************************************************************************************************
*                                            AS IF HANDLE
*********************************************************************************************************
*/

#define  USBD_AUDIO_AS_IF_HANDLE_CREATE(as_if, ix)          DEF_BIT_FIELD_WR((as_if)->Handle, \
                                                                             ((ix) & 0x00FFu) , \
                                                                              0x00FFu)

#define  USBD_AUDIO_AS_IF_HANDLE_VALIDATE(as_if, as_handle) (((as_handle) == (as_if)->Handle) ? DEF_OK : DEF_FAIL)

#define  USBD_AUDIO_AS_IF_HANDLE_INVALIDATE(as_if)          ((as_if)->Handle = ((as_if)->Handle + 0x0100u))

#define  USBD_AUDIO_AS_IF_HANDLE_IX_GET(as_handle)          ((CPU_INT08U)(as_handle & 0x00FFu))


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

typedef  enum  usbd_audio_state {                               /* Audio class states.                                  */
    USBD_AUDIO_STATE_NONE = 0u,
    USBD_AUDIO_STATE_INIT,
    USBD_AUDIO_STATE_CFG
} USBD_AUDIO_STATE;

typedef  enum  usbd_audio_stream_dir {                          /* Audio stream dir.                                    */
    USBD_AUDIO_STREAM_NONE = 0u,
    USBD_AUDIO_STREAM_IN,
    USBD_AUDIO_STREAM_OUT
} USBD_AUDIO_STREAM_DIR;

typedef  enum  usbd_audio_entity_type {                          /* Audio entity type.                                  */
    USBD_AUDIO_ENTITY_UNKNOWN = 0u,
    USBD_AUDIO_ENTITY_IT,
    USBD_AUDIO_ENTITY_OT,
    USBD_AUDIO_ENTITY_FU,
    USBD_AUDIO_ENTITY_MU,
    USBD_AUDIO_ENTITY_SU
} USBD_AUDIO_ENTITY_TYPE;


/*
*********************************************************************************************************
*                                        FORWARD DECLARATIONS
*********************************************************************************************************
*/

typedef  struct  usbd_audio_ctrl      USBD_AUDIO_CTRL;
typedef  struct  usbd_audio_comm      USBD_AUDIO_COMM;
typedef  struct  usbd_audio_entity    USBD_AUDIO_ENTITY;
typedef  struct  usbd_audio_it        USBD_AUDIO_IT;
typedef  struct  usbd_audio_ot        USBD_AUDIO_OT;
typedef  struct  usbd_audio_fu        USBD_AUDIO_FU;
typedef  struct  usbd_audio_mu        USBD_AUDIO_MU;
typedef  struct  usbd_audio_su        USBD_AUDIO_SU;
typedef  struct  usbd_audio_as_if     USBD_AUDIO_AS_IF;
typedef  struct  usbd_audio_buf_desc  USBD_AUDIO_BUF_DESC;


/*
*********************************************************************************************************
*                                        AUDIO CLASS CTRL INFO
*********************************************************************************************************
*/

struct  usbd_audio_ctrl {
    CPU_INT08U                DevNbr;                           /* Dev   nbr.                                           */
    CPU_INT08U                ClassNbr;                         /* Class nbr.                                           */
    USBD_AUDIO_STATE          State;                            /* Audio class state.                                   */
    USBD_AUDIO_COMM          *CommPtr;                          /* Comm info ptr.                                       */
    USBD_AUDIO_COMM          *CommHeadPtr;                      /* Comm list head ptr.                                  */
    USBD_AUDIO_COMM          *CommTailPtr;                      /* Comm list tail ptr.                                  */
    CPU_INT08U                CommCnt;                          /* Nbr of Comm Struct for this class instance.          */
    CPU_INT08U               *ReqBufPtr;                        /* Pointer to class req buf.                            */
    USBD_AUDIO_DRV            DrvInfo;                          /* Audio driver info.                                   */
    USBD_AUDIO_EVENT_FNCTS   *EventFnctPtr;                     /* Pointer to the audio event callbacks.                */

    CPU_INT08U                EntityID_Nxt;                     /* Index used for entity ID assignment.                 */
    CPU_INT08U                EntityCnt;                        /* Nbr of Terminals and Units.                          */
    USBD_AUDIO_ENTITY       **EntityID_TblPtr;                  /* Table used for ID management.                        */
};


/*
*********************************************************************************************************
*                                        AUDIO CLASS COMM INFO
*********************************************************************************************************
*/

struct  usbd_audio_comm {
    USBD_AUDIO_CTRL   *CtrlPtr;                                 /* Pointer to ctrl info.                                */
    CPU_INT08U         CfgNbr;                                  /* Cfg number.                                          */
    CPU_INT08U         AS_IF_Cnt;                               /* Nbr of AudioStreaming IFs.                           */
    USBD_AUDIO_AS_IF  *AS_IF_HeadPtr;                           /* AudioStreaming IFs list head ptr.                    */
    USBD_AUDIO_AS_IF  *AS_IF_TailPtr;                           /* AudioStreaming IFs list tail ptr.                    */
    USBD_AUDIO_COMM   *NextPtr;                                 /* Pointer to the next Comm Info structure.             */
};


/*
*********************************************************************************************************
*                                    UNIT AND TERMINAL INFO STRUCT
*********************************************************************************************************
*/

struct  usbd_audio_entity {                                     /* Overlay struct used mainly for ID management.        */
    USBD_AUDIO_ENTITY_TYPE            EntityType;               /* Terminal or unit type.                               */
};

struct  usbd_audio_it {
           USBD_AUDIO_ENTITY_TYPE     EntityType;               /* Terminal type.                                       */
           CPU_INT08U                 ID;                       /* Unique ID identifying Terminal within audio fnct.    */
           CPU_INT08U                 AssociatedOT_ID;          /* Output Terminal ID associated to this Input Terminal.*/
           USBD_AUDIO_IT_CFG         *IT_CfgPtr;                /* Ptr to the Input Terminal cfg.                       */
};

struct  usbd_audio_ot {
           USBD_AUDIO_ENTITY_TYPE     EntityType;               /* Terminal type.                                       */
           CPU_INT08U                 ID;                       /* Unique ID identifying Terminal within audio fnct.    */
           CPU_INT08U                 AssociatedIT_ID;          /* Input Terminal ID associated to this Output Terminal.*/
           CPU_INT08U                 SourceID;                 /* Unit or Terminal ID to which Terminal is connected.  */
           USBD_AUDIO_OT_CFG         *OT_CfgPtr;                /* Ptr to the Output Terminal cfg.                      */
    const  USBD_AUDIO_DRV_AC_OT_API  *OT_API_Ptr;               /* Ptr to Audio Drv OT API.                             */
           USBD_AUDIO_DRV            *DrvInfoPtr;               /* Ptr to audio drv info.                               */
};

struct  usbd_audio_fu {
           USBD_AUDIO_ENTITY_TYPE     EntityType;               /* Unit type.                                           */
           CPU_INT08U                 ID;                       /* Unique ID identifying Unit within audio fnct.        */
           CPU_INT08U                 SourceID;                 /* Unit or Terminal ID to which Unit is connected to.   */
           USBD_AUDIO_FU_CFG         *FU_CfgPtr;                /* Ptr to the Feature Unit cfg.                         */
    const  USBD_AUDIO_DRV_AC_FU_API  *FU_API_Ptr;               /* Ptr to Audio Drv FU API.                             */
           USBD_AUDIO_DRV            *DrvInfoPtr;               /* Ptr to audio drv info.                               */
};

struct  usbd_audio_mu {
           USBD_AUDIO_ENTITY_TYPE     EntityType;               /* Unit type.                                           */
           CPU_INT08U                 ID;                       /* Unique ID identifying Unit within audio fnct.        */
           CPU_INT08U                *SourceID_TblPtr;          /* Tbl of Unit or Terminal ID to which Unit is connected*/
           CPU_INT32U                 ControlsSize;             /* bmControls tbl size.                                 */
           CPU_INT08U                *ControlsTblPtr;           /* Bitmap tbl indicating which mixing Ctrls are ...     */
                                                                /* ...programmable.                                     */
           USBD_AUDIO_MU_CFG         *MU_CfgPtr;                /* Ptr to the Mixer Unit cfg.                           */
    const  USBD_AUDIO_DRV_AC_MU_API  *MU_API_Ptr;               /* Ptr to Audio Drv MU API.                             */
    USBD_AUDIO_DRV                   *DrvInfoPtr;               /* Ptr to audio drv info.                               */
};

struct  usbd_audio_su {
           USBD_AUDIO_ENTITY_TYPE     EntityType;               /* Unit type.                                           */
           CPU_INT08U                 ID;                       /* Unique ID identifying Unit within audio fnct.        */
           CPU_INT08U                *SourceID_TblPtr;          /* Tbl of Unit or Terminal ID to which Unit is connected*/
           USBD_AUDIO_SU_CFG         *SU_CfgPtr;                /* Ptr to the Selector Unit cfg.                        */
    const  USBD_AUDIO_DRV_AC_SU_API  *SU_API_Ptr;               /* Ptr to Audio Drv SU API.                             */
           USBD_AUDIO_DRV            *DrvInfoPtr;               /* Ptr to audio drv info.                               */
};


/*
*********************************************************************************************************
*                                         AUDIO STREAMING IF
*
* Note(s) : (1) Each AudioStreaming interface has a ring buffer queue managed by four indexes. It allows
*               to manage a producer/consumer communication model between the USB and codec sides:
*
*               Producer -> Consumer
*               USB      -> Codec (Playback)
*               Codec    -> USB   (Record)
*
*               The meaning of indexes is the following depending of playback or record stream:
*
*                PLAYBACK
*                ProducerStartIx    Core or Playback task submits a USB transfer
*                ProducerEndIx      Core task calls USBD_Audio_PlaybackIsocCmpl() to finish a USB transfer
*                ConsumerStartIx    Playback task submits an audio buffer to the codec driver
*                ConsumerEndIx      Codec driver has finished an audio transfer
*
*                RECORD
*                ProducerStartIx    Codec driver starts an audio transfer
*                ProducerEndIx      Codec driver has finished an audio transfer
*                ConsumerStartIx    Core or Record task submits a USB transfer
*                ConsumerEndIx      Core task calls USBD_Audio_RecordIsocCmpl() to finish a USB transfer
*
*           (2) The structure USBD_AUDIO_AS_IF_SETTINGS contains stream characteristics that will
*               be the same across different device configurations.
*
*           (3) For certain sampling frequencies (i.e. 11.025, 22.050 and 44.1 KHz), a data rate
*               adjustment is necessary because the number of audio samples per ms is not an
*               integer number. Partial audio samples are not possible. For those sampling
*               frequencies, the table below gives the required adjustment:
*
*               Samples per frm/ms | Typical Packet Size | Adjustment
*               11.025             | 11 samples          | 12 samples every 40 packets (i.e. ms)
*               22.050             | 22 samples          | 23 samples every 20 packets (i.e. ms)
*               44.1               | 44 samples          | 45 samples every 10 packets (i.e. ms)
*********************************************************************************************************
*/

                                                                /* AudioStreaming IF ring buf queue (see Note #1).      */
typedef struct  usbd_audio_as_if_ring_buf_q {
           USBD_AUDIO_BUF_DESC            *BufDescTblPtr;
           CPU_INT16U                      ProducerStartIx;
           CPU_INT16U                      ProducerEndIx;
           CPU_INT16U                      ConsumerStartIx;
           CPU_INT16U                      ConsumerEndIx;
} USBD_AUDIO_AS_IF_RING_BUF_Q;

                                                                /* AudioStreaming alt settings (see Note #2).           */
typedef struct  usbd_audio_as_if_alt {
           USBD_AUDIO_AS_ALT_CFG          *AS_CfgPtr;           /* Pointer to the Audiostreaming cfg.                   */
           CPU_INT08U                      DataIsocAddr;        /* Data isoc EP addr.                                   */
           CPU_INT08U                      SynchIsocAddr;       /* Synchronization isoc EP addr.                        */
           CPU_INT16U                      MaxPktLen;           /* Max pkt len in byte.                                 */
} USBD_AUDIO_AS_IF_ALT;

                                                                /* Audio playback synch struct.                         */
typedef  struct  usbd_audio_playback_synch {
           CPU_INT16U                      SynchFrameNbr;       /* Frame nbr used to track bRefresh period synch EP.    */
                                                                /* Nbr of buff diff at which light feedback is applied. */
           CPU_INT08S                      SynchBoundaryLightPos;
           CPU_INT08S                      SynchBoundaryLightNeg;

           CPU_INT32U                      FeedbackNominalVal;  /* Nominal feedback val (already bit-shifted).          */
           CPU_INT32U                      FeedbackCurVal;      /* Current feedback val (already bit-shifted).          */
           CPU_BOOLEAN                     FeedbackValUpdate;   /* Flag indicating if a new feedback val must be sent.  */

           CPU_INT08S                      PrevBufDiff;         /* Prev diff between nbr of rx'd and consumed bufs.     */
           CPU_INT16U                      PrevFrameNbr;        /* Prev frame nbr during which a situation occurred.    */

           CPU_INT32U                     *SynchBufPtr;         /* Ptr to single synch buf.                             */
           CPU_BOOLEAN                     SynchBufFree;        /* Flag indicating if synch buf free.                   */
} USBD_AUDIO_PLAYBACK_SYNCH;

typedef struct  usbd_audio_as_if_settings {                     /* See Note #1.                                         */
    const  USBD_AUDIO_DRV_AS_API          *AS_API_Ptr;          /* Ptr to Audio Drv AS API.                             */
           USBD_AUDIO_DRV                 *DrvInfoPtr;          /* Ptr to audio drv info.                               */
           CPU_INT08U                      Ix;                  /* AS IF Settings ix.                                   */
           CPU_INT08U                      TerminalID;          /* Terminal ID associated to this AS IF.                */
           CPU_INT16U                      BufTotalNbr;         /* Nbr of buf allocated for this stream.                */
           CPU_INT16U                      BufTotalLen;         /* Total len of a buf.                                  */
           CPU_INT08U                     *BufMemPtr;           /* Ptr to mem region containing buf.                    */

           USBD_AUDIO_STREAM_DIR           StreamDir;           /* Stream dir: IN or OUT.                               */
           CPU_BOOLEAN                     StreamStarted;       /* Flag indicating stream start.                        */
           CPU_INT16U                      StreamPreBufMax;     /* Max buf to acculumate for pre-buffering stream.      */
           CPU_BOOLEAN                     StreamPrimingDone;   /* Flag indicating stream priming done.                 */
           USBD_AUDIO_AS_IF_RING_BUF_Q     StreamRingBufQ;      /* Ring Buf Q.                                          */
                                                                /* PLAYBACK STATE:                                      */
#if (USBD_AUDIO_CFG_PLAYBACK_EN          == DEF_ENABLED) && \
    (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
           USBD_AUDIO_PLAYBACK_SYNCH       PlaybackSynch;       /* Struct containing synch infos.                       */
#endif
                                                                /* RECORD STATE:                                        */
#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
           CPU_INT16U                      RecordBufLen;        /* Buf len used during stream comm.                     */
           CPU_INT16U                      RecordRateAdjMs;     /* Data rate adjustment in milliseconds (see Note #2).  */
           CPU_INT32U                      RecordRateAdjXferCtr;/* Ctr tracking data rate adjustment.                   */
           CPU_INT16S                      RecordIsocTxOngoingCnt;  /* Nbr of isoc IN xfer in progress.                 */
#endif
                                                                /* BUILT-IN STREAM CORR:                                */
#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_CORR_EN   == DEF_ENABLED)
           CPU_INT16U                      CorrPeriod;          /* Period at which corr must be monitored.              */
           CPU_INT16U                      CorrFrameNbr;        /* Last frame used to track corr period.                */
           USBD_AUDIO_PLAYBACK_CORR_FNCT   CorrCallbackPtr;     /* Ptr to app callback for playback corr.               */
#endif
#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_PLAYBACK_CORR_EN     == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_CORR_EN       == DEF_ENABLED)
                                                               /* Nbr of buff diff at which heavy correction is applied.*/
           CPU_INT08S                      CorrBoundaryHeavyPos;
           CPU_INT08S                      CorrBoundaryHeavyNeg;
#endif
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
           USBD_AUDIO_STAT                *StatPtr;             /* Statistics for given AS IF.                          */
#endif
} USBD_AUDIO_AS_IF_SETTINGS;

struct  usbd_audio_as_if {
           USBD_AUDIO_AS_HANDLE            Handle;              /* AudioStreaming IF handle.                            */
           USBD_AUDIO_COMM                *CommPtr;             /* Ptr to comm struct.                                  */
           CPU_INT08U                      DevNbr;              /* Dev nbr.                                             */
           CPU_INT08U                      ClassNbr;            /* Class nbr.                                           */
           CPU_INT08U                      AS_IF_Nbr;           /* AudioStreaming IF nbr attributed by the core.        */
           USBD_AUDIO_AS_IF_SETTINGS      *AS_IF_SettingsPtr;   /* Ptr to AS settings.                                  */
                                                                /* ALTERNATE SETTINGS:                                  */
           USBD_AUDIO_AS_IF_ALT           *AS_IF_AltCurPtr;     /* Alt setting cur ptr.                                 */

           USBD_AUDIO_AS_IF               *NextPtr;             /* Ptr to nxt AudioStreaming IF struct.                 */
};

                                                                /* Buf desc used during streaming.                      */
struct  usbd_audio_buf_desc {
           void                           *BufPtr;
           CPU_INT16U                      BufLen;
};


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*********************************************************************************************************
*/

#define  USBD_AUDIO_BIT_SET_CNT(nbr_bit_set, var_to_test)   do {                                                                      \
                                                                CPU_INT32U  __bitmap;                                                 \
                                                                                                                                      \
                                                                __bitmap = (var_to_test);                                             \
                                                                for ((nbr_bit_set) = 0u; (nbr_bit_set) < __bitmap; (nbr_bit_set)++) { \
                                                                    __bitmap &= (__bitmap - 1u);                                      \
                                                                }                                                                     \
                                                            } while (0);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Implemented in usbd_audio_processing.c.              */
void                 USBD_Audio_ProcessingInit     (       CPU_INT16U               msg_qty,
                                                           USBD_ERR                *p_err);

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
USBD_AUDIO_AS_IF    *USBD_Audio_AS_IF_Alloc        (       USBD_ERR                *p_err);
#endif

void                 USBD_Audio_AC_TerminalCtrl    (       CPU_INT08U               terminal_id,
                                                           USBD_AUDIO_ENTITY_TYPE   recipient,
                                                    const  void                    *p_recipient_info,
                                                           CPU_INT08U               b_req,
                                                           CPU_INT16U               w_val,
                                                           CPU_INT08U              *p_buf,
                                                           CPU_INT16U               req_len,
                                                           USBD_ERR                *p_err);

void                 USBD_Audio_AC_UnitCtrl        (       CPU_INT08U               unit_id,
                                                           USBD_AUDIO_ENTITY_TYPE   recipient,
                                                    const  void                    *p_recipient_info,
                                                           CPU_INT08U               b_req,
                                                           CPU_INT16U               w_val,
                                                           CPU_INT08U              *p_buf,
                                                           CPU_INT16U               req_len,
                                                           USBD_ERR                *p_err);

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
void                 USBD_Audio_AS_EP_CtrlProcess  (       USBD_AUDIO_AS_IF        *p_as_if,
                                                    const  USBD_AUDIO_AS_ALT_CFG   *p_as_cfg,
                                                           CPU_INT16U               ep_addr,
                                                           CPU_INT08U               b_req,
                                                           CPU_INT16U               w_val,
                                                           CPU_INT08U              *p_buf,
                                                           CPU_INT16U               req_len,
                                                           USBD_ERR                *p_err);
#endif

#if (USBD_AUDIO_CFG_RECORD_EN == DEF_ENABLED)
void                 USBD_Audio_RecordTaskHandler  (void);
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)
void                 USBD_Audio_PlaybackTaskHandler(void);
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
void                 USBD_Audio_AS_IF_Start        (       USBD_AUDIO_AS_IF        *p_as_if,
                                                           USBD_ERR                *p_err);

void                 USBD_Audio_AS_IF_Stop         (       USBD_AUDIO_AS_IF        *p_as_if,
                                                           USBD_ERR                *p_err);
#endif


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
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif
