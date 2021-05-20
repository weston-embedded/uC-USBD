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
*                                    USB DEVICE CONFIGURATION FILE
*
*                                              TEMPLATE
*
* Filename : usbd_dev_cfg.h
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This USB device configuration header file is protected from multiple pre-processor
*               inclusion through use of the USB device configuration module present pre-processor
*               macro definition.
*********************************************************************************************************
*/

#ifndef  USBD_DEV_CFG_MODULE_PRESENT                            /* See Note #1.                                         */
#define  USBD_DEV_CFG_MODULE_PRESENT


/*
*********************************************************************************************************
*                                      USB DEVICE CONFIGURATION
*********************************************************************************************************
*/

extern  const USBD_DEV_CFG  USBD_DevCfg_Template;


/*
*********************************************************************************************************
*                                   USB DEVICE DRIVER CONFIGURATION
*********************************************************************************************************
*/

extern  const USBD_DRV_CFG  USBD_DrvCfg_Template;


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif
