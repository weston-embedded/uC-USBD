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
*                                        USB DEVICE PHDC CLASS
*
* Filename : usbd_phdc.h
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  USBD_PHDC_CLASS_MODULE_PRESENT
#define  USBD_PHDC_CLASS_MODULE_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../Source/usbd_core.h"


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   USBD_PHDC_MODULE
#define  USBD_PHDC_EXT
#else
#define  USBD_PHDC_EXT  extern
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  USBD_PHDC_NBR_NONE                   DEF_INT_08U_MAX_VAL



/*
*********************************************************************************************************
*                                    LATENCY / RELIABILITY BITMAPS
*********************************************************************************************************
*/

#define  USBD_PHDC_LATENCY_VERYHIGH_RELY_BEST           DEF_BIT_05
#define  USBD_PHDC_LATENCY_HIGH_RELY_BEST               DEF_BIT_04
#define  USBD_PHDC_LATENCY_MEDIUM_RELY_BEST             DEF_BIT_03
#define  USBD_PHDC_LATENCY_MEDIUM_RELY_BETTER           DEF_BIT_02
#define  USBD_PHDC_LATENCY_MEDIUM_RELY_GOOD             DEF_BIT_01
#define  USBD_PHDC_LATENCY_LOW_RELY_GOOD                DEF_BIT_00


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

typedef  CPU_INT08U  LATENCY_RELY_FLAGS;


/*
*********************************************************************************************************
*                         ASYNCHRONOUS CALLBACK FUNCTION DATA TYPE
*********************************************************************************************************
*/

typedef  void  (*USBD_PHDC_PREAMBLE_EN_NOTIFY)(CPU_INT08U   class_nbr,
                                               CPU_BOOLEAN  preamble_en);


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                   APPLICATION FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void         USBD_PHDC_Init        (       USBD_ERR                      *p_err);

CPU_INT08U   USBD_PHDC_Add         (       CPU_BOOLEAN                    data_fmt_11073,
                                           CPU_BOOLEAN                    preamble_capable,
                                           USBD_PHDC_PREAMBLE_EN_NOTIFY   preamble_en_notify,
                                           CPU_INT16U                     low_latency_interval,
                                           USBD_ERR                      *p_err);

CPU_BOOLEAN  USBD_PHDC_CfgAdd      (       CPU_INT08U                     class_nbr,
                                           CPU_INT08U                     dev_nbr,
                                           CPU_INT08U                     cfg_nbr,
                                           USBD_ERR                      *p_err);

CPU_BOOLEAN  USBD_PHDC_IsConn      (       CPU_INT08U                     class_nbr);

void         USBD_PHDC_RdCfg       (       CPU_INT08U                     class_nbr,
                                           LATENCY_RELY_FLAGS             latency_rely,
                                    const  CPU_INT08U                    *p_data_opaque,
                                           CPU_INT08U                     data_opaque_len,
                                           USBD_ERR                      *p_err);

void         USBD_PHDC_WrCfg       (       CPU_INT08U                     class_nbr,
                                           LATENCY_RELY_FLAGS             latency_rely,
                                    const  CPU_INT08U                    *p_data_opaque,
                                           CPU_INT08U                     data_opaque_len,
                                           USBD_ERR                      *p_err);

void         USBD_PHDC_11073_ExtCfg(       CPU_INT08U                     class_nbr,
                                           CPU_INT16U                    *p_dev_specialization,
                                           CPU_INT08U                     nbr_dev_specialization,
                                           USBD_ERR                      *p_err);

CPU_INT08U   USBD_PHDC_PreambleRd  (       CPU_INT08U                     class_nbr,
                                           void                          *p_buf,
                                           CPU_INT08U                     buf_len,
                                           CPU_INT08U                    *p_nbr_xfer,
                                           CPU_INT16U                     timeout,
                                           USBD_ERR                      *p_err);

CPU_INT16U   USBD_PHDC_Rd          (       CPU_INT08U                     class_nbr,
                                           void                          *p_buf,
                                           CPU_INT16U                     buf_len,
                                           CPU_INT16U                     timeout,
                                           USBD_ERR                      *p_err);

void         USBD_PHDC_PreambleWr  (       CPU_INT08U                     class_nbr,
                                           void                          *p_data_opaque,
                                           CPU_INT08U                     data_opaque_len,
                                           LATENCY_RELY_FLAGS             latency_rely,
                                           CPU_INT08U                     nbr_xfers,
                                           CPU_INT16U                     timeout,
                                           USBD_ERR                      *p_err);

void         USBD_PHDC_Wr          (       CPU_INT08U                     class_nbr,
                                           void                          *p_buf,
                                           CPU_INT16U                     buf_len,
                                           LATENCY_RELY_FLAGS             latency_rely,
                                           CPU_INT16U                     timeout,
                                           USBD_ERR                      *p_err);

void         USBD_PHDC_Reset       (       CPU_INT08U                     class_nbr);


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


#ifndef  USBD_PHDC_CFG_MAX_NBR_DEV
#error  "USBD_PHDC_CFG_MAX_NBR_DEV not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#if  (USBD_PHDC_CFG_MAX_NBR_DEV < 1u)
#error  "USBD_PHDC_CFG_MAX_NBR_DEV illegally #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#ifndef  USBD_PHDC_CFG_MAX_NBR_CFG
#error  "USBD_PHDC_CFG_MAX_NBR_CFG not #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#if  (USBD_PHDC_CFG_MAX_NBR_CFG < 1u)
#error  "USBD_PHDC_CFG_MAX_NBR_CFG illegally #define'd in 'usbd_cfg.h' [MUST be >= 1]"
#endif

#ifndef  USBD_PHDC_CFG_DATA_OPAQUE_MAX_LEN
#error  "USBD_PHDC_CFG_DATA_OPAQUE_MAX_LEN not #define'd in 'usbd_cfg.h' [MUST be >= 0]"
#endif

#if  (USBD_PHDC_CFG_DATA_OPAQUE_MAX_LEN < 0u)
#error  "USBD_PHDC_CFG_DATA_OPAQUE_MAX_LEN illegally #define'd in 'usbd_cfg.h' [MUST be >= 0]"
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif

