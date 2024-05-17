/**
 *  \file posal_data_log_i.h
 * \brief
 *  	 Internal definitions needed for data logging utilit
 *
 * \copyright
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef POSAL_DATA_LOG_I_H
#define POSAL_DATA_LOG_I_H
/* ----------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------- */
// Needed for (PACK) definition
#include "ar_error_codes.h"
#include "posal_types.h"
#include "log.h"
#include "diagpkt.h"

//#define DATA_LOGGING_DBG

/* ----------------------------------------------------------------------------
 * Global Declarations/Definitions
 * ------------------------------------------------------------------------- */

/** Maximum size of the log packet. The maximum packet size that can be
  allocated is 6176 bytes.

  Sometimes, as per on-target logging, buffer allocation fails if this size
  is used. In these cases, set the log buffer size to one of the following
  values:

  @indent 4 x LCM(1, 2, 3, 4, 5, 6, 7, 8) = 3360 bytes

  The log buffer size must never be more than 6176 bytes per log packet. */
#define MAX_LOG_PKT_SIZE (3360)

/* Log container tags */
#define AUDIOLOG_CONTAINER_LOG_HEADER 0x0001      /** Container tag for the log header. */
#define AUDIOLOG_CONTAINER_USER_SESSION 0x0002    /** Container tag for the user session. */
#define AUDIOLOG_CONTAINER_PCM_DATA_FORMAT 0x0004 /** Container tag for the PCM data format. */
#define AUDIOLOG_CONTAINER_BS_DATA_FORMAT 0x0005  /** Container tag for the bitstream data format. */

#define LOGGING_MAX_NUM_CH 32 /** Maximum number of channels supported for multichannel PCM logging. */

/** POSAL log codes for audio/voice data path logging. */
#define POSAL_LOG_CODE_UNUSED 0x0            /**< Unused. */
#define POSAL_LOG_CODE_AUD_DEC_IN 0x152E     /**<  audio decoder input. */
#define POSAL_LOG_CODE_AUD_POPP_IN 0x152F    /**<  audio POPP input. */
#define POSAL_LOG_CODE_AUD_COPP_IN 0x1531    /**<  audio COPP input. */
#define POSAL_LOG_CODE_AUD_POPREP_IN 0x1534  /**<  audio POPREP input. */
#define POSAL_LOG_CODE_AUD_COPREP_IN 0x1532  /**<  audio COPREP input. */
#define POSAL_LOG_CODE_AUD_MTMX_RX_IN 0x1530 /**<  audio matrix Rx input. */
#define POSAL_LOG_CODE_AUD_MTMX_TX_IN 0x1533 /**<  audio matrix Tx input. */
#define POSAL_LOG_CODE_AUD_ENC_IN 0x1535     /**<  audio encoder input. */
#define POSAL_LOG_CODE_AUD_ENC_OUT 0x1536    /**<  audio encoder output. */
#define POSAL_LOG_CODE_AFE_RX_TX_OUT 0x1586  /**<  AFE Rx output and AFE Tx input. */
#define POSAL_LOG_CODE_VPTX 0x158A           /**<  vptx near and far input and output. */
#define POSAL_LOG_CODE_VPRX 0x158A           /**< Logs PCM data at vprx output. */
#define POSAL_LOG_CODE_VOC_ENC_IN 0x158B     /**<  voice encoder input  */
#define POSAL_LOG_CODE_VOC_PKT_OUT 0x14EE    /**<  voice encoder output . */
#define POSAL_LOG_CODE_VOC_PKT_IN 0x14EE     /**<  voice decoder input. */
#define POSAL_LOG_CODE_VOC_DEC_OUT 0x158B    /**<  voice decoder output. */
#define POSAL_LOG_CODE_AFE_ALGO_CALIB 0x17D8 /**< Logs run-time parameters of algorithms in AFE. */
#define POSAL_LOG_CODE_PP_RTM 0x184B         /**< Logs RTM parameters of algorithms in PP. */
#define POSAL_LOG_CODE_LSM_OUTPUT 0x1882     /**<  LSM output. */
#define POSAL_LOG_CODE_AUD_DEC_OUT 0x19AF    /**<  audio Decoder output. */
#define POSAL_LOG_CODE_AUD_COPP_OUT 0x19B0   /**<  audio COPP output. */
#define POSAL_LOG_CODE_AUD_POPP_OUT 0x19B1   /**<  audio POPP output. */

#define SEG_PKT (0xDEADBEEF) /** Magic number for indicating the segmented packet. */

enum posal_interleaving_t
{
   POSAL_INTERLEAVED,
   /**< Data for all channels is present in one buffer. The samples are
        interleaved per channel. */

   POSAL_DEINTERLEAVED_PACKED,
   /**< Data for all channels is present in one buffer. All of the samples of
        one channel are present, followed by all of the samples of
        the next channel, etc. */

   POSAL_DEINTERLEAVED_UNPACKED,
   /**< Data for each channel is present in a different buffer. In this case,
        the capi_stream_data_t::buf_ptr field points to an array of buffer
        structures with the number of elements equal to the number of channels.
        The bufs_num field must be set to the number of channels. */

   POSAL_INVALID_INTERLEAVING = 0xFFFFFFFF
   /**< Interleaving is not valid. */
};
/*==========================================================================
  Structure definitions
  ========================================================================== */
/* Overall log packet structure
  For PCM data, the containers are in following order:
    AUDIOLOG_HEADER
    AUDIOLOG_USER_SESSION
    AUDIOLOG_PCM_DATA_FORMAT

  For bitsream data,
    AUDIOLOG_HEADER
    AUDIOLOG_USER_SESSION
    AUDIOLOG_BITSTREAM_DATA_FORMAT
  */

/** User session container.  */
typedef struct __attribute__((__packed__))
{
   uint16_t size;
   /**< Size information for this container. */

   uint16_t tag;
   /**< Tag information for this container. */

   uint32_t user_session_id;
   /**< User session ID. Currently not used; set this parameter to zero. */

   uint64_t time_stamp;
   /**< Timestamp (in microseconds) for the log packet. */
}
audiolog_user_session_t;

/** Audio log PCM data format container. */
typedef struct __attribute__((__packed__))
{
   uint16_t                        minor_version;
   /**< minor version of pcm header for this container. */

   uint16_t                       reserved;
   /**< reserved field for 4 byte alignment */

   uint16_t size;
   /**< Size information for this container. */

   uint16_t tag;
   /**< Tag information for this container. */

   uint32_t log_tap_id;
   /**< GUID for the tap point. */

   uint32_t sampling_rate;
   /**< PCM sampling rate (8000 Hz, 48000 Hz, etc.). */

   uint16_t num_channels;
   /**< Number of channels in the PCM stream. */

   uint8_t pcm_width;
   /**< Bits per sample for the PCM data. */

   uint8_t interleaved;
   /**< Specifies whether the data is interleaved. */

   uint8_t channel_mapping[LOGGING_MAX_NUM_CH];
   /**< Array of channel mappings. @newpagetable */
}
audiolog_pcm_data_format_t;

/** Audio log bitstream data format container. */
typedef struct __attribute__((__packed__))
{
   uint16_t size;
   /**< Size information for this container. */

   uint16_t tag;
   /**< Tag information for this container. */

   uint32_t log_tap_id;
   /**< GUID for the tap point. */

   uint32_t media_fmt_id;
   /**< Media format ID for the audio/voice encoder/decoder. */
}
audiolog_bitstream_data_format_t;

/** Common header structure for PCM/bitstream data. */
typedef struct __attribute__((__packed__))
{
   uint16_t size;
   /**< Size information for this container. */

   uint16_t tag;
   /**< Tag information for this container. */

   uint32_t log_session_id;
   /**< Log session ID. */

   uint32_t log_seg_number;
   /**< Log segment number. Currently not used; set this field to zero. */

   uint32_t segment_size;
   /**< Size in bytes of the payload, excluding the header. */

   uint32_t fragment_offset;
   /**< Fragment offset. Currently not used. */

   audiolog_user_session_t user_session_info;
   /**< Audio log user session. */
}
audiolog_header_cmn_t;

/** Audio log header for PCM data. */
typedef struct __attribute__((__packed__))
{
   audiolog_header_cmn_t cmn_struct;
   /**< Common header structure for PCM/bitstream data. */

   audiolog_pcm_data_format_t pcm_data_fmt;
   /**< Audio log PCM data format. */
}
audiolog_header_pcm_data_t;

/** Audio log header for bitstream data. */
typedef struct __attribute__((__packed__))
{
   audiolog_header_cmn_t cmn_struct;
   /**< Common header structure for PCM/bitstream data. */

   audiolog_bitstream_data_format_t bs_data_fmt;
   /**< Audio log bitstream data format. */
}
audiolog_header_bitstream_data_t;

/** Complete log packet structure for logging PCM data. */
typedef struct __attribute__((__packed__))
{
   log_hdr_type hdr;
   /**< Length of the packet in bytes, including this header, the QXDM Pro log
        code, and system timestamp. Each log packet must begin with a
        member of type log_hdr_type. */

   audiolog_header_pcm_data_t log_header_pcm;
   /**< Audio log header for PCM data. */

   uint8_t payload[1];
   /**< Logged PCM data. */
}
log_pkt_pcm_data_t;

/** Complete log packet structure for logging bitstream data. */
typedef struct __attribute__((__packed__))
{
   log_hdr_type hdr;
   /**< Length of the packet in bytes including this header, the QXDM Pro log
        code, and system timestamp. Each log packet must begin with a
        member of type log_hdr_type. */

   audiolog_header_bitstream_data_t log_header_bs;
   /**< Audio log header for bitstream data. */

   uint8_t payload[1];
   /**< logged bitstream data. */
}
log_pkt_bitstream_data_t;

/** Complete log packet structure for logging raw data. */
typedef struct __attribute__((__packed__))
{
   log_hdr_type hdr;
   /**< Length of the packet in bytes including this header, the QXDM Pro log
        code, and system timestamp. Each log packet must begin with a
        member of type log_hdr_type. */

   uint8_t payload[1];
   /**< logged bitstream data. */
}
log_pkt_raw_data_t;


typedef union log_pkt_header_internal_t
{
   log_pkt_pcm_data_t       pcm;
   log_pkt_bitstream_data_t bitstream;
   log_pkt_raw_data_t       raw;

} log_pkt_header_internal_t;


#endif // POSAL_DATA_LOG_I_H
