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
*                                          USB DEVICE DRIVER
*
*                                           Common library
*
* Filename : usbd_drv_lib.h
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                                MODULE
*********************************************************************************************************
*/

#ifndef  USBD_DRV_LIB_MODULE_PRESENT
#define  USBD_DRV_LIB_MODULE_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   USBD_DRV_LIB_MODULE
#define  USBD_DRV_LIB_EXT
#else
#define  USBD_DRV_LIB_EXT  extern
#endif


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

                                                                /* ------------------- SETUP PACKET ------------------- */
typedef  struct  usbd_drv_lib_setup_pkt {
    CPU_INT32U  SetupPkt[2u];                                   /* Setup req buf: |Request|Value|Index|Length|          */
} USBD_DRV_LIB_SETUP_PKT;


                                                                /* ---------------- SETUP PACKET QUEUE ---------------- */
typedef  struct  usbd_drv_lib_setup_pkt_q {
    USBD_DRV_LIB_SETUP_PKT  *SetupPktTblPtr;                    /* Ptr to table that contains the Q'd setup pkt.        */

    CPU_INT08U               IxIn;                              /* Ix where to put the next rxd setup pkt.              */
    CPU_INT08U               IxOut;                             /* Ix where to get the next setup pkt to give to core.  */
    CPU_INT08U               Nbr;                               /* Actual nbr of pkts in the buf.                       */
    CPU_INT08U               TblLen;                            /* Len of setup pkt tbl.                                */
} USBD_DRV_LIB_SETUP_PKT_Q;


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
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

void  USBD_DrvLib_SetupPktQInit      (USBD_DRV_LIB_SETUP_PKT_Q  *p_setup_pkt_q,
                                      CPU_INT08U                 q_size,
                                      USBD_ERR                  *p_err);

void  USBD_DrvLib_SetupPktQClr       (USBD_DRV_LIB_SETUP_PKT_Q  *p_setup_pkt_q);

void  USBD_DrvLib_SetupPktQAdd       (USBD_DRV_LIB_SETUP_PKT_Q  *p_setup_pkt_q,
                                      USBD_DRV                  *p_drv,
                                      CPU_INT32U                *p_setup_pkt_buf);

void  USBD_DrvLib_SetupPktQSubmitNext(USBD_DRV_LIB_SETUP_PKT_Q  *p_setup_pkt_q,
                                      USBD_DRV                  *p_drv);


/*
*********************************************************************************************************
*                                              MODULE END
*********************************************************************************************************
*/

#endif
