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
*                                              AT32UC3x
*
* Filename : usbd_drv_at32uc3x.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)  : (1) You can find specific information about this driver at:
*                https://doc.micrium.com/display/USBDDRV/AT32UC3x
*
*            (2) With an appropriate BSP, this device driver will support the USB device module on
*                the Atmel 32-bit AVR UC3 A and B series.
*
*            (3) The device controller does not support synthetizing a corrupted SOF packet. It
*                reports an error flag meaning that the software should handle it properly.
*********************************************************************************************************
*/

#ifndef  USBD_DRV_AT32UC3x_MODULE_PRESENT                       /* See Note #1.                                         */
#define  USBD_DRV_AT32UC3x_MODULE_PRESENT


/*
*********************************************************************************************************
*                                          USB DEVICE DRIVER
*********************************************************************************************************
*/

extern  USBD_DRV_API  USBD_DrvAPI_AT32UC3x;


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif
