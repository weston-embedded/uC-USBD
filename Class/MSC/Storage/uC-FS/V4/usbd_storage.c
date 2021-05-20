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
* Filename : usbd_storage.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
**********************************************************************************************************
*                                            INCLUDE FILES
**********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "usbd_storage.h"
#include  "../../../usbd_msc_os.h"
#include  <Source/fs.h>
#include  <Source/fs_dev.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


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

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
                                                                /* Tbl of dev to be polled.                             */
static  USBD_STORAGE_LUN  *USBD_FS_StorageDevPollList[USBD_MSC_CFG_MAX_LUN];
#endif
                                                                /* Cached luns state.                                   */
static  CPU_BOOLEAN        USBD_FS_LunStatePresent[USBD_MSC_CFG_MAX_LUN];


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

#if     (USBD_MSC_CFG_MICRIUM_FS == DEF_DISABLED)
#error  "USBD_MSC_CFG_MICRIUM_FS illegally #defined in 'usbd_cfg.h' [MUST be DEF_ENABLED] "
#endif


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
    CPU_INT08U  ix;


    for (ix = 0; ix < USBD_MSC_CFG_MAX_LUN; ix++) {
#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
        USBD_FS_StorageDevPollList[ix] = (USBD_STORAGE_LUN *)0u;
#endif
        USBD_FS_LunStatePresent[ix]    =  DEF_FALSE;
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                              USBD_StorageAdd()
*
* Description : Initialize storage medium.
*
* Argument(s) : p_storage_lun    Pointer to logical unit storage structure.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE               Storage successfully initialized.
*                               USBD_ERR_SCSI_LU_NOTRDY     Querying storage information failed.
*
* Return(s)   : None.
*
* Note(s)     : (1) Querying a device returns information such as size, sector size, removable or not.
*                   For fixed media (e.g. RAM, Flash NAND), all the information are always obtained.
*                   For removable media (e.g. SD card), only some information can be obtained if the
*                   media is not present. In this case, even if the device information structure contains
*                   valid information, an error code may be returned and the execution continues normally.
*                   The function USBD_StorageInit() should return immediately only when a fatal error
*                   occurs.
*********************************************************************************************************
*/

void  USBD_StorageAdd (USBD_STORAGE_LUN  *p_storage_lun,
                       USBD_ERR          *p_err)
{
    FS_ERR       err_fs;
#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
    FS_DEV_INFO  dev_info;
#endif


    FSDev_Refresh(p_storage_lun->VolStrPtr,                     /* Obtain disk status.                                  */
                 &err_fs);
    if (err_fs == FS_ERR_NONE){                                 /* Initialize media present status.                     */
        p_storage_lun->MediumPresent                   = DEF_TRUE;
        USBD_FS_LunStatePresent[p_storage_lun->LunNbr] = DEF_TRUE;
    } else {
        p_storage_lun->MediumPresent                   = DEF_FALSE;
        USBD_FS_LunStatePresent[p_storage_lun->LunNbr] = DEF_FALSE;
    }

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
    FSDev_Query(p_storage_lun->VolStrPtr,
               &dev_info,
               &err_fs);
    switch (err_fs) {
                                                                /* See Note #1.                                         */
        case FS_ERR_NONE:
        case FS_ERR_DEV_NOT_PRESENT:
        case FS_ERR_DEV_IO:
             break;

        default:
            *p_err = USBD_ERR_SCSI_LU_NOTRDY;
             return;
    }

    if(dev_info.Fixed == DEF_NO) {                              /* Removable media.                                     */
        USBD_FS_StorageDevPollList[p_storage_lun->LunNbr] = p_storage_lun;
    }
#endif
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          USBD_StorageCapacityGet()
*
* Description : Get storage medium's capacity.
*
* Argument(s) : p_storage_lun    Pointer to logical unit storage structure.
*
*               p_nbr_blks       Pointer to variable that will receive the number of logical blocks.
*
*               p_blk_size       Pointer to variable that will receive the size of each block, in bytes.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE                       Medium capacity successfully gotten.
*                               USBD_ERR_SCSI_MEDIUM_NOTPRESENT     Medium not present.
*
* Return(s)   : None.
*
* Note(s)     : (1) On error set cached state to not present to prevent positive media presence
*                   returned by USBD_StorageStatusGet() when the host sends a TEST_UNIT_READY SCSI
*                   command.
*********************************************************************************************************
*/

void  USBD_StorageCapacityGet (USBD_STORAGE_LUN  *p_storage_lun,
                               CPU_INT64U        *p_nbr_blks,
                               CPU_INT32U        *p_blk_size,
                               USBD_ERR          *p_err)
{
    FS_ERR       err_fs;
    FS_DEV_INFO  dev_info;
    CPU_SR_ALLOC();


    FSDev_Query(p_storage_lun->VolStrPtr,
               &dev_info,
               &err_fs);
    if (err_fs != FS_ERR_NONE) {
       *p_err = USBD_ERR_SCSI_MEDIUM_NOTPRESENT;
                                                                /* See Note #1.                                         */
        CPU_CRITICAL_ENTER();
        USBD_FS_LunStatePresent[p_storage_lun->LunNbr] = DEF_FALSE;
        CPU_CRITICAL_EXIT();
        return;
    }

   *p_nbr_blks = dev_info.Size;
   *p_blk_size = dev_info.SecSize;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                               USBD_StorageRd()
*
* Description : Read data from the storage medium.
*
* Argument(s) : p_storage_lun    Pointer to logical unit storage structure.
*
*               blk_addr         Logical Block Address (LBA) of starting read block.
*
*               nbr_blks         Number of logical blocks to read.
*
*               p_data_buf       Pointer to buffer in which data will be stored.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE                       Medium successfully read.
*                               USBD_ERR_SCSI_MEDIUM_NOTPRESENT     Medium not present.
*
* Return(s)   : None.
*
* Note(s)     : (1) On error set cached state to not present to prevent positive media presence
*                   returned by USBD_StorageStatusGet() when the host sends a TEST_UNIT_READY SCSI
*                   command.
*********************************************************************************************************
*/

void  USBD_StorageRd (USBD_STORAGE_LUN  *p_storage_lun,
                      CPU_INT64U         blk_addr,
                      CPU_INT32U         nbr_blks,
                      CPU_INT08U        *p_data_buf,
                      USBD_ERR          *p_err)
{
    FS_ERR  err_fs;
    CPU_SR_ALLOC();


    FSDev_Rd(p_storage_lun->VolStrPtr,
             p_data_buf,
             blk_addr,
             nbr_blks,
            &err_fs);
    if (err_fs != FS_ERR_NONE) {
       *p_err = USBD_ERR_SCSI_MEDIUM_NOTPRESENT;
                                                                /* See Note #1.                                         */
        CPU_CRITICAL_ENTER();
        USBD_FS_LunStatePresent[p_storage_lun->LunNbr] = DEF_FALSE;
        CPU_CRITICAL_EXIT();
    } else {
       *p_err = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                               USBD_StorageWr()
*
* Description : Write data to the storage medium.
*
* Argument(s) : p_storage_lun    Pointer to logical unit storage structure.
*
*               blk_addr         Logical Block Address (LBA) of starting write block.
*
*               nbr_blks         Number of logical blocks to write.
*
*               p-data_buf       Pointer to buffer in which data is stored.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE                       Medium successfully written.
*                               USBD_ERR_SCSI_MEDIUM_NOTPRESENT     Medium not present.
*
* Return(s)   : None.
*
* Note(s)     : (1) On error set cached state to not present to prevent positive media presence
*                   returned by USBD_StorageStatusGet() when the host sends a TEST_UNIT_READY SCSI
*                   command.
*********************************************************************************************************
*/

void  USBD_StorageWr (USBD_STORAGE_LUN  *p_storage_lun,
                      CPU_INT64U         blk_addr,
                      CPU_INT32U         nbr_blks,
                      CPU_INT08U        *p_data_buf,
                      USBD_ERR          *p_err)
{
    FS_ERR   err_fs;
    CPU_SR_ALLOC();


    FSDev_Wr(p_storage_lun->VolStrPtr,
             p_data_buf,
             blk_addr,
             nbr_blks,
            &err_fs);
    if (err_fs != FS_ERR_NONE) {
       *p_err = USBD_ERR_SCSI_MEDIUM_NOTPRESENT;
                                                                /* See Note #1.                                         */
        CPU_CRITICAL_ENTER();
        USBD_FS_LunStatePresent[p_storage_lun->LunNbr] = DEF_FALSE;
        CPU_CRITICAL_EXIT();
    } else {
       *p_err = USBD_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                            USBD_StorageStatusGet()
*
* Description : Get storage medium's status.
*
* Argument(s) : p_storage_lun    Pointer to logical unit storage structure.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE                           Medium present.
*                               USBD_ERR_SCSI_MEDIUM_NOTPRESENT         Medium not present.
*                               USBD_ERR_SCSI_MEDIUM_NOT_RDY_TO_RDY     Medium state from not ready to ready.
*                               USBD_ERR_SCSI_MEDIUM_RDY_TO_NOT_RDY     Medium state from ready to not ready.
*
* Return(s)   : None.
*
* Note(s)     : (1) Sometimes a removable device may fail since device may be removed, powered off or
*                   suddenly unresponsive. The associated device states for the latter are: FS_DEV_STATE_CLOSED,
*                   FS_DEV_STATE_CLOSING and FS_DEV_STATE_STARTING. The device will need to be refreshed
*                   or closed and re-opened.
*
*               (2) If the return error is neither FS_ERR_NONE nor FS_ERR_DEV_INVALID_LOW_FMT, then no
*                   functioning device is present. The device must be refreshed at a later time.
*********************************************************************************************************
*/

void  USBD_StorageStatusGet (USBD_STORAGE_LUN  *p_storage_lun,
                             USBD_ERR          *p_err)
{
    FS_STATE  state;


    state = USBD_FS_LunStatePresent[p_storage_lun->LunNbr];

    if (p_storage_lun->MediumPresent == DEF_FALSE) {

        if (state == DEF_TRUE){                                 /* Media is inserted.                                   */
           *p_err                        = USBD_ERR_SCSI_MEDIUM_NOT_RDY_TO_RDY;
            p_storage_lun->MediumPresent = DEF_TRUE;
        } else {
           *p_err = USBD_ERR_SCSI_MEDIUM_NOTPRESENT;
        }

    } else {
        if (state == DEF_FALSE){                                /* Media is removed.                                    */
           *p_err                        = USBD_ERR_SCSI_MEDIUM_RDY_TO_NOT_RDY;
            p_storage_lun->MediumPresent = DEF_FALSE;
        } else {
           *p_err = USBD_ERR_NONE;
        }
    }
}


/*
*********************************************************************************************************
*                                           USBD_StorageLock()
*
* Description : Lock storage medium.
*
* Argument(s) : p_storage_lun   Pointer to logical unit storage structure.
*
*               timeout_ms      Timeout in milliseconds.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE               Medium successfully locked.
*                               USBD_ERR_SCSI_LOCK_TIMEOUT  Medium lock timed out.
*                               USBD_ERR_SCSI_LOCK          Medium lock failed.
*
* Return(s)   : None.
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  USBD_StorageLock (USBD_STORAGE_LUN  *p_storage_lun,
                        CPU_INT32U         timeout_ms,
                        USBD_ERR          *p_err)
{
    FS_ERR  err_fs;


    FSDev_AccessLock(p_storage_lun->VolStrPtr,
                     timeout_ms,
                    &err_fs);
    switch (err_fs) {
        case FS_ERR_NONE:
            *p_err = USBD_ERR_NONE;
             break;

        case FS_ERR_OS_LOCK_TIMEOUT:
            *p_err = USBD_ERR_SCSI_LOCK_TIMEOUT;
             break;

        default:
            *p_err = USBD_ERR_SCSI_LOCK;
             break;
    }
}


/*
*********************************************************************************************************
*                                             USBD_StorageUnlock()
*
* Description : Unlock storage medium.
*
* Argument(s) : p_storage_lun    Pointer to logical unit storage structure.
*
*               p_err       Pointer to variable that will receive error code from this function.
*
*                               USBD_ERR_NONE           Medium successfully unlocked.
*                               USBD_ERR_SCSI_UNLOCK    Medium unlock failed.
*
* Return(s)   : None.
*
* Note(s)     : (1) If an embedded file system application manipulates files on a medium shared with
*                   uC/USB-Device, a lock mechanism is used to ensure an exclusive access to the mediunm.
*                   uC/USB-Device can try to obtain the lock on the medium between each file system
*                   operation. In that case, the file system operations sequence can be interrupted at
*                   any time by uC/USB-Device. When uC/USB-Device releases the lock, files which were
*                   manipulated by uC/FS are invalidated because the computer host may have altered them.
*********************************************************************************************************
*/

void  USBD_StorageUnlock (USBD_STORAGE_LUN  *p_storage_lun,
                          USBD_ERR          *p_err)
{
    FS_ERR  err_fs;


    FSDev_Invalidate(p_storage_lun->VolStrPtr, &err_fs);        /* Invalidate files opened on logical unit (see Note #1)*/
    if (err_fs != FS_ERR_NONE) {
       *p_err = USBD_ERR_SCSI_UNLOCK;
        return;
    }

    FSDev_AccessUnlock(p_storage_lun->VolStrPtr, &err_fs);
    if (err_fs != FS_ERR_NONE) {
       *p_err = USBD_ERR_SCSI_UNLOCK;
        return;
    }

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
*                                      USBD_StorageRefreshTaskHandler()
*
* Description : Device medium status refresh.
*
* Argument(s) : p_arg       Pointer to task initialization argument.
*
* Return(s)   : None.
*
* Note(s)     : (1) This function must be called by the MSC OS layer periodically. The period is
*                   configurable through the constant USBD_MSC_CFG_DEV_POLL_DLY_mS defined in usbd_cfg.h.
*
*               (2) Polling list is used by removable media such as SD card whose insertion and removal
*                   detection do NOT trigger an interrupt for the CPU. Hence, periodically, the presence
*                   state (i.e. present or not) of each logical unit added to the polling list is
*                   verified.
*********************************************************************************************************
*/

#if (USBD_MSC_CFG_FS_REFRESH_TASK_EN == DEF_ENABLED)
void  USBD_StorageRefreshTaskHandler (void *p_arg)
{
    FS_ERR      err_fs;
    CPU_INT32U  i;


    (void)p_arg;
                                                                /* ------ POLLING LIST PROCESSING (see Note #1) ------- */
    for (i = 0u; i < USBD_MSC_CFG_MAX_LUN; i++) {

        if (USBD_FS_StorageDevPollList[i] != (USBD_STORAGE_LUN *)0u) {
                                                                /* Get status of removable media.                       */
            FSDev_Refresh(USBD_FS_StorageDevPollList[i]->VolStrPtr, &err_fs);
            if(err_fs == FS_ERR_NONE) {
                USBD_FS_LunStatePresent[i] = DEF_TRUE;
            } else {
                USBD_FS_LunStatePresent[i] = DEF_FALSE;
            }
        }
    }
}
#endif
