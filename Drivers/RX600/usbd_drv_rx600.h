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
*                                           USB DEVICE DRIVER
*
*                                             Renesas RX600
*                                             Renesas RX100
*                                          Renesas V850E2/Jx4-L
*
* Filename : usbd_drv_rx600.h
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/RX600
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                                MODULE
*
* Note(s) : (1) This USB device driver function header file is protected from multiple pre-processor
*               inclusion through use of the USB device driver module present pre-processor macro
*               definition.
*********************************************************************************************************
*/

#ifndef  USBD_DRV_RX600_MODULE_PRESENT                          /* See Note #1.                                         */
#define  USBD_DRV_RX600_MODULE_PRESENT


/*
*********************************************************************************************************
*                                           USB DEVICE DRIVER
*********************************************************************************************************
*/

extern  USBD_DRV_API  USBD_DrvAPI_RX600;
extern  USBD_DRV_API  USBD_DrvAPI_V850E2JX4L;


/*
*********************************************************************************************************
*                                     CIRCULAR BUFFER CONFIGURATION
*********************************************************************************************************
*/

#define  USBD_DRV_SETUP_PKT_CIRCULAR_BUF_SIZE               3u


/*
*********************************************************************************************************
*                                              MODULE END
*********************************************************************************************************
*/

#endif
