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
*                                     USB AUDIO SIMULATION DRIVER
*
* Filename : usbd_audio_drv_simulation.c
* Version  : V4.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "usbd_audio_drv_simulation.h"

#if (APP_CFG_USBD_AUDIO_EN == DEF_ENABLED)
#include  <Class/Audio/usbd_audio_processing.h>
#include  <usbd_audio_dev_cfg.h>
#include  <Source/os.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) Since in this demonstration the volume is adjusted by software and not by an hardware
*               audio codec, the range supported is smaller, to avoid longer calculations to determine
*               the gain required for a given dB value. This value has been chosen for demonstration
*               purposes since it is the lowest supported by Windows 7.
*
*           (2) Some host operating sytems use a special 'mute' value that is the lowest possible volume
*               value the device supports.
*********************************************************************************************************
*/

#ifndef OS_VERSION
#error "OS_VERSION must be #define'd."
#endif

#if   (OS_VERSION > 30000u)
#define  APP_USBD_AUDIO_DRV_SIM_OS_III_EN                          DEF_ENABLED
#else
#define  APP_USBD_AUDIO_DRV_SIM_OS_III_EN                          DEF_DISABLED
#endif

                                                                /* Value indicating that the AS_IF is not opened yet.   */
#define  USBD_AUDIO_DRV_SIMULATION_AS_HANDLE_NONE             0xFFFFu
                                                                /* Maximum volume value, used as a reference.           */
#define  USBD_AUDIO_DRV_SIMULATION_VOLUME_MAX                 0x7FFFu
                                                                /* Minimum volume value. See note #1.                   */
#define  USBD_AUDIO_DRV_SIMULATION_VOLUME_MIN                 0x62D8u
                                                                /* Special 'mute' value. See note #2.                   */
#define  USBD_AUDIO_DRV_SIMULATION_VOLUME_MUTE                    (USBD_AUDIO_DRV_SIMULATION_VOLUME_MIN - 1u)

                                                                /* Audio format used in app.                            */
#define  USBD_AUDIO_DRV_SIMULATION_DATA_CFG_FORMAT                 USBD_AUDIO_MONO
                                                                /* Bit resolution used in app.                          */
#define  USBD_AUDIO_DRV_SIMULATION_DATA_CFG_BIT_RES                USBD_AUDIO_FMT_TYPE_I_BIT_RESOLUTION_16
                                                                /* Max nb of xfers contained in circular audio buf.     */
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_DISABLED)
#define  USBD_AUDIO_DRV_SIMULATION_CIRCULAR_AUDIO_BUF_SIZE         USBD_AUDIO_DEV_CFG_RECORD_NBR_BUF
#else
#define  USBD_AUDIO_DRV_SIMULATION_CIRCULAR_AUDIO_BUF_SIZE         DEF_MAX(USBD_AUDIO_DEV_CFG_RECORD_NBR_BUF, \
                                                                           USBD_AUDIO_DEV_CFG_PLAYBACK_NBR_BUF)
#endif


#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
                                                                /* Selected sampling frequency.                         */
#define  USBD_AUDIO_DRV_SIMULATION_DATA_CFG_SAMPLING_FREQ          USBD_AUDIO_FMT_TYPE_I_SAMFREQ_48KHZ
                                                                /* Extra sample frame size in case of stream corr en.   */
#define  USBD_AUDIO_DRV_SIMULATION_DATA_XFER_EXTRA_SAMPLE_FRAME   (USBD_AUDIO_DRV_SIMULATION_DATA_CFG_FORMAT * (USBD_AUDIO_DRV_SIMULATION_DATA_CFG_BIT_RES / 8u))
                                                                /* Size of a single xfer, in bytes.                     */
#define  USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES_NOMINAL   ((USBD_AUDIO_DRV_SIMULATION_DATA_CFG_SAMPLING_FREQ / 1000u) * (USBD_AUDIO_DRV_SIMULATION_DATA_CFG_BIT_RES / 8u) * USBD_AUDIO_DRV_SIMULATION_DATA_CFG_FORMAT)
#define  USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES_CORR      (USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES_NOMINAL + USBD_AUDIO_DRV_SIMULATION_DATA_XFER_EXTRA_SAMPLE_FRAME)

#if (USBD_AUDIO_CFG_PLAYBACK_FEEDBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_PLAYBACK_CORR_EN     == DEF_ENABLED)
#define  USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES            USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES_CORR
#else
#define  USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES            USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES_NOMINAL
#endif

#if (USBD_AUDIO_CFG_RECORD_CORR_EN == DEF_ENABLED)
#define  USBD_AUDIO_DRV_SIMULATION_RECORD_DATA_XFER_SIZE_BYTES     USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES_CORR
#else
#define  USBD_AUDIO_DRV_SIMULATION_RECORD_DATA_XFER_SIZE_BYTES     USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES_NOMINAL
#endif

                                                                /* Len of xfer, adjusted for bit resolution.            */
#define  USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SAMPLES              (USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES         / (USBD_AUDIO_DRV_SIMULATION_DATA_CFG_BIT_RES / 8u))
#define  USBD_AUDIO_DRV_SIMULATION_RECORD_DATA_XFER_SAMPLES       (USBD_AUDIO_DRV_SIMULATION_RECORD_DATA_XFER_SIZE_BYTES  / (USBD_AUDIO_DRV_SIMULATION_DATA_CFG_BIT_RES / 8u))
                                                                /* Max nb of xfers contained in circular buf.           */
#define  USBD_AUDIO_DRV_SIMULATION_LOOP_MAX_NB_XFERS               DEF_MAX(USBD_AUDIO_DEV_CFG_RECORD_NBR_BUF, \
                                                                           USBD_AUDIO_DEV_CFG_PLAYBACK_NBR_BUF)
                                                                /* Size, in bytes, of circular buf.                     */
#define  USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF_SIZE_BYTES   (USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SIZE_BYTES * \
                                                                   USBD_AUDIO_DRV_SIMULATION_LOOP_MAX_NB_XFERS)
                                                                /* Size of circular buf, adjusted for bit resolution.   */
#define  USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF_LEN          (USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF_SIZE_BYTES / \
                                                                  (USBD_AUDIO_DRV_SIMULATION_DATA_CFG_BIT_RES / 8u))
                                                                /* Threshold of nb xfer req'd to send data to host.     */
#define  USBD_AUDIO_DRV_SIMULATION_LOOP_TRESHOLD_RECORD            2u
#endif


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       VOLUME CONVERSION TABLE
*
* Note(s) : (1) For each 20dB delta, the value of the linear gain is the same value, with a factor 10 of
*               difference. In other words, the linear gain of 0, 20, 40 and 60dB is the same, except for
*               the power of 10. For example, 0dB corresponds to a gain of 1 and 20dB corresponds to a
*               gain of 0.1. 40dB is a gain of 0.01 and so on. Accuracy decreases each time the gain
*               value is divided by 10, since fewer siginificant bits are kept. Since we limit the
*               volume range to a minimum of 0x62D8 (~-29 dB), precision will still be acceptable.
*
*           (2) The size of the table varies according to the resolution required and the size of the
*               loop that is expected. For example, to have a resolution of 1/256th, the table would need
*               to have a size of 5120. If the resolution needed was 1dB, the size of the table could be
*               lowered to 20.
*
*           (3) All the values in the conversion table have already been multiplied by 32768 (left-bit-
*               shifted of 15) to be able to do the required operations without using floating-point
*               values. Once a given sample will have been multiplied by the gain, it will need to be
*               divided by 32768 (right-bit-shifted by 15) to obtain the correct value. The '+1' is to
*               take into account the fact that samples must be signed (2's complement).
*
*           (4) The values contained in this table were obtained mathematically by converting a gain
*               value (in dB) to a linear gain value. They were then multiplied by 32768 (except for the
*               first value, which was multiplied by 32767 to avoid overflow on signed variables). The
*               table has a resolution of 1/16th dB, so each index represents the linear gain value for
*               16 dB values. For example, index 0 will be the linear gain for values ranging from 0 dB
*               to -1/16th dB.
*********************************************************************************************************
*/

                                                                /* Resolution of the conv table, in 1/256th dB unit.    */
#define  USBD_AUDIO_DRV_SIMULATION_CONV_TBL_DB_RES                16u
                                                                /* See Note #1.                                         */
#define  USBD_AUDIO_DRV_SIMULATION_CONV_TBL_DB_LOOP               20u
                                                                /* Size of the conversion table (see Note #2).          */
#define  USBD_AUDIO_DRV_SIMULATION_CONV_TBL_SIZE               ((256u / USBD_AUDIO_DRV_SIMULATION_CONV_TBL_DB_RES) * \
                                                                   USBD_AUDIO_DRV_SIMULATION_CONV_TBL_DB_LOOP)
                                                                /* See Note #3.                                         */
#define  USBD_AUDIO_DRV_SIMULATION_CONV_TBL_GAIN_BIT_SHIFT        16u

                                                                /* Volume conv tbl (see Note #4).                       */
static  const  CPU_INT16U  ConvTbl_dB_ToGain[USBD_AUDIO_DRV_SIMULATION_CONV_TBL_SIZE] = {
    32767u, 32533u, 32299u, 32068u, 31838u, 31610u, 31383u, 31158u, 30934u, 30713u, 30492u, 30274u, 30057u, 29841u, 29627u, 29415u,
    29204u, 28995u, 28787u, 28580u, 28375u, 28172u, 27970u, 27769u, 27570u, 27373u, 27176u, 26982u, 26788u, 26596u, 26405u, 26216u,
    26028u, 25841u, 25656u, 25472u, 25290u, 25108u, 24928u, 24749u, 24572u, 24396u, 24221u, 24047u, 23875u, 23704u, 23534u, 23365u,
    23197u, 23031u, 22866u, 22702u, 22539u, 22378u, 22217u, 22058u, 21900u, 21743u, 21587u, 21432u, 21278u, 21126u, 20974u, 20824u,
    20675u, 20526u, 20379u, 20233u, 20088u, 19944u, 19801u, 19659u, 19518u, 19378u, 19239u, 19101u, 18964u, 18828u, 18693u, 18559u,
    18426u, 18294u, 18163u, 18033u, 17903u, 17775u, 17648u, 17521u, 17396u, 17271u, 17147u, 17024u, 16902u, 16781u, 16660u, 16541u,
    16422u, 16305u, 16188u, 16072u, 15956u, 15842u, 15728u, 15616u, 15504u, 15393u, 15282u, 15173u, 15064u, 14956u, 14849u, 14742u,
    14636u, 14531u, 14427u, 14324u, 14221u, 14119u, 14018u, 13917u, 13818u, 13719u, 13620u, 13523u, 13426u, 13329u, 13234u, 13139u,
    13045u, 12951u, 12858u, 12766u, 12675u, 12584u, 12493u, 12404u, 12315u, 12227u, 12139u, 12052u, 11966u, 11880u, 11795u, 11710u,
    11626u, 11543u, 11460u, 11378u, 11296u, 11215u, 11135u, 11055u, 10976u, 10897u, 10819u, 10741u, 10664u, 10588u, 10512u, 10436u,
    10362u, 10287u, 10214u, 10140u, 10068u,  9995u,  9924u,  9853u,  9782u,  9712u,  9642u,  9573u,  9504u,  9436u,  9369u,  9301u,
     9235u,  9169u,  9103u,  9038u,  8973u,  8908u,  8845u,  8781u,  8718u,  8656u,  8594u,  8532u,  8471u,  8410u,  8350u,  8290u,
     8230u,  8171u,  8113u,  8055u,  7997u,  7940u,  7883u,  7826u,  7770u,  7714u,  7659u,  7604u,  7550u,  7495u,  7442u,  7388u,
     7335u,  7283u,  7231u,  7179u,  7127u,  7076u,  7025u,  6975u,  6925u,  6875u,  6826u,  6777u,  6728u,  6680u,  6632u,  6585u,
     6538u,  6491u,  6444u,  6398u,  6352u,  6307u,  6261u,  6216u,  6172u,  6128u,  6084u,  6040u,  5997u,  5954u,  5911u,  5869u,
     5827u,  5785u,  5743u,  5702u,  5661u,  5621u,  5580u,  5540u,  5501u,  5461u,  5422u,  5383u,  5345u,  5306u,  5268u,  5230u,
     5193u,  5156u,  5119u,  5082u,  5046u,  5009u,  4973u,  4938u,  4902u,  4867u,  4832u,  4798u,  4763u,  4729u,  4695u,  4662u,
     4628u,  4595u,  4562u,  4529u,  4497u,  4465u,  4433u,  4401u,  4369u,  4338u,  4307u,  4276u,  4245u,  4215u,  4185u,  4155u,
     4125u,  4095u,  4066u,  4037u,  4008u,  3979u,  3950u,  3922u,  3894u,  3866u,  3838u,  3811u,  3783u,  3756u,  3729u,  3703u,
     3676u,  3650u,  3624u,  3598u,  3572u,  3546u,  3521u,  3496u,  3470u,  3446u,  3421u,  3396u,  3372u,  3348u,  3324u,  3300u
};


/*
*********************************************************************************************************
*                                            LOCAL MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
typedef  struct  usbd_audio_drv_simulation_loop_circular_buf {
                                                                /* Circular buf used to save data for loopback.         */
    CPU_INT16S  Buf[USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF_LEN];
    CPU_INT32U  IxIn;                                           /* Ix used by playback (speaker) to store data.         */
    CPU_INT32U  IxOut;                                          /* Ix used by record   (mic)     to get   data.         */
    CPU_INT16U  NbrXfers;                                       /* Nbr of xfers contained in the buf.                   */
    CPU_INT32U  BufCurCnt;                                      /* Total nbr of elements currently in buf.              */
} USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF;
#endif

typedef  struct  usbd_audio_drv_simulation_fu_unit_info {
           USBD_AUDIO_AS_HANDLE   AS_Handle;                    /* Handle to audio streaming IF.                        */
           CPU_INT16U             Vol;                          /* Volume value set by the host.                        */
           CPU_BOOLEAN            Mute;                         /* Flag indicating if mute is set or not.               */
           CPU_INT32U             Gain;                         /* Volume val used to get the linear gain.              */
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_DISABLED)
           CPU_INT32U             SamFreq;                      /* Cur sample freq set by the host.                     */
    const  CPU_INT16U            *BufHeadPtr;                   /* Ptr to head of samples buf.                          */
           CPU_INT32U             BufNbSamples;                 /* Nb of samples contained in cur buf.                  */
#endif
} USBD_AUDIO_DRV_SIMULATION_FU_UNIT_INFO;

typedef struct usbd_audio_buf_info {
    CPU_INT08U  *BufPtr;
    CPU_INT16U   BufLen;
} USBD_AUDIO_BUF_INFO;

typedef  struct  usbd_audio_drv_simulation_circular_buf {
                                                                /* Circular buf contains ptr to audio buf.              */
    USBD_AUDIO_BUF_INFO   BufInfoTbl[USBD_AUDIO_DRV_SIMULATION_CIRCULAR_AUDIO_BUF_SIZE];
    CPU_INT16U            IxIn;                                 /* Ix used by playback or record to store data.         */
    CPU_INT16U            IxOut;                                /* Ix used by playback or record to get   data.         */
} USBD_AUDIO_DRV_SIMULATION_CIRCULAR_BUF;


typedef  struct  usbd_audio_drv_simulation_data {
    USBD_AUDIO_DRV_SIMULATION_FU_UNIT_INFO  MicInfo;            /* Info about the microphone (IN  direction).           */
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    USBD_AUDIO_DRV_SIMULATION_FU_UNIT_INFO  SpeakerInfo;        /* Info about the speaker    (OUT direction).           */
#endif
} USBD_AUDIO_DRV_SIMULATION_DATA;


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

static         USBD_AUDIO_DRV_SIMULATION_DATA               AudioDrvData;
static         USBD_AUDIO_DRV_SIMULATION_CIRCULAR_BUF       CircularBufRecord;

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
static         USBD_AUDIO_DRV_SIMULATION_CIRCULAR_BUF       CircularBufPlayback;
static         USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF  CircularBufLoopback;
#else
extern  const  CPU_INT16U                                   USBD_Audio_DrvSimulationDataNbSamples44_1;
extern  const  CPU_INT16U                                   USBD_Audio_DrvSimulationDataNbSamples48;
extern  const  CPU_INT16U                                   USBD_Audio_DrvSimulationDataTbl44_1[];
extern  const  CPU_INT16U                                   USBD_Audio_DrvSimulationDataTbl48[];
#endif

#if (APP_USBD_AUDIO_DRV_SIM_OS_III_EN == DEF_ENABLED)
static         OS_TCB                                       USBD_Audio_DrvSimulationTaskTCB;
#endif
static         CPU_STK                                      USBD_Audio_DrvSimulationTaskStk[APP_CFG_USBD_AUDIO_DRV_SIMULATION_STK_SIZE];

extern         CPU_INT08U                                   Mic_OT_USB_IN_ID;
extern         CPU_INT08U                                   Mic_FU_ID;
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
extern         CPU_INT08U                                   Speaker_IT_USB_OUT_ID;
extern         CPU_INT08U                                   Speaker_FU_ID;
#endif


/*
*********************************************************************************************************
*                                 AUDIO API DRIVER FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_Audio_DrvInit                     (USBD_AUDIO_DRV        *p_audio_drv,
                                                             USBD_ERR              *p_err);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_MuteManage        (USBD_AUDIO_DRV        *p_audio_drv,
                                                             CPU_INT08U             unit_id,
                                                             CPU_INT08U             log_ch_nbr,
                                                             CPU_BOOLEAN            set_en,
                                                             CPU_BOOLEAN           *p_mute);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_VolManage         (USBD_AUDIO_DRV        *p_audio_drv,
                                                             CPU_INT08U             req,
                                                             CPU_INT08U             unit_id,
                                                             CPU_INT08U             log_ch_nbr,
                                                             CPU_INT16U            *p_vol);

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_Audio_DrvAS_SamplingFreqLoopManage(USBD_AUDIO_DRV        *p_audio_drv,
                                                             CPU_INT08U             terminal_id_link,
                                                             CPU_BOOLEAN            set_en,
                                                             CPU_INT32U            *p_sampling_freq);
#else
static  CPU_BOOLEAN  USBD_Audio_DrvAS_SamplingFreqMicManage (USBD_AUDIO_DRV        *p_audio_drv,
                                                             CPU_INT08U             terminal_id_link,
                                                             CPU_BOOLEAN            set_en,
                                                             CPU_INT32U            *p_sampling_freq);
#endif

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStart              (USBD_AUDIO_DRV        *p_audio_drv,
                                                             USBD_AUDIO_AS_HANDLE   as_handle,
                                                             CPU_INT08U             terminal_id_link);

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStop               (USBD_AUDIO_DRV        *p_audio_drv,
                                                             CPU_INT08U             terminal_id_link);

static  void         USBD_Audio_DrvStreamRecordRx           (USBD_AUDIO_DRV        *p_audio_drv,
                                                             CPU_INT08U             terminal_id_link,
                                                             void                  *p_buf,
                                                             CPU_INT16U            *p_buf_len,
                                                             USBD_ERR              *p_err);

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
static  void         USBD_Audio_DrvStreamPlaybackTx         (USBD_AUDIO_DRV        *p_audio_drv,
                                                             CPU_INT08U             terminal_id_link,
                                                             void                  *p_buf,
                                                             CPU_INT16U             buf_len,
                                                             USBD_ERR              *p_err);
#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
static  void          USBD_Audio_DrvSimulationLoopTask               (void                                    *p_arg);

#else
static  void          USBD_Audio_DrvSimulationMicTask                (void                                    *p_arg);
#endif

static  CPU_BOOLEAN   USBD_Audio_DrvSimulation_CircularBufStreamStore(USBD_AUDIO_DRV_SIMULATION_CIRCULAR_BUF  *p_circular_buf,
                                                                      CPU_INT08U                              *p_buf,
                                                                      CPU_INT16U                               buf_len);

static  CPU_INT08U   *USBD_Audio_DrvSimulation_CircularBufStreamGet  (USBD_AUDIO_DRV_SIMULATION_CIRCULAR_BUF  *p_circular_buf,
                                                                      CPU_INT16U                              *p_buf_len);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)

#if    ((USBD_AUDIO_CFG_PLAYBACK_EN != DEF_ENABLED) || \
        (USBD_AUDIO_CFG_RECORD_EN   != DEF_ENABLED))
#error  "USBD_AUDIO_CFG_PLAYBACK_EN and USBD_AUDIO_CFG_RECORD_EN must be DEF_ENABLED for loopback demo."
#endif

#else

#if (USBD_AUDIO_CFG_RECORD_EN != DEF_ENABLED)
#error  "USBD_AUDIO_CFG_RECORD_EN illegally #define'd in 'usbd_cfg.h' [MUST be DEF_ENABLED]"
#endif
#endif


/*
*********************************************************************************************************
*                                  USB DEVICE CONTROLLER DRIVER API
*********************************************************************************************************
*/

const  USBD_AUDIO_DRV_COMMON_API  USBD_Audio_DrvCommonAPI_Simulation = {
    USBD_Audio_DrvInit
};

const  USBD_AUDIO_DRV_AC_FU_API  USBD_Audio_DrvFU_API_Simulation = {
    USBD_Audio_DrvCtrlFU_MuteManage,
    USBD_Audio_DrvCtrlFU_VolManage,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL,
    DEF_NULL
};

const  USBD_AUDIO_DRV_AS_API  USBD_Audio_DrvAS_API_Simulation = {
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    USBD_Audio_DrvAS_SamplingFreqLoopManage,
#else
    USBD_Audio_DrvAS_SamplingFreqMicManage,
#endif
    DEF_NULL,
    USBD_Audio_DrvStreamStart,
    USBD_Audio_DrvStreamStop,
    USBD_Audio_DrvStreamRecordRx,
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    USBD_Audio_DrvStreamPlaybackTx
#else
    DEF_NULL
#endif
};


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     DRIVER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        USBD_Audio_DrvInit()
*
* Description : Initialize software resources, audio codec and different MCU peripherals needed
*               to communicate with it.
*
* Argument(s) : p_audio_drv    Pointer to audio driver structure.
*
*               p_err          Pointer to variable that will receive the return error code from this function :
*
*                              USBD_ERR_NONE    Audio device     successfully initialized.
*                              USBD_ERR_FAIL    Audio device not successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_Audio_DrvInit (USBD_AUDIO_DRV  *p_audio_drv,
                                  USBD_ERR        *p_err)
{
    OS_ERR  err_os;


    (void)p_audio_drv;

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
                                                                /* Set default values.                                  */
    AudioDrvData.SpeakerInfo.AS_Handle = USBD_AUDIO_DRV_SIMULATION_AS_HANDLE_NONE;
    AudioDrvData.SpeakerInfo.Vol       = USBD_AUDIO_DRV_SIMULATION_VOLUME_MAX;
    AudioDrvData.SpeakerInfo.Mute      = DEF_FALSE;
    AudioDrvData.SpeakerInfo.Gain      = ConvTbl_dB_ToGain[0u];

    AudioDrvData.MicInfo.AS_Handle     = USBD_AUDIO_DRV_SIMULATION_AS_HANDLE_NONE;
    AudioDrvData.MicInfo.Vol           = USBD_AUDIO_DRV_SIMULATION_VOLUME_MAX;
    AudioDrvData.MicInfo.Mute          = DEF_FALSE;
    AudioDrvData.MicInfo.Gain          = ConvTbl_dB_ToGain[0u];

    Mem_Clr((void *)&CircularBufPlayback,
                     sizeof(CircularBufPlayback));

#else
                                                                /* Set default values.                                  */
    AudioDrvData.MicInfo.AS_Handle      =  USBD_AUDIO_DRV_SIMULATION_AS_HANDLE_NONE;
    AudioDrvData.MicInfo.Vol            =  USBD_AUDIO_DRV_SIMULATION_VOLUME_MAX;
    AudioDrvData.MicInfo.Mute           =  DEF_FALSE;
    AudioDrvData.MicInfo.Gain           =  ConvTbl_dB_ToGain[0u];
    AudioDrvData.MicInfo.SamFreq        =  USBD_AUDIO_FMT_TYPE_I_SAMFREQ_44_1KHZ;
    AudioDrvData.MicInfo.BufHeadPtr     =  USBD_Audio_DrvSimulationDataTbl44_1;
    AudioDrvData.MicInfo.BufNbSamples   =  USBD_Audio_DrvSimulationDataNbSamples44_1;
#endif

    Mem_Clr((void *)&CircularBufRecord,
                     sizeof(CircularBufRecord));

                                                                /* Create task.                                         */
#if (APP_USBD_AUDIO_DRV_SIM_OS_III_EN == DEF_ENABLED)
    OSTaskCreate(&USBD_Audio_DrvSimulationTaskTCB,
                 "Audio Driver Simulation",
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
                  USBD_Audio_DrvSimulationLoopTask,
#else
                  USBD_Audio_DrvSimulationMicTask,
#endif
                  DEF_NULL,
                  APP_CFG_USBD_AUDIO_DRV_SIMULATION_PRIO,
                 &USBD_Audio_DrvSimulationTaskStk[0u],
                  APP_CFG_USBD_AUDIO_DRV_SIMULATION_STK_SIZE / 10u,
                  APP_CFG_USBD_AUDIO_DRV_SIMULATION_STK_SIZE,
                  0u,
                  0u,
                  DEF_NULL,
                  OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err_os);
    if (err_os != OS_ERR_NONE) {
       *p_err = USBD_ERR_FAIL;
        APP_TRACE_DBG(("USBD_AudioDrvSimulation: error when creating task w/err = %d.\r\n", err_os));
        return;
    }
#else
#if (OS_STK_GROWTH == 1u)
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    err_os = OSTaskCreateExt(USBD_Audio_DrvSimulationLoopTask,
#else
    err_os = OSTaskCreateExt(USBD_Audio_DrvSimulationMicTask,
#endif
                             DEF_NULL,
                            &USBD_Audio_DrvSimulationTaskStk[APP_CFG_USBD_AUDIO_DRV_SIMULATION_STK_SIZE - 1],
                             APP_CFG_USBD_AUDIO_DRV_SIMULATION_PRIO,
                             APP_CFG_USBD_AUDIO_DRV_SIMULATION_PRIO,
                            &USBD_Audio_DrvSimulationTaskStk[0],
                             APP_CFG_USBD_AUDIO_DRV_SIMULATION_STK_SIZE,
                             DEF_NULL,
                             OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
#else
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    err_os = OSTaskCreateExt(USBD_Audio_DrvSimulationLoopTask,
#else
    err_os = OSTaskCreateExt(USBD_Audio_DrvSimulationMicTask,
#endif
                             DEF_NULL,
                            &USBD_Audio_DrvSimulationTaskStk[0],
                             APP_CFG_USBD_AUDIO_DRV_SIMULATION_PRIO,
                             APP_CFG_USBD_AUDIO_DRV_SIMULATION_PRIO,
                            &USBD_Audio_DrvSimulationTaskStk[APP_CFG_USBD_AUDIO_DRV_SIMULATION_STK_SIZE - 1],
                             APP_CFG_USBD_AUDIO_DRV_SIMULATION_STK_SIZE,
                             DEF_NULL,
                             OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
#endif
    if (err_os != OS_ERR_NONE) {
       *p_err = USBD_ERR_FAIL;
        APP_TRACE_DBG(("USBD_AudioDrvSimulation: error when creating task w/err = %d.\r\n", err_os));
        return;
    }
#endif

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   USBD_Audio_DrvCtrlFU_MuteManage()
*
* Description : Get or set mute state for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv    Pointer to audio driver structure.
*
*               unit_id        Feature Unit ID.
*
*               log_ch_nbr     Logical channel number (see Note #1).
*
*               set_en         Flag indicating to get or set the mute.
*
*               p_mute         Pointer to the mute state to get or set.
*
* Return(s)   : DEF_OK,        if NO error(s) occurred and request is supported.
*
*               DEF_FAIL,      otherwise.
*
* Note(s)     : (1) log_ch_nbr allows you to get the mute state for a specific channel. When log_ch_nbr
*                   is 0, you get the mute state for all channels. Indeed, the logical channel #0
*                   represents the master channel and encompasses all channels.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_MuteManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                      CPU_INT08U       unit_id,
                                                      CPU_INT08U       log_ch_nbr,
                                                      CPU_BOOLEAN      set_en,
                                                      CPU_BOOLEAN     *p_mute)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;
    CPU_SR_ALLOC();


    (void)p_audio_drv;
    (void)log_ch_nbr;

    if (unit_id == Mic_FU_ID) {

        if (set_en == DEF_FALSE) {                              /* Get.                                                 */
            CPU_CRITICAL_ENTER();
           *p_mute = AudioDrvData.MicInfo.Mute;
            CPU_CRITICAL_EXIT();
            req_ok = DEF_OK;
        } else {                                                /* Set.                                                 */
            CPU_CRITICAL_ENTER();
            AudioDrvData.MicInfo.Mute = *p_mute;
            CPU_CRITICAL_EXIT();
            req_ok = DEF_OK;
        }

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    } else if (unit_id == Speaker_FU_ID) {

        if (set_en == DEF_FALSE) {                              /* Get.                                                 */
            CPU_CRITICAL_ENTER();
           *p_mute = AudioDrvData.SpeakerInfo.Mute;
            CPU_CRITICAL_EXIT();
            req_ok = DEF_OK;
        } else {                                                /* Set.                                                 */
            CPU_CRITICAL_ENTER();
            AudioDrvData.SpeakerInfo.Mute = *p_mute;
            CPU_CRITICAL_EXIT();
            req_ok = DEF_OK;
        }
#endif
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                  USBD_Audio_DrvCtrlFU_VolManage()
*
* Description : Get or set volume for one or several logical channels inside a cluster.
*
* Argument(s) : p_audio_drv    Pointer to audio driver structure.
*
*               req            Volume request:
*
*                              USBD_AUDIO_REQ_GET_CUR
*                              USBD_AUDIO_REQ_GET_RES
*                              USBD_AUDIO_REQ_GET_MIN
*                              USBD_AUDIO_REQ_GET_MAX
*                              USBD_AUDIO_REQ_SET_CUR
*
*               unit_id        Feature Unit ID.
*
*               log_ch_nbr     Logical channel number.
*
*               p_vol          Pointer to the volume value to set or get.
*
* Return(s)   : DEF_OK,        if NO error(s) occurred and request is supported.
*
*               DEF_FAIL,      otherwise.
*
* Note(s)     : (1) The Volume Control values range allowed for Feature Unit is:
*
*                   (a) From +127.9961 dB (0x7FFF) down to -127.9961 dB (0x8001) for CUR, MIN, and MAX
*                       attributes.
*
*                   (b) From 1/256 dB (0x0001) to +127.9961 dB (0x7FFF) for RES attribute.
*
*               (2) For details about volume conversion (from dB to linear gain), please refer to notes
*                   about the look-up table, near the beginning of this file.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_VolManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                     CPU_INT08U       req,
                                                     CPU_INT08U       unit_id,
                                                     CPU_INT08U       log_ch_nbr,
                                                     CPU_INT16U      *p_vol)
{
    CPU_BOOLEAN                              req_ok = DEF_FAIL;
    CPU_INT16U                               calc_val;
    CPU_INT16U                               conv_tbl_ix;
    CPU_INT08U                               nb_div;
    CPU_INT08U                               ix;
    USBD_AUDIO_DRV_SIMULATION_FU_UNIT_INFO  *p_fu_unit_info;
    CPU_SR_ALLOC();


    (void)p_audio_drv;
    (void)log_ch_nbr;

    switch (req) {
        case USBD_AUDIO_REQ_GET_CUR:
             if (unit_id == Mic_FU_ID){                         /* Verify unit ID.                                      */
                 CPU_CRITICAL_ENTER();
                *p_vol = AudioDrvData.MicInfo.Vol;
                 CPU_CRITICAL_EXIT();
                 req_ok = DEF_OK;
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
             } else if (unit_id == Speaker_FU_ID){              /* Verify unit ID.                                      */
                 CPU_CRITICAL_ENTER();
                *p_vol = AudioDrvData.SpeakerInfo.Vol;
                 CPU_CRITICAL_EXIT();
                 req_ok = DEF_OK;
#endif
             }
             break;


        case USBD_AUDIO_REQ_GET_MIN:
            *p_vol  = USBD_AUDIO_DRV_SIMULATION_VOLUME_MUTE;
             req_ok = DEF_OK;
             break;


        case USBD_AUDIO_REQ_GET_MAX:
            *p_vol  = USBD_AUDIO_DRV_SIMULATION_VOLUME_MAX;
             req_ok = DEF_OK;
             break;


        case USBD_AUDIO_REQ_GET_RES:
            *p_vol  = 0x0001u;                                  /* Resolution of 1/256 dB (see Note #1b).               */
             req_ok = DEF_OK;
             break;


        case USBD_AUDIO_REQ_SET_CUR:
             if (unit_id == Mic_FU_ID){                         /* Verify unit ID.                                      */
                 p_fu_unit_info = &AudioDrvData.MicInfo;
#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
             } else if (unit_id == Speaker_FU_ID) {             /* Verify unit ID.                                      */
                 p_fu_unit_info = &AudioDrvData.SpeakerInfo;
#endif
             } else {
                 break;
             }

                                                                /* Convert dB to linear gain (see Note #2.)             */
             calc_val    = ((*p_vol + 0x8000u) & 0xFFFFu);      /* Convert volume value to positive value [0..65534].   */
                                                                /* Flip values so that 0 corresponds to highest ...     */
             calc_val    =    0xFFFFu - calc_val;               /* ... possible gain and 0xFFFF corresponds to lowest.  */

             calc_val    =    calc_val >> 4u;                   /* Divide by 16 to reduce look-up tbl size.             */
                                                                /* Get the nb of times to divide by 10 (see Note #2.)   */
             nb_div      =    calc_val / USBD_AUDIO_DRV_SIMULATION_CONV_TBL_SIZE;
                                                                /* Get ix of the look-up tbl for linear gain value ...  */
                                                                /* ... corresponding to value of gain in dB from host.  */
             conv_tbl_ix =    calc_val % USBD_AUDIO_DRV_SIMULATION_CONV_TBL_SIZE;

             calc_val    =    ConvTbl_dB_ToGain[conv_tbl_ix];   /* Get linear gain value from look-up tbl.              */

             ix          = 0u;
             while ((ix       <  nb_div) &&                     /* Divide by 10 once for each 20dB difference.          */
                    (calc_val != 0u)) {                         /* Exit loop early if gain is equal to 0.               */
                 calc_val = calc_val / 10u;
                 ix++;
             }

             CPU_CRITICAL_ENTER();
             p_fu_unit_info->Vol  = *p_vol;                     /* Set volume to actual volume val received from host.  */
             p_fu_unit_info->Gain =  calc_val;                  /* Set gain to linear gain calculated.                  */
             CPU_CRITICAL_EXIT();

             req_ok = DEF_OK;
             break;


        default:
             break;
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                               USBD_Audio_DrvAS_SamplingFreqLoopManage()
*
* Description : Get or set current sampling frequency for a particular Terminal (i.e. endpoint).
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    AudioStreaming terminal link.
*
*               set_en              Flag indicating to get or set the mute.
*
*               p_sampling_freq     Pointer to the sampling frequency value to get or set.
*
* Return(s)   : DEF_OK,             if NO error(s) occurred and request is supported.
*
*               DEF_FAIL,           otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_Audio_DrvAS_SamplingFreqLoopManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                              CPU_INT08U       terminal_id_link,
                                                              CPU_BOOLEAN      set_en,
                                                              CPU_INT32U      *p_sampling_freq)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;


    (void)p_audio_drv;
    (void)terminal_id_link;
                                                                /* ----------------------- GET ------------------------ */
    if (set_en == DEF_FALSE) {
       *p_sampling_freq = USBD_AUDIO_FMT_TYPE_I_SAMFREQ_48KHZ;
        req_ok          = DEF_OK;
                                                                /* ----------------------- SET ------------------------ */
    } else {
        if (*p_sampling_freq == USBD_AUDIO_FMT_TYPE_I_SAMFREQ_48KHZ) {
            req_ok = DEF_OK;
        }
    }

    return (req_ok);
}
#endif


/*
*********************************************************************************************************
*                               USBD_Audio_DrvAS_SamplingFreqMicManage()
*
* Description : Get or set current sampling frequency for a particular Terminal (i.e. endpoint).
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    AudioStreaming terminal link.
*
*               set_en              Flag indicating to get or set the mute.
*
*               p_sampling_freq     Pointer to the sampling frequency value to get or set.
*
* Return(s)   : DEF_OK,             if NO error(s) occurred and request is supported.
*
*               DEF_FAIL,           otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_DISABLED)
static  CPU_BOOLEAN  USBD_Audio_DrvAS_SamplingFreqMicManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                             CPU_INT08U       terminal_id_link,
                                                             CPU_BOOLEAN      set_en,
                                                             CPU_INT32U      *p_sampling_freq)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;
    CPU_SR_ALLOC();


    (void)p_audio_drv;
    (void)terminal_id_link;
                                                                /* ----------------------- GET ------------------------ */
    if (set_en == DEF_FALSE) {
        CPU_CRITICAL_ENTER();
       *p_sampling_freq = AudioDrvData.MicInfo.SamFreq;
        CPU_CRITICAL_EXIT();
        req_ok          = DEF_OK;
                                                                /* ----------------------- SET ------------------------ */
    } else {

        if ((*p_sampling_freq == USBD_AUDIO_FMT_TYPE_I_SAMFREQ_44_1KHZ) ||
            (*p_sampling_freq == USBD_AUDIO_FMT_TYPE_I_SAMFREQ_48KHZ)) {

            CPU_CRITICAL_ENTER();
            AudioDrvData.MicInfo.SamFreq = *p_sampling_freq;
            if (*p_sampling_freq == USBD_AUDIO_FMT_TYPE_I_SAMFREQ_44_1KHZ) {
                AudioDrvData.MicInfo.BufHeadPtr   = USBD_Audio_DrvSimulationDataTbl44_1;
                AudioDrvData.MicInfo.BufNbSamples = USBD_Audio_DrvSimulationDataNbSamples44_1;
            } else {
                AudioDrvData.MicInfo.BufHeadPtr   = USBD_Audio_DrvSimulationDataTbl48;
                AudioDrvData.MicInfo.BufNbSamples = USBD_Audio_DrvSimulationDataNbSamples48;
            }
            CPU_CRITICAL_EXIT();

            req_ok = DEF_OK;
        }
    }

    return (req_ok);
}
#endif


/*
*********************************************************************************************************
*                                      USBD_Audio_DrvStreamStart()
*
* Description : Start record or playback stream.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               as_handle           AudioStreaming handle.
*
*               terminal_id_link    AudioStreaming terminal link.
*
* Return(s)   : DEF_OK,             if NO error(s) occurred.
*
*               DEF_FAIL,           otherwise.
*
* Note(s)     : (1) A general purpose OS (Windows, Mac OS, Linux) opens a record stream by sending a
*                   SET_INTERFACE request to select the operational interface of an AudioStreaming
*                   interface. The SET_INTERFACE request is always followed by a SET_CUR request to
*                   configure the sampling frequency. The first record DMA transfer must be started ONLY
*                   when the sampling frequency has been configured. The Audio class ensures this
*                   sequence.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStart (USBD_AUDIO_DRV        *p_audio_drv,
                                                USBD_AUDIO_AS_HANDLE   as_handle,
                                                CPU_INT08U             terminal_id_link)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;
    CPU_SR_ALLOC();


    (void)p_audio_drv;

    if (terminal_id_link == Mic_OT_USB_IN_ID) {
        CPU_CRITICAL_ENTER();
        AudioDrvData.MicInfo.AS_Handle = as_handle;
        CPU_CRITICAL_EXIT();
        req_ok = DEF_OK;

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    } else if (terminal_id_link == Speaker_IT_USB_OUT_ID) {
        CPU_INT08U  i;


        CPU_CRITICAL_ENTER();
        AudioDrvData.SpeakerInfo.AS_Handle = as_handle;
        CPU_CRITICAL_EXIT();

        for (i = 0u; i < USBD_AUDIO_DRV_SIMULATION_LOOP_TRESHOLD_RECORD; i++) {
            USBD_Audio_PlaybackTxCmpl(as_handle);                 /* Signal playback task that rdy to consume a buf.      */
        }
        req_ok = DEF_OK;
#endif
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                      USBD_Audio_DrvStreamStop()
*
* Description : Stop record or playback stream.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    AudioStreaming terminal link.
*
* Return(s)   : DEF_OK,             if NO error(s) occurred.
*
*               DEF_FAIL,           otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStop (USBD_AUDIO_DRV  *p_audio_drv,
                                               CPU_INT08U       terminal_id_link)
{
    CPU_BOOLEAN  req_ok = DEF_FAIL;
    CPU_SR_ALLOC();


    (void)p_audio_drv;

    if (terminal_id_link == Mic_OT_USB_IN_ID) {
        CPU_CRITICAL_ENTER();
                                                                /* Clr circular buf for next stream opening.            */
        Mem_Clr((void *)&CircularBufRecord,
                         sizeof(CircularBufRecord));

        AudioDrvData.MicInfo.AS_Handle = USBD_AUDIO_DRV_SIMULATION_AS_HANDLE_NONE;
        CPU_CRITICAL_EXIT();
        req_ok = DEF_OK;

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
    } else if (terminal_id_link == Speaker_IT_USB_OUT_ID) {
        CPU_CRITICAL_ENTER();
                                                                /* Clr circular buf for next stream opening.            */
        Mem_Clr((void *)&CircularBufPlayback,
                         sizeof(CircularBufPlayback));

        AudioDrvData.SpeakerInfo.AS_Handle = USBD_AUDIO_DRV_SIMULATION_AS_HANDLE_NONE;
        CPU_CRITICAL_EXIT();

        req_ok = DEF_OK;
#endif
    }

    return (req_ok);
}


/*
*********************************************************************************************************
*                                    USBD_Audio_DrvStreamRecordRx()
*
* Description : Get a ready record buffer from codec.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    Terminal ID associated to this stream.
*
*               p_buf               Pointer to record buffer.
*
*               p_buf_len           Pointer to buffer length in bytes.
*
*               p_err               Pointer to the variable that will receive the return error code from this function:
*
*                                   USBD_ERR_NONE    Buffer successfully retrieved.
*                                   USBD_ERR_RX      Generic Rx error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_Audio_DrvStreamRecordRx (USBD_AUDIO_DRV  *p_audio_drv,
                                            CPU_INT08U       terminal_id_link,
                                            void            *p_buf,
                                            CPU_INT16U      *p_buf_len,
                                            USBD_ERR        *p_err)
{
    CPU_INT08U  *p_buf_local;


    (void)p_audio_drv;
    (void)p_buf;

    if (terminal_id_link != Mic_OT_USB_IN_ID) {
       *p_err = USBD_ERR_RX;
        return;
    }
                                                                /* Get ready record buf.                                */
    p_buf_local = USBD_Audio_DrvSimulation_CircularBufStreamGet(&CircularBufRecord, p_buf_len);
    if (p_buf_local == DEF_NULL) {
       *p_err = USBD_ERR_RX;
        return;
    }

   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   USBD_Audio_DrvStreamPlaybackTx()
*
* Description : Provide a ready playback buffer to codec.
*
* Argument(s) : p_audio_drv         Pointer to audio driver structure.
*
*               terminal_id_link    Terminal ID associated to this stream.
*
*               p_buf               Pointer to ready playback buffer.
*
*               buf_len             Buffer length in bytes.
*
*               p_err               Pointer to the variable that will receive the return error code from this function:
*
*                                   USBD_ERR_NONE    Buffer successfully retrieved.
*                                   USBD_ERR_TX      Generic Tx error.
*
* Return(s)   : DEF_OK,             if NO error(s) occurred.
*
*               DEF_FAIL,           otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
static  void  USBD_Audio_DrvStreamPlaybackTx (USBD_AUDIO_DRV  *p_audio_drv,
                                              CPU_INT08U       terminal_id_link,
                                              void            *p_buf,
                                              CPU_INT16U       buf_len,
                                              USBD_ERR        *p_err)
{
    CPU_BOOLEAN  result;


    (void)p_audio_drv;

    if (terminal_id_link != Speaker_IT_USB_OUT_ID) {
       *p_err = USBD_ERR_TX;
        return;
    }
                                                                /* Store ready playback buf.                            */
    result = USBD_Audio_DrvSimulation_CircularBufStreamStore(             &CircularBufPlayback,
                                                             (CPU_INT08U *)p_buf,
                                                                           buf_len);
    if (result != DEF_OK) {                                     /* Buffer full.                                         */
        *p_err = USBD_ERR_TX;
         return;
    }

   *p_err = USBD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                  USBD_Audio_DrvSimulationLoopTask()
*
* Description : Task emulating an audio codec driver used as a loopback device. Records audio stream
*               received from the host and sends it back.
*
* Argument(s) : p_arg    Pointer to task initialization argument.
*
* Return(s)   : none.
*
* Note(s)     : (1) For details about volume conversion (from dB to linear gain), please refer to notes
*                   about the look-up table in the section VOLUME CONVERSION TABLE near the beginning of
*                   this file.
*
*               (2) If there are still enough samples in the loopback circular buffer when the playback
*                   stream is closed, this condition allows the record processing code to process them.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_ENABLED)
static  void  USBD_Audio_DrvSimulationLoopTask(void  *p_arg)
{
    CPU_INT16S             *p_circ_buf           = &CircularBufLoopback.Buf[0u];
    CPU_INT16S             *p_buf_xfer;
    CPU_INT32U              ix;
    CPU_INT32S              calc_val;
    CPU_INT32U              mic_fu_gain;
    USBD_AUDIO_AS_HANDLE    mic_as_handle;
    CPU_INT16U              mic_fu_vol;
    CPU_BOOLEAN             mic_fu_mute;
    USBD_AUDIO_AS_HANDLE    speaker_as_handle;
    CPU_INT16U              speaker_fu_vol;
    CPU_BOOLEAN             speaker_fu_mute;
    CPU_INT32U              speaker_fu_gain;
    CPU_BOOLEAN             speaker_priming_done = DEF_NO;
    CPU_INT16U              speaker_nbr_xfers    = 0u;
    CPU_INT16U              buf_len;
    CPU_INT16U              tbl_len;
    CPU_BOOLEAN             must_dly;
    CPU_BOOLEAN             result;
    CPU_INT32U              avail_space;
#if (APP_USBD_AUDIO_DRV_SIM_OS_III_EN == DEF_ENABLED)
    OS_ERR                  err_os;
#endif
    CPU_SR_ALLOC();


    (void)p_arg;

    CircularBufLoopback.IxIn     =  0u;
    CircularBufLoopback.IxOut    =  0u;
    CircularBufLoopback.NbrXfers =  0u;

    while (DEF_TRUE) {                                          /* Task body.                                           */
                                                                /* -------- COPY PROTECTED VARS TO LOCAL VARS --------- */
        CPU_CRITICAL_ENTER();
        speaker_as_handle = AudioDrvData.SpeakerInfo.AS_Handle;
        speaker_fu_vol    = AudioDrvData.SpeakerInfo.Vol;
        speaker_fu_mute   = AudioDrvData.SpeakerInfo.Mute;
        speaker_fu_gain   = AudioDrvData.SpeakerInfo.Gain;

        mic_as_handle = AudioDrvData.MicInfo.AS_Handle;
        mic_fu_vol    = AudioDrvData.MicInfo.Vol;
        mic_fu_mute   = AudioDrvData.MicInfo.Mute;
        mic_fu_gain   = AudioDrvData.MicInfo.Gain;
        CPU_CRITICAL_EXIT();

        must_dly = DEF_TRUE;
                                                                /* --------------- PLAYBACK PROCESSING ---------------- */
                                                                /* Make sure streaming interface is open.               */
        if (speaker_as_handle != USBD_AUDIO_DRV_SIMULATION_AS_HANDLE_NONE) {

            avail_space = USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF_LEN - CircularBufLoopback.BufCurCnt;
                                                                /* Make sure circular buf can contain another xfer.     */
            if (avail_space >= USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SAMPLES) {
                                                                /* Get an audio buf.                                    */
                p_buf_xfer = (CPU_INT16S *)USBD_Audio_DrvSimulation_CircularBufStreamGet(&CircularBufPlayback, &buf_len);

                if (p_buf_xfer != DEF_NULL) {
                    must_dly = DEF_FALSE;                       /* Operation done, no need to delay.                    */
                    tbl_len  = buf_len / 2u;                    /* Convert len for 1-byte buf to 2-bytes buf.           */

                    if ((speaker_fu_vol  == USBD_AUDIO_DRV_SIMULATION_VOLUME_MUTE) ||
                        (speaker_fu_gain == 0u)                                    ||
                        (speaker_fu_mute == DEF_TRUE)) {

                        for (ix = 0u; ix < tbl_len; ++ix) {
                            p_circ_buf[CircularBufLoopback.IxIn] = 0;
                            CircularBufLoopback.IxIn             = (CircularBufLoopback.IxIn + 1u) % USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF_LEN;
                        }
                    } else {
                        for (ix = 0u; ix < tbl_len; ++ix) {     /* Fill audio buf with specified sample.                */
                                                                /* Apply linear gain on each sample (see Note #1).      */
                            calc_val                             = (CPU_INT32S)p_buf_xfer[ix];
                                                                /* Multiply by linear gain value (from look-up tbl).    */
                            calc_val                             =  calc_val * speaker_fu_gain;
                                                                /* Bit-shift to obtain correct value.                   */
                            calc_val                             =  calc_val >> USBD_AUDIO_DRV_SIMULATION_CONV_TBL_GAIN_BIT_SHIFT;
                                                                /* Save processed sample in circular buf.               */
                            p_circ_buf[CircularBufLoopback.IxIn] = (CPU_INT16S)calc_val;
                                                                /* Adjust circular buf ix.                              */
                            CircularBufLoopback.IxIn             = (CircularBufLoopback.IxIn + 1u) % USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF_LEN;
                        }
                    }

                    CircularBufLoopback.BufCurCnt += tbl_len;   /* Track cur nbr of samples in circular buf.            */

                    speaker_nbr_xfers++;                        /* Adjust nbr of audio xfers present in circular buf.   */
                    if ((speaker_nbr_xfers    == USBD_AUDIO_DRV_SIMULATION_LOOP_TRESHOLD_RECORD) &&
                        (speaker_priming_done == DEF_NO)) {
                        speaker_priming_done = DEF_YES;
                    }
                                                                /* Free processed audio buf for subsequent xfers.       */
                    USBD_Audio_PlaybackBufFree(speaker_as_handle,
                                               DEF_NULL);
                                                                /* Signal playback task that rdy to consume a new buf.  */
                    USBD_Audio_PlaybackTxCmpl(speaker_as_handle);
                }
            }
                                                                /* See Note #2.                                         */
        } else if (CircularBufLoopback.BufCurCnt < USBD_AUDIO_DRV_SIMULATION_DATA_XFER_SAMPLES) {
            speaker_priming_done = DEF_NO;
            speaker_nbr_xfers    = 0u;
        }
                                                                /* ---------------- RECORD PROCESSING ----------------- */
                                                                /* Make sure streaming interface is open.               */
        if ((mic_as_handle        != USBD_AUDIO_DRV_SIMULATION_AS_HANDLE_NONE) &&
            (speaker_priming_done == DEF_YES)) {
                                                                /* Make sure there are enough samples to read.          */
            if (CircularBufLoopback.BufCurCnt >= USBD_AUDIO_DRV_SIMULATION_RECORD_DATA_XFER_SAMPLES) {
                                                                /* Get an empty buf.                                    */
                p_buf_xfer = (CPU_INT16S *)USBD_Audio_RecordBufGet(mic_as_handle,
                                                                  &buf_len);
                if (p_buf_xfer != DEF_NULL) {
                    must_dly = DEF_FALSE;                       /* Operation done, no need to delay.                    */
                    tbl_len  = buf_len / 2u;                    /* Convert len for 1-byte buf to 2-bytes buf.           */

                    if ((mic_fu_vol  == USBD_AUDIO_DRV_SIMULATION_VOLUME_MUTE) ||
                        (mic_fu_gain == 0u)                                    ||
                        (mic_fu_mute == DEF_TRUE)) {

                        Mem_Clr((void *)&p_buf_xfer[0u],
                                         buf_len);
                        CircularBufLoopback.IxOut = (CircularBufLoopback.IxOut + (tbl_len)) % USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF_LEN;

                    } else {
                        for (ix = 0u; ix < tbl_len; ix++) {
                                                                /* Apply linear gain on each sample (see Note #1).      */
                            calc_val                  = (CPU_INT32S)p_circ_buf[CircularBufLoopback.IxOut];
                                                                /* Multiply by linear gain value (from look-up tbl).    */
                            calc_val                  =  calc_val * mic_fu_gain;
                                                                /* Bit-shift to obtain correct value.                   */
                            calc_val                  =  calc_val >> USBD_AUDIO_DRV_SIMULATION_CONV_TBL_GAIN_BIT_SHIFT;
                                                                /* Put processed value in xfer buf.                     */
                            p_buf_xfer[ix]            = (CPU_INT16S)calc_val;
                                                                /* Adjust circular buf ix.                              */
                            CircularBufLoopback.IxOut = (CircularBufLoopback.IxOut + 1u) % USBD_AUDIO_DRV_SIMULATION_LOOP_CIRCULAR_BUF_LEN;
                        }
                    }

                    CircularBufLoopback.BufCurCnt -= tbl_len;   /* Track cur nbr of samples in circular buf.            */

                                                                /* Store record buf. Audio class retrieves it later.    */
                    result = USBD_Audio_DrvSimulation_CircularBufStreamStore(             &CircularBufRecord,
                                                                             (CPU_INT08U *)p_buf_xfer,
                                                                                           buf_len);
                    if (result == DEF_OK) {
                        USBD_Audio_RecordRxCmpl(mic_as_handle); /* Signal audio class buf is ready.                     */
                    } else {                                    /* Buffer full. Record buf lost                         */
                        must_dly = DEF_TRUE;
                    }
                }
            }
        }
                                                                /* -------- DLY TO SIMULATE CODEC FUNCTIONING --------- */
        if (must_dly == DEF_TRUE) {                             /* No operations could be done, dly task.               */
#if (APP_USBD_AUDIO_DRV_SIM_OS_III_EN == DEF_ENABLED)
            OSTimeDlyHMSM(0u, 0u, 0u, 1u,
                          OS_OPT_TIME_HMSM_NON_STRICT,
                         &err_os);
            (void)err_os;
#else
            (void)OSTimeDlyHMSM(0u, 0u, 0u, 1u);
#endif
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                  USBD_Audio_DrvSimulationMicTask()
*
* Description : Task emulating an audio codec driver used as a microphone. Copies hard-coded data and
*               sends it to the host, through the audio class.
*
* Argument(s) : p_arg    Pointer to task initialization argument.
*
* Return(s)   : none.
*
* Note(s)     : (1) For details about volume conversion (from dB to linear gain), please refer to notes
*                   about the look-up table, near the beginning of this file.
*********************************************************************************************************
*/

#if (APP_CFG_USBD_AUDIO_SIMULATION_LOOP_EN == DEF_DISABLED)
static  void  USBD_Audio_DrvSimulationMicTask (void  *p_arg)
{
           CPU_INT16U             *p_buf;
    const  CPU_INT16U             *p_data_buf;
           CPU_INT32U              nb_data_samples;
           CPU_INT32U              mic_buf_tbl_ix;
           CPU_INT32U              ix;
           CPU_INT32S              calc_val;
           USBD_AUDIO_AS_HANDLE    as_handle;
           CPU_INT16U              fu_vol;
           CPU_BOOLEAN             fu_mute;
           CPU_INT32U              fu_gain;
           CPU_INT16U              buf_len;
           CPU_INT16U              tbl_len;
           CPU_BOOLEAN             must_dly;
           CPU_BOOLEAN             result;
#if (APP_USBD_AUDIO_DRV_SIM_OS_III_EN == DEF_ENABLED)
           OS_ERR                  err_os;
#endif
           CPU_SR_ALLOC();


   (void)p_arg;

    mic_buf_tbl_ix = 0u;
    while (DEF_TRUE) {                                          /* Task body.                                           */

        CPU_CRITICAL_ENTER();
        as_handle       = AudioDrvData.MicInfo.AS_Handle;       /* Copy protected vars to local vars.                   */
        fu_vol          = AudioDrvData.MicInfo.Vol;
        fu_mute         = AudioDrvData.MicInfo.Mute;
        fu_gain         = AudioDrvData.MicInfo.Gain;
        p_data_buf      = AudioDrvData.MicInfo.BufHeadPtr;
        nb_data_samples = AudioDrvData.MicInfo.BufNbSamples;
        CPU_CRITICAL_EXIT();

        must_dly = DEF_TRUE;
                                                             /* Make sure streaming interface is open.               */
        if (as_handle != USBD_AUDIO_DRV_SIMULATION_AS_HANDLE_NONE) {
                                                             /* Get an empty buf.                                    */
            p_buf = (CPU_INT16U *)USBD_Audio_RecordBufGet(as_handle, &buf_len);
            if (p_buf != DEF_NULL) {

                must_dly = DEF_FALSE;

                tbl_len = buf_len / (USBD_AUDIO_DRV_SIMULATION_DATA_CFG_BIT_RES / 8u);

                if ((fu_vol  == USBD_AUDIO_DRV_SIMULATION_VOLUME_MUTE) ||
                    (fu_gain == 0u)                                    ||
                    (fu_mute == DEF_TRUE)) {

                    fu_mute = DEF_TRUE;                         /* Mute the mic if vol is too low or gain equal 0.      */
                    Mem_Clr((void *)&p_buf[0u],
                                  buf_len);
                } else {
                    for (ix = 0u; ix < tbl_len; ++ix) {         /* Fill audio buf with specified sample.                */
                                                                /* Apply linear gain on each sample (see Note #1).      */
                        calc_val       = (CPU_INT32S)p_data_buf[mic_buf_tbl_ix];
                        calc_val       =  calc_val * fu_gain;   /* Multiply by linear gain value (from look-up tbl).    */
                                                                /* Bit-shift to obtain correct value.                   */
                        calc_val       =  calc_val >> USBD_AUDIO_DRV_SIMULATION_CONV_TBL_GAIN_BIT_SHIFT;
                        p_buf[ix]      = (CPU_INT16U)(calc_val & 0xFFFFu);
                                                                /* Adjust buf ix.                                       */
                        mic_buf_tbl_ix = (mic_buf_tbl_ix + 1u) % nb_data_samples;
                    }
                }
                                                               /* Store record buf. Audio class retrieves it later.    */
                result = USBD_Audio_DrvSimulation_CircularBufStreamStore(             &CircularBufRecord,
                                                                         (CPU_INT08U *)p_buf,
                                                                                       buf_len);
                if (result == DEF_OK) {
                    USBD_Audio_RecordRxCmpl(as_handle);         /* Signal audio class buf is ready.                     */
                } else {                                        /* Buffer full. Record buf lost                         */
                    must_dly = DEF_TRUE;
                }
            }
        }
                                                             /* Wait for streaming interface to be opened or...      */
                                                             /* ...Wait for buf to become available.                 */
        if (must_dly == DEF_TRUE) {
#if (APP_USBD_AUDIO_DRV_SIM_OS_III_EN == DEF_ENABLED)
            OSTimeDlyHMSM(0u, 0u, 0u, 1u,
                          OS_OPT_TIME_HMSM_NON_STRICT,
                         &err_os);
#else
            (void)OSTimeDlyHMSM(0u, 0u, 0u, 1u);
#endif
        }
    }
}
#endif


/*
*********************************************************************************************************
*                           USBD_Audio_DrvSimulation_CircularBufStreamStore()
*
* Description : Store buffer information in the stream circular buffer.
*
* Argument(s) : p_circular_buf    Pointer to stream circular buffer.
*
*               p_buf             Pointer to buffer.
*
*               buf_len           Buffer length in bytes.
*
* Return(s)   : DEF_OK,           if NO error(s).
*
*               DEF_FAIL,         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvSimulation_CircularBufStreamStore (USBD_AUDIO_DRV_SIMULATION_CIRCULAR_BUF  *p_circular_buf,
                                                                      CPU_INT08U                              *p_buf,
                                                                      CPU_INT16U                               buf_len)
{
    CPU_INT16U  nxt_ix;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    nxt_ix = (p_circular_buf->IxIn + 1u) % USBD_AUDIO_DRV_SIMULATION_CIRCULAR_AUDIO_BUF_SIZE;;
    if (nxt_ix == p_circular_buf->IxOut) {                      /* check if buffer full.                                */
        CPU_CRITICAL_EXIT();
        return (DEF_FAIL);
    }

    p_circular_buf->BufInfoTbl[p_circular_buf->IxIn].BufPtr = p_buf;
    p_circular_buf->BufInfoTbl[p_circular_buf->IxIn].BufLen = buf_len;
    p_circular_buf->IxIn                                    = nxt_ix;
    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                            USBD_Audio_DrvSimulation_CircularBufStreamGet()
*
* Description : Get buffer information from the stream circular buffer.
*
* Argument(s) : p_circular_buf        Pointer to stream circular buffer.
*
*               p_buf_len             Pointer to variable that will receive buffer length.
*
* Return(s)   : Pointer to buffer,    if NO error(s).
*
*               NULL pointer,         otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  *USBD_Audio_DrvSimulation_CircularBufStreamGet (USBD_AUDIO_DRV_SIMULATION_CIRCULAR_BUF  *p_circular_buf,
                                                                    CPU_INT16U                              *p_buf_len)
{
    CPU_INT08U  *p_buf;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    if (p_circular_buf->IxOut == p_circular_buf->IxIn) {        /* check if buffer empty.                               */
        CPU_CRITICAL_EXIT();
        return (DEF_NULL);
    }

    p_buf                 =  p_circular_buf->BufInfoTbl[p_circular_buf->IxOut].BufPtr;
   *p_buf_len             =  p_circular_buf->BufInfoTbl[p_circular_buf->IxOut].BufLen;
    p_circular_buf->IxOut = (p_circular_buf->IxOut + 1u) % USBD_AUDIO_DRV_SIMULATION_CIRCULAR_AUDIO_BUF_SIZE;
    CPU_CRITICAL_EXIT();

    return (p_buf);
}

#endif
