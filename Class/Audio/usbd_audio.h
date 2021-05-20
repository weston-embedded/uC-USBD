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
* Filename : usbd_audio.h
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

#ifndef  USBD_AUDIO_MODULE_PRESENT
#define  USBD_AUDIO_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <usbd_cfg.h>                                          /* Device Configuration file.                           */
#include  "../../Source/usbd_core.h"
#include  <cpu.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef   USBD_AUDIO_MODULE
#define  USBD_AUDIO_EXT
#else
#define  USBD_AUDIO_EXT  extern
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  USBD_AUDIO_STREAM_NBR_BUF_6                            6u
#define  USBD_AUDIO_STREAM_NBR_BUF_12                          12u
#define  USBD_AUDIO_STREAM_NBR_BUF_18                          18u
#define  USBD_AUDIO_STREAM_NBR_BUF_24                          24u
#define  USBD_AUDIO_STREAM_NBR_BUF_30                          30u
#define  USBD_AUDIO_STREAM_NBR_BUF_36                          36u
#define  USBD_AUDIO_STREAM_NBR_BUF_42                          42u

#define  USBD_AUDIO_TERMINAL_NO_ASSOCIATION                     0u


/*
*********************************************************************************************************
*                                      AUDIO CLASS-SPECIFIC REQ
*
* Note(s):  (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 5.2.1.1 and appendix A.9 for more details about Audio class-specific SET_XXX
*               requests.
*
*           (2) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 5.2.1.2 and appendix A.9 for more details about Audio class-specific GET_XXX
*               requests.
*********************************************************************************************************
*/
                                                                /* See Note #1.                                         */
#define  USBD_AUDIO_REQ_SET_CUR                         0x01u
#define  USBD_AUDIO_REQ_SET_MIN                         0x02u
#define  USBD_AUDIO_REQ_SET_MAX                         0x03u
#define  USBD_AUDIO_REQ_SET_RES                         0x04u
#define  USBD_AUDIO_REQ_SET_MEM                         0x05u
                                                                /* See Note #2.                                         */
#define  USBD_AUDIO_REQ_GET_CUR                         0x81u
#define  USBD_AUDIO_REQ_GET_MIN                         0x82u
#define  USBD_AUDIO_REQ_GET_MAX                         0x83u
#define  USBD_AUDIO_REQ_GET_RES                         0x84u
#define  USBD_AUDIO_REQ_GET_MEM                         0x85u
#define  USBD_AUDIO_REQ_GET_STAT                        0xFFu


/*
*********************************************************************************************************
*                                           TERMINAL TYPES
*
* Note(s) : (1) See 'USB Device Class Definition for Terminal Types, Release 1.0, March 18, 1998',
*               section 2.1 for more details about audio terminal types.
*********************************************************************************************************
*/

                                                                /* USB Terminal types.                                  */
#define  USBD_AUDIO_TERMINAL_TYPE_USB_STREAMING                 0x0101u
#define  USBD_AUDIO_TERMINAL_TYPE_USB_VENDOR                    0x01FFu

                                                                /* Input Terminal types.                                */
#define  USBD_AUDIO_TERMINAL_TYPE_IT_UNDEFINED                  0x0200u
#define  USBD_AUDIO_TERMINAL_TYPE_MIC                           0x0201u
#define  USBD_AUDIO_TERMINAL_TYPE_DESKTOP_MIC                   0x0202u
#define  USBD_AUDIO_TERMINAL_TYPE_PERSONAL_MIC                  0x0203u
#define  USBD_AUDIO_TERMINAL_TYPE_OMNI_DIR_MIC                  0x0204u
#define  USBD_AUDIO_TERMINAL_TYPE_MIC_ARRAY                     0x0205u
#define  USBD_AUDIO_TERMINAL_TYPE_PROC_MIC_ARRAY                0x0206u

                                                                /* Output Terminal types.                               */
#define  USBD_AUDIO_TERMINAL_TYPE_OT_UNDEFINED                  0x0300u
#define  USBD_AUDIO_TERMINAL_TYPE_SPEAKER                       0x0301u
#define  USBD_AUDIO_TERMINAL_TYPE_HEADPHONES                    0x0302u
#define  USBD_AUDIO_TERMINAL_TYPE_HEAD_MOUNTED                  0x0303u
#define  USBD_AUDIO_TERMINAL_TYPE_DESKTOP_SPEAKER               0x0304u
#define  USBD_AUDIO_TERMINAL_TYPE_ROOM_SPEAKER                  0x0305u
#define  USBD_AUDIO_TERMINAL_TYPE_COMM_SPEAKER                  0x0306u
#define  USBD_AUDIO_TERMINAL_TYPE_LOW_FREQ_SPEAKER              0x0307u

                                                                /* Bi-directional Terminal types.                       */
#define  USBD_AUDIO_TERMINAL_TYPE_BI_DIR_UNDEFINED              0x0400u
#define  USBD_AUDIO_TERMINAL_TYPE_HANDSET                       0x0401u
#define  USBD_AUDIO_TERMINAL_TYPE_HEADSET                       0x0402u
#define  USBD_AUDIO_TERMINAL_TYPE_SPEAKERPHONE                  0x0403u
#define  USBD_AUDIO_TERMINAL_TYPE_ECHO_SUP_SPEAKERPHONE         0x0404u
#define  USBD_AUDIO_TERMINAL_TYPE_ECHO_CANCEL_SPEAKERPHONE      0x0405u

                                                                /* Telephony Terminal types.                            */
#define  USBD_AUDIO_TERMINAL_TYPE_PHONELINE                     0x0501u
#define  USBD_AUDIO_TERMINAL_TYPE_TEL                           0x0502u
#define  USBD_AUDIO_TERMINAL_TYPE_DOWN_LINE_PHONE               0x0503u

                                                                /* External Terminal types.                             */
#define  USBD_AUDIO_TERMINAL_TYPE_ANALOG_CONNECTOR              0x0601u
#define  USBD_AUDIO_TERMINAL_TYPE_DIGITAL_IF                    0x0602u
#define  USBD_AUDIO_TERMINAL_TYPE_LINE_CONNECTOR                0x0603u
#define  USBD_AUDIO_TERMINAL_TYPE_LEGACY_CONNECTOR              0x0604u
#define  USBD_AUDIO_TERMINAL_TYPE_SPDIF_IF                      0x0605u
#define  USBD_AUDIO_TERMINAL_TYPE_1394_DA_STREAM                0x0606u
#define  USBD_AUDIO_TERMINAL_TYPE_1394_DV_STREAM                0x0607u

                                                                /* Embedded Function Terminal types.                    */
#define  USBD_AUDIO_TERMINAL_TYPE_LOW_CALIBRATION_NOISE_SRC     0x0701u
#define  USBD_AUDIO_TERMINAL_TYPE_EQUALIZATION_NOISE            0x0702u
#define  USBD_AUDIO_TERMINAL_TYPE_CD_PLAYER                     0x0703u
#define  USBD_AUDIO_TERMINAL_TYPE_DAT                           0x0704u
#define  USBD_AUDIO_TERMINAL_TYPE_DCC                           0x0705u
#define  USBD_AUDIO_TERMINAL_TYPE_MINIDISK                      0x0706u
#define  USBD_AUDIO_TERMINAL_TYPE_ANALOG_TAPE                   0x0707u
#define  USBD_AUDIO_TERMINAL_TYPE_PHONOGRAPH                    0x0708u
#define  USBD_AUDIO_TERMINAL_TYPE_VCR                           0x0709u
#define  USBD_AUDIO_TERMINAL_TYPE_VIDEO_DISC                    0x070Au
#define  USBD_AUDIO_TERMINAL_TYPE_DVD                           0x070Bu
#define  USBD_AUDIO_TERMINAL_TYPE_TC_TUNER                      0x070Cu
#define  USBD_AUDIO_TERMINAL_TYPE_SATELLITE_RECEIVER            0x070Du
#define  USBD_AUDIO_TERMINAL_TYPE_CABLE_TUNER                   0x070Eu
#define  USBD_AUDIO_TERMINAL_TYPE_DSS                           0x070Fu
#define  USBD_AUDIO_TERMINAL_TYPE_RADIO_RECEIVER                0x0710u
#define  USBD_AUDIO_TERMINAL_TYPE_RADIO_TRANSMITTER             0x0711u
#define  USBD_AUDIO_TERMINAL_TYPE_MULTITRACK_RECORDER           0x0712u
#define  USBD_AUDIO_TERMINAL_TYPE_SYNTHESIZER                   0x0713u

                                                                /* General terminal type define.                        */
#define  USBD_AUDIO_INVALID_ID                                  0u

#define  USBD_AUDIO_TERMINAL_LINK_MAX_NBR                       DEF_INT_08U_MAX_VAL
#define  USBD_AUDIO_TERMINAL_LINK_NONE                          0u


/*
*********************************************************************************************************
*                                       LOG CH SPACIAL LOCATION

* Note(s) : (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 3.7.2.3 for more details about logical channel spacial locations.
*********************************************************************************************************
*/

#define  USBD_AUDIO_SPATIAL_LOCATION_LEFT_FRONT                 DEF_BIT_00
#define  USBD_AUDIO_SPATIAL_LOCATION_RIGHT_FRONT                DEF_BIT_01
#define  USBD_AUDIO_SPATIAL_LOCATION_CENTER_FRONT               DEF_BIT_02
#define  USBD_AUDIO_SPATIAL_LOCATION_LFE                        DEF_BIT_03
#define  USBD_AUDIO_SPATIAL_LOCATION_LEFT_SURROUND              DEF_BIT_04
#define  USBD_AUDIO_SPATIAL_LOCATION_RIGHT_SURROUND             DEF_BIT_05
#define  USBD_AUDIO_SPATIAL_LOCATION_LEFT_CENTER                DEF_BIT_06
#define  USBD_AUDIO_SPATIAL_LOCATION_RIGHT_CENTER               DEF_BIT_07
#define  USBD_AUDIO_SPATIAL_LOCATION_SURROUND                   DEF_BIT_08
#define  USBD_AUDIO_SPATIAL_LOCATION_SIDE_LEFT                  DEF_BIT_09
#define  USBD_AUDIO_SPATIAL_LOCATION_SIDE_RIGHT                 DEF_BIT_10
#define  USBD_AUDIO_SPATIAL_LOCATION_TOP                        DEF_BIT_11

                                                                /* General spatial location define.                     */
#define  USBD_AUDIO_LOG_CH_NBR_MAX                             10u
#define  USBD_AUDIO_LOG_CH_CFG_MAX                             11u

#define  USBD_AUDIO_MONO                                        1u
#define  USBD_AUDIO_STEREO                                      2u
#define  USBD_AUDIO_5_1                                         6u
#define  USBD_AUDIO_7_1                                         8u

#define  USBD_AUDIO_LOG_CH_NBR_1                                1u
#define  USBD_AUDIO_LOG_CH_NBR_2                                2u
#define  USBD_AUDIO_LOG_CH_NBR_3                                3u
#define  USBD_AUDIO_LOG_CH_NBR_4                                4u
#define  USBD_AUDIO_LOG_CH_NBR_5                                5u
#define  USBD_AUDIO_LOG_CH_NBR_6                                6u
#define  USBD_AUDIO_LOG_CH_NBR_7                                7u
#define  USBD_AUDIO_LOG_CH_NBR_8                                8u
#define  USBD_AUDIO_LOG_CH_NBR_9                                9u
#define  USBD_AUDIO_LOG_CH_NBR_10                              10u


/*
*********************************************************************************************************
*                                        AUDIO DATA FMT TYPE I
*
* Note(s) : (1) See 'USB Device Class Definition for Audio Data Formats, Release 1.0, March 18, 1998',
*               section 2.2.6 and appendix A.1.1 for more details about Type I Audio Data Formats.
*********************************************************************************************************
*/
                                                                /* Data format type.                                    */
#define  USBD_AUDIO_DATA_FMT_TYPE_I_UNDEFINED                   0u
#define  USBD_AUDIO_DATA_FMT_TYPE_I_PCM                         1u
#define  USBD_AUDIO_DATA_FMT_TYPE_I_PCM8                        2u
#define  USBD_AUDIO_DATA_FMT_TYPE_I_IEEE_FLOAT                  3u
#define  USBD_AUDIO_DATA_FMT_TYPE_I_ALAW                        4u
#define  USBD_AUDIO_DATA_FMT_TYPE_I_MULAW                       5u

                                                                /* Type I fmt characteristics.                          */
#define  USBD_AUDIO_FMT_TYPE_I_SAM_FREQ_CONTINUOUS              0u
#define  USBD_AUDIO_FMT_TYPE_I_SUBFRAME_SIZE_1                  1u
#define  USBD_AUDIO_FMT_TYPE_I_SUBFRAME_SIZE_2                  2u
#define  USBD_AUDIO_FMT_TYPE_I_SUBFRAME_SIZE_3                  3u
#define  USBD_AUDIO_FMT_TYPE_I_SUBFRAME_SIZE_4                  4u
#define  USBD_AUDIO_FMT_TYPE_I_BIT_RESOLUTION_8                 8u
#define  USBD_AUDIO_FMT_TYPE_I_BIT_RESOLUTION_16               16u
#define  USBD_AUDIO_FMT_TYPE_I_BIT_RESOLUTION_24               24u
#define  USBD_AUDIO_FMT_TYPE_I_BIT_RESOLUTION_32               32u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_8KHZ                  8000u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_11KHZ                11025u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_16KHZ                16000u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_22KHZ                22050u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_32KHZ                32000u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_44_1KHZ              44100u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_48KHZ                48000u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_88_2KHZ              88200u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_96KHZ                96000u
#define  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_MAX                      USBD_AUDIO_FMT_TYPE_I_SAMFREQ_96KHZ


/*
*********************************************************************************************************
*                                     TERMINAL & UNIT'S CONTROLS
*
* Note(s) : (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 5.2.2.1.3.1 for more details about Copy Protect Control used by Terminals.
*
*           (2) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               Table 4-7 for more details about Feature Unit Controls.
*
*               (a) See See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*                   Table 5-18 for more details about volume value.
*
*           (3) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 4.3.2.3 for more details about Mixer Unit Controls.
*********************************************************************************************************
*/
                                                                /* Terminal Copy Pro Ctrl (see Note #1).                */
#define  USBD_AUDIO_CPL_NONE                                  255u
#define  USBD_AUDIO_CPL0                                        0u
#define  USBD_AUDIO_CPL1                                        1u
#define  USBD_AUDIO_CPL2                                        2u

                                                                /* Feature Unit Controls bitmap (see Note #2).          */
#define  USBD_AUDIO_FU_CTRL_NONE                                DEF_BIT_NONE
#define  USBD_AUDIO_FU_CTRL_MUTE                                DEF_BIT_00
#define  USBD_AUDIO_FU_CTRL_VOL                                 DEF_BIT_01
#define  USBD_AUDIO_FU_CTRL_BASS                                DEF_BIT_02
#define  USBD_AUDIO_FU_CTRL_MID                                 DEF_BIT_03
#define  USBD_AUDIO_FU_CTRL_TREBLE                              DEF_BIT_04
#define  USBD_AUDIO_FU_CTRL_GRAPHIC_EQUALIZER                   DEF_BIT_05
#define  USBD_AUDIO_FU_CTRL_AUTO_GAIN                           DEF_BIT_06
#define  USBD_AUDIO_FU_CTRL_DELAY                               DEF_BIT_07
#define  USBD_AUDIO_FU_CTRL_BASS_BOOST                          DEF_BIT_08
#define  USBD_AUDIO_FU_CTRL_LOUDNESS                            DEF_BIT_09
                                                                /* See Note #2a.                                        */
#define  USBD_AUDIO_FU_CTRL_VOL_SILENCE                    0x8000u

                                                                /* Graphic Equalizer frequency bands.                   */
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_14                DEF_BIT_00
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_15                DEF_BIT_01
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_16                DEF_BIT_02
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_17                DEF_BIT_03
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_18                DEF_BIT_04
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_19                DEF_BIT_05
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_20                DEF_BIT_06
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_21                DEF_BIT_07
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_22                DEF_BIT_08
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_23                DEF_BIT_09
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_24                DEF_BIT_10
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_25                DEF_BIT_11
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_26                DEF_BIT_12
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_27                DEF_BIT_13
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_28                DEF_BIT_14
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_29                DEF_BIT_15
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_30                DEF_BIT_16
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_31                DEF_BIT_17
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_32                DEF_BIT_18
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_33                DEF_BIT_19
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_34                DEF_BIT_20
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_35                DEF_BIT_21
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_36                DEF_BIT_22
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_37                DEF_BIT_23
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_38                DEF_BIT_24
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_39                DEF_BIT_25
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_40                DEF_BIT_26
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_41                DEF_BIT_27
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_42                DEF_BIT_28
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_43                DEF_BIT_29
#define  USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_ALL     0x3FFFFFFFu
#define  USBD_AUDIO_FU_OCTAVE_EQUALIZER_BANDS                  (USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_15 |  \
                                                                USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_18 |  \
                                                                USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_21 |  \
                                                                USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_24 |  \
                                                                USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_27 |  \
                                                                USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_30 |  \
                                                                USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_33 |  \
                                                                USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_36 |  \
                                                                USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_39 |  \
                                                                USBD_AUDIO_FU_GRAPHIC_EQUALIZER_BAND_42)

                                                                /* Mixer Unit Controls (see Note #3).                   */
#define  USBD_AUDIO_MU_CTRL_NONE                                DEF_BIT_NONE


/*
*********************************************************************************************************
*                                       ISOC AUDIO DATA EP INFO
*
* Note(s) : (1) See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               Table 4-21 for more details about Endpoint Controls.
*********************************************************************************************************
*/

                                                                /* EP ctrl (see Note #1).                               */
#define  USBD_AUDIO_AS_EP_CTRL_NONE                             DEF_BIT_NONE
#define  USBD_AUDIO_AS_EP_CTRL_SAMPLING_FREQ                    DEF_BIT_00
#define  USBD_AUDIO_AS_EP_CTRL_PITCH                            DEF_BIT_01
#define  USBD_AUDIO_AS_EP_CTRL_MAX_PKT_ONLY                     DEF_BIT_07

                                                                /* bLockDelayUnits (see Note #1).                       */
#define  USBD_AUDIO_AS_EP_LOCK_DLY_UND                          DEF_BIT_NONE
#define  USBD_AUDIO_AS_EP_LOCK_DLY_MS                           DEF_BIT_01
#define  USBD_AUDIO_AS_EP_LOCK_DLY_PCM                          DEF_BIT_02


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       AUDIO STREAMING HANDLE
*
* Note(s) : (1) The audio handle is a 16-bit wide value that contains the AudioStreaming interface index:
*
*                           MSB                                                               LSB
*               ----------------------------------------------------------------------------------
*               | POSITION | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
*               ----------------------------------------------------------------------------------
*               | USAGE    |           Validate Count            |          AS IF Index          |
*               ----------------------------------------------------------------------------------
*********************************************************************************************************
*/

typedef  void       *USBD_AUDIO_AS_IF_HANDLE;                   /* Handle for Audio Transport & app.                    */

typedef  CPU_INT16U  USBD_AUDIO_AS_HANDLE;                      /* Handle for Audio Transport/Processing & codec drv... */
                                                                /* ...see Note #1.                                      */


/*
*********************************************************************************************************
*                                        FORWARD DECLARATIONS
*********************************************************************************************************
*/

typedef  struct  usbd_audio_drv  USBD_AUDIO_DRV;


/*
*********************************************************************************************************
*                                        AUDIO EVENT CALLBACKS
*********************************************************************************************************
*/

typedef  const  struct  usbd_audio_event_fncts {

    void  (*Conn)   (CPU_INT08U            dev_nbr,             /* Notify app that cfg is active.                       */
                     CPU_INT08U            cfg_nbr,
                     CPU_INT08U            terminal_id,
                     USBD_AUDIO_AS_HANDLE  as_handle);

    void  (*Disconn)(CPU_INT08U            dev_nbr,             /* Notify class that cfg is not active.                 */
                     CPU_INT08U            cfg_nbr,
                     CPU_INT08U            terminal_id,
                     USBD_AUDIO_AS_HANDLE  as_handle);
} USBD_AUDIO_EVENT_FNCTS;


/*
*********************************************************************************************************
*                                          AUDIO STATISTICS
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
typedef  struct  usbd_audio_stat {
    CPU_INT32U  AudioProc_Playback_NbrIsocRxSubmitSuccess;      /* Nbr of isoc OUT xfer submitted w/ success to core.   */
    CPU_INT32U  AudioProc_Playback_NbrIsocRxSubmitErr;          /* Nbr of isoc OUT xfer submitted w/ err.               */
    CPU_INT32U  AudioProc_Playback_NbrIsocRxSubmitPlaybackTask; /* Nbr of isoc OUT xfer submitted by playback task.     */
    CPU_INT32U  AudioProc_Playback_NbrIsocRxSubmitCoreTask;     /* Nbr of isoc OUT xfer submitted by core task.         */
    CPU_INT32U  AudioProc_Playback_NbrIsocRxCmpl;               /* Nbr of isoc OUT xfer completed w/ or w/o err.        */
    CPU_INT32U  AudioProc_Playback_NbrIsocRxCmplErrOther;       /* Nbr of isoc OUT xfer completed with error.           */
    CPU_INT32U  AudioProc_Playback_NbrIsocRxCmplErrAbort;       /* Nbr of isoc OUT xfer completed with err EP_ABORT.    */
    CPU_INT32U  AudioProc_Playback_NbrIsocRxBufNotAvail;        /* Nbr of times no buf avail.                           */
    CPU_INT32U  AudioProc_Playback_NbrReqPostPlaybackTask;      /* Nbr of req submitted to playback task.               */
    CPU_INT32U  AudioProc_Playback_NbrReqPendPlaybackTask;      /* Nbr of req gotten by playback task.                  */
    CPU_INT32U  AudioProc_Playback_NbrIsocRxOngoingCnt;         /* Nbr of ongoing isoc OUT xfers.                       */
#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED)
    CPU_INT32U  AudioProc_Playback_SynchNbrBufGet;              /* Nbr of synch buf gotten from pool.                   */
    CPU_INT32U  AudioProc_Playback_SynchNbrBufFree;             /* Nbr of synch buf returned to pool.                   */
    CPU_INT32U  AudioProc_Playback_SynchNbrBufNotAvail;         /* Nbr of synch buf not avail.                          */
    CPU_INT32U  AudioProc_Playback_SynchNbrSafeZone;            /* Nbr of normal situations without corr.               */
    CPU_INT32U  AudioProc_Playback_SynchNbrOverrun;             /* Nbr of overrun situations requiring corr.            */
    CPU_INT32U  AudioProc_Playback_SynchNbrLightOverrun;        /* Nbr of light overrun situations.                     */
    CPU_INT32U  AudioProc_Playback_SynchNbrHeavyOverrun;        /* Nbr of heavy overrun situations.                     */
    CPU_INT32U  AudioProc_Playback_SynchNbrUnderrun;            /* Nbr of underrun situations requiring corr.           */
    CPU_INT32U  AudioProc_Playback_SynchNbrLightUnderrun;       /* Nbr of light underrun situations.                    */
    CPU_INT32U  AudioProc_Playback_SynchNbrHeavyUnderrun;       /* Nbr of heavy underrun situations.                    */
    CPU_INT32U  AudioProc_Playback_SynchNbrRefreshPeriodReached;/* Nbr of times refresh period is reached.              */
    CPU_INT32U  AudioProc_Playback_SynchNbrIsocTxSubmitted;     /* Nbr of isoc IN xfer submitted to core.               */
    CPU_INT32U  AudioProc_Playback_SynchNbrIsocTxCmpl;          /* Nbr of isoc IN xfer completed.                       */
#endif

    CPU_INT32U  AudioProc_Record_NbrIsocTxSubmitSuccess;        /* Nbr of isoc IN xfer submitted w/ success to core.    */
    CPU_INT32U  AudioProc_Record_NbrIsocTxSubmitErr;            /* Nbr of isoc IN xfer submitted w/ err.                */
    CPU_INT32U  AudioProc_Record_NbrIsocTxSubmitRecordTask;     /* Nbr of isoc IN xfer submitted by record task.        */
    CPU_INT32U  AudioProc_Record_NbrIsocTxSubmitCoreTask;       /* Nbr of isoc IN xfer submitted by core task.          */
    CPU_INT32U  AudioProc_Record_NbrIsocTxCmpl;                 /* Nbr of isoc IN xfer completed w/ or w/o err.         */
    CPU_INT32U  AudioProc_Record_NbrIsocTxCmplErrOther;         /* Nbr of isoc IN xfer completed with err.              */
    CPU_INT32U  AudioProc_Record_NbrIsocTxCmplErrAbort;         /* Nbr of isoc IN xfer completed with err EP_ABORT.     */
    CPU_INT32U  AudioProc_Record_NbrIsocTxBufNotAvail;          /* Nbr of times no buf avail.                           */
    CPU_INT32U  AudioProc_Record_NbrReqPostRecordTask;          /* Nbr of ready buf signaled to record task.            */
    CPU_INT32U  AudioProc_Record_NbrReqPendRecordTask;          /* Nbr of ready buf signals received by record task.    */

    CPU_INT32U  AudioProc_RingBufQ_NbrProducerStartIxCatchUp;   /* Nbr of catch of prev and/or nxt ix by ProducerStart. */
    CPU_INT32U  AudioProc_RingBufQ_NbrProducerEndIxCatchUp;     /* Nbr of catch of prev and/or nxt ix by ProducerEnd.   */
    CPU_INT32U  AudioProc_RingBufQ_NbrConsumerStartIxCatchUp;   /* Nbr of catch of prev and/or nxt ix by ConsumerStart. */
    CPU_INT32U  AudioProc_RingBufQ_NbrConsumerEndIxCatchUp;     /* Nbr of catch of prev and/or nxt ix by ConsumerEnd.   */
    CPU_INT32U  AudioProc_RingBufQ_NbrProducerStartIxWrapAround;/* Nbr of wrap-around for ix ProducerStart.             */
    CPU_INT32U  AudioProc_RingBufQ_NbrProducerEndIxWrapAround;  /* Nbr of wrap-around for ix ProducerEnd.               */
    CPU_INT32U  AudioProc_RingBufQ_NbrConsumerStartIxWrapAround;/* Nbr of wrap-around for ix ConsumerStart.             */
    CPU_INT32U  AudioProc_RingBufQ_NbrConsumerEndIxWrapAround;  /* Nbr of wrap-around for ix ConsumerEnd.               */
    CPU_INT32U  AudioProc_RingBufQ_NbrBufDescInUse;             /* Nbr of buf desc in use by playback or record.        */
    CPU_INT32U  AudioProc_RingBufQ_NbrErr;                      /* Nbr of err while getting buf from ring buf queue.    */

    CPU_INT32U  AudioProc_NbrStreamOpen;                        /* Nbr of stream openings.                              */
    CPU_INT32U  AudioProc_NbrStreamClosed;                      /* Nbr of stream closings.                              */
#if (((USBD_AUDIO_CFG_PLAYBACK_CORR_EN == DEF_ENABLED)  && \
      (USBD_AUDIO_CFG_PLAYBACK_EN      == DEF_ENABLED)) || \
     ((USBD_AUDIO_CFG_RECORD_CORR_EN   == DEF_ENABLED)  && \
      (USBD_AUDIO_CFG_RECORD_EN        == DEF_ENABLED)))
    CPU_INT32U  AudioProc_CorrNbrUnderrun;                      /* Nbr of underrun situations requiring stream corr.    */
    CPU_INT32U  AudioProc_CorrNbrOverrun;                       /* Nbr of overrun situations requiring stream corr.     */
    CPU_INT32U  AudioProc_CorrNbrSafeZone;                      /* Nbr of normal situations without stream corr.        */
#endif

    CPU_INT32U  AudioDrv_Playback_DMA_NbrXferCmpl;              /* Nbr of playback buf consumed by codec drv.           */
    CPU_INT32U  AudioDrv_Playback_DMA_NbrSilenceBuf;            /* Nbr of silence buf consumed by codec drv.            */
    CPU_INT32U  AudioDrv_Record_DMA_NbrXferCmpl;                /* Nbr of record buf produced by codec drv.             */
    CPU_INT32U  AudioDrv_Record_DMA_NbrDummyBuf;                /* Nbr of dummy buf used by codec drv because no...     */
                                                                /* ...empty record buf avail.                           */
} USBD_AUDIO_STAT;
#endif


/*
*********************************************************************************************************
*                                            TERMINAL CFG
*********************************************************************************************************
*/

typedef  const  struct  usbd_audio_it_cfg {                     /* Input Terminal.                                      */
    CPU_INT16U    TerminalType;
    CPU_INT08U    LogChNbr;                                     /* Nbr of log output ch in Terminal Output.             */
    CPU_INT16U    LogChCfg;                                     /* Spatial location of log ch.                          */
    CPU_BOOLEAN   CopyProtEn;                                   /* Dis or En Copy Protection.                           */
    CPU_INT08U    CopyProtLevel;                                /* Copy Protection Level.                               */
    CPU_CHAR     *StrPtr;                                       /* Str describing IT. Obtained by Str Desc.             */
} USBD_AUDIO_IT_CFG;



typedef  const  struct  usbd_audio_ot_cfg {                     /* Output Terminal.                                     */
    CPU_INT16U    TerminalType;
    CPU_BOOLEAN   CopyProtEn;                                   /* Copy Protect Control enable.                         */
    CPU_CHAR     *StrPtr;                                       /* Str describing OT. Obtained by Str Desc.             */
} USBD_AUDIO_OT_CFG;


/*
*********************************************************************************************************
*                                              UNIT CFG
*********************************************************************************************************
*/

typedef  const  struct  usbd_audio_fu_cfg {                     /* Feature Unit.                                        */
    CPU_INT08U   LogChNbr;                                      /* Nbr of log ch.                                       */
    CPU_INT16U  *LogChCtrlPtr;                                  /* Ptr to Feature Unit Controls tbl.                    */
    CPU_CHAR    *StrPtr;                                        /* Str describing FU. Obtained by Str Desc.             */
} USBD_AUDIO_FU_CFG;

typedef  const  struct  usbd_audio_mu_cfg {                     /* Mixer Unit.                                          */
    CPU_INT08U   NbrInPins;                                     /* Nbr of Input Pins.                                   */
    CPU_INT08U   LogInChNbr;                                    /* Nbr of log input ch.                                 */
    CPU_INT08U   LogOutChNbr;                                   /* Nbr of log output ch.                                */
    CPU_INT16U   LogOutChCfg;                                   /* Spatial location of log output ch.                   */
    CPU_CHAR    *StrPtr;                                        /* Str describing MU. Obtained by Str Desc.             */
} USBD_AUDIO_MU_CFG;

typedef  const  struct  usbd_audio_su_cfg {                     /* Selector Unit.                                       */
    CPU_INT08U   NbrInPins;                                     /* Nbr of Input Pins.                                   */
    CPU_CHAR    *StrPtr;                                        /* Str describing SU. Obtained by Str Desc.             */
} USBD_AUDIO_SU_CFG;


/*
*********************************************************************************************************
*                                         AUDIO STREAMING CFG
*
* Note(s) : (1) This structure contains relevant fields for Type I data format only. Type II and III are
*               not supported.
*
*           (2) The Delay holds a value that is a measure for the delay that is introduced in the audio
*               data stream due to internal processing of the signal within the audio function. The value
*               of the delay is expressed in number of frames (i.e. in ms).
*               See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 4.5.2 for more details about class-specific AudioStreaming descriptor.
*
*           (3) Type I format type descriptor reports the sampling frequency capabilities of the
*               isochronous data endpoint with the field 'bSamFreqType'. The possible values are:
*
*               (a) USBD_AUDIO_FMT_TYPE_I_SAM_FREQ_CONTINUOUS   Continuous sampling frequency
*
*               (b) 1..255                                      number of discrete sampling frequencies
*                                                               supported by the isochronous data endpoint.
*
*           (4) These fields relate to 'bLockDelayUnits' and 'wLockDelay' fields of Class-Specific AS
*               Isochronous Audio Data Endpoint Descriptor. 'bLockDelayUnits' and 'wLockDelay' indicate
*               to the Host how long it takes for the clock recovery circuitry of this endpoint to lock
*               and reliably produce or consume the audio data stream. Only applicable for synchronous
*               and adaptive endpoints.
*               See 'USB Device Class Definition for Audio Devices, Release 1.0, March 18, 1998',
*               section 4.6.1.2 for more details about class-specific endpoint descriptor.
*********************************************************************************************************
*/

typedef  const  struct  usbd_audio_as_alt_cfg {                 /* AudioStreaming alt IF (see Note #1).                 */
                                                                /* AS IF RELATED:                                       */
    CPU_INT08U               Dly;                               /* See Note #2.                                         */
    CPU_INT16U               FmtTag;                            /* Audio data fmt used to comm with this IF.            */
                                                                /* TYPE I FMT RELATED:                                  */
    CPU_INT08U               NbrCh;                             /* Nbr of physical ch in the audio data stream.         */
    CPU_INT08U               SubframeSize;                      /* Nbr of bytes occupied by one audio subframe.         */
    CPU_INT08U               BitRes;                            /* Effectively used bits in an audio subframe.          */
    CPU_INT08U               NbrSamplingFreq;                   /* Nbr of sampling frequencies (see Note #3).           */
    CPU_INT32U               LowerSamplingFreq;                 /* Lower bound in Hz of the sampling freq range.        */
    CPU_INT32U               UpperSamplingFreq;                 /* Upper bound in Hz of the sampling freq range.        */
    CPU_INT32U              *SamplingFreqTblPtr;                /* Tbl of discrete sampling frequencies.                */
                                                                /* AS EP RELATED:                                       */
    CPU_BOOLEAN              EP_DirIn;                          /* Flag indicating direction IN or OUT.                 */
    CPU_INT08U               EP_SynchType;                      /* Synchronization type supported by Isoc EP.           */
                                                                /* CLASS SPECIFIC EP RELATED:                           */
    CPU_INT08U               EP_Attrib;                         /* Class specific controls supported by Isoc EP.        */
    CPU_INT08U               EP_LockDlyUnits;                   /* See Note #4.                                         */
    CPU_INT16U               EP_LockDly;                        /* See Note #4.                                         */
                                                                /* SYNCH EP RELATED:                                    */
    CPU_INT08U               EP_SynchRefresh;                   /* Refresh Rate                                         */
} USBD_AUDIO_AS_ALT_CFG;

typedef  const  struct  usbd_audio_as_if_cfg {                  /* AudioStreaming IF.                                   */
    USBD_AUDIO_AS_ALT_CFG  **AS_CfgPtrTbl;                      /* Tbl of ptr to AS IF alternate settings Config.       */
    CPU_INT08U               AS_CfgAltSettingNbr;               /* Nbr of alternate settings for given AS IF.           */
} USBD_AUDIO_AS_IF_CFG;

typedef  const  struct  usbd_audio_stream_cfg {                 /* General audio stream cfg.                            */
    CPU_INT16U               MaxBufNbr;                         /* Max buf nbr.                                         */
    CPU_INT16U               CorrPeriodMs;                      /* Period at which corr must be monitored.              */
} USBD_AUDIO_STREAM_CFG;


/*
*********************************************************************************************************
*                                    PLAYBACK CORRECTION CALLBACK
*
* Note(s) : (1) Application callback used for custom playback correction algorithm provided by user
*               if available.
*********************************************************************************************************
*/

typedef  CPU_INT16U  (*USBD_AUDIO_PLAYBACK_CORR_FNCT)(USBD_AUDIO_AS_ALT_CFG  *p_as_alt_cfg,
                                                      CPU_BOOLEAN             is_underrun,
                                                      void                   *p_buf,
                                                      CPU_INT16U              buf_len_cur,
                                                      CPU_INT16U              buf_len_total,
                                                      USBD_ERR               *p_err);


/*
*********************************************************************************************************
*                                         STATISTICS CALLBACK
*
* Note(s) : (1) Statistics callback called upon device connection. It allows the user to get audio
*               statistics for a given AudioStreaming.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
typedef  void  (*USBD_AUDIO_AS_IF_STAT_FNCT)(CPU_INT08U        class_nbr,
                                             CPU_INT08U        terminal_id,
                                             USBD_AUDIO_STAT  *p_as_stat);
#endif


/*
*********************************************************************************************************
*                                          AUDIO DRIVER APIs
*********************************************************************************************************
*/

                                                                /* -------------------- COMMON API -------------------- */
typedef  struct  usbd_audio_drv_common_api {
    void         (*Init)                      (USBD_AUDIO_DRV        *p_audio_drv,
                                               USBD_ERR              *p_err);
} USBD_AUDIO_DRV_COMMON_API;

                                                                /* -------------------- AC OT API --------------------- */
typedef  struct  usbd_audio_drv_ac_ot_api {
    CPU_BOOLEAN  (*OT_CopyProtSet)            (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             terminal_id,
                                               CPU_INT08U             copy_prot_level);
} USBD_AUDIO_DRV_AC_OT_API;
                                                                /* -------------------- AC FU API --------------------- */
typedef  struct  usbd_audio_drv_ac_fu_api {
    CPU_BOOLEAN  (*FU_MuteManage)             (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_BOOLEAN            set_en,
                                               CPU_BOOLEAN           *p_mute);

    CPU_BOOLEAN  (*FU_VolManage)              (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             req,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_INT16U            *p_vol);

    CPU_BOOLEAN  (*FU_BassManage)             (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             req,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_INT08U            *p_bass);

    CPU_BOOLEAN  (*FU_MidManage)              (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             req,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_INT08U            *p_mid);

    CPU_BOOLEAN  (*FU_TrebleManage)           (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             req,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_INT08U            *p_treble);

    CPU_BOOLEAN  (*FU_GraphicEqualizerManage) (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             req,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_INT08U             nbr_bands_present,
                                               CPU_INT32U            *p_bm_bands_present,
                                               CPU_INT08U            *p_buf);

    CPU_BOOLEAN  (*FU_AutoGainManage)         (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_BOOLEAN            set_en,
                                               CPU_BOOLEAN           *p_auto_gain);

    CPU_BOOLEAN  (*FU_DlyManage)              (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             req,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_INT16U            *p_dly);

    CPU_BOOLEAN  (*FU_BassBoostManage)        (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_BOOLEAN            set_en,
                                               CPU_BOOLEAN           *p_bass_boost);

    CPU_BOOLEAN  (*FU_LoudnessManage)         (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_ch_nbr,
                                               CPU_BOOLEAN            set_en,
                                               CPU_BOOLEAN           *p_loudness);
} USBD_AUDIO_DRV_AC_FU_API;
                                                                /* -------------------- AC MU API --------------------- */
typedef  struct  usbd_audio_drv_ac_mu_api {
    CPU_BOOLEAN  (*MU_CtrlManage)             (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             req,
                                               CPU_INT08U             unit_id,
                                               CPU_INT08U             log_in_ch_nbr,
                                               CPU_INT08U             log_out_ch_nbr,
                                               CPU_INT16U            *p_ctrl);
} USBD_AUDIO_DRV_AC_MU_API;
                                                                /* -------------------- AC SU API --------------------- */
typedef  struct  usbd_audio_drv_ac_su_api {
    CPU_BOOLEAN  (*SU_InPinManage)            (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             unit_id,
                                               CPU_BOOLEAN            set_en,
                                               CPU_INT08U            *p_in_pin_nbr);
} USBD_AUDIO_DRV_AC_SU_API;
                                                                /* ---------------------- AS API ---------------------- */
typedef  struct  usbd_audio_drv_as_api {
    CPU_BOOLEAN   (*AS_SamplingFreqManage)    (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             terminal_id_link,
                                               CPU_BOOLEAN            set_en,
                                               CPU_INT32U            *p_sampling_freq);

    CPU_BOOLEAN   (*AS_PitchManage)           (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             terminal_id_link,
                                               CPU_BOOLEAN            set_en,
                                               CPU_BOOLEAN           *p_pitch);

    CPU_BOOLEAN   (*StreamStart)              (USBD_AUDIO_DRV        *p_audio_drv,
                                               USBD_AUDIO_AS_HANDLE   as_handle,
                                               CPU_INT08U             terminal_id_link);

    CPU_BOOLEAN   (*StreamStop)               (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             terminal_id_link);

    void          (*StreamRecordRx)           (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             terminal_id_link,
                                               void                  *p_buf,
                                               CPU_INT16U            *p_buf_len,
                                               USBD_ERR              *p_err);

    void          (*StreamPlaybackTx)         (USBD_AUDIO_DRV        *p_audio_drv,
                                               CPU_INT08U             terminal_id_link,
                                               void                  *p_buf,
                                               CPU_INT16U             buf_len,
                                               USBD_ERR              *p_err);
} USBD_AUDIO_DRV_AS_API;


/*
*********************************************************************************************************
*                                       AUDIO DRIVER DATA TYPE
*********************************************************************************************************
*/

struct  usbd_audio_drv {
    void  *DataPtr;
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


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

void                     USBD_Audio_Init           (       CPU_INT16U                      msg_qty,
                                                           USBD_ERR                       *p_err);

CPU_INT08U               USBD_Audio_Add            (       CPU_INT08U                      entity_cnt,
                                                    const  USBD_AUDIO_DRV_COMMON_API      *p_audio_drv_common_api,
                                                    const  USBD_AUDIO_EVENT_FNCTS         *p_audio_app_fnct,
                                                           USBD_ERR                       *p_err);

void                     USBD_Audio_CfgAdd         (       CPU_INT08U                      class_nbr,
                                                           CPU_INT08U                      dev_nbr,
                                                           CPU_INT08U                      cfg_nbr,
                                                           USBD_ERR                       *p_err);

CPU_BOOLEAN              USBD_Audio_IsConn         (       CPU_INT08U                      class_nbr);

CPU_INT08U               USBD_Audio_IT_Add         (       CPU_INT08U                      class_nbr,
                                                    const  USBD_AUDIO_IT_CFG              *p_it_cfg,
                                                           USBD_ERR                       *p_err);

CPU_INT08U               USBD_Audio_OT_Add         (       CPU_INT08U                      class_nbr,
                                                    const  USBD_AUDIO_OT_CFG              *p_ot_cfg,
                                                    const  USBD_AUDIO_DRV_AC_OT_API       *p_ot_api,
                                                           USBD_ERR                       *p_err);

CPU_INT08U               USBD_Audio_FU_Add         (       CPU_INT08U                      class_nbr,
                                                    const  USBD_AUDIO_FU_CFG              *p_fu_cfg,
                                                    const  USBD_AUDIO_DRV_AC_FU_API       *p_fu_api,
                                                           USBD_ERR                       *p_err);

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
CPU_INT08U               USBD_Audio_MU_Add         (       CPU_INT08U                      class_nbr,
                                                    const  USBD_AUDIO_MU_CFG              *p_mu_cfg,
                                                    const  USBD_AUDIO_DRV_AC_MU_API       *p_mu_api,
                                                           USBD_ERR                       *p_err);
#endif

#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
CPU_INT08U               USBD_Audio_SU_Add         (       CPU_INT08U                      class_nbr,
                                                    const  USBD_AUDIO_SU_CFG              *p_su_cfg,
                                                    const  USBD_AUDIO_DRV_AC_SU_API       *p_su_api,
                                                           USBD_ERR                       *p_err);
#endif

void                    USBD_Audio_IT_Assoc        (       CPU_INT08U                      class_nbr,
                                                           CPU_INT08U                      it_id,
                                                           CPU_INT08U                      assoc_terminal_id,
                                                           USBD_ERR                       *p_err);

void                    USBD_Audio_OT_Assoc        (       CPU_INT08U                      class_nbr,
                                                           CPU_INT08U                      ot_id,
                                                           CPU_INT08U                      entity_id_src,
                                                           CPU_INT08U                      assoc_terminal_id,
                                                           USBD_ERR                       *p_err);

void                    USBD_Audio_FU_Assoc        (       CPU_INT08U                      class_nbr,
                                                           CPU_INT08U                      fu_id,
                                                           CPU_INT08U                      src_entity_id,
                                                           USBD_ERR                       *p_err);

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
void                    USBD_Audio_MU_Assoc        (       CPU_INT08U                      class_nbr,
                                                           CPU_INT08U                      mu_id,
                                                           CPU_INT08U                     *p_src_entity_id,
                                                           CPU_INT08U                      nbr_input_pins,
                                                           USBD_ERR                       *p_err);
#endif

#if (USBD_AUDIO_CFG_MAX_NBR_SU > 0u)
void                    USBD_Audio_SU_Assoc        (       CPU_INT08U                      class_nbr,
                                                           CPU_INT08U                      su_id,
                                                           CPU_INT08U                     *p_src_entity_id,
                                                           CPU_INT08U                      nbr_input_pins,
                                                           USBD_ERR                       *p_err);
#endif

#if (USBD_AUDIO_CFG_MAX_NBR_MU > 0u)
void                    USBD_Audio_MU_MixingCtrlSet(       CPU_INT08U                      class_nbr,
                                                           CPU_INT08U                      mu_id,
                                                           CPU_INT08U                      log_in_ch_nbr,
                                                           CPU_INT08U                      log_out_ch_nbr,
                                                           USBD_ERR                       *p_err);
#endif

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
USBD_AUDIO_AS_IF_HANDLE  USBD_Audio_AS_IF_Cfg      (const  USBD_AUDIO_STREAM_CFG          *p_stream_cfg,
                                                    const  USBD_AUDIO_AS_IF_CFG           *p_as_if_cfg,
                                                    const  USBD_AUDIO_DRV_AS_API          *p_as_api,
                                                           MEM_SEG                        *p_seg,
                                                           CPU_INT08U                      terminal_ID,
                                                           USBD_AUDIO_PLAYBACK_CORR_FNCT   corr_callback,
                                                           USBD_ERR                       *p_err);

void                     USBD_Audio_AS_IF_Add      (       CPU_INT08U                      class_nbr,
                                                           CPU_INT08U                      cfg_nbr,
                                                           USBD_AUDIO_AS_IF_HANDLE         as_if_handle,
                                                    const  USBD_AUDIO_AS_IF_CFG           *p_as_if_cfg,
                                                    const  CPU_CHAR                       *p_as_cfg_name,
                                                           USBD_ERR                       *p_err);
#endif

#if (USBD_AUDIO_CFG_STAT_EN == DEF_ENABLED)
USBD_AUDIO_STAT  *USBD_Audio_AS_IF_StatGet         (       USBD_AUDIO_AS_HANDLE            as_handle);
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  USBD_AUDIO_CFG_PLAYBACK_EN
#error  "USBD_AUDIO_CFG_PLAYBACK_EN not #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#if    ((USBD_AUDIO_CFG_PLAYBACK_EN != DEF_ENABLED) && \
        (USBD_AUDIO_CFG_PLAYBACK_EN != DEF_DISABLED))
#error  "USBD_AUDIO_CFG_PLAYBACK_EN illegally #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#ifndef  USBD_AUDIO_CFG_RECORD_EN
#error  "USBD_AUDIO_CFG_RECORD_EN not #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#if    ((USBD_AUDIO_CFG_RECORD_EN != DEF_ENABLED) && \
        (USBD_AUDIO_CFG_RECORD_EN != DEF_DISABLED))
#error  "USBD_AUDIO_CFG_RECORD_EN illegally #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#if    (((USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
         (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)) && \
         (USBD_CFG_EP_ISOC_EN        == DEF_DISABLED))
#error  "USBD_CFG_EP_ISOC_EN illegally #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_AIC
#error  "USBD_AUDIO_CFG_MAX_NBR_AIC not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#if     (USBD_AUDIO_CFG_MAX_NBR_AIC < 1u)
#error  "USBD_AUDIO_CFG_MAX_NBR_AIC illegally #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_CFG
#error  "USBD_AUDIO_CFG_MAX_NBR_CFG not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#if     (USBD_AUDIO_CFG_MAX_NBR_CFG < 1u)
#error  "USBD_AUDIO_CFG_MAX_NBR_CFG illegally #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_AS_IF_PLAYBACK
#error  "USBD_AUDIO_CFG_MAX_NBR_AS_IF_PLAYBACK not #define'd in 'usbd_cfg.h' [MUST be >= 0]"
#endif

#if     (USBD_AUDIO_CFG_MAX_NBR_AS_IF_PLAYBACK < 0u)
#error  "USBD_AUDIO_CFG_MAX_NBR_AS_IF_PLAYBACK illegally #define'd in 'usbd_cfg.h' [MUST be >= 0]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_AS_IF_RECORD
#error  "USBD_AUDIO_CFG_MAX_NBR_AS_IF_RECORD not #define'd in 'usbd_cfg.h' [MUST be >= 0]"
#endif

#if     (USBD_AUDIO_CFG_MAX_NBR_AS_IF_RECORD < 0u)
#error  "USBD_AUDIO_CFG_MAX_NBR_AS_IF_RECORD illegally #define'd in 'usbd_cfg.h' [MUST be >= 0]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_IF_ALT
#error  "USBD_AUDIO_CFG_MAX_NBR_IF_ALT not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#if     (USBD_AUDIO_CFG_MAX_NBR_IF_ALT < 1u)
#error  "USBD_AUDIO_CFG_MAX_NBR_IF_ALT illegally #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_IT
#error  "USBD_AUDIO_CFG_MAX_NBR_IT not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#if     (USBD_AUDIO_CFG_MAX_NBR_IT < 1u)
#error  "USBD_AUDIO_CFG_MAX_NBR_IT illegally #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_OT
#error  "USBD_AUDIO_CFG_MAX_NBR_OT not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#if     (USBD_AUDIO_CFG_MAX_NBR_OT < 1u)
#error  "USBD_AUDIO_CFG_MAX_NBR_OT illegally #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_FU
#error  "USBD_AUDIO_CFG_MAX_NBR_FU not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#if     (USBD_AUDIO_CFG_MAX_NBR_FU < 1u)
#error  "USBD_AUDIO_CFG_MAX_NBR_FU illegally #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#ifndef  USBD_AUDIO_CFG_FU_MAX_CTRL
#error  "USBD_AUDIO_CFG_FU_MAX_CTRL not #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#if    ((USBD_AUDIO_CFG_FU_MAX_CTRL != DEF_ENABLED) && \
        (USBD_AUDIO_CFG_FU_MAX_CTRL != DEF_DISABLED))
#error  "USBD_AUDIO_CFG_FU_MAX_CTRL illegally #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_MU
#error  "USBD_AUDIO_CFG_MAX_NBR_MU not #define'd in 'usbd_cfg.h' [MUST be >= 1]"

#elif     (USBD_AUDIO_CFG_MAX_NBR_MU < 0u)
#error  "USBD_AUDIO_CFG_MAX_NBR_MU illegally #define'd in 'usbd_cfg.h' [MUST be >= 0]"
#endif

#ifndef  USBD_AUDIO_CFG_MAX_NBR_SU
#error  "USBD_AUDIO_CFG_MAX_NBR_SU not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#if     (USBD_AUDIO_CFG_MAX_NBR_SU < 0u)
#error  "USBD_AUDIO_CFG_MAX_NBR_SU illegally #define'd in 'usbd_cfg.h' [MUST be >= 0]"
#endif

#ifndef  USBD_AUDIO_CFG_CLASS_REQ_MAX_LEN
#error  "USBD_AUDIO_CFG_CLASS_REQ_MAX_LEN not #define'd in 'usbd_cfg.h' [MUST be >= 4] "
#endif

#if     (USBD_AUDIO_CFG_CLASS_REQ_MAX_LEN < 4u)
#error  "USBD_AUDIO_CFG_CLASS_REQ_MAX_LEN illegally #define'd in 'usbd_cfg.h' [MUST be >= 4]"
#endif

#ifndef  USBD_AUDIO_CFG_BUF_ALIGN_OCTETS
#error  "USBD_AUDIO_CFG_BUF_ALIGN_OCTETS not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#ifndef  USBD_AUDIO_CFG_PLAYBACK_CORR_EN
#error  "USBD_AUDIO_CFG_PLAYBACK_CORR_EN not #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#if    ((USBD_AUDIO_CFG_PLAYBACK_CORR_EN != DEF_ENABLED) && \
        (USBD_AUDIO_CFG_PLAYBACK_CORR_EN != DEF_DISABLED))
#error  "USBD_AUDIO_CFG_PLAYBACK_CORR_EN illegally #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#ifndef  USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN
#error  "USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN not #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#if    ((USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN != DEF_ENABLED) && \
        (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN != DEF_DISABLED))
#error  "USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN illegally #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#ifndef  USBD_AUDIO_CFG_RECORD_CORR_EN
#error  "USBD_AUDIO_CFG_RECORD_CORR_EN not #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif

#if    ((USBD_AUDIO_CFG_RECORD_CORR_EN != DEF_ENABLED) && \
        (USBD_AUDIO_CFG_RECORD_CORR_EN != DEF_DISABLED))
#error  "USBD_AUDIO_CFG_RECORD_CORR_EN illegally #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif
