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
*                                          USB DEVICE DRIVER
*
*                                              TEMPLATE
*
* Filename : usbd_audio_drv_uda1380_codec.c
* Version  : V4.06.01
*********************************************************************************************************
* Note(s)       : (1) The relevant documentation for this driver is:
*
*                     (a) "UDA1380 Stereo audio coder-decoder for MD, CD and MP3, 2004 Apr 22" that can be
*                         found at www.nxp.com/documents/data_sheet/UDA1380.pdf
*
*                 (2) This codec driver relies on the Common Driver Library (CDL) software package. This
*                      software package provides a generic set of drivers for LPC3131 MCU peripherals.
*                      Among the drivers, there is support for the I2S peripheral attached to the NXP
*                      UDA380 audio codec on the LPC3131 Embedded Artists board. This driver uses the
*                      following drivers from CDL:
*
*                      CGU driver
*                      I2C driver
*                      DMA controller driver
*                      NXP UDA1380 driver (using I2S driver)
*
*                      The CDL is available at http://www.lpclinux.com/LPC313x/LPC313xCDL. Micrium
*                      does NOT provide support for the use of the CDL software package. Refer directly
*                      to the CDL documentation.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../usbd_audio.h"
#include  "usbd_audio_drv_uda1380_codec.h"
#include  <csp.h>

#include  <lpc313x_cgu_driver.h>
#include  <lpc313x_dma_driver.h>
#include  <uda1380_codec_driver.h>
#include  <lpc313x_mstr_i2c_driver.h>
#include  <lpc313x_i2s_clock_driver.h>
#include  <lpc_arm922t_cp15_driver.h>
#include  <lpc313x_i2s_driver.h>
#include  <usbd_audio_dev_cfg.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s):  (1) See 'UM10314, LPC3130/31 User manual, Rev. 2, 23 May 2012', 'Table 166. SOFT_INT'
*               for more details about DMA request sources
*********************************************************************************************************
*/

#if (AUDIO_FUNCTION == AUDIO_SPEAKER)
#define  USBD_AUDIO_DRV_CIRCULAR_AUDIO_BUF_SIZE          USBD_AUDIO_DEV_CFG_PLAYBACK_NBR_BUF
#elif (AUDIO_FUNCTION == AUDIO_MICROPHONE)
#define  USBD_AUDIO_DRV_CIRCULAR_AUDIO_BUF_SIZE          USBD_AUDIO_DEV_CFG_RECORD_NBR_BUF
#elif (AUDIO_FUNCTION == AUDIO_HEADSET)
#define  USBD_AUDIO_DRV_CIRCULAR_AUDIO_BUF_SIZE          DEF_MAX(USBD_AUDIO_DEV_CFG_RECORD_NBR_BUF, \
                                                                 USBD_AUDIO_DEV_CFG_PLAYBACK_NBR_BUF)
#endif

#define  USBD_AUDIO_DRV_PLAYBACK_BUF_QUEUE_TRESHOLD      2u
                                                                /* --------------------- UDA1380 ---------------------- */
#define  UDA1380_MASTER_VOL_CTRL_MIN_dB             0xC200u
#define  UDA1380_MASTER_VOL_CTRL_MAX_dB             0x0000u
#define  UDA1380_MASTER_VOL_CTRL_RES_dB             0x0040u
#define  UDA1380_MASTER_VOL_CTRL_SILENCE               252u

#define  UDA1380_DECIMATOR_VOL_CTRL_MIN_dB          0xC080u
#define  UDA1380_DECIMATOR_VOL_CTRL_MAX_dB          0x1800u
#define  UDA1380_DECIMATOR_VOL_CTRL_RES_dB          0x0080u
#define  UDA1380_DECIMATOR_VOL_CTRL_SILENCE            128u

#define  UDA1380_MAX_NBR_SAMPLE_RATE_SUPPORTED           2u

#define  UDA1380_DEFAULT_SAMPLE_RATE                USBD_AUDIO_FMT_TYPE_I_SAMFREQ_48KHZ
#define  UDA1380_BIT_RES                            USBD_AUDIO_FMT_TYPE_I_BIT_RESOLUTION_16
#define  UDA1380_STEREO                             USBD_AUDIO_STEREO
#define  UDA1380_MONO                               USBD_AUDIO_MONO

                                                                /* ----------------------- CGU ------------------------ */
#define  CGU_REG_BASE_ADDR                               (CPU_ADDR)(0x13004000)
#define  CGU_REG_ESRx(clk_nbr)                           (*(CPU_REG32 *)(CGU_REG_BASE_ADDR  + ((clk_nbr) * 4u) + 0x03A0u))
#define  CGU_REG_BCRx(clk_nbr)                           (*(CPU_REG32 *)(CGU_REG_BASE_ADDR  + ((clk_nbr) * 4u) + 0x0504u))

#define  CGU_REG_ESR_SEL_FDC17                           0u
#define  CGU_REG_ESR_SEL_FDC19                           2u
#define  CGU_REG_ESR_SEL_FDC20                           3u

#define  CGU_REG_BCR_CLK1024FS_IX                        4u
                                                                /* ----------------------- I2S ------------------------ */
#define  I2S_TX1_REG_INTERLEAVED_0              0x16000160u
#define  I2S_RX1_REG_LEFT_16BIT                 0x16000200u
#define  I2S_RX1_REG_LEFT_32BIT_0               0x16000220u
#define  I2S_RX1_REG_INTERLEAVED_0              0x16000260u

                                                                /* ------------------ DMA CONTROLLER ------------------ */
                                                                /* DMA channels nbr.                                    */
#define  DMA_CH_0                                        0u
#define  DMA_CH_1                                        1u
#define  DMA_CH_2                                        2u
#define  DMA_CH_3                                        3u

#define  DMA_MAX_NBR_CH_USED                             3u

#define  DMA_CFG_XFER_SIZE_WORD                          4u
#define  DMA_CFG_XFER_SIZE_HALF_WORD                     2u
#define  DMA_CFG_XFER_SIZE_BYTE                          1u

#define  DMA_CFG_XFER_SIZE_HALF_WORD_POS            DEF_BIT_10

#define  DMA_IRQ_MASK_FINISHED_CH                   DEF_BIT_00
#define  DMA_IRQ_MASK_HALF_WAY_CH                   DEF_BIT_01
#define  DMA_IRQ_MASK_ABORT                         DEF_BIT_31

                                                                /* DMA req src.                                         */
#define  DMA_REQ_SRC_I2STX1_LEFT                         8u
#define  DMA_REQ_SRC_I2STX1_RIGHT                        9u
#define  DMA_REQ_SRC_I2SRX1_LEFT                        12u
#define  DMA_REQ_SRC_I2SRX1_RIGHT                       13u


/*
**********************************************************************************************************
*                                           TRACE MACROS
**********************************************************************************************************
*/

#define  USBD_DBG_AUDIO_DRV_MSG(msg)                USBD_DBG_GENERIC    ((msg),              \
                                                                          USBD_EP_ADDR_NONE, \
                                                                          USBD_IF_NBR_NONE)

#define  USBD_DBG_AUDIO_DRV_ARG(msg, arg)           USBD_DBG_GENERIC_ARG((msg),              \
                                                                          USBD_EP_ADDR_NONE, \
                                                                          USBD_IF_NBR_NONE,  \
                                                                         (arg))

#define  USBD_DBG_AUDIO_DRV_ERR(msg, err)           USBD_DBG_GENERIC_ERR((msg),              \
                                                                          USBD_EP_ADDR_NONE, \
                                                                          USBD_IF_NBR_NONE,  \
                                                                         (err))


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


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

typedef struct usbd_audio_buf_info {
    CPU_INT08U  *BufPtr;
    CPU_INT16U   BufLen;
} USBD_AUDIO_BUF_INFO;

typedef  struct  usbd_audio_drv_circular_buf {
                                                                /* Circular buf contains ptr to audio buf.              */
    USBD_AUDIO_BUF_INFO   BufInfoTbl[USBD_AUDIO_DRV_CIRCULAR_AUDIO_BUF_SIZE];
    CPU_INT16U            IxIn;                                 /* Ix used by playback or record to store data.         */
    CPU_INT16U            IxOut;                                /* Ix used by playback or record to get   data.         */
    CPU_INT16U            NbrBuf;                               /* Nb of the audio buf contained in circular buf.       */
} USBD_AUDIO_DRV_CIRCULAR_BUF;

typedef  struct  usbd_audio_drv_data {
    CPU_BOOLEAN                  FU_SpeakerMute;
    CPU_BOOLEAN                  FU_MicMute;
    CPU_INT16U                   FU_VolSpeaker;
    CPU_INT16U                   FU_VolMic;
    CPU_INT32U                   AS_SamplingFreqTbl[USBD_AUDIO_DEV_CFG_NBR_ENTITY];
    CPU_BOOLEAN                  PlaybackStreamStarted;
    CPU_BOOLEAN                  PlaybackFirstXferStart;
    CPU_BOOLEAN                  PlaybackNxtXferReady;
    USBD_AUDIO_DRV_CIRCULAR_BUF  CircularBufRecord;
    USBD_AUDIO_DRV_CIRCULAR_BUF  CircularBufPlayback;
    CPU_BOOLEAN                  DMA_AbortIntProcessFlag;
    USBD_AUDIO_AS_HANDLE         DMA_AsHandleTbl[DMA_MAX_NBR_CH_USED];
    CPU_INT32U                  *DMA_PlaybackSilenceBufPtr;
} USBD_AUDIO_DRV_DATA;


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#if 1 // debug
struct dbg_uda1380_codec {
CPU_INT32U  CircularBufPlaybackNbrBufIncrement;
CPU_INT32U  CircularBufPlaybackNbrBufDecrement;
CPU_INT32U  CircularBufPlaybackNbrTimesFull;
CPU_INT32U  CircularBufPlaybackNbrTimesEmpty;
CPU_BOOLEAN DMACh0InProgress;
CPU_BOOLEAN DMACh1InProgress;
CPU_INT32U  DMAXferCmplHalfway;
CPU_INT32U  DMAXferCmpl;
} DbgUda1380Codec = {0,0,0,0,DEF_FALSE,DEF_FALSE,0,0};
#endif

static  USBD_AUDIO_DRV_DATA  *AudioDrvDataPtr;
static  CPU_INT32U           *DMA_RecordDummyBufPtr;            /* Dummy record buf.                                    */

extern  CPU_INT08U            Mic_OT_USB_IN_ID;
extern  CPU_INT08U            Mic_FU_ID;
extern  CPU_INT08U            Speaker_IT_USB_OUT_ID;
extern  CPU_INT08U            Speaker_FU_ID;


/*
*********************************************************************************************************
*                                 AUDIO API DRIVER FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void         USBD_Audio_DrvInit                 (USBD_AUDIO_DRV        *p_audio_drv,
                                                         USBD_ERR              *p_err);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_MuteManage    (USBD_AUDIO_DRV        *p_audio_drv,
                                                         CPU_INT08U             unit_id,
                                                         CPU_INT08U             log_ch_nbr,
                                                         CPU_BOOLEAN            set_en,
                                                         CPU_BOOLEAN           *p_mute);

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_VolManage     (USBD_AUDIO_DRV        *p_audio_drv,
                                                         CPU_INT08U             req,
                                                         CPU_INT08U             unit_id,
                                                         CPU_INT08U             log_ch_nbr,
                                                         CPU_INT16U            *p_vol);
#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_Audio_DrvAS_SamplingFreqManage(USBD_AUDIO_DRV        *p_audio_drv,
                                                         CPU_INT08U             terminal_id_link,
                                                         CPU_BOOLEAN            set_en,
                                                         CPU_INT32U            *p_sampling_freq);

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStart          (USBD_AUDIO_DRV        *p_audio_drv,
                                                         USBD_AUDIO_AS_HANDLE   as_handle,
                                                         CPU_INT08U             terminal_id_link);

static  CPU_BOOLEAN  USBD_Audio_DrvStreamStop           (USBD_AUDIO_DRV        *p_audio_drv,
                                                         CPU_INT08U             terminal_id_link);

static  void         USBD_Audio_DrvStreamRecordRx       (USBD_AUDIO_DRV        *p_audio_drv,
                                                         CPU_INT08U             terminal_id_link,
                                                         void                  *p_buf,
                                                         CPU_INT16U            *p_buf_len,
                                                         USBD_ERR              *p_err);

static  void         USBD_Audio_DrvStreamPlaybackTx     (USBD_AUDIO_DRV        *p_audio_drv,
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

static  void          USBD_UDA1380_Ctrl_I2C1_ISR_Handler        (void);

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void          USBD_UDA1380_Streaming_I2S_DMA_ISR_Handler(INT_32                        ch,
                                                                 DMA_IRQ_TYPE_T                itype,
                                                                 void                         *p_dma_regs);

static  CPU_BOOLEAN   USBD_UDA1380_CircularBufStreamStore       (USBD_AUDIO_DRV_CIRCULAR_BUF  *p_circular_buf,
                                                                 CPU_INT08U                   *p_buf,
                                                                 CPU_INT16U                    buf_len);

static  CPU_INT08U   *USBD_UDA1380_CircularBufStreamGet         (USBD_AUDIO_DRV_CIRCULAR_BUF  *p_circular_buf,
                                                                 CPU_INT16U                   *p_buf_len);
#endif


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                  USB DEVICE CONTROLLER DRIVER API
*********************************************************************************************************
*/

const  USBD_AUDIO_DRV_COMMON_API  USBD_Audio_DrvCommonAPI_UDA1380_Codec = {
    USBD_Audio_DrvInit
};

const  USBD_AUDIO_DRV_AC_FU_API  USBD_Audio_DrvFU_API_UDA1380_Codec = {
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

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
const  USBD_AUDIO_DRV_AS_API  USBD_Audio_DrvAS_API_UDA1380_Codec = {
    USBD_Audio_DrvAS_SamplingFreqManage,
    DEF_NULL,
    USBD_Audio_DrvStreamStart,
    USBD_Audio_DrvStreamStop,
    USBD_Audio_DrvStreamRecordRx,
    USBD_Audio_DrvStreamPlaybackTx
};
#endif

/*
*********************************************************************************************************
*********************************************************************************************************
*                                     DRIVER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         USBD_Audio_DrvInit()
*
* Description : (1) Initialize software resources, audio codec and different MCU peripherals needed
*                   to communicate with it.
*
*                   (a) Initialize I2C peripheral used to control the codec.
*                   (b) Initialize I2S peripheral used to stream audio to/from the codec.
*                   (c) Initialize DMA controller used during streaming and to offload CPU.
*                   (d) Initialize NXP UDA1380 codec.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                               USBD_ERR_NONE                       Audio device successfully initialized.
*                               USBD_ERR_AUDIO_CODEC_INIT_FAILED    Audio device initialization failed.
*                               USBD_ERR_ALLOC                      Playback silence buffer or record dummy
*                                                                   buffer allocation failed.
*
* Return(s)   : none.
*
* Note(s)     : (1) While a playback stream is in progress, if an audio buffer is not available for the
*                   codec, a silence buffer will be send to the codec. So that the USB part should have
*                   time to free a few buffers for the codec driver.
*
*                   (a) Maximum depth of silence buffer is computed according to this formula:
*
*                       Bit rate = (sampling rate) * (bit depth) * (number of channels)
*
*                       The bit rate is expressed in bytes/ms. A sample frame is added to the result
*                       obtained with the previous formula to accommodate adjustments in the data rate
*                       (e.g. 11.025, 22.050, 44.100 KHz, etc).
*                       A sample frame represents the number of bytes required within 1 ms taking into
*                       account all channels and the bit resolution:
*
*                       Sample frame = (bit depth) *  (number of channels)
*
*               (2) While a record stream is in progress, if an audio buffer is not available for the
*                   codec, a dummy buffer is provided to the codec. So that the USB part should have time
*                   to free a few buffers for the codec driver.
*
*               (3) Change the fractional divider corresponding to the clock. This changes the clock
*                   frequency for all clocks connect to this fractional divider.
*
*               (4) I2S peripheral requires several clock to operate properly. The clocks are:
*                   General clocks
*                       I2S_CFG_PCLK,      APB clock for I2S configuration block
*                   I2STX1 interface
*                       I2STX_FIFO_1_PCLK, APB clock for I2STX_FIFO_1 (AHB_APB3_BASE base clock)
*                       I2STX_IF_1_PCLK,   APB clock for I2STX_IF_1   (AHB_APB3_BASE base clock)
*                       I2STX_BCK1_N,      I2S Bit Clock of I2STX_1   (CLK1024FS_BASE base clock)
*                       I2STX_WS1,         I2S Word Select of I2STX_1 (CLK1024FS_BASE base clock)
*                   I2SRX1 interface
*                       I2SRX_FIFO_1_PCLK, APB clock for I2SRX_FIFO_1 (AHB_APB3_BASE base clock)
*                       I2SRX_IF_1_PCLK,   APB clock for I2SRX_IF_1   (AHB_APB3_BASE base clock)
*                       I2SRX_BCK1_N,      I2S Bit Clock of I2SRX_1   (CLK1024FS_BASE base clock)
*                       I2SRX_WS1,         I2S Word Select of I2SRX_1 (CLK1024FS_BASE base clock)
*
*                   (a) Clocks coming from the AHB_APB3_BASE base clock domain are structured as shown:
*
*  FFAST (12 MHz) -> AHB0_APB3 Base -> FDC14 (6 MHz) -> clock control AHB_TO_APB3_PCLK  -> I2S peripheral
*                                                    -> clock control I2STX_FIFO_1_PCLK -> I2S peripheral
*                                                    -> clock control I2STX_IF_1_PCLK   -> I2S peripheral
*                                                    -> clock control I2SRX_FIFO_1_PCLK -> I2S peripheral
*                                                    -> clock control I2SRX_IF_1_PCLK   -> I2S peripheral
*
*                       Refer to 'UM10314 LPC3130/31 User manual, Rev. 2     23 May 2012', Fig 40.
*                       Performance setting - Example 2 for more details.
*
*                   (b) Clocks coming from the CLK1024FS_BASE base clock domain are structured as shown:
*
*  HP0 PLL -> CLK1024FS Base -> FDC17 -> clock control I2S_EDGE_DETECT_CLK -> I2S peripheral
*                                     -> clock control I2STX_WS1           -> I2S peripheral
*                                     -> clock control I2SRX_WS1           -> I2S peripheral
*                            -> FDC19 -> clock control CLK_256FS           -> I2S peripheral
*                            -> FDC20 -> clock control I2STX_BCK1_N        -> I2S peripheral
*                                     -> clock control 2SRX_BCK1_N         -> I2S peripheral
*
*                       Refer to 'UM10314 LPC3130/31 User manual, Rev. 2     23 May 2012', Fig 41.
*                       Performance setting - Example 3 for more details.
*
*               (5) CLK_256FS_O pin is attached to SYSCLK pin of UDA1380. SYSCLK is needed to use I2C
*                   interface of UDA1380.
*
*               (6) On the LPC3131-EA board, CLK_256fs output pin from I2S module is connected to SYSCLK
*                   input pin from the UDA1380 codec. SYSCLK pin is essential to communicate with I2C
*                   interface from UDA1380. Thus, an arbitrary sample frequency settings is done to
*                   communicate with I2C UDA1380 module. This sample frequency will be changed once
*                   the USB device is connected to the host PC and this one will set the proper
*                   sampling frequency.
*
*                   I2S requires various clocks set at specific rates to get the target frequency.
*                   The I2S clocking driver will generate the correct rates for each clock group.
*                   Setup the I2S clocking as follows:
*
*                   Word Select (WS, sample rate) = Fs
*                   bit clock rate (BCK)          = Fs * bit resolution * number of channels
*                   CODEC clock (SYSCLK) rate     = Fs * 256
*
*               (7) In Base Control register 7 for CLK1024FS domain, when FDRUN is set to '1',
*                   all fractional dividers of a certain base are activated simultaneously, so
*                   that they run in sync. Important for clocks BCK and WS of I2S bus.
*
*               (8) Audio Processing layer and the codec driver use this buffering model:
*
*                                      Memory
*                           +---------------------------+
*                           |                           |
* Audio Processing layer -> |    Playback buffer N      | -> DMA Channel 0 -> I2S1 Tx register
*                           |                           |
*                           +---------------------------+
*                           |                           |
* Audio Processing layer -> |    Playback buffer N+1    | -> DMA Channel 1 -> I2S1 Tx register
*                           |                           |
*                           +---------------------------+
*                           |                           |
* Audio Processing layer <- |    Record buffer N        | <- DMA Channel 2 <- I2S1 Rx register
*                           |                           |
*                           +---------------------------+
*
*                   (a) PLAYBACK: Audio Processing layer accumulates a certain number of buffers to prime
*                       the stream. Then the codec processing is started. The DMA channels 0 and 1 are
*                       reserved for playback. The buffering model is a sort of pipeline between channel 0
*                       and 1. When half of DMA channel 0 transfer has completed for buffer N, the DMA
*                       ISR prepares channel 1 transfer for buffer N+1. Upon total completion of channel 0
*                       transfer, the DMA ISR starts channel 1 transfer and a new channel 0 transfer is
*                       prepared and so on. Audio Processing layer stores the buffers ready to be consumed
*                       in a waiting list.
*
*                   (b) RECORD: As soon as record stream is started, codec driver provides record buffers
*                       ready to be consumed by using DMA channel 2. Audio Processing layer then forwards the
*                       ready buffers to the USB part.
*********************************************************************************************************
*/

static  void  USBD_Audio_DrvInit (USBD_AUDIO_DRV  *p_audio_drv,
                                  USBD_ERR        *p_err)
{
    USBD_AUDIO_DRV_DATA  *p_drv_data;
    CGU_FDIV_SETUP_T      fdiv_cfg;
    CPU_INT32S            result;
    CPU_INT32U            i2c_dev;
    CPU_INT32S            i2s_tx_dev;
    CPU_INT32S            i2s_rx_dev;
    CPU_INT08U            i;
    CPU_SIZE_T            buf_len;
    CPU_INT08U            sample_frame;
    LIB_ERR               err_lib;
    DMAC_REGS_T          *p_dma_reg;


                                                                /* ------------------- VARIOUS INIT ------------------- */
                                                                /* Allocate driver internal data.                       */
    p_audio_drv->DataPtr = Mem_HeapAlloc(              sizeof(USBD_AUDIO_DRV_DATA),
                                                       sizeof(CPU_DATA),
                                         (CPU_SIZE_T *)DEF_NULL,
                                                      &err_lib);
    if (p_audio_drv->DataPtr == (void *)0) {
       *p_err = USBD_ERR_ALLOC;
        return;
    }

    Mem_Clr((void *)p_audio_drv->DataPtr,
                   (sizeof(USBD_AUDIO_DRV_DATA)));

    AudioDrvDataPtr = (USBD_AUDIO_DRV_DATA *)p_audio_drv->DataPtr;
    p_drv_data      = (USBD_AUDIO_DRV_DATA *)p_audio_drv->DataPtr;

                                                                /* Silence buf for playback (see Note #1).              */
    buf_len      = (UDA1380_DEFAULT_SAMPLE_RATE / 1000u) * (UDA1380_BIT_RES / 8u) * UDA1380_STEREO;
    sample_frame = (UDA1380_BIT_RES / 8u) * UDA1380_STEREO;     /* See Note #1a.                                        */
    buf_len     +=  sample_frame;

    p_drv_data->DMA_PlaybackSilenceBufPtr = (CPU_INT32U *)Mem_HeapAlloc(               buf_len,
                                                                                       USBD_AUDIO_CFG_BUF_ALIGN_OCTETS,
                                                                        (CPU_SIZE_T  *)DEF_NULL,
                                                                                      &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        USBD_DBG_AUDIO_DRV_ERR("USBD_Audio_DrvInit(): cannot alloc silence buf w/err=%d\n", *p_err);
        return;
    }

    Mem_Clr((void *)p_drv_data->DMA_PlaybackSilenceBufPtr,
                    buf_len);

                                                                /* Dummy buf for record (see Note #2).                  */
    buf_len               = (UDA1380_DEFAULT_SAMPLE_RATE / 1000u) * (UDA1380_BIT_RES / 8u) * UDA1380_MONO;
    DMA_RecordDummyBufPtr = (CPU_INT32U *)Mem_HeapAlloc(               buf_len,
                                                                       USBD_AUDIO_CFG_BUF_ALIGN_OCTETS,
                                                        (CPU_SIZE_T  *)DEF_NULL,
                                                                      &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = USBD_ERR_ALLOC;
        USBD_DBG_AUDIO_DRV_ERR("USBD_Audio_DrvInit(): cannot alloc dummy record buf w/err=%d\n", *p_err);
        return;
    }

    Mem_Clr((void *)DMA_RecordDummyBufPtr,
                    buf_len);

                                                                /* --------------- I2C PERIPHERAL INIT ---------------- */
    cgu_clk_en_dis(CGU_SB_I2C1_PCLK_ID, DEF_TRUE);              /* Enable I2C system clock.                             */
                                                                /* Set base freq for domain AHB0_APB1 (FFAST, 12Mhz).   */
    cgu_set_base_freq(CGU_SB_AHB0_APB1_BASE_ID, CGU_FIN_SELECT_FFAST);
                                                                /* Set I2C clock between 100KHz & 400KHz for write.     */
    fdiv_cfg.stretch = 1;
    fdiv_cfg.n       = 1;
    fdiv_cfg.m       = 32;
    cgu_set_subdomain_freq(CGU_SB_I2C1_PCLK_ID, fdiv_cfg);      /* See Note #3.                                         */
    cgu_soft_reset_module(I2C1_PNRES_SOFT);
                                                                /* Install I2C ISR.                                     */
    CSP_IntVectReg(              CSP_INT_CTRL_NBR_MAIN,
                                (CSP_INT_SRC_NBR_I2C_01),
                   (CPU_FNCT_PTR)USBD_UDA1380_Ctrl_I2C1_ISR_Handler,
                   (void       *)0u);

    CSP_IntSrcCfg(CSP_INT_CTRL_NBR_MAIN,
                 (CSP_INT_SRC_NBR_I2C_01),
                  CSP_INT_PRIO_LOWEST,
                  CSP_INT_POL_LEVEL_HIGH);

    CSP_IntEn(CSP_INT_CTRL_NBR_MAIN, CSP_INT_SRC_NBR_I2C_01);

                                                                /* --------------- I2S PERIPHERAL INIT ---------------- */
    i2s_tx_dev = i2s_open(     I2S_TX1,                         /* Open the I2S TX1 device.                             */
                          (PFV)NULL);
    if (i2s_tx_dev == 0) {
       *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
        return;
    }

    i2s_ioctl(i2s_tx_dev,                                       /* set I2S format for I2S TX1 - default.                */
              I2S_FORMAT_SETTINGS,
              I2S);

    i2s_rx_dev = i2s_open(     I2S_RX1,                         /* Open the I2S RX1 device.                             */
                          (PFV)NULL);
    if (i2s_rx_dev == 0) {
       *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
        return;
    }

    i2s_ioctl(i2s_rx_dev,                                       /* Set I2S format for I2S RX1 - default.                */
              I2S_FORMAT_SETTINGS,
              I2S);

                                                                /* Init clock domain #4 = AHB0_APB3 (See Note #4a).     */
    CSP_PM_PerClkDivCfg(CSP_PM_PER_CLK_NBR_I2STX_FIFO_1, 2u);   /* FDC14 is equal to (FFAST clock / 2) = 6 MHz.         */

                                                                /* Select fractional divider 14 for all these clocks... */
                                                                /* ...needed by the I2S peripheral internal.            */
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_AHB_TO_APB3)  = CGU_SB_ESR_ENABLE;
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2S_CFG)      = CGU_SB_ESR_ENABLE;
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_EDGE_DET)     = CGU_SB_ESR_ENABLE;
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2STX_FIFO_1) = CGU_SB_ESR_ENABLE;
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2STX_IF_1)   = CGU_SB_ESR_ENABLE;
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2SRX_FIFO_1) = CGU_SB_ESR_ENABLE;
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2SRX_IF_1)   = CGU_SB_ESR_ENABLE;
                                                                /* Init clock domain #7 = CLK1024FS (See Note #4b).     */
    cgu_set_base_freq(CGU_SB_CLK1024FS_BASE_ID,                 /* Set base freq for domain CLK1024FS to HP PPL0.       */
                      CGU_FIN_SELECT_HPPLL0);

    CGU_REG_BCRx(CGU_REG_BCR_CLK1024FS_IX) = 0u;                /* Disable all fractional dividers.                     */

                                                                /* See Note #5.                                         */
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_CLK_256FS)    = CGU_SB_ESR_ENABLE | CGU_SB_ESR_SELECT(CGU_REG_ESR_SEL_FDC19);
                                                                /* Select the required fractional divider for I2STX1... */
                                                                /* ...and I2SRX1 which interface with NXP UDA1380 codec.*/
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2S_EDGE_DET) = CGU_SB_ESR_ENABLE | CGU_SB_ESR_SELECT(CGU_REG_ESR_SEL_FDC17);
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2STX_WS1)    = CGU_SB_ESR_ENABLE | CGU_SB_ESR_SELECT(CGU_REG_ESR_SEL_FDC17);
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2STX_BCK1_N) = CGU_SB_ESR_ENABLE | CGU_SB_ESR_SELECT(CGU_REG_ESR_SEL_FDC20);
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2SRX_WS1)    = CGU_SB_ESR_ENABLE | CGU_SB_ESR_SELECT(CGU_REG_ESR_SEL_FDC17);
    CGU_REG_ESRx(CSP_PM_PER_CLK_NBR_I2SRX_BCK1_N) = CGU_SB_ESR_ENABLE | CGU_SB_ESR_SELECT(CGU_REG_ESR_SEL_FDC20);

                                                                /* Arbitrary clocks settings for I2S (see Note #6).     */
    result = lpc313x_i2s_set_fsmult_rate(256, CLK_USE_256FS);
    if (result <= 0) {
       *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
        return;
    }

#if (AUDIO_TEST_PLAYBACK_CORR_UNDERRUN == DEF_DISABLED) && (AUDIO_TEST_PLAYBACK_CORR_OVERRUN == DEF_DISABLED)
    result = lpc313x_i2s_main_clk_rate(UDA1380_DEFAULT_SAMPLE_RATE);
    if (result <= 0) {
       *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
        return;
    }
    result = lpc313x_i2s_chan_clk_enable(CLK_TX_1,
                                         UDA1380_DEFAULT_SAMPLE_RATE,
                                        (UDA1380_DEFAULT_SAMPLE_RATE * UDA1380_BIT_RES * UDA1380_STEREO));
    if (result <= 0) {
       *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
        return;
    }
    result = lpc313x_i2s_chan_clk_enable(CLK_RX_1,
                                         UDA1380_DEFAULT_SAMPLE_RATE,
                                        (UDA1380_DEFAULT_SAMPLE_RATE * UDA1380_BIT_RES * UDA1380_STEREO));
    if (result <= 0) {
       *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
        return;
    }
#else  // !!!! built-in correction testing
    {
        CPU_INT32U  sampling_freq;
#if (AUDIO_TEST_PLAYBACK_CORR_UNDERRUN == DEF_ENABLED)
        sampling_freq = AUDIO_TEST_PLAYBACK_CORR_UNDERRUN_FREQ;
#elif (AUDIO_TEST_PLAYBACK_CORR_OVERRUN == DEF_ENABLED)
        sampling_freq = AUDIO_TEST_PLAYBACK_CORR_OVERRUN_FREQ;
#endif

        result = lpc313x_i2s_main_clk_rate(sampling_freq);
        if (result <= 0) {
           *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
            return;
        }
        result = lpc313x_i2s_chan_clk_enable(CLK_TX_1,
                                             sampling_freq,
                                            (sampling_freq * UDA1380_BIT_RES * UDA1380_STEREO));
        if (result <= 0) {
           *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
            return;
        }
        result = lpc313x_i2s_chan_clk_enable(CLK_RX_1,
                                             sampling_freq,
                                            (sampling_freq * UDA1380_BIT_RES * UDA1380_STEREO));
        if (result <= 0) {
           *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
            return;
        }
    }
#endif

    CGU_REG_BCRx(CGU_REG_BCR_CLK1024FS_IX) = CGU_SB_BCR_FD_RUN; /* Enable all fractional dividers (see Note #7).        */


                                                                /* ----------- DMA CONTROLLER INIT FOR I2S ------------ */
    result = dma_init();
    if (result == _ERROR) {
       *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
        return;
    }

    p_dma_reg = dma_get_base();

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
    for (i = 0; i < DMA_MAX_NBR_CH_USED; i++) {                 /* Alloc DMA ch for playback & record (see Note #8).    */
        result = dma_alloc_channel(        i,
                                           USBD_UDA1380_Streaming_I2S_DMA_ISR_Handler,
                                   (void *)p_dma_reg);
        if (result == _ERROR) {
           *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
            return;
        }
    }
#endif
                                                                /* install DMA ISR.                                     */
    CSP_IntVectReg(              CSP_INT_CTRL_NBR_MAIN,
                                (CSP_INT_SRC_NBR_DMA_00),
                   (CPU_FNCT_PTR)dma_interrupt,
                   (void       *)0u);

    CSP_IntSrcCfg(CSP_INT_CTRL_NBR_MAIN,
                 (CSP_INT_SRC_NBR_DMA_00),
                  CSP_INT_PRIO_LOWEST,
                  CSP_INT_POL_LEVEL_HIGH);

    CSP_IntEn(CSP_INT_CTRL_NBR_MAIN, CSP_INT_SRC_NBR_DMA_00);

                                                                /* ---------------- UDA1380 CODEC INIT ---------------- */
    i2c_dev = 0;
    result = uda1380_driver_init(i2c_dev, 0x1a);
    if (result < 0) {
         lpc313x_i2s_main_clk_rate(0);
         lpc313x_i2s_chan_clk_enable(CLK_TX_1, 0, 0);
        *p_err = USBD_ERR_AUDIO_CODEC_INIT_FAILED;
         return;
    }
                                                                /* Default settings for codec.                          */
    uda1380_enable_audio(DEF_TRUE);                             /* Enable playback & record paths.                      */
    uda1380_playback_set_volume(0, 0);                          /* Left and right channels set to 0 dB.                 */
    uda1380_playback_mute(DEF_FALSE);                           /* Unmute left and right channels.                      */


   *p_err = USBD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   USBD_Audio_DrvCtrlFU_MuteManage()
*
* Description : Get or set mute state for one or all logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number (see Note #1).
*
*               set_en          Flag indicating to get or set the mute.
*
*               p_mute          Pointer to the mute state to get or set.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
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
    USBD_AUDIO_DRV_DATA  *p_drv_data;


    (void)log_ch_nbr;

    p_drv_data = (USBD_AUDIO_DRV_DATA *)p_audio_drv->DataPtr;

    if ((unit_id != Mic_FU_ID) &&
        (unit_id != Speaker_FU_ID)) {

        return (DEF_FAIL);
    }

    if (unit_id == Mic_FU_ID) {
                                                                /* ----------------------- GET ------------------------ */
        if (set_en == DEF_FALSE) {
           *p_mute = p_drv_data->FU_MicMute;
                                                                /* ----------------------- SET ------------------------ */
        } else {
            p_drv_data->FU_MicMute = *p_mute;
            uda1380_playback_mute(*p_mute);
        }

    } else if (unit_id == Speaker_FU_ID) {
                                                                /* ----------------------- GET ------------------------ */
        if (set_en == DEF_FALSE) {
           *p_mute = p_drv_data->FU_SpeakerMute;
                                                                /* ----------------------- SET ------------------------ */
        } else {
            p_drv_data->FU_SpeakerMute = *p_mute;
            uda1380_playback_mute(*p_mute);
        }
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                  USBD_Audio_DrvCtrlFU_VolManage()
*
* Description : Get or set volume for one or several logical channels inside a cluster.
*
* Argument(s) : p_audio_drv     Pointer to audio driver structure.
*
*               req             Volume request:
*
*                               USBD_AUDIO_REQ_GET_CUR
*                               USBD_AUDIO_REQ_GET_RES
*                               USBD_AUDIO_REQ_GET_MIN
*                               USBD_AUDIO_REQ_GET_MAX
*                               USBD_AUDIO_REQ_SET_CUR
*
*               unit_id         Feature Unit ID.
*
*               log_ch_nbr      Logical channel number.
*
*               p_vol           Pointer to the volume value to set or get.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) Speaker uses the Master volume control register which controls left and right channels.
*                   The volume attenuation ranges from 0 to -78 dB and -    dB in steps of 0.25 dB.
*                   See 'UDA1380 Stereo audio coder-decoder for MD, CD and MP3, 2004 Apr 22',
*                   section 11.6 for more details about 'Master volume control' register.
*
*                   (a) Range 0 to -78 dB is coded with a 8-bit register. -78 dB is encoded with the
*                       value 248. Thus the maximum number of 0.25 db steps is 248. Translated in the
*                       Audio 1.0 range, the minimum dB value is 248 * 0.25 = 62 dB => -62 dB. Audio 1.0
*                       allows steps of 1/256 => -62 db / (1/256) = -15872 coded on a 16-bit signed value
*                       gives 0xC200.
*
*                   (b) Codec uses steps of 0.25 dB. Audio 1.0 uses steps of 1/256. Hence, the resolution
*                       of codec translated in the Audio 1.0 range is 0.25 / (1/256) = 64 = 0x40.
*
*               (1) Microphone uses the Decimator Volume Control register which controls left and right
*                   channels. The volume attenuation ranges from +24 to    63.5 dB and -    dB in steps of
*                   0.5 dB.
*                   See 'UDA1380 Stereo audio coder-decoder for MD, CD and MP3, 2004 Apr 22',
*                   section 11.6 for more details about 'Decimator Volume Control' register.
*
*                   (a) Range +24 to -63.5 dB is coded with a 8-bit register.
*
*                       +24 dB is encoded with the value 48. Thus the maximum number of 0.5 db steps is
*                       48. Translated in the Audio 1.0 range, the minimum dB value is 48 * 0.5 = 24 dB
*                       Audio 1.0 allows steps of 1/256 => 24 db / (1/256) = 6144 coded on a 16-bit
*                       signed value gives 0x1800.
*
*                       -63.5 dB is encoded with the value 128. Thus the maximum number of 0.5 db steps is
*                       127. Translated in the Audio 1.0 range, the minimum dB value is 127 * 0.5 = 63.5 dB
*                       => -63.5 dB. Audio 1.0 allows steps of 1/256 => -63.5 db / (1/256) = -16256 coded
*                       on a 16-bit signed value gives 0xC080.
*
*                   (b) Codec uses steps of 0.5 dB. Audio 1.0 uses steps of 1/256. Hence, the resolution
*                       of codec translated in the Audio 1.0 range is 0.5 / (1/256) = 128 = 0x80.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  USBD_Audio_DrvCtrlFU_VolManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                     CPU_INT08U       req,
                                                     CPU_INT08U       unit_id,
                                                     CPU_INT08U       log_ch_nbr,
                                                     CPU_INT16U      *p_vol)
{
    USBD_AUDIO_DRV_DATA  *p_drv_data;
    CPU_INT16S            temp;
    CPU_INT08U            vol_to_set;


    (void)log_ch_nbr;

    p_drv_data = (USBD_AUDIO_DRV_DATA *)p_audio_drv->DataPtr;

     if ((unit_id != Speaker_FU_ID) &&
         (unit_id != Mic_FU_ID)) {

         return (DEF_FAIL);
     }

    switch (req) {
        case USBD_AUDIO_REQ_GET_CUR:
             if (unit_id == Speaker_FU_ID) {
                 *p_vol = p_drv_data->FU_VolSpeaker;
             } else {
                 *p_vol = p_drv_data->FU_VolMic;
             }
             break;

        case USBD_AUDIO_REQ_GET_MIN:
             if (unit_id == Speaker_FU_ID) {
                 *p_vol = UDA1380_MASTER_VOL_CTRL_MIN_dB;       /* -78 dB (see Note #1a).                               */
             } else {
                 *p_vol = UDA1380_DECIMATOR_VOL_CTRL_MIN_dB;    /* -63.5 dB (see Note #2a).                             */
             }
             break;

        case USBD_AUDIO_REQ_GET_MAX:
             if (unit_id == Speaker_FU_ID) {
                 *p_vol = UDA1380_MASTER_VOL_CTRL_MAX_dB;       /* 0 dB.                                                */
             } else {
                 *p_vol = UDA1380_DECIMATOR_VOL_CTRL_MAX_dB;    /* +24 dB (see Note #2a).                               */
             }
             break;

        case USBD_AUDIO_REQ_GET_RES:
             if (unit_id == Speaker_FU_ID) {
                 *p_vol = UDA1380_MASTER_VOL_CTRL_RES_dB;       /* Resolution of 0.25 dB (see Note #1b).                */
             } else {
                 *p_vol = UDA1380_DECIMATOR_VOL_CTRL_RES_dB;    /* Resolution of 0.5 dB (see Note #2b).                 */
             }
             break;

        case USBD_AUDIO_REQ_SET_CUR:
             if (unit_id == Speaker_FU_ID) {

                 p_drv_data->FU_VolSpeaker = *p_vol;

                 if (*p_vol == USBD_AUDIO_FU_CTRL_VOL_SILENCE) {
                     vol_to_set = UDA1380_MASTER_VOL_CTRL_SILENCE;
                 }
                                                                /* Negative dB.                                         */
                 temp       = (CPU_INT16S)*p_vol;
                 vol_to_set = (CPU_INT08U)(-temp / UDA1380_MASTER_VOL_CTRL_RES_dB);

                 USBD_DBG_AUDIO_DRV_ARG("Vol set to %d for Unit.\r\n", vol_to_set);
                                                                /* Set Left and right vol channels.                     */
                 uda1380_playback_set_volume(vol_to_set, vol_to_set);

             } else {
                 p_drv_data->FU_VolMic = *p_vol;

                 if (*p_vol == USBD_AUDIO_FU_CTRL_VOL_SILENCE) {
                     vol_to_set = UDA1380_DECIMATOR_VOL_CTRL_SILENCE;
                 }
                                                                /* Negative dB.                                         */
                 if (*p_vol > UDA1380_DECIMATOR_VOL_CTRL_MIN_dB) {
                     temp       = (CPU_INT16S)*p_vol;
                     vol_to_set = (CPU_INT08U)(-temp / UDA1380_DECIMATOR_VOL_CTRL_RES_dB);
                 } else {                                       /* Positive dB.                                         */
                     vol_to_set = (CPU_INT08U)(*p_vol / UDA1380_DECIMATOR_VOL_CTRL_RES_dB);
                 }

                 USBD_DBG_AUDIO_DRV_ARG("Vol set to %d for Unit.\r\n", vol_to_set);
                                                                /* Set Left and right vol channels.                     */
                 uda1380_record_set_volume(vol_to_set, vol_to_set);
             }
             USBD_DBG_AUDIO_DRV_ARG("Vol set to 0x%X for Unit.\n", *p_vol);
             break;

        default:
             return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                 USBD_Audio_DrvAS_SamplingFreqManage()
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
* Return(s)   : DEF_OK,   if NO error(s) occurred and request is supported.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : None.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_Audio_DrvAS_SamplingFreqManage (USBD_AUDIO_DRV  *p_audio_drv,
                                                          CPU_INT08U       terminal_id_link,
                                                          CPU_BOOLEAN      set_en,
                                                          CPU_INT32U      *p_sampling_freq)
{
    USBD_AUDIO_DRV_DATA  *p_drv_data;
    CPU_INT32U            result;
    CPU_SR_ALLOC();


    (void)terminal_id_link;

    p_drv_data = (USBD_AUDIO_DRV_DATA *)p_audio_drv->DataPtr;

                                                                /* ----------------------- GET ------------------------ */
    if (set_en == DEF_FALSE) {
       *p_sampling_freq = p_drv_data->AS_SamplingFreqTbl[terminal_id_link - 1u];
                                                                /* ----------------------- SET ------------------------ */
    } else {

        CPU_CRITICAL_ENTER();
        p_drv_data->AS_SamplingFreqTbl[terminal_id_link - 1u] = *p_sampling_freq;
        CPU_CRITICAL_EXIT();

#if (AUDIO_TEST_PLAYBACK_CORR_UNDERRUN == DEF_ENABLED) //
    *p_sampling_freq = AUDIO_TEST_PLAYBACK_CORR_UNDERRUN_FREQ;
#elif (AUDIO_TEST_PLAYBACK_CORR_OVERRUN == DEF_ENABLED)
    *p_sampling_freq = AUDIO_TEST_PLAYBACK_CORR_OVERRUN_FREQ;
#endif
        result = lpc313x_i2s_main_clk_rate(*p_sampling_freq);
        if (result == 0) {
            return (DEF_FAIL);
        }

        result = lpc313x_i2s_chan_clk_enable(CLK_TX_1,
                                             *p_sampling_freq,
                                            (*p_sampling_freq * UDA1380_BIT_RES * UDA1380_STEREO));
        if (result == 0) {
            return (DEF_FAIL);
        }

        result = lpc313x_i2s_chan_clk_enable(CLK_RX_1,
                                             *p_sampling_freq,
                                            (*p_sampling_freq * UDA1380_BIT_RES * UDA1380_STEREO));
        if (result == 0) {
            return (DEF_FAIL);
        }

        USBD_DBG_AUDIO_DRV_ARG("Sampling frequency Control set to %d for Terminal.\n", sampling_freq);
    }

    return (DEF_OK);
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
* Return(s)   : DEF_OK,   if NO error(s) occurred.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) A general purpose OS (Windows, Mac OS, Linux) opens a record stream by sending a
*                   SET_INTERFACE request to select the operational interface of an AudioStreaming
*                   interface. The SET_INTERFACE request is always followed by a SET_CUR request to
*                   configure the sampling frequency. The first record DMA transfer must be started ONLY
*                   when the sampling frequency has been configured. The Audio class ensures this
*                   sequence.
*
*               (2) The DMA transfer length must be equal to the (transfer length - 1). Indeed, the
*                   LPC3131 DMA controller will perform a number of transfers equal to the value
*                   programmed in the register TRANSFER_LENGTH + 1.
*                   See 'UM10314, LPC3130/31 User manual, Rev. 2, 23 May 2012', Table 151 for more
*                   details.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_Audio_DrvStreamStart (USBD_AUDIO_DRV        *p_audio_drv,
                                                USBD_AUDIO_AS_HANDLE   as_handle,
                                                CPU_INT08U             terminal_id_link)
{
    USBD_AUDIO_DRV_DATA  *p_drv_data = (USBD_AUDIO_DRV_DATA *)p_audio_drv->DataPtr;
    CPU_BOOLEAN           req_ok     = DEF_FAIL;
    CPU_INT32U            irq_mask;
    CPU_INT16U            buf_len;
    DMAC_REGS_T          *p_dma_reg;
    CPU_INT08U           *p_buf;
    CPU_SR_ALLOC();


    if (terminal_id_link == Mic_OT_USB_IN_ID) {                 /* See Note #1.                                         */
        p_drv_data->DMA_AsHandleTbl[DMA_CH_2] = as_handle;      /* Save AS handle associated to record for later use.   */

        p_dma_reg = dma_get_base();
                                                                /* Get an empty buf.                                    */
        p_buf = (CPU_INT08U *)USBD_Audio_RecordBufGet(p_drv_data->DMA_AsHandleTbl[DMA_CH_2], &buf_len);
        if (p_buf == (CPU_INT08U *)0) {
            return (DEF_FAIL);
        }
                                                                /* Prepare initial DMA xfer (see Note #2).              */
        p_dma_reg->channel[DMA_CH_2].source      =  I2S_RX1_REG_LEFT_32BIT_0;
        p_dma_reg->channel[DMA_CH_2].destination = (CPU_INT32U)p_buf;
        p_dma_reg->channel[DMA_CH_2].length      = (CPU_INT32U)((buf_len / DMA_CFG_XFER_SIZE_WORD) - 1u);
        p_dma_reg->channel[DMA_CH_2].counter     =  0u;
        p_dma_reg->channel[DMA_CH_2].config      = (DMA_CFG_TX_WORD                                 |
                                                    DMA_CFG_RD_SLV_NR(DMA_REQ_SRC_I2SRX1_LEFT + 1u) |
                                                    DMA_CFG_WR_SLV_NR(0));

                                                                /* Enable DMA channel interrupts for ch #2.             */
        irq_mask = (DMA_IRQ_MASK_FINISHED_CH) << (2 * DMA_CH_2);
        DEF_BIT_CLR(p_dma_reg->irq_mask, irq_mask);

        p_dma_reg->channel[DMA_CH_2].enable = 1;                /* Start DMA xfer on ch #2.                             */
        req_ok = DEF_OK;

    } else if (terminal_id_link == Speaker_IT_USB_OUT_ID) {
                                                                /* Save AS handle associated to playback for later usage*/
        p_drv_data->DMA_AsHandleTbl[DMA_CH_0] = as_handle;
        p_drv_data->DMA_AsHandleTbl[DMA_CH_1] = as_handle;
                                                                /* Signal playback task nbr of buf that can be queued.  */
        USBD_Audio_PlaybackTxCmpl(as_handle);
        USBD_Audio_PlaybackTxCmpl(as_handle);

        CPU_CRITICAL_ENTER();
        p_drv_data->PlaybackStreamStarted = DEF_TRUE;           /* Mark stream as open.                                 */
        CPU_CRITICAL_EXIT();

        req_ok = DEF_OK;
    }

    return (req_ok);
}
#endif


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
* Return(s)   : DEF_OK,   if NO error(s) occurred.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) At the time the stream closes, audio transfer(s) can still be in progress. The
*                   associated buffer(s) must be freed once the remaining audio transfer(s) finished.
*                   From the DMA ISR, the buffer(s) will be freed. This flag ensures that the DMA ISR
*                   will only notify the playback task about buffer(s) freeing and not about audio
*                   transfer completion.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_Audio_DrvStreamStop (USBD_AUDIO_DRV  *p_audio_drv,
                                               CPU_INT08U       terminal_id_link)
{
    USBD_AUDIO_DRV_DATA  *p_drv_data = (USBD_AUDIO_DRV_DATA *)p_audio_drv->DataPtr;
    CPU_BOOLEAN           req_ok = DEF_FAIL;
    CPU_SR_ALLOC();


    if (terminal_id_link == Mic_OT_USB_IN_ID) {
        CPU_CRITICAL_ENTER();
                                                                /* Clr circular buf for next stream opening.            */
        Mem_Clr((void *)&p_drv_data->CircularBufRecord,
                         sizeof(p_drv_data->CircularBufRecord));
        CPU_CRITICAL_EXIT();

        req_ok = DEF_OK;

    } else if (terminal_id_link == Speaker_IT_USB_OUT_ID) {
        CPU_CRITICAL_ENTER();
                                                                /* Clr circular buf for next stream opening.            */
        Mem_Clr((void *)&p_drv_data->CircularBufPlayback,
                         sizeof(p_drv_data->CircularBufPlayback));

        p_drv_data->PlaybackFirstXferStart = DEF_FALSE;         /* Reset flag for next stream opening.                  */
        p_drv_data->PlaybackStreamStarted  = DEF_FALSE;         /* Mark stream as closed (see Note #1).                 */
        CPU_CRITICAL_EXIT();

        req_ok = DEF_OK;
    }

    return (req_ok);
}
#endif


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
*                                   USBD_ERR_NONE   Buffer successfully retrieved.
*
* Return(s)   : Pointer to a ready record buffer, if NO error(s) occurred.
*
*               Null pointer,                     otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_DrvStreamRecordRx (USBD_AUDIO_DRV  *p_audio_drv,
                                            CPU_INT08U       terminal_id_link,
                                            void            *p_buf,
                                            CPU_INT16U      *p_buf_len,
                                            USBD_ERR        *p_err)
{
    (void)p_audio_drv;
    (void)p_buf_len;
    (void)p_buf;

    if (terminal_id_link == Mic_OT_USB_IN_ID) {
       *p_err = USBD_ERR_NONE;
       } else {
       *p_err = USBD_ERR_RX;
    }
}
#endif


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
*                                   USBD_ERR_NONE   Buffer successfully retrieved.
*
* Return(s)   : DEF_OK,   if NO error(s) occurred.
*
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_Audio_DrvStreamPlaybackTx (USBD_AUDIO_DRV  *p_audio_drv,
                                              CPU_INT08U       terminal_id_link,
                                              void            *p_buf,
                                              CPU_INT16U       buf_len,
                                              USBD_ERR        *p_err)
{
    USBD_AUDIO_DRV_DATA  *p_drv_data;
    DMAC_REGS_T          *p_dma_reg;
    CPU_INT08U           *p_initial_buf;
    CPU_INT16U            circular_buf_size;
    CPU_INT32U            irq_mask;
    CPU_INT16U            initial_buf_len;
    CPU_BOOLEAN           result;
    CPU_SR_ALLOC();


    p_drv_data = (USBD_AUDIO_DRV_DATA *)p_audio_drv->DataPtr;
    p_dma_reg  =  dma_get_base();

    if (terminal_id_link == Speaker_IT_USB_OUT_ID) {
                                                                /* ------------- STORE READY PLAYBACK BUF ------------- */
        result = USBD_UDA1380_CircularBufStreamStore(             &p_drv_data->CircularBufPlayback,
                                                     (CPU_INT08U *)p_buf,
                                                                   buf_len);
        if (result == DEF_OK) {
                                                                /* ----------------- INITIAL DMA XFER ----------------- */
            CPU_CRITICAL_ENTER();
            circular_buf_size = p_drv_data->CircularBufPlayback.NbrBuf;
            if ((circular_buf_size                  == USBD_AUDIO_DRV_PLAYBACK_BUF_QUEUE_TRESHOLD) &&
                (p_drv_data->PlaybackFirstXferStart == DEF_FALSE)) {
                CPU_CRITICAL_EXIT();

                                                                /* Get 1st playback rdy buf.                            */
                p_initial_buf = USBD_UDA1380_CircularBufStreamGet(&p_drv_data->CircularBufPlayback, &initial_buf_len);

                                                                /* Prepare DMA xfer.                                    */
                                                                /* See Note #1.                                         */
                p_dma_reg->channel[DMA_CH_0].source      = (CPU_INT32U)p_initial_buf;
                p_dma_reg->channel[DMA_CH_0].destination =  I2S_TX1_REG_INTERLEAVED_0;
                p_dma_reg->channel[DMA_CH_0].length      = (CPU_INT32U)((initial_buf_len / DMA_CFG_XFER_SIZE_WORD) - 1u);
                p_dma_reg->channel[DMA_CH_0].counter     =  0u;
                p_dma_reg->channel[DMA_CH_0].config      = (DMA_CFG_TX_WORD      |
                                                            DMA_CFG_RD_SLV_NR(0) |
                                                            DMA_CFG_WR_SLV_NR(DMA_REQ_SRC_I2STX1_LEFT + 1u));

                                                                /* Enable DMA channel interrupts for ch #0 & #1.        */
                irq_mask = (DMA_IRQ_MASK_HALF_WAY_CH |
                            DMA_IRQ_MASK_FINISHED_CH) << (2 * DMA_CH_0);
                DEF_BIT_CLR(p_dma_reg->irq_mask, irq_mask);

                irq_mask = (DMA_IRQ_MASK_HALF_WAY_CH |
                            DMA_IRQ_MASK_FINISHED_CH) << (2 * DMA_CH_1);
                DEF_BIT_CLR(p_dma_reg->irq_mask, irq_mask);

                p_dma_reg->channel[DMA_CH_0].enable = 1;        /* Start DMA xfer on ch #0.                             */
                DbgUda1380Codec.DMACh0InProgress = DEF_TRUE;

                CPU_CRITICAL_ENTER();
                p_drv_data->PlaybackFirstXferStart = DEF_TRUE;
                CPU_CRITICAL_EXIT();
            } else {
                CPU_CRITICAL_EXIT();
            }

           *p_err = USBD_ERR_NONE;
        } else {
           *p_err = USBD_ERR_TX;
        }
    } else {
       *p_err = USBD_ERR_TX;
    }
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
*                                 USBD_UDA1380_Ctrl_I2C1_ISR_Handler()
*
* Description : I2C1 peripheral Interrupt Service Routine (ISR) handler.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  USBD_UDA1380_Ctrl_I2C1_ISR_Handler (void)
{
    i2c1_mstr_int_hanlder();
}


/*
*********************************************************************************************************
*                                    Streaming_I2S_DMA_ISR_Handler()
*
* Description : I2S1 peripheral Interrupt Service Routine (ISR) handler.
*
* Argument(s) : ch          DMA channel number
*
*               itype       Type of interrupt for a specific channel
*
*                           DMA_IRQ_FINISHED        End of DMA transfer
*                           DMA_IRQ_HALFWAY         Half of DMA transfer completed
*                           DMA_IRQ_DMAABORT        DMA transfer aborted
*
*               p_dma_regs  Pointer to DMA registers interface
*
* Return(s)   : none.
*
* Note(s)     : (1) When the halfway interrupt is generated and the stream is open, the next audio
*                   transfer is prepared and ready to be started on the next DMA channel. In the meantime,
*                   the host closes the stream. When receiving the DMA transfer completion interrupt,
*                   the audio transfer ready to start must be enabled even if the stream is marked as
*                   closed. The flag 'PlaybackNxtXferReady' allows that. Upon completion of this remaining
*                   transfer, the associated buffer will be freed.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  void  USBD_UDA1380_Streaming_I2S_DMA_ISR_Handler (INT_32           ch,
                                                          DMA_IRQ_TYPE_T   itype,
                                                          void            *p_dma_regs)
{
    USBD_AUDIO_DRV_DATA  *p_drv_data =                AudioDrvDataPtr;
    DMAC_REGS_T          *p_dma_reg  = (DMAC_REGS_T *)p_dma_regs;
    CPU_INT08U           *p_buf;
    CPU_INT08U            nxt_ch;
    CPU_INT32U            ch_src;
    CPU_INT32U            ch_len;
    CPU_INT16U            buf_len;
    CPU_INT32U            ch_destination;
    CPU_INT32U            ch_xfer_size;
    CPU_INT32U            ch_cfg_xfer_size;


    switch (ch) {
                                                                /* --------------------- PLAYBACK --------------------- */
        case DMA_CH_0:
        case DMA_CH_1:
             if ((itype == DMA_IRQ_HALFWAY) ||
                 (itype == DMA_IRQ_FINISHED)) {

                 if (ch == DMA_CH_0) {
                     nxt_ch = DMA_CH_1;
                 } else if (ch == DMA_CH_1) {
                     nxt_ch = DMA_CH_0;
                 }
             }

             if (itype == DMA_IRQ_HALFWAY) {
                 DbgUda1380Codec.DMAXferCmplHalfway++;
                                                                /* --------------- GET RDY PLAYBACK BUF --------------- */
                                                                /* Ensure stream is open.                               */
                 if (p_drv_data->PlaybackStreamStarted == DEF_TRUE) {

                     p_buf = USBD_UDA1380_CircularBufStreamGet(&p_drv_data->CircularBufPlayback, &buf_len);
                     if (p_buf != (CPU_INT08U *)0) {
                         ch_src = (CPU_INT32U)p_buf;
                         ch_len =  buf_len;
                     } else {
                         ch_src = (CPU_INT32U)p_drv_data->DMA_PlaybackSilenceBufPtr;
                         ch_len = (p_drv_data->AS_SamplingFreqTbl[Speaker_IT_USB_OUT_ID - 1u] / 1000u) * (UDA1380_BIT_RES / 8u) * UDA1380_STEREO;
                         USBD_AUDIO_DRV_STAT_INC(p_drv_data->DMA_AsHandleTbl[nxt_ch], AudioDrv_Playback_DMA_NbrSilenceBuf);
                     }

                                                                /* --------------- DMA XFER PREPARATION --------------- */
                                                                /* Use a different DMA ch for next xfer.                */
                     p_dma_reg->channel[nxt_ch].source      =  ch_src;
                     p_dma_reg->channel[nxt_ch].destination =  I2S_TX1_REG_INTERLEAVED_0;
                     p_dma_reg->channel[nxt_ch].length      = ((ch_len / DMA_CFG_XFER_SIZE_WORD) - 1u);
                     p_dma_reg->channel[nxt_ch].counter     =  0u;
                     p_dma_reg->channel[nxt_ch].config      = (DMA_CFG_TX_WORD      |
                                                               DMA_CFG_RD_SLV_NR(0) |
                                                               DMA_CFG_WR_SLV_NR(DMA_REQ_SRC_I2STX1_LEFT + 1u));

                     p_drv_data->PlaybackNxtXferReady = DEF_TRUE;

                 } else {
                     p_drv_data->PlaybackNxtXferReady = DEF_FALSE;
                 }
                                                                /* Ack Half Way int.                                    */
                 p_dma_reg->irq_status_clear = (DMA_IRQ_MASK_HALF_WAY_CH << (ch * 2u));

             } else if (itype == DMA_IRQ_FINISHED) {
                 DbgUda1380Codec.DMAXferCmpl++;
                 USBD_AUDIO_DRV_STAT_INC(p_drv_data->DMA_AsHandleTbl[ch], AudioDrv_Playback_DMA_NbrXferCmpl);

                                                                /* Ack Finished int for current DMA ch.                 */
                 p_dma_reg->irq_status_clear   = (DMA_IRQ_MASK_FINISHED_CH << (ch * 2u));
                 p_dma_reg->channel[ch].enable =  0u;           /* Disable current DMA ch.                              */

                 if (p_dma_reg->channel[ch].source != (CPU_INT32U)p_drv_data->DMA_PlaybackSilenceBufPtr) {
                                                                /* Free consumed audio samples playback buf.            */
                     USBD_Audio_PlaybackBufFree(p_drv_data->DMA_AsHandleTbl[ch], DEF_NULL);

                                                                /* Ensure stream is open.                               */
                     if (p_drv_data->PlaybackStreamStarted == DEF_TRUE) {
                                                                /* Signal playback task that rdy to consume a new buf.  */
                        USBD_Audio_PlaybackTxCmpl(p_drv_data->DMA_AsHandleTbl[ch]);
                     }
                 }
                                                                /*  See Note #1.                                        */
                 if (p_drv_data->PlaybackNxtXferReady == DEF_TRUE) {
                     p_dma_reg->channel[nxt_ch].enable = 1u;    /* Start next DMA xfer.                                 */
                 }
             }
             break;

        case DMA_CH_2:                                          /* ---------------------- RECORD ---------------------- */
             USBD_AUDIO_DRV_STAT_INC(p_drv_data->DMA_AsHandleTbl[ch], AudioDrv_Record_DMA_NbrXferCmpl);

                                                                /* Ack Finished int for current DMA ch.                 */
             p_dma_reg->irq_status_clear   = ((DMA_IRQ_MASK_HALF_WAY_CH | DMA_IRQ_MASK_FINISHED_CH) << (ch * 2u));
             p_dma_reg->channel[ch].enable =  0u;                /* Disable current DMA ch.                              */

                                                                /* --- NOTIFY AUDIO XFER COMPLETION TO AUDIO CLASS ---- */
             if (p_dma_reg->channel[ch].destination != (CPU_INT32U)DMA_RecordDummyBufPtr) {
                 USBD_Audio_RecordRxCmpl(p_drv_data->DMA_AsHandleTbl[ch]);
             }
                                                                /* ----------------- GET AN EMPTY BUF ----------------- */
             p_buf = (CPU_INT08U *)USBD_Audio_RecordBufGet(p_drv_data->DMA_AsHandleTbl[ch], &buf_len);
                                                                /* --------------- DMA XFER PREPARATION --------------- */
             if (p_buf != (CPU_INT08U *)0) {
                 ch_destination = (CPU_INT32U)p_buf;
                 ch_len         =  buf_len;
             } else {
                 ch_destination = (CPU_INT32U)DMA_RecordDummyBufPtr;
                 ch_len         = (p_drv_data->AS_SamplingFreqTbl[Mic_OT_USB_IN_ID - 1u] / 1000u) * (UDA1380_BIT_RES / 8u) * UDA1380_MONO;
                 USBD_AUDIO_DRV_STAT_INC(p_drv_data->DMA_AsHandleTbl[ch], AudioDrv_Record_DMA_NbrDummyBuf);
             }
                                                                /* Verify if buf len multiple of words...               */
             if ((ch_len % DMA_CFG_XFER_SIZE_WORD) == 0u) {     /* ...YES: DMA xfer of words.                           */
                 ch_src           = I2S_RX1_REG_LEFT_32BIT_0;
                 ch_xfer_size     = DMA_CFG_XFER_SIZE_WORD;
                 ch_cfg_xfer_size = DMA_CFG_TX_WORD;
             } else {                                           /* ...NO: DMA xfer of half-words because bit...         */
                                                                /* ...resolution is 2 bytes.                            */
                 ch_src           = I2S_RX1_REG_LEFT_16BIT;
                 ch_xfer_size     = DMA_CFG_XFER_SIZE_HALF_WORD;
                 ch_cfg_xfer_size = DMA_CFG_TX_HWORD;
             }

             p_dma_reg->channel[ch].source      =  ch_src;
             p_dma_reg->channel[ch].destination =  ch_destination;
             p_dma_reg->channel[ch].length      = ((ch_len / ch_xfer_size) - 1u);
             p_dma_reg->channel[ch].counter     =  0u;
             p_dma_reg->channel[ch].config      = (ch_cfg_xfer_size                                |
                                                   DMA_CFG_RD_SLV_NR(DMA_REQ_SRC_I2SRX1_LEFT + 1u) |
                                                   DMA_CFG_WR_SLV_NR(0));

             p_dma_reg->channel[ch].enable = 1u;                /* Start DMA xfer.                                      */
             break;

        default:
             break;
    }
}
#endif


/*
*********************************************************************************************************
*                                 USBD_UDA1380_CircularBufStreamStore()
*
* Description : Store buffer information in the stream circular buffer.
*
* Argument(s) : p_circular_buf  Pointer to stream circular buffer.
*
*               p_buf           Pointer to buffer.
*
*               buf_len         Buffer length in bytes.
*
* Return(s)   : DEF_OK,   if NO error(s).
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_BOOLEAN  USBD_UDA1380_CircularBufStreamStore (USBD_AUDIO_DRV_CIRCULAR_BUF  *p_circular_buf,
                                                          CPU_INT08U                   *p_buf,
                                                          CPU_INT16U                    buf_len)
{
    CPU_INT16U  nxt_ix;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    nxt_ix = (p_circular_buf->IxIn + 1u) % USBD_AUDIO_DRV_CIRCULAR_AUDIO_BUF_SIZE;;
    if (nxt_ix == p_circular_buf->IxOut) {                      /* check if buffer full.                                */
        return (DEF_FAIL);
    }

    p_circular_buf->BufInfoTbl[p_circular_buf->IxIn].BufPtr = p_buf;
    p_circular_buf->BufInfoTbl[p_circular_buf->IxIn].BufLen = buf_len;
    p_circular_buf->IxIn                                    = nxt_ix;
    p_circular_buf->NbrBuf++;
    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                  USBD_UDA1380_CircularBufStreamGet()
*
* Description : Get buffer information from the stream circular buffer.
*
* Argument(s) : p_circular_buf  Pointer to stream circular buffer.
*
*               p_buf_len       Pointer to variable that will receive buffer length.
*
* Return(s)   : Pointer to buffer, if NO error(s).
*               NULL pointer,      otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (USBD_AUDIO_CFG_PLAYBACK_EN == DEF_ENABLED) || \
    (USBD_AUDIO_CFG_RECORD_EN   == DEF_ENABLED)
static  CPU_INT08U  *USBD_UDA1380_CircularBufStreamGet (USBD_AUDIO_DRV_CIRCULAR_BUF  *p_circular_buf,
                                                        CPU_INT16U                   *p_buf_len)
{
    CPU_INT08U  *p_buf;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    if (p_circular_buf->IxOut == p_circular_buf->IxIn) {        /* check if buffer empty.                               */
        return (DEF_NULL);
    }

    p_buf                 =  p_circular_buf->BufInfoTbl[p_circular_buf->IxOut].BufPtr;
   *p_buf_len             =  p_circular_buf->BufInfoTbl[p_circular_buf->IxOut].BufLen;
    p_circular_buf->IxOut = (p_circular_buf->IxOut + 1u) % USBD_AUDIO_DRV_CIRCULAR_AUDIO_BUF_SIZE;
    p_circular_buf->NbrBuf--;
    CPU_CRITICAL_EXIT();

    return (p_buf);
}
#endif

