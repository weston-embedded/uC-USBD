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
*                                        USB DEVICE HID REPORT
*
* Filename : usbd_hid_report.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#include  "../../Source/usbd_core.h"
#include  "usbd_hid_report.h"
#include  "usbd_hid.h"
#include  <lib_mem.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  USBD_HID_REPORT_ITEM_SIZE_MASK                 0x03u
#define  USBD_HID_REPORT_ITEM_TYPE_MASK                 0x0Cu
#define  USBD_HID_REPORT_ITEM_TAG_MASK                  0xF0u

#define  USBD_HID_IDLE_INFINITE                         0x00u
#define  USBD_HID_IDLE_ALL_REPORT                       0x00u


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        FORWARD DECLARATIONS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  usbd_hid_report_item {
    CPU_INT08U  ReportID;
    CPU_INT16U  Size;
    CPU_INT16U  Cnt;
} USBD_HID_REPORT_ITEM;


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

static  USBD_HID_REPORT_ID   USBD_HID_ReportID_Tbl[USBD_HID_CFG_MAX_NBR_REPORT_ID];


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

static  CPU_INT16U           USBD_HID_ReportID_Tbl_Ix;
static  USBD_HID_REPORT_ID  *USBD_HID_ReportID_TmrList;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void                  USBD_HID_ReportClr     (USBD_HID_REPORT       *p_report);

static  USBD_HID_REPORT_ID   *USBD_HID_ReportID_Alloc(void);

static  USBD_HID_REPORT_ID   *USBD_HID_ReportID_Get  (USBD_HID_REPORT       *p_report,
                                                      USBD_HID_REPORT_TYPE     report_type,
                                                      CPU_INT08U               report_id);


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
*                                       USBD_HID_Report_Init()
*
* Description : Initialize HID report module.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE   HID report module initialized successfully.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_Report_Init (USBD_ERR  *p_err)
{
    USBD_HID_REPORT_ID  *p_report_id;
    CPU_INT16U           ix;


    for (ix = 0; ix < USBD_HID_CFG_MAX_NBR_REPORT_ID; ix++) {
        p_report_id = &USBD_HID_ReportID_Tbl[ix];

        p_report_id->ID      =  0;
        p_report_id->Size    =  0;
        p_report_id->DataPtr = (CPU_INT08U         *)0;
        p_report_id->NextPtr = (USBD_HID_REPORT_ID *)0;

        p_report_id->ClassNbr   =  USBD_CLASS_NBR_NONE;
        p_report_id->IdleCnt    =  0;
        p_report_id->IdleRate   =  USBD_HID_IDLE_INFINITE;
        p_report_id->Update     =  DEF_NO;
        p_report_id->TmrNextPtr = (USBD_HID_REPORT_ID *)0;
    }

    USBD_HID_ReportID_Tbl_Ix  = 0;
    USBD_HID_ReportID_TmrList = (USBD_HID_REPORT_ID *)0;

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       USBD_HID_Report_Parse()
*
* Description : Parse HID report.
*
* Argument(s) : class_nbr           Class instance number.
*
*               p_report_data       Pointer to HID report descriptor.
*
*               report_data_len     Length of  HID report descriptor.
*
*               p_report            Pointer to HID report structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           HID report parsed successfully.
*                               USBD_ERR_NULL_PTR       Argument 'p_report_data'/'p_report' passed a NULL
*                                                           pointer.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'p_report_data'/
*                                                           'report_data_len'.
*                               USBD_ERR_FAIL           Invalid HID descriptor data, mismatch on end of
*                                                           collection, or mismatch on pop items.
*                               USBD_ERR_ALLOC          No more Report ID or global item structure available,
*                                                           or memory allocation failed for report buffers.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_Report_Parse (       CPU_INT08U        class_nbr,
                             const  CPU_INT08U       *p_report_data,
                                    CPU_INT16U          report_data_len,
                                    USBD_HID_REPORT  *p_report,
                                    USBD_ERR         *p_err)
{
    USBD_HID_REPORT_ID    *p_report_id;
    USBD_HID_REPORT_ITEM  *p_item;
    USBD_HID_REPORT_ITEM   item_tbl[USBD_HID_CFG_MAX_NBR_REPORT_PUSHPOP + 1];
    CPU_INT08U             item_tbl_size;
    CPU_INT08U             col_nesting;
    CPU_INT32U             data;
    CPU_INT08U             tag;
    CPU_INT08U             report_type;
    LIB_ERR                err_lib;


    p_report_id   = (USBD_HID_REPORT_ID *)0;
    item_tbl_size =  0;
    col_nesting   =  0;

    p_item           = &item_tbl[0];
    p_item->ReportID =  0;
    p_item->Size     =  0;
    p_item->Cnt      =  0;

    USBD_HID_ReportClr(p_report);

    while (report_data_len > 0) {
        data = 0;

        tag = *p_report_data;
        p_report_data++;
          report_data_len--;


        switch (tag & USBD_HID_REPORT_ITEM_SIZE_MASK) {
            case 3:                                             /* Item size: 4 bytes.                                  */
                 if (report_data_len < 4) {
                    *p_err = USBD_ERR_HID_REPORT_INVALID;
                     return;
                 }

                 data = MEM_VAL_GET_INT32U_LITTLE(p_report_data);

                 p_report_data     += 4;
                   report_data_len -= 4;
                 break;

            case 2:                                             /* Item size: 2 bytes.                                  */
                 if (report_data_len < 2) {
                    *p_err = USBD_ERR_HID_REPORT_INVALID;
                     return;
                 }

                 data = MEM_VAL_GET_INT16U_LITTLE(p_report_data);

                 p_report_data     += 2;
                   report_data_len -= 2;
                 break;

            case 1:                                             /* Item size: 1 byte.                                   */
                 if (report_data_len < 1) {
                    *p_err = USBD_ERR_HID_REPORT_INVALID;
                     return;
                 }

                 data = *p_report_data;

                 p_report_data++;
                   report_data_len--;
                 break;

            case 0:                                             /* Item size: 0 bytes.                                  */
            default:
                 break;
        }

        switch (tag & (USBD_HID_REPORT_ITEM_TYPE_MASK |
                       USBD_HID_REPORT_ITEM_TAG_MASK)) {
            case USBD_HID_MAIN_INPUT:
                 p_report_id = USBD_HID_ReportID_Get(p_report,
                                                     USBD_HID_REPORT_TYPE_INPUT,
                                                     p_item->ReportID);
                 if (p_report_id == (USBD_HID_REPORT_ID *)0) {
                    *p_err  = USBD_ERR_HID_REPORT_ALLOC;
                     return;
                 }

                 p_report_id->Size     += p_item->Cnt * p_item->Size;
                 p_report_id->ClassNbr  = class_nbr;

                 if (p_report->MaxInputReportSize < p_report_id->Size) {
                     p_report->MaxInputReportSize = p_report_id->Size;
                 }
                 break;

            case USBD_HID_MAIN_OUTPUT:
                 p_report_id = USBD_HID_ReportID_Get(p_report,
                                                     USBD_HID_REPORT_TYPE_OUTPUT,
                                                     p_item->ReportID);
                 if (p_report_id == (USBD_HID_REPORT_ID *)0) {
                    *p_err  = USBD_ERR_HID_REPORT_ALLOC;
                     return;
                 }

                 p_report_id->Size     += p_item->Cnt * p_item->Size;
                 p_report_id->ClassNbr  = class_nbr;

                 if (p_report->MaxOutputReportSize < p_report_id->Size) {
                     p_report->MaxOutputReportSize = p_report_id->Size;
                 }
                 break;

            case USBD_HID_MAIN_FEATURE:
                 p_report_id = USBD_HID_ReportID_Get(p_report,
                                                     USBD_HID_REPORT_TYPE_FEATURE,
                                                     p_item->ReportID);
                 if (p_report_id == (USBD_HID_REPORT_ID *)0) {
                    *p_err  = USBD_ERR_HID_REPORT_ALLOC;
                     return;
                 }

                 p_report_id->Size     += p_item->Cnt * p_item->Size;
                 p_report_id->ClassNbr  = class_nbr;

                 if (p_report->MaxFeatureReportSize < p_report_id->Size) {
                     p_report->MaxFeatureReportSize = p_report_id->Size;
                 }
                 break;


            case USBD_HID_MAIN_COLLECTION:
                 col_nesting++;
                 break;

            case USBD_HID_MAIN_ENDCOLLECTION:
                 if (col_nesting == 0) {
                    *p_err = USBD_ERR_HID_REPORT_INVALID;
                     return;
                 }

                 col_nesting--;
                 break;


            case USBD_HID_GLOBAL_REPORT_SIZE:
                 p_item->Size = data & DEF_INT_16_MASK;
                 break;

            case USBD_HID_GLOBAL_REPORT_COUNT:
                 p_item->Cnt = data & DEF_INT_16_MASK;
                 break;

            case USBD_HID_GLOBAL_REPORT_ID:
                 p_item->ReportID     = data & DEF_INT_08_MASK;
                 p_report->HasReports = DEF_YES;
                 break;


            case USBD_HID_GLOBAL_PUSH:
#if (USBD_HID_CFG_MAX_NBR_REPORT_PUSHPOP > 0)
                 if (item_tbl_size >= USBD_HID_CFG_MAX_NBR_REPORT_PUSHPOP) {
                    *p_err = USBD_ERR_HID_REPORT_PUSH_POP_ALLOC;
                     return;
                 }

                 p_item = &item_tbl[item_tbl_size + 1];

                 p_item->ReportID = item_tbl[item_tbl_size].ReportID;
                 p_item->Size     = item_tbl[item_tbl_size].Size;
                 p_item->Cnt      = item_tbl[item_tbl_size].Cnt;

                 item_tbl_size++;
                 break;
#else
                *p_err = USBD_ERR_HID_REPORT_PUSH_POP_ALLOC;
                 return;
#endif

            case USBD_HID_GLOBAL_POP:
                 if (item_tbl_size == 0) {
                    *p_err = USBD_ERR_HID_REPORT_INVALID;
                     return;
                 }

                 item_tbl_size--;
                 p_item = &item_tbl[item_tbl_size];
                 break;


            case USBD_HID_LOCAL_USAGE:
            case USBD_HID_LOCAL_USAGE_MIN:
            case USBD_HID_LOCAL_USAGE_MAX:
            case USBD_HID_GLOBAL_USAGE_PAGE:
            case USBD_HID_GLOBAL_LOG_MIN:
            case USBD_HID_GLOBAL_LOG_MAX:
            case USBD_HID_GLOBAL_PHY_MIN:
            case USBD_HID_GLOBAL_PHY_MAX:
            case USBD_HID_GLOBAL_UNIT_EXPONENT:
            case USBD_HID_GLOBAL_UNIT:
            case USBD_HID_LOCAL_DESIGNATOR_INDEX:
            case USBD_HID_LOCAL_DESIGNATOR_MIN:
            case USBD_HID_LOCAL_DESIGNATOR_MAX:
            case USBD_HID_LOCAL_STRING_INDEX:
            case USBD_HID_LOCAL_STRING_MIN:
            case USBD_HID_LOCAL_STRING_MAX:
            case USBD_HID_LOCAL_DELIMITER:
                 break;

            default:
                *p_err = USBD_ERR_HID_REPORT_INVALID;
                 return;
        }
    }

    if (col_nesting > 0) {
       *p_err = USBD_ERR_HID_REPORT_INVALID;
        return;
    }

                                                                /* ----------- CONVERT REPORT SIZE TO OCTETS ---------- */
    p_report->MaxInputReportSize += 7;
    p_report->MaxInputReportSize /= 8;

    p_report->MaxOutputReportSize += 7;
    p_report->MaxOutputReportSize /= 8;

    p_report->MaxFeatureReportSize += 7;
    p_report->MaxFeatureReportSize /= 8;

    if (p_report->HasReports == DEF_YES) {                      /* Reserve space for Report ID.                         */
        if (p_report->MaxInputReportSize > 0) {
            p_report->MaxInputReportSize++;
        }
        if (p_report->MaxOutputReportSize > 0) {
            p_report->MaxOutputReportSize++;
        }
        if (p_report->MaxFeatureReportSize > 0) {
            p_report->MaxFeatureReportSize++;
        }
    }

    if (p_report->MaxOutputReportSize > 0) {
        p_report->MaxOutputReportPtr = (CPU_INT08U *)Mem_HeapAlloc(              p_report->MaxOutputReportSize,
                                                                                 USBD_CFG_BUF_ALIGN_OCTETS,
                                                                   (CPU_SIZE_T *)DEF_NULL,
                                                                                &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }
    }

    if (p_report->MaxFeatureReportSize > 0) {
        p_report->MaxFeatureReportPtr = (CPU_INT08U *)Mem_HeapAlloc(              p_report->MaxFeatureReportSize,
                                                                                  USBD_CFG_BUF_ALIGN_OCTETS,
                                                                    (CPU_SIZE_T *)DEF_NULL,
                                                                                 &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = USBD_ERR_ALLOC;
            return;
        }
    }

    for (report_type = 0; report_type < 3; report_type++) {
        p_report_id = p_report->Reports[report_type];
        while (p_report_id != (USBD_HID_REPORT_ID *)0) {
            p_report_id->Size += 7;
            p_report_id->Size /= 8;

            if (p_report_id->Size > 0) {
                if (p_report->HasReports == DEF_YES) {
                    p_report_id->Size++;                        /* Reserve space for report ID.                         */
                }

                if (report_type == 0) {                         /* Input reports use individual buf.                    */
                    p_report_id->DataPtr = (CPU_INT08U *)Mem_HeapAlloc(              p_report_id->Size,
                                                                                     USBD_CFG_BUF_ALIGN_OCTETS,
                                                                       (CPU_SIZE_T *)DEF_NULL,
                                                                                    &err_lib);
                    if (err_lib != LIB_MEM_ERR_NONE) {
                       *p_err = USBD_ERR_ALLOC;
                        return;
                    }
                                                                 /* The first byte must be the report ID.                */
                    Mem_Clr(&p_report_id->DataPtr[0u], p_report_id->Size);
                    p_report_id->DataPtr[0u] = p_report_id->ID;
                }
            }

            p_report_id = p_report_id->NextPtr;
        }
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     USBD_HID_ReportID_InfoGet()
*
* Description : Retrieve HID report length and pointer to its data area.
*
* Argument(s) : p_report        Pointer to HID report structure.
*
*               report_type     HID report type.
*
*               report_id       HID report ID.
*
*               p_buf           Pointer to variable that will receive the pointer to the HID report data area.
*
*               p_is_largest    Pointer to variable that will receive the indication that the HID input or
*                                   feature report is the largest report from the list of input or feature
*                                   reports.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           HID report size retrieved successfully.
*                               USBD_ERR_NULL_PTR       Argument 'p_report'/'p_is_largest' passed a NULL
*                                                           pointer.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'report_type'/
*                                                           'report_id'.
*
* Return(s)   : Length of HID report, in octets.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  USBD_HID_ReportID_InfoGet (const  USBD_HID_REPORT        *p_report,
                                              USBD_HID_REPORT_TYPE      report_type,
                                              CPU_INT08U                report_id,
                                              CPU_INT08U            **p_buf,
                                              CPU_BOOLEAN            *p_is_largest,
                                              USBD_ERR               *p_err)
{
    USBD_HID_REPORT_ID  *p_report_id;


    switch (report_type) {
        case USBD_HID_REPORT_TYPE_INPUT:
             p_report_id = p_report->Reports[0];
             break;

        case USBD_HID_REPORT_TYPE_OUTPUT:
             p_report_id = p_report->Reports[1];
             break;

        case USBD_HID_REPORT_TYPE_FEATURE:
             p_report_id = p_report->Reports[2];
             break;

        case USBD_HID_REPORT_TYPE_NONE:
        default:
            *p_err = USBD_ERR_INVALID_ARG;
             return (0);
    }

    while (p_report_id != (USBD_HID_REPORT_ID *)0) {
        if (p_report_id->ID == report_id) {
            switch (report_type) {
                case USBD_HID_REPORT_TYPE_INPUT:
                    if (p_buf != (CPU_INT08U **)0) {
                       *p_buf  =  p_report_id->DataPtr;
                    }
                    *p_is_largest = (p_report_id->Size == p_report->MaxInputReportSize) ? DEF_YES : DEF_NO;
                     break;

                case USBD_HID_REPORT_TYPE_OUTPUT:
                    if (p_buf != (CPU_INT08U **)0) {
                       *p_buf  = p_report->MaxOutputReportPtr;
                    }
                    *p_is_largest = DEF_NO;
                     break;

                case USBD_HID_REPORT_TYPE_FEATURE:
                default:
                    if (p_buf != (CPU_INT08U **)0) {
                       *p_buf  =  p_report->MaxFeatureReportPtr;
                    }
                    *p_is_largest = (p_report_id->Size == p_report->MaxFeatureReportSize) ? DEF_YES : DEF_NO;
                     break;
            }
           *p_err = USBD_ERR_NONE;
            return (p_report_id->Size);
        }

        p_report_id = p_report_id->NextPtr;
    }

   *p_err = USBD_ERR_INVALID_ARG;
    return (0);
}


/*
*********************************************************************************************************
*                                  USBD_HID_Report_TmrTaskHandler()
*
* Description : Process all periodic HID input reports.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_Report_TmrTaskHandler (void)
{
    USBD_HID_REPORT_ID  *p_report_id;
    USBD_HID_REPORT_ID  *p_report_id_prev;
    CPU_BOOLEAN          service;
    USBD_ERR             err;
    CPU_SR_ALLOC();


    p_report_id_prev = (USBD_HID_REPORT_ID *)0;
    CPU_CRITICAL_ENTER();
    p_report_id      =  USBD_HID_ReportID_TmrList;
    CPU_CRITICAL_EXIT();

    while (p_report_id != (USBD_HID_REPORT_ID *)0) {
        service = DEF_NO;

        CPU_CRITICAL_ENTER();
        if (p_report_id->IdleRate == USBD_HID_IDLE_INFINITE) {
            p_report_id->IdleCnt  = 0u;
            p_report_id->Update   = DEF_NO;

            if (p_report_id_prev == (USBD_HID_REPORT_ID *)0) {
                USBD_HID_ReportID_TmrList    = p_report_id->TmrNextPtr;
            } else {
                p_report_id_prev->TmrNextPtr = p_report_id->TmrNextPtr;
            }

            p_report_id->TmrNextPtr = (USBD_HID_REPORT_ID *)0;

        } else {
            if (p_report_id->Update == DEF_YES) {
                p_report_id->Update  = DEF_NO;

                if (p_report_id->IdleCnt > 1) {
                    p_report_id->IdleCnt = p_report_id->IdleRate;
                }
            }

            if (p_report_id->IdleCnt > 1) {
                p_report_id->IdleCnt--;
            } else {
                p_report_id->IdleCnt = p_report_id->IdleRate;
                service = DEF_YES;
            }
        }
        CPU_CRITICAL_EXIT();

        if (service == DEF_YES) {
            USBD_HID_Wr(p_report_id->ClassNbr,
                        p_report_id->DataPtr,
                        p_report_id->Size,
                        100,
                       &err);
        }

        p_report_id_prev = p_report_id;
        p_report_id      = p_report_id->TmrNextPtr;
    }
}


/*
*********************************************************************************************************
*                                     USBD_HID_ReportID_IdleGet()
*
* Description : Retrieve HID input report idle rate.
*
* Argument(s) : p_report    Pointer to HID report structure.
*
*               report_id   HID report ID.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           HID input report idle rate retrieved successfully.
*                               USBD_ERR_NULL_PTR       Argument 'p_report' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'report_id'.
*
* Return(s)   : Idle rate.
*
* Note(s)     : (1) Idle rate is in 4 millisecond units.
*********************************************************************************************************
*/

CPU_INT08U  USBD_HID_ReportID_IdleGet (const  USBD_HID_REPORT  *p_report,
                                              CPU_INT08U          report_id,
                                              USBD_ERR         *p_err)
{
    USBD_HID_REPORT_ID  *p_report_id;


    p_report_id = p_report->Reports[0];

    while (p_report_id != (USBD_HID_REPORT_ID *)0) {
        if (p_report_id->ID == report_id) {
           *p_err = USBD_ERR_NONE;
            return (p_report_id->IdleRate);
        }

        p_report_id = p_report_id->NextPtr;
    }

   *p_err = USBD_ERR_INVALID_ARG;
    return (0);
}


/*
*********************************************************************************************************
*                                     USBD_HID_ReportID_IdleSet()
*
* Description : Set HID input report idle rate.
*
* Argument(s) : p_report    Pointer to HID report structure.
*
*               report_id   HID report ID.
*
*               idle_rate   Report idle rate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           HID input report idle rate retrieved successfully.
*                               USBD_ERR_NULL_PTR       Argument 'p_report' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'report_id'.
*
* Return(s)   : none.
*
* Note(s)     : (1) Idle rate is in 4 millisecond units.
*********************************************************************************************************
*/

void  USBD_HID_ReportID_IdleSet (const  USBD_HID_REPORT  *p_report,
                                        CPU_INT08U          report_id,
                                        CPU_INT08U        idle_rate,
                                        USBD_ERR         *p_err)
{
    USBD_HID_REPORT_ID  *p_report_id;
    CPU_SR_ALLOC();


    p_report_id = p_report->Reports[0];

   *p_err = USBD_ERR_INVALID_ARG;

    while (p_report_id != (USBD_HID_REPORT_ID *)0) {
         if ((p_report_id->ID == report_id) ||
             (  report_id     == USBD_HID_IDLE_ALL_REPORT)) {
             if (idle_rate != USBD_HID_IDLE_INFINITE) {
                 CPU_CRITICAL_ENTER();
                                                         /* Add report ID into timer list.                       */
                 if (p_report_id->IdleRate == USBD_HID_IDLE_INFINITE) {
                     p_report_id->TmrNextPtr   = USBD_HID_ReportID_TmrList;
                     USBD_HID_ReportID_TmrList = p_report_id;
                 }

                 p_report_id->IdleRate = idle_rate;
                 p_report_id->Update   = DEF_YES;

                 CPU_CRITICAL_EXIT();
             } else {
                 CPU_CRITICAL_ENTER();
                 p_report_id->IdleRate = USBD_HID_IDLE_INFINITE;
                 CPU_CRITICAL_EXIT();
             }

            *p_err = USBD_ERR_NONE;

             if (report_id != USBD_HID_IDLE_ALL_REPORT) {
                 return;
             }
         }

         p_report_id = p_report_id->NextPtr;
     }
}


/*
*********************************************************************************************************
*                                   USBD_HID_Report_RemoveAllIdle()
*
* Description : Remove all HID input report from periodic service list.
*
* Argument(s) : p_report    Pointer to HID report structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE           HID input report idle rate retrieved successfully.
*                               USBD_ERR_NULL_PTR       Argument 'p_report' passed a NULL pointer.
*                               USBD_ERR_INVALID_ARG    Invalid argument(s) passed to 'report_id'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  USBD_HID_Report_RemoveAllIdle (const  USBD_HID_REPORT  *p_report)
{
    USBD_HID_REPORT_ID  *p_report_id;
    CPU_SR_ALLOC();


    p_report_id = p_report->Reports[0];

    while (p_report_id != (USBD_HID_REPORT_ID *)0) {
        CPU_CRITICAL_ENTER();                                   /* Remove only reports present on timer list.           */
        if (p_report_id->TmrNextPtr != (USBD_HID_REPORT_ID *)0) {
            p_report_id->IdleRate = USBD_HID_IDLE_INFINITE;
        }
        CPU_CRITICAL_EXIT();

        p_report_id = p_report_id->NextPtr;
    }
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
*                                        USBD_HID_ReportClr()
*
* Description : Initialize HID report structure.
*
* Argument(s) : p_report        Pointer to HID report structure.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_HID_ReportClr (USBD_HID_REPORT  *p_report)
{
    p_report->HasReports           = 0;
    p_report->MaxInputReportSize   = 0;
    p_report->MaxOutputReportSize  = 0;
    p_report->MaxFeatureReportSize = 0;

    p_report->MaxOutputReportPtr  = (CPU_INT08U *)0;
    p_report->MaxFeatureReportPtr = (CPU_INT08U *)0;

    p_report->Reports[0] = (USBD_HID_REPORT_ID  *)0;
    p_report->Reports[1] = (USBD_HID_REPORT_ID  *)0;
    p_report->Reports[2] = (USBD_HID_REPORT_ID  *)0;
}


/*
*********************************************************************************************************
*                                      USBD_HID_ReportID_Alloc()
*
* Description : Allocate an instance of HID report ID structure.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to HID report ID structure, if NO error(s).
*
*               Pointer to NULL,                    otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  USBD_HID_REPORT_ID  *USBD_HID_ReportID_Alloc (void)
{
    USBD_HID_REPORT_ID  *p_report_id;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    if (USBD_HID_ReportID_Tbl_Ix >= USBD_HID_CFG_MAX_NBR_REPORT_ID) {
        CPU_CRITICAL_EXIT();
        return ((USBD_HID_REPORT_ID *)0);
    }

    p_report_id = &USBD_HID_ReportID_Tbl[USBD_HID_ReportID_Tbl_Ix];

    USBD_HID_ReportID_Tbl_Ix++;
    CPU_CRITICAL_EXIT();

    return (p_report_id);
}


/*
*********************************************************************************************************
*                                        USBD_HID_ReportID_Get()
*
* Description : Retrieve HID report ID structure.
*
* Argument(s) : p_report        Pointer to HID report structure.
*
*               report_type     HID report type.
*
*               report_id       HID report ID.
*
* Return(s)   : Pointer to HID report ID structure, if NO error(s).
*
*               Pointer to NULL,                    otherwise.
*
* Note(s)     : (1) If HID report ID structure is not available for the specific report type and ID, an
*                   instance of HID report ID structure is allocated and linked into the HID report
*                   structure.
*********************************************************************************************************
*/

static  USBD_HID_REPORT_ID  *USBD_HID_ReportID_Get (USBD_HID_REPORT       *p_report,
                                                    USBD_HID_REPORT_TYPE     report_type,
                                                    CPU_INT08U               report_id)
{
    USBD_HID_REPORT_ID  *p_report_id;
    USBD_HID_REPORT_ID  *p_report_id_prev;
    CPU_INT08U           type;


    switch (report_type) {
        case USBD_HID_REPORT_TYPE_INPUT:
             type = 0;
             break;

        case USBD_HID_REPORT_TYPE_OUTPUT:
             type = 1;
             break;

        case USBD_HID_REPORT_TYPE_FEATURE:
             type = 2;
             break;

        case USBD_HID_REPORT_TYPE_NONE:
        default:
             return ((USBD_HID_REPORT_ID *)0);
    }


    if (p_report->HasReports == DEF_NO) {
        if (report_id > 0) {
            return ((USBD_HID_REPORT_ID *)0);
        }

        if (p_report->Reports[type] == (USBD_HID_REPORT_ID *)0) {
            p_report->Reports[type] = USBD_HID_ReportID_Alloc();
            if (p_report->Reports[type] == (USBD_HID_REPORT_ID *)0) {
                return ((USBD_HID_REPORT_ID *)0);
            }
        }

        p_report_id = p_report->Reports[type];

        return (p_report_id);
    }


    p_report_id      = p_report->Reports[type];
    p_report_id_prev = (USBD_HID_REPORT_ID *)0;
    while (p_report_id != (USBD_HID_REPORT_ID *)0) {
        if (p_report_id->ID == report_id) {
            return (p_report_id);
        }

        p_report_id_prev = p_report_id;
        p_report_id      = p_report_id->NextPtr;
    }

    p_report_id = USBD_HID_ReportID_Alloc();
    if (p_report_id == (USBD_HID_REPORT_ID *)0) {
        return ((USBD_HID_REPORT_ID *)0);
    }

    p_report_id->ID = report_id;

    if (p_report->Reports[type] == (USBD_HID_REPORT_ID *)0) {
        p_report->Reports[type]  =  p_report_id;
    } else {
        p_report_id_prev->NextPtr = p_report_id;
    }

    return (p_report_id);
}
