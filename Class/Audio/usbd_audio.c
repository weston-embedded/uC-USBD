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
* Filename : usbd_audio.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)       : (1) This Audio class complies with the following specifications:
*
*                     (a) 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998'
*                     (b) 'USB Device Class Definition for Terminal Types, Release 1.0, March 18, 1998'
*                     (c) 'USB Device Class Definition for Audio Data Formats, Release 1.0, March 18,
*                         1998'
*                     (d) 'USB Audio Device Class Specification for Basic Audio Devices, Release 1.0,
*                          November 24, 2009'
*
*                 (2) This Audio class does NOT support:
*
*                     (a) Processor Unit
*                     (b) Extension Unit
*                     (c) Data format Type II
*                     (d) Data format Type III
*                     (e) Associated interfaces
*                     (f) Class-specific requests: GET_STAT, GET_MEM, SET_MEM
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
#define    USBD_AUDIO_MODULE
#include  "usbd_audio.h"
#include  "usbd_audio_os.h"
#include  "usbd_audio_processing.h"
#include  "usbd_audio_internal.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  USBD_DBG_AUDIO_MSG(msg)                            USBD_DBG_GENERIC    ((msg),              \
                                                                                  USBD_EP_ADDR_NONE, \
                                                                                  USBD_IF_NBR_NONE)

#define  USBD_DBG_AUDIO_ERR(msg, err)                       USBD_DBG_GENERIC_ERR((msg),              \
                                                                                  USBD_EP_ADDR_NONE, \
                                                                                  USBD_IF_NBR_NONE,  \
                                                                                 (err))

#define  USBD_AUDIO_CTRL_REQ_TIMEOUT_mS                 5000u

#define  USBD_AUDIO_REQ_ENTITY_ID_MASK                0xFF00u
#define  USBD_AUDIO_REQ_IF_MASK                       0x00FFu

#define  USBD_AUDIO_TYPE_IT                                1u   /* Input Terminal Type.                                 */
#define  USBD_AUDIO_TYPE_OT                                2u   /* Output Terminal Type.                                */
#define  USBD_AUDIO_TYPE_FU                                3u   /* Function Unit Type.                                  */
#define  USBD_AUDIO_TYPE_MU                                4u   /* Mixer Unit Type.                                     */
#define  USBD_AUDIO_TYPE_SU                                5u   /* Selector Unit Type.                                  */
#define  USBD_AUDIO_TYPE_PU                                6u   /* Processor Unit Type.                                 */


/*
*********************************************************************************************************
*                                          AUDIO CLASS DESC
*
* Note(s):  (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 3.2 and appendix A.2 for more details about audio subclass.
*
*           (2) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               appendix A.4 for more details about Audio class-specific descriptor type.
*
*           (3) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 4.3 for more details about AudioControl interface descriptors length.
*
*               (a) See Table 4-2, 'bLength' field description for more details.
*
*               (b) See Table 4-3, 'bLength' field description for more details.
*
*               (c) See Table 4-4, 'bLength' field description for more details.
*
*               (d) See Table 4-5, 'bLength' field description for more details.
*
*               (e) See Table 4-6, 'bLength' field description for more details.
*
*               (f) See Table 4-7, 'bLength' field description for more details.
*
*               (g) See Table 4-8, 'bLength' field description for more details.
*
*               (h) See Table 4-19, 'bLength' field description for more details.
*
*               (g) See Table 4-21, 'bLength' field description for more details.
*
*           (4) See 'USB Device Class Definition for Audio Data Formats, Release 1.0, March 18, 1998',
*               Table 2-1, 'bLength' field description for more details.
*********************************************************************************************************
*/
                                                                /* Audio IF subclass (see Note #1).                     */
#define  USBD_AUDIO_SUBCLASS_AUDIO_CTRL                 0x01u
#define  USBD_AUDIO_SUBCLASS_AUDIO_STREAMING            0x02u
#define  USBD_AUDIO_SUBCLASS_MIDI_STREAMING             0x03u

                                                                /* Audio class-specific desc type (see Note #2).        */
#define  USBD_AUDIO_DESC_TYPE_CS_IF                     0x24u
#define  USBD_AUDIO_DESC_TYPE_CS_EP                     0x25u

                                                                /* class-specific desc len (see Note #3).               */
#define  USBD_AUDIO_DESC_LEN_AC_HEADER_MIN                 8u   /* See Note #3a.                                        */
#define  USBD_AUDIO_DESC_LEN_AC_IT                        12u   /* See Note #3b.                                        */
#define  USBD_AUDIO_DESC_LEN_AC_OT                         9u   /* See Note #3c.                                        */
#define  USBD_AUDIO_DESC_LEN_AC_MU_MIN                    10u   /* See Note #3d.                                        */
#define  USBD_AUDIO_DESC_LEN_AC_SU_MIN                     6u   /* See Note #3e.                                        */
#define  USBD_AUDIO_DESC_LEN_AC_FU_MIN                     7u   /* See Note #3f.                                        */
#define  USBD_AUDIO_DESC_LEN_AC_PU_MIN                    13u   /* See Note #3g.                                        */
#define  USBD_AUDIO_DESC_LEN_AS_GENERAL                    7u   /* See Note #3h.                                        */
#define  USBD_AUDIO_DESC_LEN_EP_GENERAL                    7u   /* See Note #3g.                                        */
#define  USBD_AUDIO_DESC_LEN_TYPE_I_FMT_MIN                8u   /* See Note #4.                                         */
#define  USBD_AUDIO_DESC_LEN_TYPE_I_SAM_FREQ               3u


/*
*********************************************************************************************************
*                                            AUDIO CTRL IF
* Note(s):  (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               appendix A.5 for more details about AudioControl class-specific interface descriptor
*               subtype.
*********************************************************************************************************
*/

                                                                /* Desc subtype (see Note #1).                          */
#define  USBD_AUDIO_DESC_SUBTYPE_AC_HEADER              0x01u
#define  USBD_AUDIO_DESC_SUBTYPE_IT                     0x02u
#define  USBD_AUDIO_DESC_SUBTYPE_OT                     0x03u
#define  USBD_AUDIO_DESC_SUBTYPE_MU                     0x04u
#define  USBD_AUDIO_DESC_SUBTYPE_SU                     0x05u
#define  USBD_AUDIO_DESC_SUBTYPE_FU                     0x06u
#define  USBD_AUDIO_DESC_SUBTYPE_PU                     0x07u
#define  USBD_AUDIO_DESC_SUBTYPE_XU                     0x08u


/*
*********************************************************************************************************
*                                         AUDIO STREAMING IF
*
* Note(s):  (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               appendix A.6 for more details about AudioStreaming class-specific interface descriptor
*               subtype.
*
*           (2) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               appendix A.8 for more details about AudioStreaming class-specific endpoint descriptor
*               subtype.
*********************************************************************************************************
*/

                                                                /* AS IF desc subtype (see Note #1).                    */
#define  USBD_AUDIO_DESC_SUBTYPE_AS_GENERAL             0x01u
#define  USBD_AUDIO_DESC_SUBTYPE_FMT_TYPE               0x02u
#define  USBD_AUDIO_DESC_SUBTYPE_FMT_SPECIFIC           0x03u

                                                                /* Audio EP desc subtype (see Note #2).                 */
#define  USBD_AUDIO_DESC_SUBTYPE_EP_GENERAL             0x01u


/*
*********************************************************************************************************
*                                           AUDIO DATA FMT
*
* Note(s):  (1) See 'USB Device Class Definition for Audio Data Formats, Release 1.0, March 18, 1998',
*               Appendix A.2 for more details about audio data format Type codes.
*********************************************************************************************************
*/

                                                                /* See Note #1.                                         */
#define  USBD_AUDIO_FMT_TYPE_UNDEFINED                  0x00u
#define  USBD_AUDIO_FMT_TYPE_I                          0x01u
#define  USBD_AUDIO_FMT_TYPE_II                         0x02u
#define  USBD_AUDIO_FMT_TYPE_III                        0x03u

#define  USBD_AUDIO_FMT_TYPE_I_CONT_FREQ_NBR               2u   /* Continuous freq has a lower and upper freq.          */


/*
*********************************************************************************************************
*                                          AUDIO DATA STREAM
*
* Note(s):  (1) The maximum throughput for Audio 1.0 is determined by the maximum packet size for
*               an isochronous endpoint at full-speed, that is 1023 bytes/ms.
*
*           (2) With stream correction enabled, it is required to allocate a certain number of buffers
*               that takes into account the underrun and overrun boundaries around the pre-buffering
*               value.
*
*           (3) Without any stream correction enabled, it is required to allocate at least
*               USBD_AUDIO_STREAM_BUF_QTY_MIN buffers per stream to ensure the proper functioning of the
*               audio stream given a pre-buffering equal to half of the maximum number of buffers.
*               The minimum pre-buffering is 2 buffers, that is 2 ms worth of audio data.
*********************************************************************************************************
*/

#define  USBD_AUDIO_MAX_THROUGHPUT                      1023u   /* Max throughput in bytes/ms for Audio 1.0...          */
                                                                /* ...(see Note #1).                                    */

#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_PLAYBACK_CORR_EN     == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_CORR_EN       == DEF_ENABLED)
#define  USBD_AUDIO_STREAM_BUF_QTY_MIN                     6u   /* See Note #2.                                         */
#else
#define  USBD_AUDIO_STREAM_BUF_QTY_MIN                     4u   /* See Note #3.                                         */
#endif

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
*
* Note(s):  (1) This table contains one AIC (Audio Interface Collection) per element. The first element
*               (index 0) corresponds to class number 0 and so on.
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Audio class instances tbl (see Note #1).             */
static  USBD_AUDIO_CTRL            USBD_Audio_CtrlTbl[USBD_AUDIO_CFG_MAX_NBR_AIC];
static  CPU_INT08U                 USBD_Audio_CtrlNbrNext;
                                                                /* Audio class comm tbl.                                */
static  USBD_AUDIO_COMM            USBD_Audio_CommTbl[USBD_AUDIO_MAX_NBR_COMM];
static  CPU_INT08U                 USBD_Audio_CommNbrNext;
                                                                /* Input Terminal tbl.                                  */
static  USBD_AUDIO_IT              USBD_Audio_IT_Tbl[USBD_AUDIO_CFG_MAX_NBR_IT];
static  CPU_INT08U                 USBD_Audio_IT_NbrNext;
                                                                /* Output Terminal tbl.                                 */
static  USBD_AUDIO_OT              USBD_Audio_OT_Tbl[USBD_AUDIO_CFG_MAX_NBR_OT];
static  CPU_INT08U                 USBD_Audio_OT_NbrNext;
                                                                /* Feature Unit tbl.                                    */
static  USBD_AUDIO_FU              USBD_Audio_FU_Tbl[USBD_AUDIO_CFG_MAX_NBR_FU];
static  CPU_INT08U                 USBD_Audio_FU_NbrNext;

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
                                                                /* Mixer Unit tbl.                                      */
static  USBD_AUDIO_MU              USBD_Audio_MU_Tbl[USBD_AUDIO_CFG_MAX_NBR_MU];
static  CPU_INT08U                 USBD_Audio_MU_NbrNext;
#endif

#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
                                                                /* Selector Unit tbl.                                   */
static  USBD_AUDIO_SU              USBD_Audio_SU_Tbl[USBD_AUDIO_CFG_MAX_NBR_SU];
static  CPU_INT08U                 USBD_Audio_SU_NbrNext;
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
                                                                /* Alternate Setting tbl.                               */
static  USBD_AUDIO_AS_IF_ALT       USBD_Audio_AS_IF_AltTbl[USBD_AUDIO_MAX_NBR_IF_ALT];
static  CPU_INT08U                 USBD_Audio_AS_IF_AltNbrNext;
                                                                /* AS IF settings tbl.                                  */
static  USBD_AUDIO_AS_IF_SETTINGS  USBD_Audio_AS_IF_SettingsTbl[USBD_AUDIO_MAX_NBR_AS_IF_SETTINGS];
static  CPU_INT08U                 USBD_Audio_AS_IF_SettingsNbrNext;
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* AudioControl IF Class callbacks.                     */
static  void                   USBD_Audio_AC_Conn            (       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     void                    *p_if_class_arg);

static  void                   USBD_Audio_AC_Disconn         (       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     void                    *p_if_class_arg);

static  void                   USBD_Audio_AC_IF_Desc         (       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     CPU_INT08U               if_nbr,
                                                                     CPU_INT08U               if_alt_nbr,
                                                                     void                    *p_if_class_arg,
                                                                     void                    *p_if_alt_class_arg);

static  CPU_INT16U             USBD_Audio_AC_IF_DescSizeGet  (       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     CPU_INT08U               if_nbr,
                                                                     CPU_INT08U               if_alt_nbr,
                                                                     void                    *p_if_class_arg,
                                                                     void                    *p_if_alt_class_arg);

static  CPU_BOOLEAN            USBD_Audio_AC_ClassReq        (       CPU_INT08U               dev_nbr,
                                                              const  USBD_SETUP_REQ          *p_setup_req,
                                                                     void                    *p_if_class_arg);

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
                                                                /* AudioStreaming IF Class callbacks.                   */
static  void                   USBD_Audio_AS_Conn            (       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     void                    *p_if_class_arg);

static  void                   USBD_Audio_AS_AltSettingUpdate(       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     CPU_INT08U               if_nbr,
                                                                     CPU_INT08U               if_alt_nbr,
                                                                     void                    *p_if_class_arg,
                                                                     void                    *p_if_alt_class_arg);

static  void                   USBD_Audio_AS_IF_Desc         (       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     CPU_INT08U               if_nbr,
                                                                     CPU_INT08U               if_alt_nbr,
                                                                     void                    *p_if_class_arg,
                                                                     void                    *p_if_alt_class_arg);

static  CPU_INT16U             USBD_Audio_AS_IF_DescSizeGet  (       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     CPU_INT08U               if_nbr,
                                                                     CPU_INT08U               if_alt_nbr,
                                                                     void                    *p_if_class_arg,
                                                                     void                    *p_if_alt_class_arg);

static  void                   USBD_Audio_AS_EP_Desc         (       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     CPU_INT08U               if_nbr,
                                                                     CPU_INT08U               if_alt_nbr,
                                                                     CPU_INT08U               ep_addr,
                                                                     void                    *p_if_class_arg,
                                                                     void                    *p_if_alt_class_arg);

static  CPU_INT16U             USBD_Audio_AS_EP_DescSizeGet  (       CPU_INT08U               dev_nbr,
                                                                     CPU_INT08U               cfg_nbr,
                                                                     CPU_INT08U               if_nbr,
                                                                     CPU_INT08U               if_alt_nbr,
                                                                     CPU_INT08U               ep_addr,
                                                                     void                    *p_if_class_arg,
                                                                     void                    *p_if_alt_class_arg);

static  CPU_BOOLEAN            USBD_Audio_AS_ClassReq        (       CPU_INT08U               dev_nbr,
                                                              const  USBD_SETUP_REQ          *p_setup_req,
                                                                     void                    *p_if_class_arg);
#endif

static  void                  *USBD_Audio_AC_RecipientGet    (const  USBD_AUDIO_CTRL         *p_ctrl,
                                                                     CPU_INT08U               entity_id,
                                                                     USBD_AUDIO_ENTITY_TYPE  *p_recipient_type);

#if (USBD_AUDIO_CFG_PLAYBACK_EN  == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN    == DEF_ENABLED) || \
    (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
static  USBD_AUDIO_COMM       *USBD_Audio_CommGet            (const  USBD_AUDIO_CTRL         *p_ctrl,
                                                                     CPU_INT08U               cfg_nbr);
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_INT16U             USBD_Audio_MaxPktLenGet       (       USBD_AUDIO_AS_ALT_CFG   *p_as_cfg,
                                                                     USBD_ERR                *p_err);
#endif


/*
*********************************************************************************************************
*                                          AUDIO CLASS DRIVER
*********************************************************************************************************
*/

static  USBD_CLASS_DRV  USBD_Audio_AC_Drv = {                   /* AudioControl IF class driver.                        */
    USBD_Audio_AC_Conn,
    USBD_Audio_AC_Disconn,
    DEF_NULL,
    DEF_NULL,
    USBD_Audio_AC_IF_Desc,
    USBD_Audio_AC_IF_DescSizeGet,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    USBD_Audio_AC_ClassReq,
    DEF_NULL,

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
    DEF_NULL,
    DEF_NULL
#endif
};

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  USBD_CLASS_DRV  USBD_Audio_AS_Drv = {                   /* AudioStreaming IF class driver.                      */
    USBD_Audio_AS_Conn,
    DEF_NULL,
    USBD_Audio_AS_AltSettingUpdate,
    DEF_NULL,
    USBD_Audio_AS_IF_Desc,
    USBD_Audio_AS_IF_DescSizeGet,
    USBD_Audio_AS_EP_Desc,
    USBD_Audio_AS_EP_DescSizeGet,
    DEF_NULL,
    USBD_Audio_AS_ClassReq,
    DEF_NULL,

#if (USBD_CFG_MS_OS_DESC_EN == DEF_ENABLED)
    DEF_NULL,
    DEF_NULL
#endif
};
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
*                                          USBD_Audio_Init()
*
* Description : Initialize Audio class.
*
* Argument(s) : msg_qty     Maximum quantity of messages for playback and record tasks' queues.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                           USBD_ERR_NONE   Audio class successfully initialized.
*
*                           -RETURNED BY USBD_Audio_ProcessingInit()-
*                           See USBD_Audio_ProcessingInit() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_Audio_Init (CPU_INT16U   msg_qty,
                       USBD_ERR    *p_err)
{
    CPU_INT08U  ix;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (msg_qty == 0u) {                                        /* Validate msg qty.                                    */
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }
#endif

                                                                /* ------------ INITIALIZE CLASS STRUCTURES ----------- */
    for (ix = 0u; ix < USBD_AUDIO_CFG_MAX_NBR_AIC; ix++) {      /* Init audio class instances tbl.                      */

        Mem_Clr((void *)&USBD_Audio_CtrlTbl[ix],
                        sizeof(USBD_AUDIO_CTRL));

        USBD_Audio_CtrlTbl[ix].DevNbr = USBD_DEV_NBR_NONE;
    }

    Mem_Clr((void *)&USBD_Audio_CommTbl[0u],                    /* Init Audio class comm tbl.                           */
                    (USBD_AUDIO_MAX_NBR_COMM * sizeof(USBD_AUDIO_COMM)));

    Mem_Clr((void *)&USBD_Audio_IT_Tbl[0u],                     /* Init Input Terminal tbl.                             */
                    (USBD_AUDIO_CFG_MAX_NBR_IT * sizeof(USBD_AUDIO_IT)));

    Mem_Clr((void *)&USBD_Audio_OT_Tbl[0u],                     /* Init Output Terminal tbl.                            */
                    (USBD_AUDIO_CFG_MAX_NBR_OT * sizeof(USBD_AUDIO_OT)));

    Mem_Clr((void *)&USBD_Audio_FU_Tbl[0u],                     /* Init Feature Unit tbl.                               */
                    (USBD_AUDIO_CFG_MAX_NBR_FU * sizeof(USBD_AUDIO_FU)));

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
    Mem_Clr((void *)&USBD_Audio_MU_Tbl[0u],                     /* Init Mixer Unit tbl.                                 */
                    (USBD_AUDIO_CFG_MAX_NBR_MU * sizeof(USBD_AUDIO_MU)));
#endif

#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
    Mem_Clr((void *)&USBD_Audio_SU_Tbl[0u],                     /* Init Selector Unit tbl.                              */
                    (USBD_AUDIO_CFG_MAX_NBR_SU * sizeof(USBD_AUDIO_SU)));
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
    {
        USBD_AUDIO_AS_IF_ALT  *p_as_if_alt;


        for (ix = 0u; ix < USBD_AUDIO_MAX_NBR_IF_ALT; ix++) {   /* Init alternate setting tbl.                          */
            p_as_if_alt                = &USBD_Audio_AS_IF_AltTbl[ix];
            p_as_if_alt->AS_CfgPtr     =  DEF_NULL;
            p_as_if_alt->DataIsocAddr  =  USBD_EP_ADDR_NONE;
            p_as_if_alt->SynchIsocAddr =  USBD_EP_ADDR_NONE;
            p_as_if_alt->MaxPktLen     =  0u;
        }

        Mem_Clr((void *)&USBD_Audio_AS_IF_SettingsTbl[0u],      /* Init AS IF settings tbl.                             */
                        (USBD_AUDIO_MAX_NBR_AS_IF_SETTINGS * sizeof(USBD_AUDIO_AS_IF_SETTINGS)));

        USBD_Audio_AS_IF_AltNbrNext = 0u;
    }
#endif

    USBD_Audio_CtrlNbrNext = 0u;
    USBD_Audio_CommNbrNext = 0u;
    USBD_Audio_IT_NbrNext  = 0u;
    USBD_Audio_OT_NbrNext  = 0u;
    USBD_Audio_FU_NbrNext  = 0u;
#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
    USBD_Audio_MU_NbrNext  = 0u;
#endif
#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
    USBD_Audio_SU_NbrNext  = 0u;
#endif

    USBD_Audio_ProcessingInit(msg_qty, p_err);                  /* Init Audio Processing layer.                         */
}


/*
*********************************************************************************************************
*                                           USBD_Audio_Add()
*
* Description : Create a new instance of the audio class (see Note #1).
*
* Argument(s) : entity_cnt              Number of entities within the audio function.
*
*               p_audio_drv_common_api  Pointer to common audio codec driver API.
*
*               p_audio_event_fnct      Pointer to audio event callbacks.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE                   Audio class instance successfully added.
*                           USBD_ERR_NULL_PTR               Argument 'p_audio_drv_common_api' passed a
*                                                           NULL pointer.
*                           USBD_ERR_AUDIO_INSTANCE_ALLOC   Audio class instances NOT available.
*                           USBD_ERR_ALLOC                  Cannot allocate request buffer from heap.
*
*                           -RETURNED BY p_audio_drv_common_api->Init()-
*                           See Audio Peripheral Driver Init() function for additional return error codes.
*
* Return(s)   : Class instance number, if NO error(s).
*
*               USBD_CLASS_NBR_NONE,   otherwise.
*
* Note(s)     : (1) One audio class instance = one audio function = one Audio Interface Collection (AIC).
*
*               (2) In this version of the audio class, only the 1st form of data parameter is
*                   considered. Size of the request buffer should be therefore limited.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   Table 5-20, for an example of the first form request.
*********************************************************************************************************
*/

CPU_INT08U  USBD_Audio_Add (       CPU_INT08U                  entity_cnt,
                            const  USBD_AUDIO_DRV_COMMON_API  *p_audio_drv_common_api,
                            const  USBD_AUDIO_EVENT_FNCTS     *p_audio_event_fnct,
                                   USBD_ERR                   *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    CPU_INT08U        class_nbr;
    LIB_ERR           err_lib;
    CPU_SR_ALLOC();


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(USBD_CLASS_NBR_NONE);
    }

    if ((p_audio_drv_common_api       == DEF_NULL) ||
        (p_audio_drv_common_api->Init == DEF_NULL)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (USBD_CLASS_NBR_NONE);
    }
#endif
                                                                /* ---------------- ADD CLASS INSTANCE ---------------- */
    CPU_CRITICAL_ENTER();
    class_nbr = USBD_Audio_CtrlNbrNext;                         /* Alloc new audio class instance.                      */

    if (class_nbr >= USBD_AUDIO_CFG_MAX_NBR_AIC) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_INSTANCE_ALLOC;
        return (USBD_CLASS_NBR_NONE);
    }

    USBD_Audio_CtrlNbrNext++;                                   /* Next avail audio class instance nbr.                 */
    CPU_CRITICAL_EXIT();
                                                                /* ------------- STORE CLASS INSTANCE INFO ------------ */
    p_ctrl            = &USBD_Audio_CtrlTbl[class_nbr];         /* Get audio class instance.                            */
    p_ctrl->ClassNbr  =  class_nbr;
                                                                /* See Note #2.                                         */
    p_ctrl->ReqBufPtr = (CPU_INT08U *)Mem_SegAllocExt("Audio Request Buffer",
                                                       DEF_NULL,
                                                       USBD_AUDIO_CFG_CLASS_REQ_MAX_LEN,
                                                       USBD_CFG_BUF_ALIGN_OCTETS,
                                                       DEF_NULL,
                                                      &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return (USBD_CLASS_NBR_NONE);
    }

    p_ctrl->EventFnctPtr = p_audio_event_fnct;
    p_ctrl->EntityID_Nxt = 0u;

    p_ctrl->EntityCnt       = entity_cnt;
                                                                /* Alloc ptr to entity struct for ID management.    */
    p_ctrl->EntityID_TblPtr = (USBD_AUDIO_ENTITY **)Mem_SegAlloc("Entity ID Tbl",
                                                                  DEF_NULL,
                                                                 (entity_cnt * sizeof(USBD_AUDIO_ENTITY *)),
                                                                 &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return (USBD_CLASS_NBR_NONE);
    }
                                                                /* Init the audio driver associated to this AIC.        */
    p_audio_drv_common_api->Init(&p_ctrl->DrvInfo,
                                  p_err);

    return (class_nbr);
}


/*
*********************************************************************************************************
*                                          USBD_Audio_CfgAdd()
*
* Description : Add an audio class instance into the specified configuration (see Note #1).
*
* Argument(s) : class_nbr   Class instance number.
*
*               dev_nbr     Device number.
*
*               cfg_nbr     Configuration index to add Audio class instance to.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE                   Audio class configuration successfully added.
*                           USBD_ERR_CFG_INVALID_NBR        Invalid configuration number.
*                           USBD_ERR_CLASS_INVALID_NBR      Invalid class number.
*                           USBD_ERR_INVALID_ARG            Class number associated with a different device number.
*                           USBD_ERR_AUDIO_INSTANCE_ALLOC   Audio class communication instances NOT available.
*
*                           -RETURNED BY USBD_IF_Add()-
*                           See USBD_IF_Add() for additional return error codes.
*
* Return(s)   : None.
*
* Note(s)     : (1) Called several times, it allows to create multiple instances and multiple configurations.
*                   For instance, the following architecture could be created :
*
*                    Audio Class 0 (device number 0)
*                       |-- Configuration 0
*                       |-- Configuration 1
*
*                    Audio Class 1 (device number 1)
*                       |-- Configuration 0
*                       |-- Configuration 1
*
*               (2) Configuration Descriptor corresponding to an audio device has the following format:
*
*                   Configuration Descriptor
*                   |-- Interface Descriptor (AudioControl)                                 [standard]
*                       |-- Header Descriptor                                               [class-specific]
*                       |-- Unit Descriptor(s)                                              [class-specific]
*                       |-- Terminal Descriptor(s)                                          [class-specific]
*                       |-- Endpoint Descriptor (Interrupt IN) - optional                   [standard]
*                   |-- Interface Descriptor (AudioStreaming)                               [standard]
*                       |-- AS Interface Descriptor                                         [class-specific]
*                       |-- AS Format Type Descriptor                                       [class-specific]
*                           |-- AS Format-Specific Descriptor(s)                            [class-specific]
*                       |-- Endpoint Descriptor (Isochronous IN or OUT Data)                [standard]
*                           |-- AS Isochronous Audio Data Endpoint Descriptor               [class-specific]
*                           |-- Endpoint Descriptor (Isochronous OUT or IN Synch endpoint)  [standard]
*                   |-- Interface Descriptor (AudioStreaming)                               [standard]
*                       |-- ...
*                   |-- Interface Descriptor (AudioStreaming)                               [standard]
*                       |-- ...
*
*                   The function USBD_Audio_CfgAdd() only adds the Interface Descriptor (AudioControl).
*                   Other standard and class-specific descriptors are added by functions:
*                   USBD_Audio_IT_Add()
*                   USBD_Audio_OT_Add()
*                   USBD_Audio_FU_Add()
*                   USBD_Audio_MU_Add()
*                   USBD_Audio_SU_Add()
*                   USBD_Audio_AS_IF_Add()
*********************************************************************************************************
*/

void  USBD_Audio_CfgAdd (CPU_INT08U   class_nbr,
                         CPU_INT08U   dev_nbr,
                         CPU_INT08U   cfg_nbr,
                         USBD_ERR    *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_COMM  *p_comm;
    CPU_INT16U        comm_ix;
    CPU_SR_ALLOC();


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    if (cfg_nbr == USBD_CFG_NBR_NONE) {                         /* Check that config number is valid                    */
       *p_err = USBD_ERR_CFG_INVALID_NBR;
        return;
    }

    CPU_CRITICAL_ENTER();
    if (class_nbr >= USBD_Audio_CtrlNbrNext) {                  /* Check that class number is valid                     */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }
    CPU_CRITICAL_EXIT();
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if ((p_ctrl->DevNbr != USBD_DEV_NBR_NONE) &&                /* Chk if class is associated with a different dev.     */
        (p_ctrl->DevNbr != dev_nbr)) {
        *p_err = USBD_ERR_INVALID_ARG;
         return;
    }

    p_comm = USBD_Audio_CommGet(p_ctrl, cfg_nbr);
    if(p_comm != DEF_NULL) {
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }
#endif
                                                                /* --------------- ALLOC NEW COMM INFO ---------------- */
    CPU_CRITICAL_ENTER();
    comm_ix = USBD_Audio_CommNbrNext;

    if (comm_ix >= USBD_AUDIO_MAX_NBR_COMM) {                   /* Check that comm index is within max range.           */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_INSTANCE_ALLOC;
        return;
    }

    USBD_Audio_CommNbrNext++;
    CPU_CRITICAL_EXIT();

    p_comm         = &USBD_Audio_CommTbl[comm_ix];
    p_comm->CfgNbr =  cfg_nbr;
                                                                /* ------- CFG DESC CONSTRUCTION (see Note #2) -------- */
    (void)USBD_IF_Add(        dev_nbr,                          /* Add AudioControl IF to cfg.                          */
                              cfg_nbr,
                             &USBD_Audio_AC_Drv,
                      (void *)p_comm,
                              DEF_NULL,
                              USBD_CLASS_CODE_AUDIO,
                              USBD_AUDIO_SUBCLASS_AUDIO_CTRL,
                              USBD_PROTOCOL_CODE_USE_IF_DESC,
                             "Audio Control Interface",
                              p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }
                                                                /* ---------- STORE AUDIO CLASS INSTANCE INFO ----------*/
    p_ctrl->DevNbr  = dev_nbr;                                  /* Save device number                                   */
    p_comm->CtrlPtr = p_ctrl;                                   /* Save pointer to control struct                       */

    CPU_CRITICAL_ENTER();
    p_ctrl->State = USBD_AUDIO_STATE_INIT;
    CPU_CRITICAL_EXIT();
                                                                /* Add class instance info to audio comm list.          */
    if (p_ctrl->CommHeadPtr == DEF_NULL) {                      /* First element of list.                               */
        p_ctrl->CommHeadPtr = p_comm;
    } else {
        p_ctrl->CommTailPtr->NextPtr = p_comm;                  /* Element at the end of list.                          */
    }
    p_ctrl->CommTailPtr = p_comm;
    p_ctrl->CommCnt++;                                          /* Increment nbr of audio comm info struct.             */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_Audio_IsConn()
*
* Description : Get the audio class connection state.
*
* Argument(s) : class_nbr   Class instance number.
*
* Return(s)   : DEF_YES, if Audio class is connected.
*
*               DEF_NO,  otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  USBD_Audio_IsConn (CPU_INT08U  class_nbr)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_DEV_STATE    state;
    USBD_ERR          err;
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_CRITICAL_ENTER();
    if (class_nbr >= USBD_Audio_CtrlNbrNext) {                  /* Check audio class instance nbr.                      */
        CPU_CRITICAL_EXIT();
        return (DEF_NO);
    }
    CPU_CRITICAL_EXIT();
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];
    state  =  USBD_DevStateGet(p_ctrl->DevNbr, &err);           /* Get dev state.                                       */

    CPU_CRITICAL_ENTER();
    if ((err           == USBD_ERR_NONE            ) &&
        (state         == USBD_DEV_STATE_CONFIGURED) &&
        (p_ctrl->State == USBD_AUDIO_STATE_CFG     )) {
        CPU_CRITICAL_EXIT();

        return (DEF_YES);
    } else {
        CPU_CRITICAL_EXIT();
        return (DEF_NO);
    }
}


/*
*********************************************************************************************************
*                                         USBD_Audio_IT_Add()
*
* Description : Add an Input Terminal to the specified class instance (i.e. audio function).
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_it_cfg    Pointer to the Input Terminal configuration structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Input Terminal successfully added.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                           USBD_ERR_NULL_PTR           Null pointer passed to 'p_it_cfg'.
*                           USBD_ERR_INVALID_ARG        Wrong copy protection level OR
*                                                       invalid spatial locations.
*                           USBD_ERR_AUDIO_IT_ALLOC     No Input Terminal structure available OR
*                                                       no entity structure available.
*
*                           -RETURNED BY USBD_StrAdd()-
*                           See USBD_StrAdd() for additional return error codes.
*
* Return(s)   : Terminal ID assigned by audio class, if NO error(s).
*
*               0,                                   otherwise (see Note #1).
*
* Note(s)     : (1) Audio 1.0 specification indicates that ID #0 is reserved for undefined ID. Thus it
*                   indicates an error.
*********************************************************************************************************
*/

CPU_INT08U  USBD_Audio_IT_Add (       CPU_INT08U          class_nbr,
                               const  USBD_AUDIO_IT_CFG  *p_it_cfg,
                                      USBD_ERR           *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_IT    *p_it;
    CPU_INT08U        it_nbr;
    CPU_INT08U        ix;
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_INT08U        log_ch_cnt;
#endif
    CPU_SR_ALLOC();


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }

    CPU_CRITICAL_ENTER();
    if (class_nbr >= USBD_Audio_CtrlNbrNext) {                  /* Check Audio Class nbr.                               */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }
    CPU_CRITICAL_EXIT();

    if (p_it_cfg == DEF_NULL) {                                 /* Check if IT config is null                           */
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }

    switch (p_it_cfg->CopyProtLevel) {                          /* Check copy protection level.                         */
        case USBD_AUDIO_CPL_NONE:
        case USBD_AUDIO_CPL0:
        case USBD_AUDIO_CPL1:
        case USBD_AUDIO_CPL2:
             break;

        default:
            *p_err = USBD_ERR_INVALID_ARG;
             return (0u);
    }

    if (p_it_cfg->LogChCfg == 0x00u) {                          /* Non-predefined spatial positions.                    */

        if (p_it_cfg->LogChNbr > USBD_AUDIO_LOG_CH_NBR_MAX) {   /* Check if nbr of log ch exceeds max allowed.          */
           *p_err = USBD_ERR_INVALID_ARG;
            return (0u);
        }

        switch (p_it_cfg->LogChNbr) {
                                                                /* Default spatial location supported.                  */
            case USBD_AUDIO_MONO:
            case USBD_AUDIO_STEREO:
            case 3u:
            case 4u:
            case USBD_AUDIO_5_1:
            case USBD_AUDIO_7_1:
                 break;

            default:
                *p_err = USBD_ERR_INVALID_ARG;
                 return (0u);
        }
    } else {                                                    /* Predefined spatial positions.                        */
                                                                /* Cnt nbr of log ch from spatial locations bitmap.     */
        USBD_AUDIO_BIT_SET_CNT(log_ch_cnt, p_it_cfg->LogChCfg);
                                                                /* Check if nbr of log ch deduced from spatial...       */
                                                                /* ...locations matches nbr of log ch declared.         */
        if ((p_it_cfg->LogChNbr != log_ch_cnt) ||
            (p_it_cfg->LogChNbr >  USBD_AUDIO_LOG_CH_NBR_MAX) ) {
           *p_err = USBD_ERR_INVALID_ARG;
            return (0u);
        }
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */

                                                                /* ------------ GET INPUT TERMINAL STRUCT ------------- */
    CPU_CRITICAL_ENTER();
    it_nbr = USBD_Audio_IT_NbrNext;

    if (it_nbr >= USBD_AUDIO_CFG_MAX_NBR_IT) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_IT_ALLOC;
        return (0u);
    }

    USBD_Audio_IT_NbrNext++;
    CPU_CRITICAL_EXIT();

    p_it = &USBD_Audio_IT_Tbl[it_nbr];

                                                                /* ---------- ADD NEW IT TO CLASS CTRL INFO ----------- */
    p_it->IT_CfgPtr = p_it_cfg;                                 /* Save configuration for later use                     */

    CPU_CRITICAL_ENTER();
    if (p_ctrl->EntityID_Nxt > p_ctrl->EntityCnt) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_IT_ALLOC;
        return (0u);
    }
    p_it->ID = p_ctrl->EntityID_Nxt + 1u;                       /* ID #0 reserved. Assigned ID needs +1 (see Note #1).  */
    ix       = p_ctrl->EntityID_Nxt;                            /* ID used as a 0-index.                                */
    p_ctrl->EntityID_Nxt++;                                     /* Nxt avail ID.                                        */
    CPU_CRITICAL_EXIT();

    p_it->EntityType            = USBD_AUDIO_ENTITY_IT;
    p_ctrl->EntityID_TblPtr[ix] = (USBD_AUDIO_ENTITY *)p_it;

    USBD_StrAdd(p_ctrl->DevNbr,                                 /* Add string describing this IT.                       */
                p_it_cfg->StrPtr,
                p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

   *p_err = USBD_ERR_NONE;
    return (p_it->ID);
}


/*
*********************************************************************************************************
*                                         USBD_Audio_OT_Add()
*
* Description : Add an Output Terminal to the specified class instance (i.e. audio function).
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_ot_cfg    Pointer to the Output Terminal configuration structure.
*
*               p_ot_api    Pointer to the audio codec API associated to this Output Terminal.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Output Terminal successfully added.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                           USBD_ERR_NULL_PTR           Null pointer passed to 'p_ot_cfg'/'p_ot_api'.
*                           USBD_ERR_AUDIO_OT_ALLOC     No Output Terminal structure available OR
*                                                       no entity structure available.
*
*                           -RETURNED BY USBD_StrAdd()-
*                           See USBD_StrAdd() for additional return error codes.
*
* Return(s)   : Terminal ID assigned by audio class, if NO error(s).
*
*               0,                                   otherwise (see Note #1).
*
* Note(s)     : (1) Audio 1.0 specification indicates that ID #0 is reserved for undefined ID. Thus it
*                   indicates an error.
*********************************************************************************************************
*/

CPU_INT08U  USBD_Audio_OT_Add (       CPU_INT08U                 class_nbr,
                               const  USBD_AUDIO_OT_CFG         *p_ot_cfg,
                               const  USBD_AUDIO_DRV_AC_OT_API  *p_ot_api,
                                      USBD_ERR                  *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_OT    *p_ot;
    CPU_INT08U        ot_nbr;
    CPU_INT08U        ix;
    CPU_SR_ALLOC();


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }

    CPU_CRITICAL_ENTER();
    if (class_nbr >= USBD_Audio_CtrlNbrNext) {                  /* Check Audio Class nbr.                               */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }
    CPU_CRITICAL_EXIT();

    if (p_ot_cfg == DEF_NULL) {                                 /* Check pointer to OT configuration.                   */
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
                                                                /* If Copy Prot ctrl enabled, check if OT API present.  */
    if ((p_ot_cfg->CopyProtEn      == DEF_ENABLED) &&
        ((p_ot_api                 == DEF_NULL) ||
         (p_ot_api->OT_CopyProtSet == DEF_NULL))) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */

                                                                /* ------------ GET OUTPUT TERMINAL STRUCT ------------ */
    CPU_CRITICAL_ENTER();
    ot_nbr = USBD_Audio_OT_NbrNext;

    if (ot_nbr >= USBD_AUDIO_CFG_MAX_NBR_OT) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_OT_ALLOC;
        return (0u);
    }

    USBD_Audio_OT_NbrNext++;

    CPU_CRITICAL_EXIT();

    p_ot = &USBD_Audio_OT_Tbl[ot_nbr];

                                                                /* ---------- ADD NEW OT TO CLASS CTRL INFO ----------- */
    p_ot->OT_CfgPtr  =  p_ot_cfg;                               /* Save configuration for later use                     */
    p_ot->OT_API_Ptr =  p_ot_api;
    p_ot->DrvInfoPtr = &p_ctrl->DrvInfo;

    CPU_CRITICAL_ENTER();
    if (p_ctrl->EntityID_Nxt > p_ctrl->EntityCnt) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_OT_ALLOC;
        return (0u);
    }
    p_ot->ID = p_ctrl->EntityID_Nxt + 1u;                       /* ID #0 reserved. Assigned ID needs +1 (see Note #1).  */
    ix       = p_ctrl->EntityID_Nxt;                            /* ID used as a 0-index.                                */
    p_ctrl->EntityID_Nxt++;                                     /* Nxt avail ID.                                        */
    CPU_CRITICAL_EXIT();

    p_ot->EntityType            =  USBD_AUDIO_ENTITY_OT;
    p_ctrl->EntityID_TblPtr[ix] = (USBD_AUDIO_ENTITY *)p_ot;

    USBD_StrAdd(p_ctrl->DevNbr,                                 /* Add string describing this OT.                       */
                p_ot_cfg->StrPtr,
                p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

   *p_err = USBD_ERR_NONE;
    return (p_ot->ID);
}


/*
*********************************************************************************************************
*                                         USBD_Audio_FU_Add()
*
* Description : Add a Feature Unit to the specified class instance (i.e. audio function).
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_fu_cfg    Pointer to the Feature Unit configuration structure.
*
*               p_fu_api    Pointer to the audio codec API associated to this Feature Unit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Feature Unit successfully added.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                           USBD_ERR_NULL_PTR           Null pointer passed to 'p_fu_cfg'/'p_fu_api'.
*                           USBD_ERR_AUDIO_FU_ALLOC     No Feature Unit structure available OR
*                                                       no entity structure available.
*
*                           -RETURNED BY USBD_StrAdd()-
*                           See USBD_StrAdd() for additional return error codes.
*
* Return(s)   : Unit ID assigned by audio class, if NO error(s).
*
*               0,                               otherwise (see Note #1).
*
* Note(s)     : (1) Audio 1.0 specification indicates that ID #0 is reserved for undefined ID. Thus it
*                   indicates an error.
*********************************************************************************************************
*/

CPU_INT08U  USBD_Audio_FU_Add (       CPU_INT08U                 class_nbr,
                               const  USBD_AUDIO_FU_CFG         *p_fu_cfg,
                               const  USBD_AUDIO_DRV_AC_FU_API  *p_fu_api,
                                      USBD_ERR                  *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_FU    *p_fu;
    CPU_INT08U        fu_nbr;
    CPU_INT08U        ix;
    CPU_SR_ALLOC();


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }

    CPU_CRITICAL_ENTER();
    if (class_nbr >= USBD_Audio_CtrlNbrNext) {                  /*  Check Audio Class nbr.                              */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }
    CPU_CRITICAL_EXIT();

    if (p_fu_cfg == DEF_NULL) {                                 /* Check Pointer to FU configuration                    */
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
                                                                /* Any FU should supports minimally mute and vol ctrl.  */
    if ((p_fu_api                == DEF_NULL) ||
        (p_fu_api->FU_MuteManage == DEF_NULL)                     ||
        (p_fu_api->FU_VolManage  == DEF_NULL)) {

       *p_err = USBD_ERR_NULL_PTR;
        return (USBD_CLASS_NBR_NONE);
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */

                                                                /* ------------- GET FEATURE UNIT STRUCT -------------- */
    CPU_CRITICAL_ENTER();
    fu_nbr = USBD_Audio_FU_NbrNext;

    if (fu_nbr >= USBD_AUDIO_CFG_MAX_NBR_FU) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_FU_ALLOC;
        return (0u);
    }

    USBD_Audio_FU_NbrNext++;

    CPU_CRITICAL_EXIT();

    p_fu = &USBD_Audio_FU_Tbl[fu_nbr];

                                                                /* ---------- ADD NEW FU TO CLASS CTRL INFO ----------- */
    p_fu->FU_CfgPtr  =  p_fu_cfg;                               /* Save configuration for later use                     */
    p_fu->FU_API_Ptr =  p_fu_api;
    p_fu->DrvInfoPtr = &p_ctrl->DrvInfo;

    CPU_CRITICAL_ENTER();
    if (p_ctrl->EntityID_Nxt > p_ctrl->EntityCnt) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_FU_ALLOC;
        return (0u);
    }
    p_fu->ID = p_ctrl->EntityID_Nxt + 1u;                       /* ID #0 reserved. Assigned ID needs +1 (see Note #1).  */
    ix       = p_ctrl->EntityID_Nxt;                            /* ID used as a 0-index.                                */
    p_ctrl->EntityID_Nxt++;                                     /* Nxt avail ID.                                        */
    CPU_CRITICAL_EXIT();

    p_fu->EntityType            =  USBD_AUDIO_ENTITY_FU;
    p_ctrl->EntityID_TblPtr[ix] = (USBD_AUDIO_ENTITY *)p_fu;

    USBD_StrAdd(p_ctrl->DevNbr,                                 /* Add string describing this FU.                       */
                p_fu_cfg->StrPtr,
                p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

   *p_err = USBD_ERR_NONE;
    return (p_fu->ID);
}


/*
*********************************************************************************************************
*                                         USBD_Audio_MU_Add()
*
* Description : Add a Mixer Unit to the specified class instance (i.e. audio function).
*
* Argument(s) : class_nbr   Class instance number
*
*               p_mu_cfg    Pointer to the Mixer Unit configuration structure.
*
*               p_mu_api    Pointer to the audio codec API associated to this Mixer Unit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Mixer Unit successfully added.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                           USBD_ERR_NULL_PTR           Null pointer passed to 'p_mu_cfg'.
*                           USBD_ERR_AUDIO_MU_ALLOC     No Mixer Unit structure available OR
*                                                       no entity structure available.
*                           USBD_ERR_ALLOC              No space on heap to allocate mixing controls bitmap.
*
*                           -RETURNED BY USBD_StrAdd()-
*                           See USBD_StrAdd() for additional return error codes.
*
* Return(s)   : Unit ID assigned by audio class, if NO error(s).
*
*               0,                               otherwise (see Note #1).
*
* Note(s)     : (1) Audio 1.0 specification indicates that ID #0 is reserved for undefined ID. Thus it
*                   indicates an error.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
CPU_INT08U  USBD_Audio_MU_Add (       CPU_INT08U                 class_nbr,
                               const  USBD_AUDIO_MU_CFG         *p_mu_cfg,
                               const  USBD_AUDIO_DRV_AC_MU_API  *p_mu_api,
                                      USBD_ERR                  *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_MU    *p_mu;
    CPU_INT08U        mu_nbr;
    CPU_INT32U        nbr_mixing_ctrl;
    LIB_ERR           err_lib;
    CPU_INT08U        ix;
    CPU_SR_ALLOC();


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }

    CPU_CRITICAL_ENTER();
    if (class_nbr >= USBD_Audio_CtrlNbrNext) {                  /* Check Audio Class nbr.                               */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }
    CPU_CRITICAL_EXIT();

    if (p_mu_cfg == DEF_NULL) {                                 /* Check Pointer to MU configuration                    */
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */

                                                                /* -------------- GET MIXER UNIT STRUCT --------------- */
    CPU_CRITICAL_ENTER();
    mu_nbr = USBD_Audio_MU_NbrNext;

    if (mu_nbr >= USBD_AUDIO_CFG_MAX_NBR_MU) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_MU_ALLOC;
        return (0u);
    }

    USBD_Audio_MU_NbrNext++;

    CPU_CRITICAL_EXIT();

    p_mu = &USBD_Audio_MU_Tbl[mu_nbr];

                                                                /* ---------- ADD NEW MU TO CLASS COMM INFO ----------- */
                                                                /* Alloc programmable ctrl bitmap tbl.                  */
    nbr_mixing_ctrl = p_mu_cfg->LogInChNbr * p_mu_cfg->LogOutChNbr;
    if ((nbr_mixing_ctrl % DEF_OCTET_NBR_BITS) == 0) {
        p_mu->ControlsSize = nbr_mixing_ctrl / DEF_OCTET_NBR_BITS;
    } else {
        p_mu->ControlsSize = (nbr_mixing_ctrl / DEF_OCTET_NBR_BITS) + 1u;
    }

    p_mu->ControlsTblPtr = (CPU_INT08U *)Mem_SegAlloc("Mixing Controls Tbl",
                                                       DEF_NULL,
                                                       p_mu->ControlsSize,
                                                      &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return (0u);
    }

    Mem_Clr((void *)p_mu->ControlsTblPtr,
                    p_mu->ControlsSize);

    p_mu->MU_CfgPtr  =  p_mu_cfg;                               /* Save configuration for later use                     */
    p_mu->MU_API_Ptr =  p_mu_api;
    p_mu->DrvInfoPtr = &p_ctrl->DrvInfo;

    CPU_CRITICAL_ENTER();
    if (p_ctrl->EntityID_Nxt > p_ctrl->EntityCnt) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_MU_ALLOC;
        return (0u);
    }
    p_mu->ID = p_ctrl->EntityID_Nxt + 1u;                       /* ID #0 reserved. Assigned ID needs +1 (see Note #1).  */
    ix       = p_ctrl->EntityID_Nxt;                            /* ID used as a 0-index.                                */
    p_ctrl->EntityID_Nxt++;                                     /* Nxt avail ID.                                        */
    CPU_CRITICAL_EXIT();

    p_mu->EntityType            =  USBD_AUDIO_ENTITY_MU;
    p_ctrl->EntityID_TblPtr[ix] = (USBD_AUDIO_ENTITY *)p_mu;

    USBD_StrAdd(p_ctrl->DevNbr,                                 /* Add string describing this MU.                       */
                p_mu_cfg->StrPtr,
                p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

   *p_err = USBD_ERR_NONE;
    return (p_mu->ID);
}
#endif


/*
*********************************************************************************************************
*                                         USBD_Audio_SU_Add()
*
* Description : Add a Selector Unit to the specified class instance (i.e. audio function).
*
* Argument(s) : class_nbr   Class instance number.
*
*               p_su_cfg    Pointer to the Selector Unit configuration structure.
*
*               p_su_api    Pointer to the audio codec API associated to this Selector Unit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Selector Unit successfully added.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                           USBD_ERR_NULL_PTR           Null pointer passed to 'p_su_cfg'/'p_su_api'.
*                           USBD_ERR_AUDIO_SU_ALLOC     No Selector Unit structure available OR
*                                                       no entity structure available.
*
*                           -RETURNED BY USBD_StrAdd()-
*                           See USBD_StrAdd() for additional return error codes.
*
* Return(s)   : Unit ID assigned by audio class, if NO error(s).
*
*               0,                               otherwise (see Note #1).
*
* Note(s)     : (1) Audio 1.0 specification indicates that ID #0 is reserved for undefined ID. Thus it
*                   indicates an error.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
CPU_INT08U  USBD_Audio_SU_Add (       CPU_INT08U                 class_nbr,
                               const  USBD_AUDIO_SU_CFG         *p_su_cfg,
                               const  USBD_AUDIO_DRV_AC_SU_API  *p_su_api,
                                      USBD_ERR                  *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_SU    *p_su;
    CPU_INT08U        su_nbr;
    CPU_INT08U        ix;
    CPU_SR_ALLOC();


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }

    CPU_CRITICAL_ENTER();
    if (class_nbr >= USBD_Audio_CtrlNbrNext) {                  /* Check Audio Class nbr.                               */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return (0u);
    }
    CPU_CRITICAL_EXIT();

    if (p_su_cfg == DEF_NULL) {                                 /* Check Pointer to SU configuration                    */
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
                                                                /* Check if SU API provided.                            */
    if ((p_su_api                 == DEF_NULL) ||
        (p_su_api->SU_InPinManage == DEF_NULL)) {
       *p_err = USBD_ERR_NULL_PTR;
        return (0u);
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */

                                                                /* ------------- GET SELECTOR UNIT STRUCT ------------- */
    CPU_CRITICAL_ENTER();
    su_nbr = USBD_Audio_SU_NbrNext;

    if (su_nbr >= USBD_AUDIO_CFG_MAX_NBR_SU) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_SU_ALLOC;
        return (0u);
    }

    USBD_Audio_SU_NbrNext++;
    CPU_CRITICAL_EXIT();

    p_su = &USBD_Audio_SU_Tbl[su_nbr];

                                                                /* ---------- ADD NEW MU TO CLASS COMM INFO ----------- */
    p_su->SU_CfgPtr  =  p_su_cfg;                               /* Save configuration for later use                     */
    p_su->SU_API_Ptr =  p_su_api;
    p_su->DrvInfoPtr = &p_ctrl->DrvInfo;

    CPU_CRITICAL_ENTER();
    if (p_ctrl->EntityID_Nxt > p_ctrl->EntityCnt) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_AUDIO_SU_ALLOC;
        return (0u);
    }
    p_su->ID = p_ctrl->EntityID_Nxt + 1u;                       /* ID #0 reserved. Assigned ID needs +1 (see Note #1).  */
    ix       = p_ctrl->EntityID_Nxt;                            /* ID used as a 0-index.                                */
    p_ctrl->EntityID_Nxt++;                                     /* Nxt avail ID.                                        */
    CPU_CRITICAL_EXIT();

    p_su->EntityType            =  USBD_AUDIO_ENTITY_SU;
    p_ctrl->EntityID_TblPtr[ix] = (USBD_AUDIO_ENTITY *)p_su;

    USBD_StrAdd(p_ctrl->DevNbr,                                 /* Add string describing this SU.                       */
                p_su_cfg->StrPtr,
                p_err);
    if (*p_err != USBD_ERR_NONE) {
        return (0u);
    }

   *p_err = USBD_ERR_NONE;
    return (p_su->ID);
}
#endif


/*
*********************************************************************************************************
*                                         USBD_Audio_IT_Assoc()
*
* Description : Associate the Input Terminal to an Output Terminal (see Note #1).
*
* Argument(s) : class_nbr           Class instance number.
*
*               it_id               Input Terminal ID.
*
*               assoc_terminal_id   Output Terminal ID associated (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Input Terminal successfully associated.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*
* Return(s)   : none.
*
* Note(s)     : (1) The Input Terminal descriptor has the field 'bAssocTerminal'. It associates an
*                   Output Terminal to this Input Terminal, effectively implementing a bi-directional
*                   Terminal pair.
*
*               (2) If there is no associated Output Terminal, you can specify
*                   USBD_AUDIO_TERMINAL_NO_ASSOCIATION.
*********************************************************************************************************
*/

void  USBD_Audio_IT_Assoc (CPU_INT08U   class_nbr,
                           CPU_INT08U   it_id,
                           CPU_INT08U   assoc_terminal_id,
                           USBD_ERR    *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_IT    *p_it;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {                                /* Validate error ptr.                                  */
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_Audio_CtrlNbrNext) {              /* Check Audio Class nbr.                               */
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_CLASS_INVALID_NBR;
            return;
        }
        CPU_CRITICAL_EXIT();
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */
                                                                /* ID used as a 0-index.                                */
    p_it   = (USBD_AUDIO_IT *)p_ctrl->EntityID_TblPtr[(it_id - 1u)];

    p_it->AssociatedOT_ID = assoc_terminal_id;                  /* Save associated OT ID.                               */
   *p_err                 = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_Audio_OT_Assoc()
*
* Description : Specify the entity ID connected to this Output Terminal and associate the Output Terminal
*               to an Input Terminal (see Note #1).
*
* Argument(s) : class_nbr           Class instance number.
*
*               ot_id               Output Terminal ID.
*
*               src_entity_id       ID of the Unit or Terminal to which this Terminal is connected.
*
*               assoc_terminal_id   Input Terminal ID associated (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Output Terminal successfully associated.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*
* Return(s)   : none.
*
* Note(s)     : (1) The Output Terminal descriptor has the field 'bAssocTerminal'. It associates an
*                   Input Terminal to this Output Terminal, effectively implementing a bi-directional
*                   Terminal pair.
*
*               (2) If there is no associated Input Terminal, you can specify
*                   USBD_AUDIO_TERMINAL_NO_ASSOCIATION.
*********************************************************************************************************
*/

void  USBD_Audio_OT_Assoc (CPU_INT08U   class_nbr,
                           CPU_INT08U   ot_id,
                           CPU_INT08U   src_entity_id,
                           CPU_INT08U   assoc_terminal_id,
                           USBD_ERR    *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_OT    *p_ot;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {                                /* Validate error ptr.                                  */
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_Audio_CtrlNbrNext) {              /* Check Audio Class nbr.                               */
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_CLASS_INVALID_NBR;
            return;
        }
        CPU_CRITICAL_EXIT();
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */
                                                                /* ID used as a 0-index.                                */
    p_ot   = (USBD_AUDIO_OT *)p_ctrl->EntityID_TblPtr[(ot_id - 1u)];

    p_ot->AssociatedIT_ID = assoc_terminal_id;                  /* Save associated IT ID.                               */
    p_ot->SourceID        = src_entity_id;                      /* Save unit or terminal ID.                            */
   *p_err                 = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_Audio_FU_Assoc()
*
* Description : Specify the entity ID connected to this Feature Unit.
*
* Argument(s) : class_nbr       Class instance number.
*
*               fu_id           Feature Unit ID.
*
*               entity_id_src   ID of the Unit or Terminal to which this Unit is connected.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Feature Unit successfully associated.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                           USBD_ERR_INVALID_ARG        Output Terminal cannot be connected as input of
*                                                       a Feature Unit.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_Audio_FU_Assoc (CPU_INT08U   class_nbr,
                           CPU_INT08U   fu_id,
                           CPU_INT08U   src_entity_id,
                           USBD_ERR    *p_err)
{
    USBD_AUDIO_CTRL    *p_ctrl;
    USBD_AUDIO_FU      *p_fu;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {                                /* Validate error ptr.                                  */
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_Audio_CtrlNbrNext) {              /* Check Audio Class nbr.                               */
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_CLASS_INVALID_NBR;
            return;
        }
        CPU_CRITICAL_EXIT();
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */
                                                                /* ID used as a 0-index.                                */
    p_fu   = (USBD_AUDIO_FU *)p_ctrl->EntityID_TblPtr[(fu_id - 1u)];

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        USBD_AUDIO_ENTITY  *p_entity_src;


        p_entity_src = p_ctrl->EntityID_TblPtr[(src_entity_id - 1u)];
        if (p_entity_src->EntityType == USBD_AUDIO_ENTITY_OT) { /* No OT can be connected as an input of FU.            */
           *p_err = USBD_ERR_INVALID_ARG;
            return;
        }
    }
#endif

    p_fu->SourceID = src_entity_id;                             /* Save unit or terminal ID.                            */
   *p_err          = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_Audio_MU_Assoc()
*
* Description : Specify the entities ID connected to this Mixer Unit.
*
* Argument(s) : class_nbr           Class instance number.
*
*               mu_id               Mixer Unit ID.
*
*               p_src_entity_id     Pointer to table containing IDs of Units and/or Terminals to which
*                                   Input Pins of this Mixer Unit are connected.
*
*               nbr_input_pins      Number of input pins.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Mixer Unit successfully associated.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                           USBD_ERR_NULL_PTR           Null pointer passed to 'p_src_entity_id'.
*                           USBD_ERR_INVALID_ARG        Output Terminal cannot be connected as input of
*                                                       a Mixer Unit.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
void  USBD_Audio_MU_Assoc (CPU_INT08U   class_nbr,
                           CPU_INT08U   mu_id,
                           CPU_INT08U  *p_src_entity_id,
                           CPU_INT08U   nbr_input_pins,
                           USBD_ERR    *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_MU    *p_mu;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {                                /* Validate error ptr.                                  */
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_Audio_CtrlNbrNext) {              /* Check Audio Class nbr.                               */
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_CLASS_INVALID_NBR;
            return;
        }
        CPU_CRITICAL_EXIT();

        if (p_src_entity_id == DEF_NULL) {                      /* Check src IDs ptr.                                   */
           *p_err = USBD_ERR_NULL_PTR;
            return;
        }
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */
                                                                /* ID used as a 0-index.                                */
    p_mu   = (USBD_AUDIO_MU *)p_ctrl->EntityID_TblPtr[(mu_id - 1u)];

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_INT08U          ix;
        CPU_INT08U          entity_id_src;
        USBD_AUDIO_ENTITY  *p_entity_src;

        for (ix = 0; ix < nbr_input_pins; ix++) {
            entity_id_src = p_src_entity_id[ix];
            p_entity_src  = p_ctrl->EntityID_TblPtr[(entity_id_src - 1u)];
                                                                /* No OT can be connected as an input of MU.            */
            if (p_entity_src->EntityType == USBD_AUDIO_ENTITY_OT) {
               *p_err = USBD_ERR_INVALID_ARG;
                return;
            }
        }
    }
#else
    (void)nbr_input_pins;
#endif

    p_mu->SourceID_TblPtr = p_src_entity_id;                    /* Save unit and/or terminal IDs.                       */
   *p_err                 = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                         USBD_Audio_SU_Assoc()
*
* Description : Specify the entities ID connected to this Selector Unit.
*
* Argument(s) : class_nbr           Class instance number.
*
*               su_id               Selector Unit ID.
*
*               p_src_entity_id     Pointer to table containing IDs of Units and/or Terminals to which
*                                   Input Pins of this Selector Unit are connected.
*
*               nbr_input_pins      Number of input pins.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE               Selector Unit successfully associated.
*                           USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                           USBD_ERR_NULL_PTR           Null pointer passed to 'p_src_entity_id'.
*                           USBD_ERR_INVALID_ARG        Output Terminal cannot be connected as input of
*                                                       a Selector Unit.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
void  USBD_Audio_SU_Assoc (CPU_INT08U   class_nbr,
                           CPU_INT08U   su_id,
                           CPU_INT08U  *p_src_entity_id,
                           CPU_INT08U   nbr_input_pins,
                           USBD_ERR    *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_SU    *p_su;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {                                /* Validate error ptr.                                  */
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_Audio_CtrlNbrNext) {              /* Check Audio Class nbr.                               */
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_CLASS_INVALID_NBR;
            return;
        }
        CPU_CRITICAL_EXIT();

        if (p_src_entity_id == DEF_NULL) {                      /* Check src IDs ptr.                                   */
           *p_err = USBD_ERR_NULL_PTR;
            return;
        }
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */
                                                                /* ID used as a 0-index.                                */
    p_su   = (USBD_AUDIO_SU *)p_ctrl->EntityID_TblPtr[(su_id - 1u)];

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_INT08U          ix;
        CPU_INT08U          entity_id_src;
        USBD_AUDIO_ENTITY  *p_entity_src;

        for (ix = 0; ix < nbr_input_pins; ix++) {
            entity_id_src = p_src_entity_id[ix];
            p_entity_src  = p_ctrl->EntityID_TblPtr[(entity_id_src - 1u)];
                                                                /* No OT can be connected as an input of SU.            */
            if (p_entity_src->EntityType == USBD_AUDIO_ENTITY_OT) {
               *p_err = USBD_ERR_INVALID_ARG;
                return;
            }
        }
    }
#else
    (void)nbr_input_pins;
#endif

    p_su->SourceID_TblPtr = p_src_entity_id;                    /* Save unit and/or terminal IDs.                       */
   *p_err                 = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_MU_MixingCtrlSet()
*
* Description : Set programmable mixing control.
*
* Argument(s) : class_nbr       Class instance number.
*
*               mu_id           Mixer Unit ID.
*
*               log_in_ch_nbr   Logical input channel number.
*
*               log_out_ch_nbr  Logical output channel number.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               Mixing control successfully set.
*                               USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                               USBD_ERR_NULL_PTR           Mixer Unit API not provided.
*
* Return(s)   : none.
*
* Note(s)     : (1) See function USBD_Audio_AC_UnitCtrl() note #3.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
void  USBD_Audio_MU_MixingCtrlSet (CPU_INT08U   class_nbr,
                                   CPU_INT08U   mu_id,
                                   CPU_INT08U   log_in_ch_nbr,
                                   CPU_INT08U   log_out_ch_nbr,
                                   USBD_ERR    *p_err)
{
    USBD_AUDIO_CTRL  *p_ctrl;
    USBD_AUDIO_MU    *p_mu;
    CPU_INT08U        byte_ix;
    CPU_INT08U        bit_shift;
    CPU_INT32U        bit_nbr;


                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    {
        CPU_SR_ALLOC();


        if (p_err == DEF_NULL) {                                /* Validate error ptr.                                  */
            CPU_SW_EXCEPTION(;);
        }

        CPU_CRITICAL_ENTER();
        if (class_nbr >= USBD_Audio_CtrlNbrNext) {              /* Check Audio Class nbr.                               */
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_CLASS_INVALID_NBR;
            return;
        }
        CPU_CRITICAL_EXIT();
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */
                                                                /* ID used as a 0-index.                                */
    p_mu   = (USBD_AUDIO_MU *)p_ctrl->EntityID_TblPtr[(mu_id - 1u)];

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* If MU has programmable ctrls, API must be provided.  */
    if ((p_mu->MU_API_Ptr                == DEF_NULL) &&
        (p_mu->MU_API_Ptr->MU_CtrlManage == DEF_NULL)) {
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif
                                                                /* Set this mixing ctrl as programmable within...       */
                                                                /* ...bitmap (see Note #1).                             */
    bit_nbr   = ((log_in_ch_nbr - 1u) * p_mu->MU_CfgPtr->LogOutChNbr) + (log_out_ch_nbr - 1u);
    byte_ix   =   bit_nbr / DEF_OCTET_NBR_BITS;
    bit_shift =   bit_nbr % DEF_OCTET_NBR_BITS;

    DEF_BIT_SET(p_mu->ControlsTblPtr[byte_ix], ((DEF_BIT_07) >> bit_shift));
   *p_err = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        USBD_Audio_AS_IF_Cfg()
*
* Description : Configure AudioStreaming interface settings by:
*
*                   1) Allocating an AudioStreaming Settings structure.
*                   2) Initializing this structure with the stream information.
*                   3) Allocating buffers that will be used by the given AudioStreaming interface.
*
* Argument(s) : p_stream_cfg    Pointer to general stream configuration.
*
*               p_as_if_cfg     Pointer to AudioStreaming interface configuration structure.
*
*               p_as_api        Pointer to AudioStreaming interface API.
*
*               p_seg           Pointer to memory segment used for buffers allocation. If null pointer,
*                               buffers are allocated from the heap. Otherwise, buffers are allocated
*                               from a memory region created by the application.
*
*               terminal_ID     Terminal ID associated to this AudioStreaming interface.
*
*               corr_callback   Application callback for user-defined correction algorithm.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           USBD_ERR_NONE           Buffers successfully allocated.
*                           USBD_ERR_NULL_PTR       NULL pointer passed to 'p_as_if_cfg'/'p_as_api'/
*                                                   'p_stream_cfg'.
*                           USBD_ERR_INVALID_ARG    Invalid 'p_stream_cfg' or 'p_as_if_cfg' field.
*                           USBD_ERR_ALLOC          AS IF Settings structure, buffers, events or audio
*                                                   statistics structure allocation failed.
*
*                           -RETURNED BY USBD_Audio_MaxPktLenGet()-
*                           See USBD_Audio_MaxPktLenGet() for additional return error codes.
*
* Return(s)   : Handle to AudioStreaming interface, if NO error(s).
*
*               0,                                  otherwise.
*
* Note(s)     : (1) Streams can be corrected using the following corrections: built-in (for playback
*                   and record) and feedback (for playback only).
*                   Playback and record streams correction rely on a buffers difference monitoring.
*                   The difference involves the rate at which the buffers are produced (by USB or codec)
*                   versus the rate at which they are consumed (by codec or USB). The buffers total
*                   number allocated for a stream is divided by 6. The quotient of this division
*                   represents an interval used to set different boundaries used for the stream
*                   correction. The following figure illustrates what the boundaries are depending
*                   of the correction method:
*
* Buffers Number    0   1   2   3   4   5   6   7   8   9   10  11  12
* Buffers Diff     -6  -5  -4  -3  -2  -1   0  +1  +2  +3  +4  +5  +6
*                   |       |       |       |       |       |       |
* Boundary          |     heavy   light           light   heavy     |
* Built-In Corr     | corr  |           safe zone           | corr  |
* Feedback Corr     |  correction   |   safe zone   |  correction   |
*
*                   In the figure, the stream has 12 buffers. Thus, the quotient is 2. The heavy
*                   boundaries will be -4 and +4 for built-in and feedback corrections. The feedback
*                   correction uses two more light boundaries (here -2 and +2) to apply a progressive
*                   correction.
*
*               (2) Allocation of audio buffers is done from a memory segment. This memory segment
*                   will be organized when the stream is open. When allocating the memory segment
*                   for the stream's audio buffers, the buffer's alignment must take into account
*                   any alignment to DMA and cache. The user must properly configure the constant
*                   USBD_AUDIO_CFG_BUF_ALIGN_OCTETS given any DMA and cache alignment requirements.
*
*               (3) Pre-buffering is equal to half of buffers total number allocated for this stream.
*                   This pre-bufferring value eases the stream safe zone monitoring for the correction.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
USBD_AUDIO_AS_IF_HANDLE  USBD_Audio_AS_IF_Cfg (const  USBD_AUDIO_STREAM_CFG          *p_stream_cfg,
                                               const  USBD_AUDIO_AS_IF_CFG           *p_as_if_cfg,
                                               const  USBD_AUDIO_DRV_AS_API          *p_as_api,
                                                      MEM_SEG                        *p_seg,
                                                      CPU_INT08U                      terminal_ID,
                                                      USBD_AUDIO_PLAYBACK_CORR_FNCT   corr_callback,
                                                      USBD_ERR                       *p_err)
{
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    USBD_AUDIO_AS_ALT_CFG      *p_as_cfg;
    USBD_AUDIO_AS_IF_HANDLE     as_if_handle;
    USBD_AUDIO_STREAM_DIR       stream_dir;
    CPU_INT08U                  as_if_settings_ix;
    CPU_INT16U                  mem_blk_len;
    CPU_INT16U                  max_mem_blk_len;
    CPU_INT16U                  mem_blk_len_worst;
    CPU_INT08U                  as_alt_ix;
    CPU_INT16U                  max_pkt_len;
    LIB_ERR                     err_lib;
#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_CORR_EN   == DEF_ENABLED)
    CPU_INT08U                  audio_frame_len;
#endif
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_INT32U                  max_sam_freq;
    CPU_INT08U                  sam_freq_ix;
    CPU_INT16U                  cur_max_throughput;
#endif
    CPU_SR_ALLOC();


#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_DISABLED)
    (void)corr_callback;
#endif
                                                                /* ------------------- VALIDATE ARG ------------------- */
#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NULL);
    }

    if (p_as_if_cfg == DEF_NULL) {                              /* Check ptr to AS IF cfg.                              */
       *p_err = USBD_ERR_NULL_PTR;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }

    if (p_as_api == DEF_NULL) {
       *p_err = USBD_ERR_NULL_PTR;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }

    if ((p_as_api->AS_SamplingFreqManage == DEF_NULL) ||       /* Any AS must provide minimally these fnct.            */
        (p_as_api->StreamStart           == DEF_NULL) ||
        (p_as_api->StreamStop            == DEF_NULL)) {

       *p_err = USBD_ERR_NULL_PTR;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
#endif

    if ((USBD_AUDIO_CFG_BUF_ALIGN_OCTETS < USBD_CFG_BUF_ALIGN_OCTETS) ||
       ((USBD_AUDIO_CFG_BUF_ALIGN_OCTETS % USBD_CFG_BUF_ALIGN_OCTETS) != 0u)) {
       *p_err = USBD_ERR_INVALID_ARG;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }

    p_as_cfg   =  p_as_if_cfg->AS_CfgPtrTbl[0u];
    stream_dir = (p_as_cfg->EP_DirIn == DEF_YES) ? USBD_AUDIO_STREAM_IN : USBD_AUDIO_STREAM_OUT;

#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* If playback stream, associated fnct must be provided.*/
    if ((stream_dir                 == USBD_AUDIO_STREAM_OUT) &&
        (p_as_api->StreamPlaybackTx == DEF_NULL)) {
       *p_err = USBD_ERR_NULL_PTR;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
                                                                /* If record stream, associated fnct must be provided.  */
    if ((stream_dir               == USBD_AUDIO_STREAM_IN) &&
        (p_as_api->StreamRecordRx == DEF_NULL)) {
       *p_err = USBD_ERR_NULL_PTR;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
                                                                /* General stream cfg content check.                    */
    if (p_stream_cfg == DEF_NULL) {                             /* Check ptr to general stream cfg struct.              */
       *p_err = USBD_ERR_NULL_PTR;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
                                                                /* Ensure a min of buf w/o or w/ stream corr.           */
    if (p_stream_cfg->MaxBufNbr < USBD_AUDIO_STREAM_BUF_QTY_MIN) {
       *p_err = USBD_ERR_INVALID_ARG;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }

#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_CORR_EN   == DEF_ENABLED)
    if (p_stream_cfg->CorrPeriodMs < 1u) {                      /* Corr period must be at least 1 ms.                   */
       *p_err = USBD_ERR_INVALID_ARG;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
                                                                /* When stream corr en, max buf nbr must be multiple... */
                                                                /* ...of 6 to ensure proper corr oper (see Note #1).    */
    if ((p_stream_cfg->MaxBufNbr % USBD_AUDIO_STREAM_CORR_BOUNDARY_INTERVAL) != 0u) {
       *p_err = USBD_ERR_INVALID_ARG;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
#endif
                                                                /* AS IF cfg content check.                             */
    if (p_as_if_cfg->AS_CfgAltSettingNbr > USBD_AUDIO_CFG_MAX_NBR_IF_ALT) {
       *p_err = USBD_ERR_INVALID_ARG;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
                                                                /* Check nbr of alt settings                            */
    for (as_alt_ix = 0u; as_alt_ix < p_as_if_cfg->AS_CfgAltSettingNbr; as_alt_ix++) {

        p_as_cfg = p_as_if_cfg->AS_CfgPtrTbl[as_alt_ix];
        if (p_as_cfg == DEF_NULL) {                             /* Check ptr to AudioStreaming alt setting cfg.         */
           *p_err = USBD_ERR_NULL_PTR;
            return ((USBD_AUDIO_AS_IF_HANDLE)0);
        }

        if ((p_as_cfg->NbrSamplingFreq    != 0u) &&
            (p_as_cfg->SamplingFreqTblPtr == DEF_NULL)) {
           *p_err = USBD_ERR_NULL_PTR;
            return ((USBD_AUDIO_AS_IF_HANDLE)0);
        }
    }
                                                                /* Verify that among all alt setting for given AS IF,...*/
                                                                /* ...max allowed Audio 1.0 throughput not exceeded.    */
    cur_max_throughput = 0u;
    for (as_alt_ix = 0u; as_alt_ix < p_as_if_cfg->AS_CfgAltSettingNbr; as_alt_ix++) {

        p_as_cfg = p_as_if_cfg->AS_CfgPtrTbl[as_alt_ix];

        if (p_as_cfg->NbrSamplingFreq == 0u) {
            max_sam_freq = p_as_cfg->UpperSamplingFreq;
        } else {
            max_sam_freq = 0u;
            for(sam_freq_ix = 0u; sam_freq_ix < p_as_cfg->NbrSamplingFreq; sam_freq_ix++) {
                if(p_as_cfg->SamplingFreqTblPtr[sam_freq_ix] > max_sam_freq) {
                    max_sam_freq = p_as_cfg->SamplingFreqTblPtr[sam_freq_ix];
                }
            }
        }

        cur_max_throughput = (max_sam_freq / DEF_TIME_NBR_mS_PER_SEC) * (p_as_cfg->BitRes / DEF_OCTET_NBR_BITS) * p_as_cfg->NbrCh;

        if (cur_max_throughput > USBD_AUDIO_MAX_THROUGHPUT) {
           *p_err = USBD_ERR_INVALID_ARG;
            return ((USBD_AUDIO_AS_IF_HANDLE)0);
        }
    }
#endif
                                                                /* --------------- AS IF SETTINGS ALLOC --------------- */
    CPU_CRITICAL_ENTER();
    as_if_settings_ix = USBD_Audio_AS_IF_SettingsNbrNext;       /* Alloc new AS IF settings.                            */
    if (as_if_settings_ix >= USBD_AUDIO_MAX_NBR_IF_ALT) {
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_ALLOC;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
    USBD_Audio_AS_IF_SettingsNbrNext++;                         /* Next avail AS IF settings nbr.                       */
    CPU_CRITICAL_EXIT();

    p_as_if_settings = &USBD_Audio_AS_IF_SettingsTbl[as_if_settings_ix];
                                                                /* Returned to app for later use by audio class.        */
    as_if_handle     = (USBD_AUDIO_AS_IF_HANDLE)p_as_if_settings;

                                                                /* ------------------ BUF POOL ALLOC ------------------ */
    max_mem_blk_len = 0u;
                                                                /* Find largest buffer among all alt settings.          */
    for (as_alt_ix = 0u; as_alt_ix < p_as_if_cfg->AS_CfgAltSettingNbr; as_alt_ix++) {

        p_as_cfg    = p_as_if_cfg->AS_CfgPtrTbl[as_alt_ix];
        max_pkt_len = USBD_Audio_MaxPktLenGet(p_as_cfg,         /* Max pkt size.                                        */
                                              p_err);
        if (*p_err != USBD_ERR_NONE) {
            return ((USBD_AUDIO_AS_IF_HANDLE)0);
        }
                                                                /* Max buf len.                                         */
#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_CORR_EN   == DEF_ENABLED)
        audio_frame_len = p_as_cfg->SubframeSize * p_as_cfg->NbrCh;
        mem_blk_len     = max_pkt_len + audio_frame_len;
#else
        mem_blk_len = max_pkt_len;
#endif

        if (mem_blk_len > max_mem_blk_len) {                    /* Keep largest buffer size among all alt settings.     */
            max_mem_blk_len = mem_blk_len;
        }
    }

                                                                /* See Note #2.                                         */
    mem_blk_len_worst           =  MATH_ROUND_INC_UP(max_mem_blk_len, USBD_AUDIO_CFG_BUF_ALIGN_OCTETS);
    p_as_if_settings->BufMemPtr = (CPU_INT08U *)Mem_SegAllocHW("Audio Data Buf Mem",
                                                                p_seg,
                                                               (mem_blk_len_worst * p_stream_cfg->MaxBufNbr),
                                                                USBD_AUDIO_CFG_BUF_ALIGN_OCTETS,
                                                                DEF_NULL,
                                                               &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
                                                                /* Alloc buf desc from heap.                                */
    p_as_if_settings->StreamRingBufQ.BufDescTblPtr = (USBD_AUDIO_BUF_DESC *)Mem_SegAllocHW("Buf Desc Pool",
                                                                                            DEF_NULL,
                                                                                           (sizeof(USBD_AUDIO_BUF_DESC) * p_stream_cfg->MaxBufNbr),
                                                                                            sizeof(CPU_ALIGN),
                                                                                            DEF_NULL,
                                                                                           &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
                                                                /* --------------- AS IF SETTINGS INIT ---------------- */
    p_as_if_settings->AS_API_Ptr        = p_as_api;
    p_as_if_settings->Ix                = as_if_settings_ix;
    p_as_if_settings->TerminalID        = terminal_ID;
    p_as_if_settings->BufTotalNbr       = p_stream_cfg->MaxBufNbr;
    p_as_if_settings->BufTotalLen       = max_mem_blk_len;
    p_as_if_settings->StreamDir         = stream_dir;
    p_as_if_settings->StreamStarted     = DEF_NO;
                                                                /* See Note #3.                                         */
    p_as_if_settings->StreamPreBufMax   = p_stream_cfg->MaxBufNbr / 2u;
    p_as_if_settings->StreamPrimingDone = DEF_NO;

    USBD_Audio_OS_RingBufQLockCreate(as_if_settings_ix, p_err);
    if (*p_err != USBD_ERR_NONE) {
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }


#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_CORR_EN   == DEF_ENABLED)
    p_as_if_settings->CorrPeriod = p_stream_cfg->CorrPeriodMs;
#endif
#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_PLAYBACK_CORR_EN     == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_CORR_EN       == DEF_ENABLED)
    p_as_if_settings->CorrBoundaryHeavyPos = (p_stream_cfg->MaxBufNbr / USBD_AUDIO_STREAM_CORR_BOUNDARY_INTERVAL) * 2u;
    p_as_if_settings->CorrBoundaryHeavyNeg =  p_as_if_settings->CorrBoundaryHeavyPos * -1;
#endif

    if (stream_dir == USBD_AUDIO_STREAM_OUT) {
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED)

#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
        p_as_if_settings->PlaybackSynch.FeedbackValUpdate = DEF_NO;

                                                                /* Alloc synch buffs for AS IF.                         */
        if ((((p_as_cfg->EP_SynchType & USBD_EP_TYPE_SYNC_MASK) == USBD_EP_TYPE_SYNC_ASYNC   )  &&
             ( stream_dir                                       == USBD_AUDIO_STREAM_OUT     )) ||
            (((p_as_cfg->EP_SynchType & USBD_EP_TYPE_SYNC_MASK) == USBD_EP_TYPE_SYNC_ADAPTIVE)  &&
             ( stream_dir                                       == USBD_AUDIO_STREAM_IN)     )) {

            p_as_if_settings->PlaybackSynch.SynchBufFree = DEF_YES;
            p_as_if_settings->PlaybackSynch.SynchBufPtr  = (CPU_INT32U *)Mem_SegAllocExt("Playback Synch Buf",
                                                                                          DEF_NULL,
                                                                                          sizeof(CPU_INT32U),
                                                                                          USBD_CFG_BUF_ALIGN_OCTETS,
                                                                                          DEF_NULL,
                                                                                         &err_lib);

            p_as_if_settings->PlaybackSynch.SynchBoundaryLightPos = p_stream_cfg->MaxBufNbr / USBD_AUDIO_STREAM_CORR_BOUNDARY_INTERVAL;
            p_as_if_settings->PlaybackSynch.SynchBoundaryLightNeg = p_as_if_settings->PlaybackSynch.SynchBoundaryLightPos * -1;
        }
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED)
        p_as_if_settings->CorrCallbackPtr = corr_callback;
#endif
#endif
    }

#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
                                                                /* Alloc a stat struct from the heap.                   */
    p_as_if_settings->StatPtr = (USBD_AUDIO_STAT *)Mem_SegAlloc("Audio Stat Struct",
                                                                 DEF_NULL,
                                                                 sizeof(USBD_AUDIO_STAT),
                                                                &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        return ((USBD_AUDIO_AS_IF_HANDLE)0);
    }
#endif

   *p_err = USBD_ERR_NONE;
    return (as_if_handle);
}
#endif


/*
*********************************************************************************************************
*                                        USBD_Audio_AS_IF_Add()
*
* Description : Add the AudioStreaming interface to the given configuration.
*
* Argument(s) : class_nbr       Class instance number.
*
*               cfg_nbr         Configuration number.
*
*               as_if_handle    Handle to AudioStreaming interface.
*
*               p_as_if_cfg     Pointer to AudioStreaming interface configuration.
*
*               p_as_cfg_name   Pointer to AudioStreaming interface name.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               AudioStreaming interface successfully added.
                                USBD_ERR_CLASS_INVALID_NBR  Invalid class number.
*                               USBD_ERR_NULL_PTR           NULL pointer passed to 'p_as_if_cfg'.
*                               USBD_ERR_INVALID_ARG        Invalid 'p_comm' retrieved from 'class_nbr'
*                               USBD_ERR_AUDIO_AS_IF_ALLOC  No more AudioStreaming interface alternate
*                                                           setting available.
*
*                               -RETURNED BY USBD_Audio_AS_IF_Alloc()-
*                               See USBD_Audio_AS_IF_Alloc() for additional return error codes.
*
*                               -RETURNED BY USBD_IF_Add()-
*                               See USBD_IF_Add() for additional return error codes.
*
*                               -RETURNED BY USBD_IF_AltAdd()-
*                               See USBD_IF_AltAdd() for additional return error codes.
*
*                               -RETURNED BY USBD_Audio_MaxPktLenGet()-
*                               See USBD_Audio_MaxPktLenGet() for additional return error codes.
*
*                               -RETURNED BY USBD_IsocAdd()-
*                               See USBD_IsocAdd() for additional return error codes.
*
*                               -RETURNED BY USBD_IsocSyncAddrSet()-
*                               See USBD_IsocSyncAddrSet() for additional return error codes.
*
*                               -RETURNED BY USBD_IsocSyncRefreshSet()-
*                               See USBD_IsocSyncRefreshSet() for additional return error codes.
**
* Return(s)   : None.
*
* Note(s)     : (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 3.7.2 for more details about AudioStreaming Interface.
*
*               (2) For FS, 1 ms = 1 frame. For HS, 1 ms = 8 microframes = 1 frame.
*
*               (3) Regardless of the isochronous endpoint speed (full- or high-speed), the polling
*                   interval is always 1 ms with Audio 1.0. Hence, the number of transactions per frame
*                   will always be 1.
*
*               (4) Feedback is restricted to Adaptive Source endpoint or Asynchronous Sink endpoint.
*                   If explicit synchronization mechanism is needed to maintain synchronization during
*                   transfers, the information carried over the synchronization path must be available
*                   every 2 ^ (10 - P) frames, with P ranging from 1 to 9 (512 ms down to 2 ms).
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 3.7.2.2 for more details about Isochronous Synch Endpoint.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
void  USBD_Audio_AS_IF_Add (       CPU_INT08U                class_nbr,
                                   CPU_INT08U                cfg_nbr,
                                   USBD_AUDIO_AS_IF_HANDLE   as_if_handle,
                            const  USBD_AUDIO_AS_IF_CFG     *p_as_if_cfg,
                            const  CPU_CHAR                 *p_as_cfg_name,
                                   USBD_ERR                 *p_err)
{
    USBD_AUDIO_CTRL        *p_ctrl;
    USBD_AUDIO_COMM        *p_comm;
    USBD_AUDIO_AS_IF       *p_as_if;
    USBD_AUDIO_AS_ALT_CFG  *p_as_cfg;
    USBD_AUDIO_AS_IF_ALT   *p_as_if_alt;
    CPU_INT08U              if_nbr;
    CPU_INT08U              if_alt_nbr;
    CPU_INT08U              data_ep_addr;
    CPU_INT08U              as_alt_ix;
    CPU_INT08U              as_alt_ix_global;
    CPU_INT16U              interval;
#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
    USBD_AUDIO_STREAM_DIR   stream_dir;
    CPU_BOOLEAN             synch_dir_in;
    CPU_INT08U              synch_ep_addr;
    CPU_INT08U              synch_ep_xfer_len;
#endif
    CPU_SR_ALLOC();


#if (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ------------------- VALIDATE ARG ------------------- */
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }

    CPU_CRITICAL_ENTER();
    if (class_nbr >= USBD_Audio_CtrlNbrNext) {                  /* Check Audio Class nbr.                               */
        CPU_CRITICAL_EXIT();
       *p_err = USBD_ERR_CLASS_INVALID_NBR;
        return;
    }
    CPU_CRITICAL_EXIT();

    if (p_as_if_cfg == DEF_NULL) {                              /* Check ptr to AS IF cfg.                              */
       *p_err = USBD_ERR_NULL_PTR;
        return;
    }
#endif

    p_ctrl = &USBD_Audio_CtrlTbl[class_nbr];                    /* Get audio class instance.                            */
    p_comm =  USBD_Audio_CommGet(p_ctrl, cfg_nbr);              /* Get comm struct.                                     */
    if (p_comm == DEF_NULL) {                                   /* Check if the cfg exist.                              */
       *p_err = USBD_ERR_INVALID_ARG;
        return;
    }

    p_as_if = USBD_Audio_AS_IF_Alloc(p_err);                    /* Get AS IF struct.                                    */
    if (*p_err != USBD_ERR_NONE) {
        return;
    }
                                                                /* ---------------- AUDIO STREAMING IF ---------------- */
                                                                /* See Note #1.                                         */
                                                                /* Add default AudioStreaming IF 0 to cfg.              */
    if_nbr = USBD_IF_Add(        p_ctrl->DevNbr,
                                 cfg_nbr,
                                &USBD_Audio_AS_Drv,
                         (void *)p_as_if,
                                 DEF_NULL,
                                 USBD_CLASS_CODE_AUDIO,
                                 USBD_AUDIO_SUBCLASS_AUDIO_STREAMING,
                                 0u,
                                "0-Bandwidth AudioStreaming Interface",
                                 p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }
                                                                /* Store the required info for internal use.            */
    p_as_if->CommPtr           = (USBD_AUDIO_COMM *)p_comm;
    p_as_if->DevNbr            =  p_ctrl->DevNbr;               /* Store dev nbr assigned by the core.                  */
    p_as_if->ClassNbr          =  p_ctrl->ClassNbr;             /* Store class instance nbr.                            */
    p_as_if->AS_IF_Nbr         =  if_nbr;                       /* Store AudioStreaming IF nbr given by the core.       */
    p_as_if->AS_IF_SettingsPtr = (USBD_AUDIO_AS_IF_SETTINGS *)as_if_handle;

    p_as_if->AS_IF_SettingsPtr->DrvInfoPtr = &p_ctrl->DrvInfo;  /* Store audio drv info.                                */

                                                                /* Create a lock for this AudioStreaming IF.            */
    USBD_Audio_OS_AS_IF_LockCreate(USBD_AUDIO_AS_IF_HANDLE_IX_GET(p_as_if->Handle), p_err);
    if (*p_err != USBD_ERR_NONE) {
        return;
    }
                                                                /* For Audio 1.0, isoc EP interval always 1 ms...       */
                                                                /* ...(see Note #2).                                    */
    if (DEF_BIT_IS_CLR(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
        interval = 1u;                                          /* In FS, bInterval in frames.                          */
    } else {
        interval = 8u;                                          /* In HS, bInterval in microframes.                     */
    }

    for (as_alt_ix = 0u; as_alt_ix < p_as_if_cfg->AS_CfgAltSettingNbr; as_alt_ix++) {
                                                                /* ------ AUDIO STREAMING IF ALTERNATIVE SETTING ------ */
                                                                /* Add operational alternate if to cfg.                 */
        CPU_CRITICAL_ENTER();
        as_alt_ix_global = USBD_Audio_AS_IF_AltNbrNext;         /* Alloc new alt setting.                               */

        if (as_alt_ix_global >= USBD_AUDIO_MAX_NBR_IF_ALT) {
            CPU_CRITICAL_EXIT();
           *p_err = USBD_ERR_AUDIO_AS_IF_ALLOC;
            return;
        }

        USBD_Audio_AS_IF_AltNbrNext++;                          /* Next avail alt setting nbr.                          */
        CPU_CRITICAL_EXIT();

        p_as_if_alt            = &USBD_Audio_AS_IF_AltTbl[as_alt_ix_global];
        p_as_cfg               =  p_as_if_cfg->AS_CfgPtrTbl[as_alt_ix];
        p_as_if_alt->AS_CfgPtr =  p_as_cfg;

        if_alt_nbr = USBD_IF_AltAdd(        p_ctrl->DevNbr,
                                            cfg_nbr,
                                            if_nbr,
                                    (void *)p_as_if_alt,
                                            p_as_cfg_name,
                                            p_err);
        if (*p_err != USBD_ERR_NONE) {
            return;
        }
                                                                /* -------------- MAXIMUM PACKET LENGTH --------------- */
        p_as_if_alt->MaxPktLen = USBD_Audio_MaxPktLenGet(p_as_cfg,
                                                         p_err);
        if (*p_err != USBD_ERR_NONE) {
            return;
        }
                                                                /* ----------- ISOCHRONOUS ENDPOINT ADDRESS ----------- */
                                                                /* Add Isoc EP to alternate IF 1.                       */
        data_ep_addr = USBD_IsocAdd(p_ctrl->DevNbr,
                                    cfg_nbr,
                                    if_nbr,
                                    if_alt_nbr,
                                    p_as_cfg->EP_DirIn,
                                    p_as_cfg->EP_SynchType,
                                    p_as_if_alt->MaxPktLen,
                                    USBD_EP_TRANSACTION_PER_UFRAME_1,   /* See Note #3.                                 */
                                    interval,
                                    p_err);
        if (*p_err != USBD_ERR_NONE) {
            return;
        }

        p_as_if_alt->DataIsocAddr = data_ep_addr;

#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
        stream_dir = (p_as_cfg->EP_DirIn == DEF_YES) ? USBD_AUDIO_STREAM_IN : USBD_AUDIO_STREAM_OUT;
                                                                /* See Note #4.                                         */
        if ((((p_as_cfg->EP_SynchType & USBD_EP_TYPE_SYNC_MASK) == USBD_EP_TYPE_SYNC_ASYNC   )  &&
             ( stream_dir                                       == USBD_AUDIO_STREAM_OUT     )) ||
            (((p_as_cfg->EP_SynchType & USBD_EP_TYPE_SYNC_MASK) == USBD_EP_TYPE_SYNC_ADAPTIVE)  &&
             ( stream_dir                                       == USBD_AUDIO_STREAM_IN)     )) {

                                                                /* Dir of Synch EP is the opposite of Data EP.          */
            synch_dir_in = (stream_dir == USBD_AUDIO_STREAM_IN) ? DEF_NO : DEF_YES;

                                                                /* Add Synch EP to alternate IF 1.                      */
            if (DEF_BIT_IS_CLR(cfg_nbr, USBD_CFG_NBR_SPD_BIT) == DEF_YES) {
                synch_ep_xfer_len = USBD_AUDIO_FS_SYNCH_EP_XFER_LEN;
            } else {
                synch_ep_xfer_len = USBD_AUDIO_HS_SYNCH_EP_XFER_LEN;
            }

            synch_ep_addr = USBD_IsocAdd(p_ctrl->DevNbr,
                                         cfg_nbr,
                                         if_nbr,
                                         if_alt_nbr,
                                         synch_dir_in,
                                         USBD_EP_TYPE_SYNC_NONE | USBD_EP_TYPE_USAGE_FEEDBACK,
                                         synch_ep_xfer_len,
                                         USBD_EP_TRANSACTION_PER_UFRAME_1,  /* See Note #3.                             */
                                         interval,
                                         p_err);
            if (*p_err != USBD_ERR_NONE) {
                return;
            }
                                                                /* Addr of Synch EP associated to Data EP.              */
            USBD_IsocSyncAddrSet(p_ctrl->DevNbr,
                                 cfg_nbr,
                                 if_nbr,
                                 if_alt_nbr,
                                 data_ep_addr,
                                 synch_ep_addr,
                                 p_err);
            if (*p_err != USBD_ERR_NONE) {
                return;
            }

            p_as_if_alt->SynchIsocAddr = synch_ep_addr;
                                                                /* Indicate synch feedback data rate.                   */
            USBD_IsocSyncRefreshSet(p_ctrl->DevNbr,
                                    cfg_nbr,
                                    if_nbr,
                                    if_alt_nbr,
                                    synch_ep_addr,
                                    p_as_cfg->EP_SynchRefresh,
                                    p_err);
            if (*p_err != USBD_ERR_NONE) {
                return;
            }
        }
#endif
    }
                                                                /* ----------- SAVE AS IF STRUCT IN A LIST ------------ */
    if (p_comm->AS_IF_HeadPtr == DEF_NULL) {                    /* First element of list.                               */
        p_comm->AS_IF_HeadPtr = p_as_if;
    } else {
        p_comm->AS_IF_TailPtr->NextPtr = p_as_if;               /* Element at the end of list.                          */
    }

    p_comm->AS_IF_TailPtr = p_as_if;
    p_comm->AS_IF_Cnt++;                                        /* Increment nbr of AudioStreaming IF.                  */

   *p_err = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_AS_IF_StatGet()
*
* Description : Get the statistics structure associated to a given AudioStreaming interface.
*
* Argument(s) : as_handle   AudioStreaming interface handle.
*
* Return(s)   : Pointer to the audio stream statistics.
*
* Note(s)     : None.
*********************************************************************************************************
*/
#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
USBD_AUDIO_STAT  *USBD_Audio_AS_IF_StatGet (USBD_AUDIO_AS_HANDLE  as_handle)
{
    USBD_AUDIO_STAT  *p_as_if_stat;


    p_as_if_stat = USBD_Audio_StatGet(as_handle);

    return (p_as_if_stat);
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
*                                        USBD_Audio_AC_Conn()
*
* Description : Notify class that configuration is active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_Audio_AC_Conn (CPU_INT08U   dev_nbr,
                                  CPU_INT08U   cfg_nbr,
                                  void        *p_if_class_arg)
{
    USBD_AUDIO_COMM  *p_comm;
    USBD_AUDIO_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


    (void)dev_nbr;
    (void)cfg_nbr;

    p_comm = (USBD_AUDIO_COMM *)p_if_class_arg;
    p_ctrl =  p_comm->CtrlPtr;

    CPU_CRITICAL_ENTER();
    p_ctrl->CommPtr = p_comm;
    p_ctrl->State   = USBD_AUDIO_STATE_CFG;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                       USBD_Audio_AC_Disconn()
*
* Description : Notify class that configuration is not active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  USBD_Audio_AC_Disconn (CPU_INT08U   dev_nbr,
                                     CPU_INT08U   cfg_nbr,
                                     void        *p_if_class_arg)
{
    USBD_AUDIO_COMM  *p_comm;
    USBD_AUDIO_CTRL  *p_ctrl;
    CPU_SR_ALLOC();


    p_comm = (USBD_AUDIO_COMM *)p_if_class_arg;
    p_ctrl =  p_comm->CtrlPtr;
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
    {
        USBD_AUDIO_AS_IF           *p_as_if;
        USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
        CPU_INT08U                  i;
        USBD_ERR                    err_usbd;


        p_as_if          =  p_comm->AS_IF_HeadPtr;
        p_as_if_settings =  p_as_if->AS_IF_SettingsPtr;

        for (i = 0u; i < p_comm->AS_IF_Cnt; i++) {

            USBD_Audio_AS_IF_Stop(p_as_if,
                                 &err_usbd);
            if (err_usbd != USBD_ERR_NONE) {
                USBD_DBG_AUDIO_ERR("AC_Disconn(): cannot stop stream on AS IF w/ err = %d\n", err_usbd);
            }

            p_as_if = p_as_if->NextPtr;

            if (p_ctrl->EventFnctPtr->Disconn != DEF_NULL) {    /* Notify app about cfg deactivation by host.           */
                p_ctrl->EventFnctPtr->Disconn(dev_nbr,
                                              cfg_nbr,
                                              p_as_if_settings->TerminalID,
                                              p_as_if->Handle);
            }
        }
    }
#else
    (void)dev_nbr;
    (void)cfg_nbr;
#endif

    CPU_CRITICAL_ENTER();
    p_ctrl->CommPtr = DEF_NULL;
    p_ctrl->State   = USBD_AUDIO_STATE_INIT;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                       USBD_Audio_AC_IF_Desc()
*
* Description : Class interface control descriptor callback for AudioControl interface.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate number.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.
*
* Return(s)   : None.
*
* Note(s)     : (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 4.3.2 for more details about Class-Specific AC Interface Descriptor.
*
*               (2) Total number of bytes returned for the class-specific AudioControl interface
*                   descriptor. It includes the combined length of this descriptor header and all Unit
*                   and Terminal descriptors.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   Table 4-2, 'wTotalLength' field description for more details.
*
*               (3) Audio 1.0 specification does NOT define a specific order for the class-specific
*                   descriptors following the AudioControl interface.
*
*               (4) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 4.3.2.1 for more details about Input Terminal Descriptor.
*
*               (5) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 4.3.2.2 for more details about Output Terminal Descriptor.
*
*               (6) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 4.3.2.5 for more details about Feature Unit Descriptor.
*
*                   (a) The size of the Feature Unit Descriptor corresponds to 7+(ch+1)*n where ch is
*                       number of logical channels and n is the size in bytes of each bmaControls.
*                       See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                       Table 4-7, 'bLength' field description for more details.
*
*               (7) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 4.3.2.3 for more details about Mixer Unit Descriptor.
*
*                   (a) The size of the Mixer Unit Descriptor corresponds to 10+p+N where p is the number
*                       of Input Pins and N is the number of bytes to use to store the bit Array.
*                       See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                       Table 4-5, 'bLength' field description for more details.
*
*               (8) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 4.3.2.4 for more details about Selector Unit Descriptor.
*
*                   (a) The size of the Selector Unit Descriptor corresponds to 6+p where p is number of
*                       Input Pins.
*                       See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                       Table 4-6, 'bLength' field description for more details.
*********************************************************************************************************
*/

static  void  USBD_Audio_AC_IF_Desc (CPU_INT08U   dev_nbr,
                                     CPU_INT08U   cfg_nbr,
                                     CPU_INT08U   if_nbr,
                                     CPU_INT08U   if_alt_nbr,
                                     void        *p_if_class_arg,
                                     void        *p_if_alt_class_arg)
{
    USBD_AUDIO_CTRL         *p_ctrl;
    USBD_AUDIO_COMM         *p_comm;
    USBD_AUDIO_IT           *p_it;
    USBD_AUDIO_OT           *p_ot;
    USBD_AUDIO_FU           *p_fu;
    USBD_AUDIO_MU           *p_mu;
    USBD_AUDIO_SU           *p_su;
    USBD_AUDIO_AS_IF        *p_as_if;
    USBD_AUDIO_IT_CFG       *p_it_cfg;
    USBD_AUDIO_OT_CFG       *p_ot_cfg;
    USBD_AUDIO_FU_CFG       *p_fu_cfg;
    USBD_AUDIO_MU_CFG       *p_mu_cfg;
    USBD_AUDIO_SU_CFG       *p_su_cfg;
    CPU_INT08U               desc_size;
    CPU_INT16U               total_len;
    CPU_INT08U               ix;
    CPU_INT08U               ctrl_ix;
    CPU_INT08U               src_ix;
    CPU_INT08U               str_ix;
    USBD_AUDIO_ENTITY_TYPE   entity_type;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_AUDIO_COMM *)p_if_class_arg;
    p_ctrl =  p_comm->CtrlPtr;
                                                                /* ----------------- AC IF HEADER DESC ---------------- */
                                                                /* See Note #1.                                         */
    desc_size = USBD_AUDIO_DESC_LEN_AC_HEADER_MIN + p_comm->AS_IF_Cnt;
    USBD_DescWr08(p_ctrl->DevNbr, desc_size);
    USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_TYPE_CS_IF);
    USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_SUBTYPE_AC_HEADER);
    USBD_DescWr16(p_ctrl->DevNbr, 0x0100u);

    total_len = USBD_Audio_AC_IF_DescSizeGet(dev_nbr,
                                             cfg_nbr,
                                             if_nbr,
                                             if_alt_nbr,
                                             p_if_class_arg,
                                             p_if_alt_class_arg);
    USBD_DescWr16(p_ctrl->DevNbr, total_len);                   /* See Note #2.                                         */
    USBD_DescWr08(p_ctrl->DevNbr, p_comm->AS_IF_Cnt);
    p_as_if = p_comm->AS_IF_HeadPtr;
    for (ix = 0u; ix < p_comm->AS_IF_Cnt; ix++) {
        USBD_DescWr08(p_ctrl->DevNbr, p_as_if->AS_IF_Nbr);
        p_as_if = p_as_if->NextPtr;
    }

    for (ix = 0u; ix < p_ctrl->EntityCnt; ix++) {

        entity_type = p_ctrl->EntityID_TblPtr[ix]->EntityType;

        switch (entity_type) {                                  /* See Note #3.                                         */
            case USBD_AUDIO_ENTITY_IT:                          /* ---------------- INPUT TERMINAL DESC --------------- */
                                                                /* See Note #4.                                         */
                 p_it     = (USBD_AUDIO_IT *)p_ctrl->EntityID_TblPtr[ix];
                 p_it_cfg =  p_it->IT_CfgPtr;

                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_LEN_AC_IT);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_TYPE_CS_IF);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_SUBTYPE_IT);
                 USBD_DescWr08(p_ctrl->DevNbr, p_it->ID);
                 USBD_DescWr16(p_ctrl->DevNbr, p_it_cfg->TerminalType);
                 USBD_DescWr08(p_ctrl->DevNbr, p_it->AssociatedOT_ID);
                 USBD_DescWr08(p_ctrl->DevNbr, p_it_cfg->LogChNbr);
                 USBD_DescWr16(p_ctrl->DevNbr, p_it_cfg->LogChCfg);
                 USBD_DescWr08(p_ctrl->DevNbr, 0u);
                 str_ix = USBD_StrIxGet(p_ctrl->DevNbr, p_it_cfg->StrPtr);
                 USBD_DescWr08(p_ctrl->DevNbr, str_ix);
                 break;


            case USBD_AUDIO_ENTITY_OT:                          /* ---------------- OUTPUT TERMINAL DESC -------------- */
                                                                /* See Note #5.                                         */
                 p_ot     = (USBD_AUDIO_OT *)p_ctrl->EntityID_TblPtr[ix];
                 p_ot_cfg =  p_ot->OT_CfgPtr;

                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_LEN_AC_OT);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_TYPE_CS_IF);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_SUBTYPE_OT);
                 USBD_DescWr08(p_ctrl->DevNbr, p_ot->ID);
                 USBD_DescWr16(p_ctrl->DevNbr, p_ot_cfg->TerminalType);
                 USBD_DescWr08(p_ctrl->DevNbr, p_ot->AssociatedIT_ID);
                 USBD_DescWr08(p_ctrl->DevNbr, p_ot->SourceID);
                 str_ix = USBD_StrIxGet(p_ctrl->DevNbr, p_ot_cfg->StrPtr);
                 USBD_DescWr08(p_ctrl->DevNbr, str_ix);
                 break;


            case USBD_AUDIO_ENTITY_FU:                          /* ---------------- FEATURE UNIT DESC ----------------- */
                                                                /* See Note #6.                                         */
                 p_fu     = (USBD_AUDIO_FU *)p_ctrl->EntityID_TblPtr[ix];
                 p_fu_cfg =  p_fu->FU_CfgPtr;
                                                                /* See Note #6a.                                        */
                 desc_size = USBD_AUDIO_DESC_LEN_AC_FU_MIN + (p_fu_cfg->LogChNbr + 1) * 2u;
                 USBD_DescWr08(p_ctrl->DevNbr, desc_size);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_TYPE_CS_IF);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_SUBTYPE_FU);
                 USBD_DescWr08(p_ctrl->DevNbr, p_fu->ID);
                 USBD_DescWr08(p_ctrl->DevNbr, p_fu->SourceID);
                 USBD_DescWr08(p_ctrl->DevNbr, 2u);
                 for (ctrl_ix = 0; ctrl_ix < (p_fu_cfg->LogChNbr + 1); ctrl_ix++) {
                     USBD_DescWr16(p_ctrl->DevNbr, p_fu_cfg->LogChCtrlPtr[ctrl_ix]);
                 }
                 str_ix = USBD_StrIxGet(p_ctrl->DevNbr, p_fu_cfg->StrPtr);
                 USBD_DescWr08(p_ctrl->DevNbr, str_ix);
                 break;


            case USBD_AUDIO_ENTITY_MU:                          /* ----------------- MIXER UNIT DESC ------------------ */
                                                                /* See Note #7.                                         */
                 p_mu     = (USBD_AUDIO_MU *)p_ctrl->EntityID_TblPtr[ix];
                 p_mu_cfg =  p_mu->MU_CfgPtr;

                                                                /* See Note #7a.                                        */
                 desc_size = USBD_AUDIO_DESC_LEN_AC_MU_MIN + p_mu_cfg->NbrInPins + p_mu->ControlsSize;
                 USBD_DescWr08(p_ctrl->DevNbr, desc_size);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_TYPE_CS_IF);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_SUBTYPE_MU);
                 USBD_DescWr08(p_ctrl->DevNbr, p_mu->ID);
                 USBD_DescWr08(p_ctrl->DevNbr, p_mu_cfg->NbrInPins);
                 for (src_ix = 0u; src_ix < p_mu_cfg->NbrInPins; src_ix++) {
                     USBD_DescWr08(p_ctrl->DevNbr, p_mu->SourceID_TblPtr[src_ix]);
                 }
                 USBD_DescWr08(p_ctrl->DevNbr, p_mu_cfg->LogOutChNbr);
                 USBD_DescWr16(p_ctrl->DevNbr, p_mu_cfg->LogOutChCfg);
                 USBD_DescWr08(p_ctrl->DevNbr, 0u);
                 for (ctrl_ix = 0u; ctrl_ix < p_mu->ControlsSize; ctrl_ix++) {
                     USBD_DescWr08(p_ctrl->DevNbr, p_mu->ControlsTblPtr[ctrl_ix]);
                 }
                 str_ix = USBD_StrIxGet(p_ctrl->DevNbr, p_mu_cfg->StrPtr);
                 USBD_DescWr08(p_ctrl->DevNbr, str_ix);
                 break;


            case USBD_AUDIO_ENTITY_SU:                          /* ---------------- SELECTOR UNIT DESC ---------------- */
                                                                /* See Note #8.                                         */
                 p_su     = (USBD_AUDIO_SU *)p_ctrl->EntityID_TblPtr[ix];
                 p_su_cfg =  p_su->SU_CfgPtr;

                                                                /* See Note #8a.                                        */
                 desc_size = USBD_AUDIO_DESC_LEN_AC_SU_MIN + p_su_cfg->NbrInPins;
                 USBD_DescWr08(p_ctrl->DevNbr, desc_size);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_TYPE_CS_IF);
                 USBD_DescWr08(p_ctrl->DevNbr, USBD_AUDIO_DESC_SUBTYPE_SU);
                 USBD_DescWr08(p_ctrl->DevNbr, p_su->ID);
                 USBD_DescWr08(p_ctrl->DevNbr, p_su_cfg->NbrInPins);
                 for (src_ix = 0u; src_ix < p_su_cfg->NbrInPins; src_ix++) {
                     USBD_DescWr08(p_ctrl->DevNbr, p_su->SourceID_TblPtr[src_ix]);
                 }
                 str_ix = USBD_StrIxGet(p_ctrl->DevNbr, p_su_cfg->StrPtr);
                 USBD_DescWr08(p_ctrl->DevNbr, str_ix);
                 break;


            default:
                 break;
        }
    }
}


/*
*********************************************************************************************************
*                                   USBD_Audio_AC_IF_DescSizeGet()
*
* Description : Retrieve the size of the class interface descriptor for AudioControl interface.
*               (See Note #1.)
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate number.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.

*
* Return(s)   : Size of the class interface descriptor.
*
* Note(s)     : (1) Total number of bytes returned for the class-specific AudioControl interface
*                   descriptor. It includes the combined length of this descriptor header and all Unit
*                   and Terminal descriptors.
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   Table 4-2, 'wTotalLength' field description for more details.
*********************************************************************************************************
*/

static  CPU_INT16U  USBD_Audio_AC_IF_DescSizeGet (CPU_INT08U   dev_nbr,
                                                  CPU_INT08U   cfg_nbr,
                                                  CPU_INT08U   if_nbr,
                                                  CPU_INT08U   if_alt_nbr,
                                                  void        *p_if_class_arg,
                                                  void        *p_if_alt_class_arg)
{
    USBD_AUDIO_CTRL         *p_ctrl;
    USBD_AUDIO_COMM         *p_comm;
    USBD_AUDIO_FU           *p_fu;
    USBD_AUDIO_MU           *p_mu;
    USBD_AUDIO_SU           *p_su;
    CPU_INT16U               desc_size;
    CPU_INT08U               ix;
    USBD_AUDIO_ENTITY_TYPE   entity_type;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;
    (void)if_alt_nbr;
    (void)p_if_alt_class_arg;

    p_comm = (USBD_AUDIO_COMM *)p_if_class_arg;
    p_ctrl =  p_comm->CtrlPtr;

    desc_size = USBD_AUDIO_DESC_LEN_AC_HEADER_MIN + p_comm->AS_IF_Cnt;

    for (ix = 0u; ix < p_ctrl->EntityCnt; ix++) {

        entity_type = p_ctrl->EntityID_TblPtr[ix]->EntityType;

        switch (entity_type) {
            case USBD_AUDIO_ENTITY_IT:
                 desc_size += USBD_AUDIO_DESC_LEN_AC_IT;
                 break;


            case USBD_AUDIO_ENTITY_OT:
                 desc_size += USBD_AUDIO_DESC_LEN_AC_OT;
                 break;


            case USBD_AUDIO_ENTITY_FU:
                 p_fu       = (USBD_AUDIO_FU *)p_ctrl->EntityID_TblPtr[ix];
                 desc_size += (USBD_AUDIO_DESC_LEN_AC_FU_MIN + (p_fu->FU_CfgPtr->LogChNbr + 1u) * 2u);
                 break;


            case USBD_AUDIO_ENTITY_MU:
                 p_mu       = (USBD_AUDIO_MU *)p_ctrl->EntityID_TblPtr[ix];
                 desc_size += (USBD_AUDIO_DESC_LEN_AC_MU_MIN + p_mu->MU_CfgPtr->NbrInPins + p_mu->ControlsSize);
                 break;


            case USBD_AUDIO_ENTITY_SU:
                 p_su       = (USBD_AUDIO_SU *)p_ctrl->EntityID_TblPtr[ix];
                 desc_size += (USBD_AUDIO_DESC_LEN_AC_SU_MIN + p_su->SU_CfgPtr->NbrInPins);
                 break;


            default:
                 break;
        }
    }

    return (desc_size);
}

/*
*********************************************************************************************************
*                                       USBD_Audio_AC_ClassReq()
*
* Description : Process class-specific requests for AudioControl interface. (see Note #1)
*
* Argument(s) : dev_nbr         Device number.
*
*               p_setup_req     Pointer to setup request structure.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.2 for more details about AudioControl Requests.
*
*               (2) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.1.1 for more details about Set Request.
*
*               (3) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.1.2 for more details about Get Request.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_AC_ClassReq (       CPU_INT08U       dev_nbr,
                                             const  USBD_SETUP_REQ  *p_setup_req,
                                                    void            *p_if_class_arg)
{
    USBD_AUDIO_COMM         *p_comm;
    USBD_AUDIO_CTRL         *p_ctrl;
    USBD_ERR                 err;
    USBD_AUDIO_ENTITY_TYPE   recipient;
    void                    *p_recipient_info;
    CPU_INT08U               entity_id;
    CPU_INT16U               req_len;
    CPU_BOOLEAN              dev_to_host;


    switch (p_setup_req->bRequest) {                            /* Discard class req not supported by this class.       */
        case USBD_AUDIO_REQ_GET_CUR:
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_CUR:
        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
             break;

        case USBD_AUDIO_REQ_GET_STAT:
        case USBD_AUDIO_REQ_GET_MEM:
        case USBD_AUDIO_REQ_SET_MEM:
        default:
             return (DEF_FAIL);                                 /* Class req not supported. Ctrl EP will be stalled.    */
    }

    req_len = p_setup_req->wLength;                             /* Length of param block.                               */
    if (req_len > USBD_AUDIO_CFG_CLASS_REQ_MAX_LEN) {           /* Check if enough room to process class req data.      */
        return (DEF_FAIL);
    }

    p_comm           = (USBD_AUDIO_COMM *)p_if_class_arg;
    p_ctrl           =  p_comm->CtrlPtr;
    dev_to_host      =  DEF_BIT_IS_SET(p_setup_req->bmRequestType, USBD_REQ_DIR_BIT);
    entity_id        = (p_setup_req->wIndex & USBD_AUDIO_REQ_ENTITY_ID_MASK) >> 8u;
    p_recipient_info =  USBD_Audio_AC_RecipientGet(p_ctrl, entity_id, &recipient);

    if (recipient == USBD_AUDIO_ENTITY_UNKNOWN) {
        return (DEF_FAIL);
    }
                                                                /* -------------------- SET REQUEST ------------------- */
                                                                /* See Note #2.                                          */
    if (dev_to_host == DEF_NO) {
        USBD_CtrlRx(        dev_nbr,                            /* Receive param block from host.                       */
                    (void *)p_ctrl->ReqBufPtr,
                            req_len,
                            USBD_AUDIO_CTRL_REQ_TIMEOUT_mS,
                           &err);
        if (err != USBD_ERR_NONE) {
            USBD_DBG_AUDIO_ERR("AC (SET req): Ctrl Rx fails w/ err = %d\n", err);
            return (DEF_FAIL);
        }
    }
                                                                /* ------------------ REQ EXECUTION ------------------- */
    switch (recipient) {
        case USBD_AUDIO_ENTITY_IT:
        case USBD_AUDIO_ENTITY_OT:
             USBD_Audio_AC_TerminalCtrl(entity_id,
                                        recipient,
                                        p_recipient_info,
                                        p_setup_req->bRequest,
                                        p_setup_req->wValue,
                                        p_ctrl->ReqBufPtr,
                                        req_len,
                                       &err);
             break;


        case USBD_AUDIO_ENTITY_FU:
        case USBD_AUDIO_ENTITY_MU:
        case USBD_AUDIO_ENTITY_SU:
             USBD_Audio_AC_UnitCtrl(entity_id,
                                    recipient,
                                    p_recipient_info,
                                    p_setup_req->bRequest,
                                    p_setup_req->wValue,
                                    p_ctrl->ReqBufPtr,
                                    req_len,
                                   &err);
             break;


         default:
             err = USBD_ERR_FAIL;
             break;
    }

    if (err != USBD_ERR_NONE) {
        return (DEF_FAIL);
    }
                                                                /* -------------------- GET REQUESTS ------------------ */
                                                                /* See Note #3.                                         */
    if (dev_to_host == DEF_YES) {
        USBD_CtrlTx(        dev_nbr,                            /* Send param block to host.                            */
                    (void *)p_ctrl->ReqBufPtr,
                            req_len,
                            USBD_AUDIO_CTRL_REQ_TIMEOUT_mS,
                            DEF_NO,
                           &err);
        if (err != USBD_ERR_NONE) {
            return (DEF_FAIL);
        }
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                        USBD_Audio_AS_Conn()
*
* Description : Notify class that configuration is active.
*
* Argument(s) : dev_nbr         Device number.
*
*               cfg_nbr         Configuration number.
*
*               p_if_class_arg  Pointer to class argument specific to interface.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_AS_Conn (CPU_INT08U   dev_nbr,
                                  CPU_INT08U   cfg_nbr,
                                  void        *p_if_class_arg)
{
    USBD_AUDIO_AS_IF  *p_as_if;
    USBD_AUDIO_CTRL   *p_ctrl;


    p_as_if = (USBD_AUDIO_AS_IF *)p_if_class_arg;
    p_ctrl  =  p_as_if->CommPtr->CtrlPtr;
                                                                /* Notify app about cfg activation by host.             */
    if (p_ctrl->EventFnctPtr->Conn != DEF_NULL) {
        p_ctrl->EventFnctPtr->Conn(dev_nbr,
                                   cfg_nbr,
                                   p_as_if->AS_IF_SettingsPtr->TerminalID,
                                   p_as_if->Handle);
    }
}
#endif


/*
*********************************************************************************************************
*                                  USBD_Audio_AS_AltSettingUpdate()
*
* Description : Notify class that interface alternate setting has been updated.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_AS_AltSettingUpdate (CPU_INT08U   dev_nbr,
                                              CPU_INT08U   cfg_nbr,
                                              CPU_INT08U   if_nbr,
                                              CPU_INT08U   if_alt_nbr,
                                              void        *p_if_class_arg,
                                              void        *p_if_alt_class_arg)
{
    USBD_AUDIO_AS_IF           *p_as_if;
    USBD_AUDIO_AS_IF_ALT       *p_as_if_alt;
    USBD_AUDIO_AS_IF_SETTINGS  *p_as_if_settings;
    USBD_ERR                    err_usbd;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;

    p_as_if          = (USBD_AUDIO_AS_IF     *)p_if_class_arg;
    p_as_if_alt      = (USBD_AUDIO_AS_IF_ALT *)p_if_alt_class_arg;
    p_as_if_settings =  p_as_if->AS_IF_SettingsPtr;

    if (p_as_if->AS_IF_Nbr == 0u) {                             /* Should not be an AudioControl interface.             */
        return;
    }
                                                                /* Stop current streaming on specified IF nbr if a ...  */
                                                                /* ...previous one was already started.                 */
    if (p_as_if->AS_IF_AltCurPtr != DEF_NULL) {

        USBD_Audio_AS_IF_Stop(p_as_if,
                             &err_usbd);
        if (err_usbd != USBD_ERR_NONE) {
            USBD_DBG_AUDIO_ERR("AS_UpdateAltSetting(): cannot stop stream on AS IF w/ err = %d\n", err_usbd);
            return;
        }
    }

    if (if_alt_nbr == 0u) {                                     /* Default IF has NO EP.                                */
        p_as_if->AS_IF_AltCurPtr = DEF_NULL;                    /* Reset the current alt setting.                       */

    } else {                                                    /* Operational IF.                                      */
        p_as_if->AS_IF_AltCurPtr = p_as_if_alt;                 /* Select Isoc EP associated with specified alt nbr.    */
                                                                /* Start streaming on new alt setting.                  */
        if (p_as_if_settings->StreamDir == USBD_AUDIO_STREAM_OUT) {

            USBD_Audio_AS_IF_Start(p_as_if,
                                  &err_usbd);
            if (err_usbd != USBD_ERR_NONE) {
                USBD_DBG_AUDIO_ERR("AS_UpdateAltSetting(): cannot start stream on AS IF w/ err = %d\n", err_usbd);
            }
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                       USBD_Audio_AS_IF_Desc()
*
* Description : Class interface descriptor callback for AudioStreaming interface.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.
*
* Return(s)   : None.
*
* Note(s)     : (1) AudioStreaming interface alternate setting 0 is a zero-bandwidth interface. It is not
*                   followed by class-specific descriptors.
*                   See 'USB Audio Device Class Specification for Basic Audio Devices, Release 1.0,
*                   November 24, 2009', section 5.3.3.3.1 for more details.
*
*               (2) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 4.5.2 for more details about Class-Specific AS Interface Descriptor.
*
*               (3) See 'USB Device Class Definition for Audio Data Formats, Release 1.0, March 18, 1998',
*                   section 2.2.5 for more details about Type I Format Type Descriptor.
*
*                   (a) Size of the descriptor correspond to 8+(ns*3) where ns is the number of sampling
*                       frequencies.
*                       See 'USB Device Class Definition for Audio Data Formats, Release 1.0, March 18, 1998',
*                       Table 2-1, 'bLength' field description for more details.
*
*               (4) A sampling frequency value holds on 3 bytes according to Audio 1.0 specification.
*                   The use of a temporary buffer ensures to correctly write the sampling frequency value
*                   into the descriptor Type I Format descriptor regardless of the CPU endianness.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_AS_IF_Desc (CPU_INT08U   dev_nbr,
                                     CPU_INT08U   cfg_nbr,
                                     CPU_INT08U   if_nbr,
                                     CPU_INT08U   if_alt_nbr,
                                     void        *p_if_class_arg,
                                     void        *p_if_alt_class_arg)
{
    USBD_AUDIO_AS_IF       *p_as_if;
    USBD_AUDIO_AS_IF_ALT   *p_as_if_alt;
    USBD_AUDIO_AS_ALT_CFG  *p_as_cfg;
    CPU_INT08U              desc_size;
    CPU_INT08U              sam_freq_ix;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;

    if (if_alt_nbr == 0u) {
        return;                                                 /* See Note #1.                                         */
    }

    p_as_if     = (USBD_AUDIO_AS_IF     *)p_if_class_arg;
    p_as_if_alt = (USBD_AUDIO_AS_IF_ALT *)p_if_alt_class_arg;
    p_as_cfg    =  p_as_if_alt->AS_CfgPtr;

                                                                /* ----------------- AS IF GENERAL DESC --------------- */
                                                                /* See Note #2.                                         */
    USBD_DescWr08(p_as_if->DevNbr, USBD_AUDIO_DESC_LEN_AS_GENERAL);
    USBD_DescWr08(p_as_if->DevNbr, USBD_AUDIO_DESC_TYPE_CS_IF);
    USBD_DescWr08(p_as_if->DevNbr, USBD_AUDIO_DESC_SUBTYPE_AS_GENERAL);
    USBD_DescWr08(p_as_if->DevNbr, p_as_if->AS_IF_SettingsPtr->TerminalID);
    USBD_DescWr08(p_as_if->DevNbr, p_as_cfg->Dly);
    USBD_DescWr16(p_as_if->DevNbr, p_as_cfg->FmtTag);
                                                                /* ------------------ TYPE I FMT DESC ----------------- */
                                                                /* See Note #3 & 3a.                                    */
    if (p_as_cfg->NbrSamplingFreq == 0u) {                      /* Continuous sampling freq.                            */
        desc_size = USBD_AUDIO_DESC_LEN_TYPE_I_FMT_MIN + (USBD_AUDIO_FMT_TYPE_I_CONT_FREQ_NBR * USBD_AUDIO_DESC_LEN_TYPE_I_SAM_FREQ);
    } else {                                                    /* Discrete sampling frequencies.                       */
        desc_size = USBD_AUDIO_DESC_LEN_TYPE_I_FMT_MIN + (p_as_cfg->NbrSamplingFreq * USBD_AUDIO_DESC_LEN_TYPE_I_SAM_FREQ);
    }

    USBD_DescWr08(p_as_if->DevNbr, desc_size);
    USBD_DescWr08(p_as_if->DevNbr, USBD_AUDIO_DESC_TYPE_CS_IF);
    USBD_DescWr08(p_as_if->DevNbr, USBD_AUDIO_DESC_SUBTYPE_FMT_TYPE);
    USBD_DescWr08(p_as_if->DevNbr, USBD_AUDIO_FMT_TYPE_I);
    USBD_DescWr08(p_as_if->DevNbr, p_as_cfg->NbrCh);
    USBD_DescWr08(p_as_if->DevNbr, p_as_cfg->SubframeSize);
    USBD_DescWr08(p_as_if->DevNbr, p_as_cfg->BitRes);
    USBD_DescWr08(p_as_if->DevNbr, p_as_cfg->NbrSamplingFreq);

    if (p_as_cfg->NbrSamplingFreq == 0u) {                      /* Continuous sampling freq (see Note #4).              */

        USBD_DescWr24(p_as_if->DevNbr, p_as_cfg->LowerSamplingFreq);
        USBD_DescWr24(p_as_if->DevNbr, p_as_cfg->UpperSamplingFreq);

    } else {                                                    /* Discrete sampling frequencies (see Note #4).         */
        for (sam_freq_ix = 0u; sam_freq_ix < p_as_cfg->NbrSamplingFreq; sam_freq_ix++) {

            USBD_DescWr24(p_as_if->DevNbr, p_as_cfg->SamplingFreqTblPtr[sam_freq_ix]);
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                   USBD_Audio_AS_IF_DescSizeGet()
*
* Description : Retrieve the size of the class interface descriptor for AudioStreaming interface.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.
*
* Return(s)   : Size of the class interface descriptor.
*
* Note(s)     : (1) See 'USB Device Class Definition for Audio Data Formats, Release 1.0, March 18, 1998',
*                   section 2.2.5 for more details about Type I Format Type Descriptor.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_INT16U  USBD_Audio_AS_IF_DescSizeGet (CPU_INT08U   dev_nbr,
                                                  CPU_INT08U   cfg_nbr,
                                                  CPU_INT08U   if_nbr,
                                                  CPU_INT08U   if_alt_nbr,
                                                  void        *p_if_class_arg,
                                                  void        *p_if_alt_class_arg)
{
    USBD_AUDIO_AS_IF_ALT   *p_as_if_alt;
    USBD_AUDIO_AS_ALT_CFG  *p_as_cfg;
    CPU_INT16U              desc_size;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;
    (void)p_if_class_arg;

    if (if_alt_nbr == 0u) {                                     /* Alt setting 0 has no class-specific desc.            */
        return (0u);
    }

    p_as_if_alt = (USBD_AUDIO_AS_IF_ALT *)p_if_alt_class_arg;
    p_as_cfg    =  p_as_if_alt->AS_CfgPtr;
    desc_size   =  USBD_AUDIO_DESC_LEN_AS_GENERAL;

    if (p_as_cfg->NbrSamplingFreq == 0u) {                      /* Continuous sampling freq.                            */
        desc_size += (USBD_AUDIO_DESC_LEN_TYPE_I_FMT_MIN + (USBD_AUDIO_FMT_TYPE_I_CONT_FREQ_NBR * USBD_AUDIO_DESC_LEN_TYPE_I_SAM_FREQ));
    } else {                                                    /* Discrete sampling frequencies.                       */
                                                                /* See Note #1.                                         */
        desc_size += (USBD_AUDIO_DESC_LEN_TYPE_I_FMT_MIN + (p_as_cfg->NbrSamplingFreq * USBD_AUDIO_DESC_LEN_TYPE_I_SAM_FREQ));
    }

    return (desc_size);
}
#endif


/*
**********************************************************************************************************
*                                       USBD_Audio_AS_EP_Desc()
*
* Description : Write class-specific endpoint descriptor for AudioStreaming interface.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               ep_addr             Endpoint address.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.
*
* Return(s)   : None.
*
* Note(s)     : (1) AudioStreaming interface alternate setting 0 has NO endpoints.
*                   See 'USB Audio Device Class Specification for Basic Audio Devices, Release 1.0,
*                   November 24, 2009', section 5.3.3.3.1 for more details.
*
*               (2) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 4.6.1.2 for more details about Class-Specific AS Isochronous Audio Data
*                   Endpoint Descriptor.
**********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_AS_EP_Desc (CPU_INT08U   dev_nbr,
                                     CPU_INT08U   cfg_nbr,
                                     CPU_INT08U   if_nbr,
                                     CPU_INT08U   if_alt_nbr,
                                     CPU_INT08U   ep_addr,
                                     void        *p_if_class_arg,
                                     void        *p_if_alt_class_arg)
{
    USBD_AUDIO_AS_IF       *p_as_if;
    USBD_AUDIO_AS_IF_ALT   *p_as_if_alt;
    USBD_AUDIO_AS_ALT_CFG  *p_as_cfg;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;

    if (if_alt_nbr == 0u) {
        return;                                                 /* See Note #1.                                         */
    }

    p_as_if     = (USBD_AUDIO_AS_IF     *)p_if_class_arg;
    p_as_if_alt = (USBD_AUDIO_AS_IF_ALT *)p_if_alt_class_arg;

    if (p_as_if_alt->SynchIsocAddr == ep_addr) {                /* Synch EP does NOT have class-specific desc.          */
        return;
    }

    p_as_cfg    = p_as_if_alt->AS_CfgPtr;
                                                                /* -------------- DATA EP GENERAL DESC ---------------- */
                                                                /* See Note #2.                                         */
    USBD_DescWr08(p_as_if->DevNbr, USBD_AUDIO_DESC_LEN_EP_GENERAL);
    USBD_DescWr08(p_as_if->DevNbr, USBD_AUDIO_DESC_TYPE_CS_EP);
    USBD_DescWr08(p_as_if->DevNbr, USBD_AUDIO_DESC_SUBTYPE_EP_GENERAL);
    USBD_DescWr08(p_as_if->DevNbr, p_as_cfg->EP_Attrib);
    USBD_DescWr08(p_as_if->DevNbr, p_as_cfg->EP_LockDlyUnits);
    USBD_DescWr16(p_as_if->DevNbr, p_as_cfg->EP_LockDly);
}
#endif


/*
**********************************************************************************************************
*                                   USBD_Audio_AS_EP_DescSizeGet()
*
* Description : Return size of class-specific endpoint descriptor for AudioStreaming interface.
*
* Argument(s) : dev_nbr             Device number.
*
*               cfg_nbr             Configuration number.
*
*               if_nbr              Interface number.
*
*               if_alt_nbr          Interface alternate setting number.
*
*               ep_addr             Endpoint address.
*
*               p_if_class_arg      Pointer to class argument specific to interface.
*
*               p_if_alt_class_arg  Pointer to class argument specific to alternate interface.
*
* Return(s)   : Size of class-specific endpoint descriptor in octets.
*
* Note(s)     : None.
**********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_INT16U  USBD_Audio_AS_EP_DescSizeGet (CPU_INT08U   dev_nbr,
                                                  CPU_INT08U   cfg_nbr,
                                                  CPU_INT08U   if_nbr,
                                                  CPU_INT08U   if_alt_nbr,
                                                  CPU_INT08U   ep_addr,
                                                  void        *p_if_class_arg,
                                                  void        *p_if_alt_class_arg)
{
    CPU_INT16U             desc_size;
    USBD_AUDIO_AS_IF_ALT  *p_as_if_alt;


    (void)dev_nbr;
    (void)cfg_nbr;
    (void)if_nbr;
    (void)p_if_class_arg;

    if (if_alt_nbr == 0u) {                                     /* Alt setting 0 has no class-specific desc.            */
        return (0u);
    }

    p_as_if_alt = (USBD_AUDIO_AS_IF_ALT *)p_if_alt_class_arg;

    if (p_as_if_alt->SynchIsocAddr == ep_addr) {                /* Synch EP does NOT have class-specific desc.          */
        return (0u);
    }

    desc_size = USBD_AUDIO_DESC_LEN_EP_GENERAL;

    return (desc_size);
}
#endif


/*
*********************************************************************************************************
*                                      USBD_Audio_AS_ClassReq()
*
* Description : Process class-specific requests for AudioStreaming interface (see Note #1).
*
* Argument(s) : dev_nbr         Device number.
*
*               p_setup_req     Pointer to setup request structure.
*
*               p_if_class_arg  Pointer to class argument.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.1.1 for more details about Set Request.
*
*               (2) Requests intended for the AudioStreaming interface deals ONLY with audio data format-
*                   specific requests. Depending on the Audio Data Format used by the interface
*                   (reflected in the wFormatTag field of the class-specific AS interface descriptor),
*                   the set of requests to control the interface may vary.
*                   According to 'USB Device Class Definition for Audio Data Formats, Release 1.0,
*                   March 18, 1998' specification, Type I data format does not define any AudioStreaming
*                   interface requests. Since the current Audio class implementation supports only Type I
*                   data format , all the requests will be stalled.
*
*                   See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.3.1 for more details about AudioStreaming interface Control requests.
*
*               (3) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   section 5.2.1.2 for more details about Get Request.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_Audio_AS_ClassReq (       CPU_INT08U       dev_nbr,
                                             const  USBD_SETUP_REQ  *p_setup_req,
                                                    void            *p_if_class_arg)
{
    USBD_AUDIO_COMM        *p_comm;
    USBD_AUDIO_CTRL        *p_ctrl;
    USBD_AUDIO_AS_ALT_CFG  *p_as_cfg;
    USBD_AUDIO_AS_IF       *p_as_if;
    USBD_AUDIO_AS_IF_ALT   *p_as_alt;
    USBD_ERR                err = USBD_ERR_NONE;
    CPU_INT08U              recipient;
    CPU_INT16U              req_len;
    CPU_BOOLEAN             dev_to_host;
    CPU_INT16U              ep_addr;


    switch (p_setup_req->bRequest) {                            /* Discard class req not supported by this class.       */
        case USBD_AUDIO_REQ_GET_CUR:
        case USBD_AUDIO_REQ_GET_MIN:
        case USBD_AUDIO_REQ_GET_MAX:
        case USBD_AUDIO_REQ_GET_RES:
        case USBD_AUDIO_REQ_SET_CUR:
        case USBD_AUDIO_REQ_SET_MIN:
        case USBD_AUDIO_REQ_SET_MAX:
        case USBD_AUDIO_REQ_SET_RES:
             break;

        case USBD_AUDIO_REQ_GET_STAT:
        case USBD_AUDIO_REQ_GET_MEM:
        case USBD_AUDIO_REQ_SET_MEM:
        default:
             return (DEF_FAIL);                                 /* Class req not supported. Ctrl EP will be stalled.    */
    }

    req_len = p_setup_req->wLength;                             /* Length of param block.                               */
    if (req_len > USBD_AUDIO_CFG_CLASS_REQ_MAX_LEN) {           /* Check if enough room to process class req data.      */
        return (DEF_FAIL);
    }

    p_as_if = (USBD_AUDIO_AS_IF *)p_if_class_arg;
    p_comm  =  p_as_if->CommPtr;
    p_ctrl  =  p_comm->CtrlPtr;

    dev_to_host = DEF_BIT_IS_SET(p_setup_req->bmRequestType, USBD_REQ_DIR_BIT);
    recipient   = p_setup_req->bmRequestType & USBD_REQ_RECIPIENT_MASK;
                                                                /* -------------------- SET REQUEST ------------------- */
                                                                /* See Note #1.                                         */
    if (dev_to_host == DEF_NO) {
                                                                /* Receive param block from host.                       */
        USBD_CtrlRx(        dev_nbr,
                    (void *)p_ctrl->ReqBufPtr,
                            req_len,
                            USBD_AUDIO_CTRL_REQ_TIMEOUT_mS,
                           &err);
        if (err != USBD_ERR_NONE) {
            return (DEF_FAIL);
        }
    }
                                                                /* ------------------ REQ EXECUTION ------------------- */
    switch (recipient) {
        case USBD_REQ_RECIPIENT_ENDPOINT:
             ep_addr  = p_setup_req->wIndex;
             p_as_alt = p_as_if->AS_IF_AltCurPtr;

             if ((p_as_alt               == DEF_NULL) &&
                 (p_as_alt->DataIsocAddr != ep_addr)) {
                 return (DEF_FAIL);
             }
             p_as_cfg = p_as_alt->AS_CfgPtr;

             USBD_Audio_AS_EP_CtrlProcess(p_as_if,
                                          p_as_cfg,
                                          ep_addr,
                                          p_setup_req->bRequest,
                                          p_setup_req->wValue,
                                          p_ctrl->ReqBufPtr,
                                          req_len,
                                         &err);
             break;


        case USBD_REQ_RECIPIENT_INTERFACE:
        default:
             err = USBD_ERR_AUDIO_REQ_INVALID_CTRL;             /* See Note #2.                                         */
             break;
    }

    if (err != USBD_ERR_NONE) {
        return (DEF_FAIL);
    }
                                                                /* -------------------- GET REQUEST ------------------- */
                                                                /* See Note #3.                                         */
    if (dev_to_host == DEF_YES) {
        USBD_CtrlTx(        dev_nbr,                            /* Send param block to host.                            */
                    (void *)p_ctrl->ReqBufPtr,
                            req_len,
                            USBD_AUDIO_CTRL_REQ_TIMEOUT_mS,
                            DEF_NO,
                           &err);
        if (err != USBD_ERR_NONE) {
            return (DEF_FAIL);
        }
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                     USBD_Audio_AC_RecipientGet()
*
* Description : Get the entity recipient of the class request (see Note #1).
*
* Argument(s) : p_ctrl              Pointer to the class instance control structure.
*
*               entity_id           Terminal/Unit ID.
*
*               p_recipient_type    Pointer to the recipient Entity type.
*
* Return(s)   : Pointer to the recipient information structure, if NO error(s).
*
*               Null Pointer,                                   otherwise.
*
* Note(s)     : (1) The entity can be:
*
*                   (a) a Terminal (Input or Output).
*
*                   (b) an Unit (Feature, Mixer or Selector).
*
*               (2) A 'virtual' Entity IF represents the entire global AudioControl interface. According
*                   to Audio 1.0 specification. This can be used to report global AudioControl
*                   interface changes to the host. This type of entity is not supported by the current
*                   audio class.
*
*               (3) The audio class assigns IDs to terminals and units between 1 and the total number of
*                   entities composing the audio function. Any entity ID greater than the total number of
*                   entities is unknown.
*********************************************************************************************************
*/

static  void  *USBD_Audio_AC_RecipientGet (const  USBD_AUDIO_CTRL         *p_ctrl,
                                                  CPU_INT08U               entity_id,
                                                  USBD_AUDIO_ENTITY_TYPE  *p_recipient_type)
{
    void           *p_recipient_info;
    USBD_AUDIO_IT  *p_it;
    USBD_AUDIO_OT  *p_ot;
    USBD_AUDIO_FU  *p_fu;
    USBD_AUDIO_MU  *p_mu;
    USBD_AUDIO_SU  *p_su;


    if (entity_id == 0u) {
       *p_recipient_type = USBD_AUDIO_ENTITY_UNKNOWN;           /* Entity is a 'virtual' Entity IF (see Note #2).       */
        return (DEF_NULL);
    }

    if (entity_id > p_ctrl->EntityCnt) {                        /* Verify if entity exists (see Note #3).               */
       *p_recipient_type = USBD_AUDIO_ENTITY_UNKNOWN;
        return (DEF_NULL);
    }

   *p_recipient_type = p_ctrl->EntityID_TblPtr[(entity_id - 1u)]->EntityType;
    switch(*p_recipient_type) {
        case USBD_AUDIO_ENTITY_IT:
             p_it             = (USBD_AUDIO_IT *)p_ctrl->EntityID_TblPtr[(entity_id - 1u)];
             p_recipient_info = (void *)p_it;
             break;

        case USBD_AUDIO_ENTITY_OT:
             p_ot             = (USBD_AUDIO_OT *)p_ctrl->EntityID_TblPtr[(entity_id - 1u)];
             p_recipient_info = (void *)p_ot;
             break;

        case USBD_AUDIO_ENTITY_FU:
             p_fu             = (USBD_AUDIO_FU *)p_ctrl->EntityID_TblPtr[(entity_id - 1u)];
             p_recipient_info = (void *)p_fu;
             break;

        case USBD_AUDIO_ENTITY_MU:
             p_mu             = (USBD_AUDIO_MU *)p_ctrl->EntityID_TblPtr[(entity_id - 1u)];
             p_recipient_info = (void *)p_mu;
             break;

        case USBD_AUDIO_ENTITY_SU:
             p_su             = (USBD_AUDIO_SU *)p_ctrl->EntityID_TblPtr[(entity_id - 1u)];
             p_recipient_info = (void *)p_su;
             break;

        default:
            *p_recipient_type = USBD_AUDIO_ENTITY_UNKNOWN;
             p_recipient_info = DEF_NULL;
             break;
    }

    return (p_recipient_info);
}


/*
*********************************************************************************************************
*                                        USBD_Audio_CommGet()
*
* Description : Get the Comm structure with the specified configuration number.
*
* Argument(s) : p_ctrl          Pointer to the class instance control structure.
*
*               cfg_nbr         Configuration number to look for.
*
* Return(s)   : Pointer to the audio communication structure, if NO error(s).
*
*               Null Pointer,                                 otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN  == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN    == DEF_ENABLED) || \
    (USBD_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
static  USBD_AUDIO_COMM  *USBD_Audio_CommGet (const  USBD_AUDIO_CTRL  *p_ctrl,
                                                     CPU_INT08U        cfg_nbr)
{
    USBD_AUDIO_COMM  *p_comm;
    CPU_INT08U        ix;


    p_comm = p_ctrl->CommHeadPtr;
    for(ix = 0u; ix < p_ctrl->CommCnt; ix++) {
        if (p_comm->CfgNbr == cfg_nbr) {
            return (p_comm);
        } else {
            p_comm = p_comm->NextPtr;
        }
    }

    return (DEF_NULL);
}
#endif


/*
*********************************************************************************************************
*                                       USBD_Audio_MaxPktLenGet()
*
* Description : Compute maximum isochronous packet length.
*
* Argument(s) : p_as_cfg        Pointer to the AudioStreaming interface structure.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   Maximum packet length successfully computed.
*                               USBD_ERR_FAIL   Wrong synchronization type.
*
* Return(s)   : Maximum isochronous packet length, if NO error(s).
*
*               0,                                 otherwise.
*
* Note(s)     : (1) If the endpoint uses adaptive or asynchronous synchronization, one additional sample
*                   frame should be added to accommodate adjustments in the data rate. Section "2.2.1
*                   USB Packets" of Audio Data Formats 1.0 Specification limits variation in the packet
*                   size by +/- 1 sample frame.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_INT16U  USBD_Audio_MaxPktLenGet (USBD_AUDIO_AS_ALT_CFG  *p_as_cfg,
                                             USBD_ERR               *p_err)
{
    CPU_INT16U  audio_frame_per_pkt;
    CPU_INT08U  audio_frame_len;
    CPU_INT32U  max_sam_freq;
    CPU_INT08U  sync_type;
    CPU_INT08U  sam_freq_ix;
    CPU_INT16U  max_pkt_len;


    audio_frame_len = p_as_cfg->SubframeSize * p_as_cfg->NbrCh;

    if (p_as_cfg->NbrSamplingFreq == 0u) {
        max_sam_freq = p_as_cfg->UpperSamplingFreq;
    } else {
        max_sam_freq = 0u;
        for(sam_freq_ix = 0u; sam_freq_ix < p_as_cfg->NbrSamplingFreq; sam_freq_ix++) {
            if(p_as_cfg->SamplingFreqTblPtr[sam_freq_ix] > max_sam_freq) {
                max_sam_freq = p_as_cfg->SamplingFreqTblPtr[sam_freq_ix];
            }
        }
    }
                                                                /* Find the max number of audio frame per pkt (1ms)...  */
    audio_frame_per_pkt = max_sam_freq / 1000u;                 /* ...i.e. nbr of audio samples per frame.              */
    if ((max_sam_freq % 1000u) > 0u) {
        audio_frame_per_pkt += 1u;
    }
                                                                /* Find the max pkt len.                                */
    sync_type = p_as_cfg->EP_SynchType & USBD_EP_TYPE_SYNC_MASK;
    switch (sync_type) {
        case USBD_EP_TYPE_SYNC_SYNC:
             max_pkt_len = audio_frame_per_pkt * audio_frame_len;
            *p_err       = USBD_ERR_NONE;
             break;


        case USBD_EP_TYPE_SYNC_ASYNC:
        case USBD_EP_TYPE_SYNC_ADAPTIVE:
                                                                /* See Note #1.                                         */
             max_pkt_len = (audio_frame_per_pkt + 1u) * audio_frame_len;
            *p_err       =  USBD_ERR_NONE;
             break;


        default:
            *p_err       = USBD_ERR_FAIL;
             max_pkt_len = 0u;
             break;
    }

    return (max_pkt_len);
}
#endif

