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
*                                    USB DEVICE CORE INTERNAL FUNCTIONS
*
* Filename : usbd_internal.h
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  USBD_INTERNAL_MODULE_PRESENT
#define  USBD_INTERNAL_MODULE_PRESENT

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "usbd_core.h"


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/


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
*                                    INTERNAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/
                                                                /* ------------ DEVICE INTERNAL FUNCTIONS  ------------ */
USBD_DRV  *USBD_DrvRefGet          (CPU_INT08U   dev_nbr);

void       USBD_CoreTaskHandler    (void);

void       USBD_DbgTaskHandler     (void);

                                                                /* ------------ ENDPOINT INTERNAL FUNCTIONS ----------- */
void       USBD_EP_Init            (void);

void       USBD_EventEP            (USBD_DRV    *p_drv,
                                    CPU_INT08U   ep_addr,
                                    USBD_ERR     err);

void       USBD_CtrlOpen           (CPU_INT08U   dev_nbr,
                                    CPU_INT16U   max_pkt_size,
                                    USBD_ERR    *p_err);

void       USBD_CtrlClose          (CPU_INT08U   dev_nbr,
                                    USBD_ERR    *p_err);

void       USBD_CtrlStall          (CPU_INT08U   dev_nbr,
                                    USBD_ERR    *p_err);

void       USBD_CtrlRxStatus       (CPU_INT08U   dev_nbr,
                                    CPU_INT16U   timeout_ms,
                                    USBD_ERR    *p_err);

void       USBD_CtrlTxStatus       (CPU_INT08U   dev_nbr,
                                    CPU_INT16U   timeout_ms,
                                    USBD_ERR    *p_err);

void       USBD_EP_Open            (USBD_DRV    *p_drv,
                                    CPU_INT08U   ep_addr,
                                    CPU_INT16U   max_pkt_size,
                                    CPU_INT08U   attrib,
                                    CPU_INT08U   interval,
                                    USBD_ERR    *p_err);

void       USBD_EP_Close           (USBD_DRV    *p_drv,
                                    CPU_INT08U   ep_addr,
                                    USBD_ERR    *p_err);

void       USBD_EP_XferAsyncProcess(USBD_DRV    *p_drv,
                                    CPU_INT08U   ep_addr,
                                    USBD_ERR     xfer_err);


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*                                      DEFINED IN OS'S usbd_os.c
*********************************************************************************************************
*/

void   USBD_OS_Init           (USBD_ERR    *p_err);

void   USBD_OS_EP_SignalCreate(CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix,
                               USBD_ERR    *p_err);

void   USBD_OS_EP_SignalDel   (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix);

void   USBD_OS_EP_SignalPend  (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix,
                               CPU_INT16U   timeout_ms,
                               USBD_ERR    *p_err);

void   USBD_OS_EP_SignalAbort (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix,
                               USBD_ERR    *p_err);

void   USBD_OS_EP_SignalPost  (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix,
                               USBD_ERR    *p_err);

void   USBD_OS_EP_LockCreate  (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix,
                               USBD_ERR    *p_err);

void   USBD_OS_EP_LockDel     (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix);

void   USBD_OS_EP_LockAcquire (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix,
                               CPU_INT16U   timeout_ms,
                               USBD_ERR    *p_err);

void   USBD_OS_EP_LockRelease (CPU_INT08U   dev_nbr,
                               CPU_INT08U   ep_ix);

void   USBD_OS_DbgEventRdy    (void);
void   USBD_OS_DbgEventWait   (void);

void  *USBD_OS_CoreEventGet   (CPU_INT32U   timeout_ms,
                               USBD_ERR    *p_err);

void   USBD_OS_CoreEventPut   (void        *p_event);


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif

