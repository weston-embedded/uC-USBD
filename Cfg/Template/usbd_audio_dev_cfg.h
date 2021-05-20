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
*                                   USB AUDIO DEVICE CONFIGURATION FILE
*
* Filename : usbd_audio_dev_cfg.h
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

#ifndef  USBD_AUDIO_DEV_CFG_MODULE_PRESENT                            /* See Note #1.                                         */
#define  USBD_AUDIO_DEV_CFG_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <Class/Audio/usbd_audio.h>
#include  "app_usbd.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  USBD_AUDIO_DEV_CFG_RECORD_CORR_PERIOD             16u  /* Record correction period in ms.                      */

#define  USBD_AUDIO_DEV_CFG_RECORD_NBR_BUF                 USBD_AUDIO_STREAM_NBR_BUF_18

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
#define  USBD_AUDIO_DEV_CFG_PLAYBACK_CORR_PERIOD           16u  /* Playback correction period in ms.                    */
#define  USBD_AUDIO_DEV_CFG_NBR_ENTITY                      6u  /* Nbr of entities (units/terminals) used by audio fnct.*/
#define  USBD_AUDIO_DEV_CFG_PLAYBACK_NBR_BUF               USBD_AUDIO_STREAM_NBR_BUF_18
#else
#define  USBD_AUDIO_DEV_CFG_NBR_ENTITY                      3u  /* Nbr of entities (units/terminals) used by audio fnct.*/
#endif


/*
*********************************************************************************************************
*                            USB AUDIO FUNCTION TOPOLOGY 1 CONFIGURATION
*********************************************************************************************************
*/

                                                                /* -------------- TERMINAL AND UNIT IDS --------------- */
extern         CPU_INT08U             Mic_IT_ID;
extern         CPU_INT08U             Mic_OT_USB_IN_ID;
extern         CPU_INT08U             Mic_FU_ID;
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
extern         CPU_INT08U             Speaker_IT_USB_OUT_ID;
extern         CPU_INT08U             Speaker_OT_ID;
extern         CPU_INT08U             Speaker_FU_ID;
#endif


                                                                /* ----------- TERMINAL, UNIT AND AS IF CFG ----------- */
extern  const  USBD_AUDIO_IT_CFG      USBD_IT_MIC_Cfg;
extern  const  USBD_AUDIO_OT_CFG      USBD_OT_USB_IN_Cfg;
extern  const  USBD_AUDIO_FU_CFG      USBD_FU_MIC_Cfg;
extern  const  USBD_AUDIO_STREAM_CFG  USBD_MicStreamCfg;
extern  const  USBD_AUDIO_AS_IF_CFG   USBD_AS_IF2_MicCfg;
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
extern  const  USBD_AUDIO_IT_CFG      USBD_IT_USB_OUT_Cfg;
extern  const  USBD_AUDIO_OT_CFG      USBD_OT_SPEAKER_Cfg;
extern  const  USBD_AUDIO_FU_CFG      USBD_FU_SPEAKER_Cfg;
extern  const  USBD_AUDIO_STREAM_CFG  USBD_SpeakerStreamCfg;
extern  const  USBD_AUDIO_AS_IF_CFG   USBD_AS_IF1_SpeakerCfg;
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif
