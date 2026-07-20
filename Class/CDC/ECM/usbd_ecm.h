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
*                                USB COMMUNICATIONS DEVICE CLASS (CDC)
*                                    ETHERNET CONTROL MODEL (ECM)
*
* Filename : usbd_ecm.h
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  USBD_ECM_MODULE_PRESENT
#define  USBD_ECM_MODULE_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../usbd_cdc.h"


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  USBD_ECM_NBR_NONE                 DEF_INT_08U_MAX_VAL
#define  USBD_ECM_MAC_ADDR_LEN             13u


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

typedef CPU_BOOLEAN  (*USBD_ECM_MGMT_REQ) (       CPU_INT08U       dev_nbr,
                                           const  USBD_SETUP_REQ  *p_setup_req,
                                                  void            *p_subclass_arg);


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void        USBD_ECM_Init       (      USBD_ERR           *p_err);

CPU_INT08U  USBD_ECM_Add        (      USBD_ECM_MGMT_REQ   mgmt_req,
                                       void               *mgmt_req_arg,
                                       CPU_BOOLEAN         end_tx_en,
                                       USBD_ERR           *p_err);

CPU_BOOLEAN  USBD_ECM_CfgAdd    (      CPU_INT08U          subclass_nbr,
                                       CPU_INT08U          dev_nbr,
                                       CPU_INT08U          cfg_nbr,
                                 const CPU_CHAR           *mac_addr,
                                       CPU_INT32U          ethernet_stats,
                                       CPU_INT16U          max_seg_size,
                                       CPU_INT16U          num_mc_filters,
                                       CPU_INT08U          num_pwr_filters,
                                       USBD_ERR           *p_err);

CPU_INT32U   USBD_ECM_DataRx    (      CPU_INT08U          subclass_nbr,
                                       CPU_INT08U         *p_buf,
                                       CPU_INT32U          buf_len,
                                       CPU_INT16U          timeout,
                                       USBD_ERR           *p_err);

CPU_INT32U   USBD_ECM_DataTx    (      CPU_INT08U          subclass_nbr,
                                       CPU_INT08U         *p_buf,
                                       CPU_INT32U          buf_len,
                                       CPU_INT16U          timeout,
                                       USBD_ERR           *p_err);

void  USBD_ECM_DataRxAsync      (      CPU_INT08U          subclass_nbr,
                                       CPU_INT08U         *p_buf,
                                       CPU_INT32U          buf_len,
                                       USBD_ASYNC_FNCT     async,
                                       void               *p_async_arg,
                                       USBD_ERR           *p_err);

CPU_INT32U  USBD_ECM_Notify     (      CPU_INT08U          subclass_nbr,
                                       CPU_INT08U          notification,
                                       CPU_INT16U          value,
                                       CPU_INT08U         *p_data,
                                       CPU_INT32U          len,
                                       USBD_ERR           *p_err);

void  USBD_ECM_NotifyNetConn    (      CPU_INT08U          subclass_nbr,
                                       CPU_BOOLEAN         conn_status,
                                       USBD_ERR           *p_err);

void  USBD_ECM_NotifyConnSpdChng(      CPU_INT08U          subclass_nbr,
                                       CPU_INT32U          dl_bit_rate,
                                       CPU_INT32U          ul_bit_rate,
                                       USBD_ERR           *p_err);


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef  USBD_ECM_CFG_MAX_NBR_DEV
#error  "USBD_ECM_CFG_MAX_NBR_DEV not #define'd in 'usbd_cfg.h' [MUST be >= 1]"

#elif   (USBD_ECM_CFG_MAX_NBR_DEV < 1u)
#error  "USBD_ECM_CFG_MAX_NBR_DEV illegally #define'd in 'usbd_cfg.h'[MUST be >= 1]"

#elif   (USBD_ECM_CFG_MAX_NBR_DEV > USBD_CDC_CFG_MAX_NBR_DEV)
#error  "USBD_ECM_CFG_MAX_NBR_DEV illegally #define'd in 'usbd_cfg.h' [MUST be >= USBD_CDC_CFG_MAX_NBR_DEV]"
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif
