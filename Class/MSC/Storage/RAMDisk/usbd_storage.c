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
*                                               RAMDISK
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

#define  USBD_RAMDISK_SIZE       (USBD_RAMDISK_CFG_BLK_SIZE * \
                                  USBD_RAMDISK_CFG_NBR_BLKS)


/*
*********************************************************************************************************
*                                             LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL DATA TYPES
*********************************************************************************************************
*/

#if (USBD_RAMDISK_CFG_BASE_ADDR == 0)
static  CPU_INT08U  USBD_RAMDISK_DataArea[USBD_RAMDISK_CFG_NBR_UNITS][USBD_RAMDISK_SIZE];
#else
static  CPU_INT08U *USBD_RAMDISK_DataArea[USBD_RAMDISK_CFG_NBR_UNITS];
#endif


/*
*********************************************************************************************************
*                                              LOCAL TABLES
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
*                                              USBD_StorageInit()
*
* Description : Initialize internal tables used by the storage layer.
*
* Argument(s) : p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE               Storage successfully initialized.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageInit (USBD_ERR  *p_err)
{
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                              USBD_StorageAdd()
*
* Description : Initialize storage medium.
*
* Argument(s) : p_storage_lun    Pointer to the logical unit storage structure.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                                USBD_ERR_NONE                  Storage successfully initialized.
*                                USBD_ERR_SCSI_LOG_UNIT_NOTRDY  Initiliazing RAM area failed.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageAdd (USBD_STORAGE_LUN  *p_storage_lun,
                       USBD_ERR          *p_err)
{
    CPU_INT32U  ix;
    CPU_INT08U  lun_nbr;


    lun_nbr = p_storage_lun->LunNbr;
                                                                /* Fill the RAM area with zeros.                        */
#if (USBD_RAMDISK_CFG_BASE_ADDR != 0)
    USBD_RAMDISK_DataArea[lun_nbr] = (CPU_INT08U *)USBD_RAMDISK_CFG_BASE_ADDR + lun_nbr * USBD_RAMDISK_SIZE;
#endif
    Mem_Clr((void     *)&USBD_RAMDISK_DataArea[lun_nbr][0],
                         USBD_RAMDISK_SIZE);

    for (ix = 0; ix < USBD_RAMDISK_SIZE; ix++) {                /* Chk if RAM area is cleared.                          */
        if (USBD_RAMDISK_DataArea[lun_nbr][ix] != 0) {
           *p_err = USBD_ERR_SCSI_LU_NOTRDY;
            return;
        }
    }

    p_storage_lun->MediumPresent = DEF_TRUE;                    /* RAMDisk medium is initially always present.          */
   *p_err                        = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_StorageCapacityGet()
*
* Description : Get storage medium's capacity.
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
    (void)p_storage_lun;

   *p_nbr_blks = USBD_RAMDISK_CFG_NBR_BLKS;
   *p_blk_size = USBD_RAMDISK_CFG_BLK_SIZE;
   *p_err      = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                               USBD_StorageRd()
*
* Description : Read data from the storage medium.
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
    CPU_INT08U  lun;
    CPU_INT64U  mem_area_size;
    CPU_INT32U  mem_size_copy;


    lun = (*p_storage_lun).LunNbr;

    if (lun > USBD_RAMDISK_CFG_NBR_UNITS) {
       *p_err = USBD_ERR_SCSI_LU_NOTSUPPORTED;
        return;
    }

    mem_area_size = ((blk_addr + nbr_blks) * USBD_RAMDISK_CFG_BLK_SIZE);
    if (mem_area_size > USBD_RAMDISK_SIZE) {
       *p_err = USBD_ERR_SCSI_LU_NOTRDY;
        return;
    }

    mem_size_copy = nbr_blks * USBD_RAMDISK_CFG_BLK_SIZE;
    Mem_Copy((void *) p_data_buf,
             (void *)&USBD_RAMDISK_DataArea[lun][blk_addr * USBD_RAMDISK_CFG_BLK_SIZE],
                      mem_size_copy);

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                               USBD_StorageWr()
*
* Description : Write data to the storage medium.
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
    CPU_INT08U  lun;
    CPU_INT64U  mem_area_size;
    CPU_INT32U  mem_size_copy;


    lun = (*p_storage_lun).LunNbr;

    if (lun > USBD_RAMDISK_CFG_NBR_UNITS) {
       *p_err = USBD_ERR_SCSI_LU_NOTSUPPORTED;
        return;
    }

    mem_area_size = ((blk_addr + nbr_blks) * USBD_RAMDISK_CFG_BLK_SIZE);
    if (mem_area_size > USBD_RAMDISK_SIZE) {
       *p_err = USBD_ERR_SCSI_LU_NOTRDY;
        return;
    }

    mem_size_copy = nbr_blks * USBD_RAMDISK_CFG_BLK_SIZE;
    Mem_Copy((void *)&USBD_RAMDISK_DataArea[lun][blk_addr * USBD_RAMDISK_CFG_BLK_SIZE],
             (void *) p_data_buf,
                      mem_size_copy);

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            USBD_StorageStatusGet()
*
* Description : Get storage medium's status.
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
                             USBD_ERR          *p_err)
{
    CPU_INT08U  lun;

    lun = p_storage_lun->LunNbr;

    if (lun > USBD_RAMDISK_CFG_NBR_UNITS) {
       *p_err = USBD_ERR_SCSI_LU_NOTSUPPORTED;
        return;
    }

    if (p_storage_lun->MediumPresent == DEF_FALSE) {
       *p_err = USBD_ERR_SCSI_MEDIUM_NOTPRESENT;
    } else {
       *p_err = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                               USBD_StorageLock()
*
* Description : Lock storage medium.
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
    (void)timeout_ms;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                               USBD_StorageUnlock()
*
* Description : Unlock storage medium.
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
   *p_err = USBD_ERR_NONE;
}
