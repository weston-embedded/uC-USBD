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
*                                              TEMPLATE
*
* Filename : usbd_storage.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include   "usbd_storage.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         USBD_StorageInit()
*
* Description : Initialize the storage medium.
*
* Argument(s) : p_err       Pointer to variable that will receive error code from this function.
*
*                                USBD_ERR_NONE                  Storage successfully initialized.
*                                USBD_ERR_SCSI_LOG_UNIT_NOTRDY  Initiliazing RAM area failed.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageInit (USBD_ERR  *p_err)
{
    /* $$$$ Insert code to initialize the storage medium. */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      USBD_StorageCapacityGet()
*
* Description : Get the capacity of the storage medium.
*
* Argument(s) : p_storage_lun    Pointer to the logical unit storage structure.
*
*               p_nbr_blks       Pointer to variable that will receive the number of logical blocks.
*
*               p_blk_size       Pointer to variable that will receive the size of each block, in bytes.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                                USBD_ERR_NONE      Medium capacity successfully gotten.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageCapacityGet (USBD_STORAGE_LUN  *p_storage_lun,
                               CPU_INT64U        *p_nbr_blks,
                               CPU_INT32U        *p_blk_size,
                               USBD_ERR          *p_err)
{
    /* $$$$ Insert code to return the capacity of the storage medium in terms of number of blocks and default size of a block. */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_StorageRd()
*
* Description : Read the data from the storage medium.
*
* Argument(s) : p_storage_lun    Pointer to the logical unit storage structure.
*
*               blk_addr         Logical Block Address (LBA) of starting read block.
*
*               nbr_blks         Number of logical blocks to read.
*
*               p_data_buf      Pointer to buffer in which data will be stored.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE                           Medium successfully read.
*                               USBD_ERR_SCSI_LOG_UNIT_NOTSUPPORTED     Logical unit not supported.
*                               USBD_ERR_SCSI_LOG_UNIT_NOTRDY           Logical unit cannot perform
*                                                                           operations.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageRd (USBD_STORAGE_LUN  *p_storage_lun,
                      CPU_INT64U         blk_addr,
                      CPU_INT32U         nbr_blks,
                      CPU_INT08U        *p_data_buf,
                      USBD_ERR          *p_err)
{
    /* $$$$ Insert code to read data from the storage medium. */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_StorageWr()
*
* Description : Write the data to the storage medium.
*
* Argument(s) : p_storage_lun    Pointer to the logical unit storage structure.
*
*               blk_addr         Logical Block Address (LBA) of starting write block.
*
*               nbr_blks         Number of logical blocks to write.
*
*               p-data_buf       Pointer to buffer in which data is stored.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE                           Medium successfully written.
*                               USBD_ERR_SCSI_LOG_UNIT_NOTSUPPORTED     Logical unit not supported.
*                               USBD_ERR_SCSI_LOG_UNIT_NOTRDY           Logical unit cannot perform
*                                                                           operations.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageWr (USBD_STORAGE_LUN  *p_storage_lun,
                      CPU_INT64U         blk_addr,
                      CPU_INT32U         nbr_blks,
                      CPU_INT08U        *p_data_buf,
                      USBD_ERR          *p_err)
{
    /* $$$$ Insert code to write data to the storage medium. */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       USBD_StorageStatusGet()
*
* Description : Get the status of the storage medium.
*
* Argument(s) : p_storage_lun    Pointer to the logical unit storage structure.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE                          Medium present.
*                               USB_ERR_SCSI_MEDIUM_NOTPRESENT         Medium not present.
*                               USBD_ERR_SCSI_LOG_UNIT_NOTSUPPORTED    Logical unit not supported.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageStatusGet (USBD_STORAGE_LUN  *p_storage_lun,
                             USBD_ERR           *p_err)
{
    /* $$$$ Insert code to determine the presence or absence of the storage medium. */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         USBD_StorageLock()
*
* Description : Lock the storage medium.
*
* Argument(s) : p_storage_lun    Pointer to the logical unit storage structure.
*
* Argument(s) : p_storage_lun   Pointer to the logical unit storage structure.
*
*               timeout_ms      Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE               Medium successfully locked.
*                               USBD_ERR_SCSI_LOCK_TIMEOUT  Medium lock timed out.
*                               USBD_ERR_SCSI_LOCK          Medium lock failed.
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageLock (USBD_STORAGE_LUN  *p_storage_lun,
                        CPU_INT32U         timeout_ms,
                        USBD_ERR          *p_err)
{
    /* $$$$ Insert code to lock access to the storage medium. This is useful to avoid an embedded file system accessing the same storage medium as the USB host. */

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_StorageUnlock()
*
* Description : Unlock the storage medium.
*
* Argument(s) : p_storage_lun    Pointer to the logical unit storage structure.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE   Medium successfully unlocked.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageUnlock (USBD_STORAGE_LUN  *p_storage_lun,
                          USBD_ERR          *p_err)
{
    /* $$$$ Insert code to unlock access to the storage medium. */

   *p_err = USBD_ERR_NONE;

}

/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                                 END
*********************************************************************************************************
*/
