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
*                                 USB DEVICE MSC CLASS STORAGE DRIVER
*
*                                              uC/FS V4
*
* Filename : usbd_storage.h
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
**********************************************************************************************************
*                                                  MODULE
**********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../../../../Source/usbd_core.h"
#include  "../../../usbd_scsi.h"
#include  <fs_cfg.h>


/*
*********************************************************************************************************
*                                                 EXTERNS
*********************************************************************************************************
*/

#ifdef   STORAGE_MODULE
#define  STORAGE_EXT
#else
#define  STORAGE_EXT  extern
#endif


/*
*********************************************************************************************************
*                                                 DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                                 MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void  USBD_StorageInit              (USBD_ERR          *p_err);

void  USBD_StorageAdd               (USBD_STORAGE_LUN  *p_storage_lun,
                                     USBD_ERR          *p_err);

void  USBD_StorageCapacityGet       (USBD_STORAGE_LUN  *p_storage_lun,
                                     CPU_INT64U        *p_nbr_blks,
                                     CPU_INT32U        *p_blk_size,
                                     USBD_ERR          *p_err);

void  USBD_StorageRd                (USBD_STORAGE_LUN  *p_storage_lun,
                                     CPU_INT64U         blk_addr,
                                     CPU_INT32U         nbr_blks,
                                     CPU_INT08U        *p_data_buf,
                                     USBD_ERR          *p_err);

void  USBD_StorageWr                (USBD_STORAGE_LUN  *p_storage_lun,
                                     CPU_INT64U         blk_addr,
                                     CPU_INT32U         nbr_blks,
                                     CPU_INT08U        *p_data_buf,
                                     USBD_ERR          *p_err);

void  USBD_StorageStatusGet         (USBD_STORAGE_LUN  *p_storage_lun,
                                     USBD_ERR          *p_err);

void  USBD_StorageLock              (USBD_STORAGE_LUN  *p_storage_lun,
                                     CPU_INT32U         timeout_ms,
                                     USBD_ERR          *p_err);

void  USBD_StorageUnlock            (USBD_STORAGE_LUN  *p_storage_lun,
                                     USBD_ERR          *p_err);

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
void  USBD_StorageRefreshTaskHandler(void *p_arg);
#endif


/*
*********************************************************************************************************
*                                          CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef  USBD_MSC_CFG_FS_REFRESH_TASK_EN
#error  "USBD_MSC_CFG_FS_REFRESH_TASK_EN not #defined'd in 'usbd_cfg.h' [MUST be DEF_ENABLED or DEF_DISABLED]"
#endif


/*
**********************************************************************************************************
*                                           MODULE END
**********************************************************************************************************
*/




