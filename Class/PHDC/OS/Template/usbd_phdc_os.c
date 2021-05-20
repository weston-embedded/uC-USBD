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
*                                 USB PHDC CLASS OPERATING SYSTEM LAYER
*                                          Micrium Template
*
* Filename : usbd_phdc_os.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../usbd_phdc.h"
#include  "../../usbd_phdc_os.h"


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


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
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  USBD_PHDC_OS_WrBulkSchedTask(void  *p_arg);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         USBD_PHDC_OS_Init()
*
* Description : Initialize PHDC OS interface.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE               OS initialization successful.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_Init (USBD_ERR  *p_err)
{
    /* $$$$ Initialize all internal variables and tasks used by OS layer. */
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        USBD_PHDC_OS_RdLock()
*
* Description : Lock PHDC read pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
*               timeout_ms  Timeout, in ms.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS signal     successfully acquired.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_RdLock (CPU_INT08U   class_nbr,
                           CPU_INT16U   timeout_ms,
                           USBD_ERR    *p_err)
{
    /* $$$$ Lock read pipe. */
   *p_err = USBD_ERR_NONE;

}


/*
*********************************************************************************************************
*                                       USBD_PHDC_OS_RdUnlock()
*
* Description : Unlock PHDC read pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_RdUnlock (CPU_INT08U  class_nbr)
{
    /* $$$$ Unlock read pipe. */
}


/*
*********************************************************************************************************
*                                      USBD_PHDC_OS_WrIntrLock()
*
* Description : Lock PHDC write interrupt pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
*               timeout_ms  Timeout, in ms.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS signal     successfully acquired.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_WrIntrLock (CPU_INT08U   class_nbr,
                               CPU_INT16U   timeout_ms,
                               USBD_ERR    *p_err)
{
    /* $$$$ Lock interrupt write pipe. */
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     USBD_PHDC_OS_WrIntrUnlock()
*
* Description : Unlock PHDC write interrupt pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_WrIntrUnlock (CPU_INT08U  class_nbr)
{
    /* $$$$ Unlock interrupt write pipe. */
}


/*
*********************************************************************************************************
*                                      USBD_PHDC_OS_WrBulkLock()
*
* Description : Lock PHDC write bulk pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
*               prio        Priority of the transfer, based on its QoS.
*
*               timeout_ms  Timeout, in ms.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           OS signal     successfully acquired.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_WrBulkLock (CPU_INT08U   class_nbr,
                               CPU_INT08U   prio,
                               CPU_INT16U   timeout_ms,
                               USBD_ERR    *p_err)
{
    /* $$$$ Lock bulk write pipe. */
   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     USBD_PHDC_OS_WrBulkUnlock()
*
* Description : Unlock PHDC write bulk pipe.
*
* Argument(s) : class_nbr   PHDC instance number;
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_WrBulkUnlock (CPU_INT08U  class_nbr)
{
    /* $$$$ Unlock bulk wirte pipe. */
}


/*
*********************************************************************************************************
*                                        USBD_PHDC_OS_Reset()
*
* Description : Reset PHDC OS layer for given instance.
*
* Argument(s) : class_nbr   PHDC instance number;
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_PHDC_OS_Reset (CPU_INT08U  class_nbr)
{
    /* $$$$ Reset OS layer. */
}


/*
*********************************************************************************************************
*                                   USBD_PHDC_OS_WrBulkSchedTask()
*
* Description : OS-dependent shell task to schedule bulk transfers in function of their priority.
*
* Argument(s) : p_arg       Pointer to task initialization argument (required by uC/OS-III).
*
* Return(s)   : none.
*
* Note(s)     : (1) Only one task handle all class instances bulk write scheduling.
*********************************************************************************************************
*/

static  void  USBD_PHDC_OS_WrBulkSchedTask (void *p_arg)
{
    /* $$$$ If QoS prioritization is used, implement this task to act as a scheduler. */
}
