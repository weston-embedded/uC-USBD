/*
*********************************************************************************************************
*                                            EXAMPLE CODE
*
*               This file is provided as an example on how to use Micrium products.
*
*               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
*               your application products.  Example code may be used as is, in whole or in
*               part, or may be used as a reference only. This file can be modified as
*               required to meet the end-product requirements.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                       USB DEVICE DRIVER BOARD SUPPORT PACKAGE (BSP) FUNCTIONS
*
*                                              TEMPLATE
*
* Filename : bsp_usbd_template.h
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This USB device driver board-specific function header file is protected from multiple
*               pre-processor inclusion through use of the USB device configuration module present pre-
*               processor macro definition.
*********************************************************************************************************
*/

#ifndef  BSP_USBD_PRESENT                                       /* See Note #1.                                         */
#define  BSP_USBD_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "usbd_drv_template.h"


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  USBD_DRV_EP_INFO  USBD_DrvEP_InfoTbl_Template[];
extern  USBD_DRV_BSP_API  USBD_DrvBSP_Template;


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif
