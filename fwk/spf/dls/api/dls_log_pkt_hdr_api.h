#ifndef _DLS_LOG_PKT_HDR_API_H_
#define _DLS_LOG_PKT_HDR_API_H_
/**
 * \file dls_log_pkt_hdr_api.h
 * \brief
 *   This file contains data structures for log packet header.
 *
 * \copyright
 *    Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *    SPDX-License-Identifier: BSD-3-Clause
 */

/*------------------------------------------------------------------------------
 *  Include Files
 *----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C"
{
#endif /*__cplusplus*/

/*------------------------------------------------------------------------------
 *  Global Definitions/Declarations
 *----------------------------------------------------------------------------*/
#define DLS_PACKED      __attribute__((packed))

//TODO: Support the list of buffers to commit and return
/* Overall log packet structure
  For PCM data, the containers are in following order:
    DLS_HEADER
    DLS_USER_SESSION
    DLS_PCM_DATA_FORMAT

  For bitsream data,
    DLS_HEADER
    DLS_USER_SESSION
    DLS_BITSTREAM_DATA_FORMAT
  */

/** User session container.  */
struct dls_user_session_t
{
   uint16_t size;
   /**< Size information for this container. */

   uint16_t tag;
   /**< Tag information for this container. */

   uint32_t user_session_id;
   /**< User session ID. Currently not used; set this parameter to zero. */

   uint32_t time_stamp_lsw;
   /**< Lower 32-bits of timestamp (in microseconds) for the log packet. */

   uint32_t time_stamp_msw;
   /**< Upper 32-bits of timestamp (in microseconds) for the log packet. */
}
DLS_PACKED
;
typedef struct dls_user_session_t dls_user_session_t;

/** Maximum number of channels supported for multichannel PCM logging. */
#define LOGGING_MAX_NUM_CH 32

/** Audio log PCM data format container. */
struct dls_pcm_data_format_t
{
   uint16_t minor_version;
   /**< minor version of pcm header for this container. */

   uint16_t reserved;
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
DLS_PACKED
;
typedef struct dls_pcm_data_format_t dls_pcm_data_format_t;

/** Audio log bitstream data format container. */
struct dls_bitstream_data_format_t
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
DLS_PACKED
;
typedef struct dls_bitstream_data_format_t dls_bitstream_data_format_t;

/** Common header structure for PCM/bitstream data. */
struct dls_header_cmn_t
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

   dls_user_session_t user_session_info;
   /**< Audio log user session. */
}
DLS_PACKED
;
typedef struct dls_header_cmn_t dls_header_cmn_t;

/** Audio log header for PCM data. */
struct dls_header_pcm_data_t
{
   dls_header_cmn_t cmn_struct;
   /**< Common header structure for PCM/bitstream data. */

   dls_pcm_data_format_t pcm_data_fmt;
   /**< Audio log PCM data format. */
}
DLS_PACKED
;
typedef struct dls_header_pcm_data_t dls_header_pcm_data_t;

/** Audio log header for bitstream data. */
struct dls_header_bitstream_data_t
{
   dls_header_cmn_t cmn_struct;
   /**< Common header structure for PCM/bitstream data. */

   dls_bitstream_data_format_t bs_data_fmt;
   /**< Audio log bitstream data format. */
}
DLS_PACKED
;
typedef struct dls_header_bitstream_data_t dls_header_bitstream_data_t;

/*! log_hdr_type ,includes length, code and timestamp */
struct dls_log_hdr_type
{
  uint32_t len;
  /*!< Specifies the length, in bytes of the
				   entry, including this header. */

  uint32_t code;
  /*!<  Specifies the log code for the entry as
				   enumerated above. Note: This is
				   specified as word to guarantee size. */

  uint32_t ts_lsw;
  /*!< The lower 32-bits of the system timestamp for the log entry. The
				   upper 48 bits represent elapsed time since
				   6 Jan 1980 00:00:00 in 1.25 ms units. The
				   low order 16 bits represent elapsed time
				   since the last 1.25 ms tick in 1/32 chip
				   units (this 16 bit counter wraps at the
				   value 49152). */
  uint32_t ts_msw;
  /*!< The upper 32-bits of the system timestamp for the log entry */
}
DLS_PACKED
;
typedef struct dls_log_hdr_type dls_log_hdr_type;

/** Complete log packet structure for logging PCM data. */
struct dls_log_pkt_pcm_data_t
{
   dls_log_hdr_type hdr;
   /**< Length of the packet in bytes, including this header, the QXDM Pro log
        code, and system timestamp. Each log packet must begin with a
        member of type log_hdr_type. */

   dls_header_pcm_data_t log_header_pcm;
   /**< Audio log header for PCM data. */

   uint8_t payload[1];
   /**< Logged PCM data. */
}
DLS_PACKED
;
typedef struct dls_log_pkt_pcm_data_t dls_log_pkt_pcm_data_t;

/** Complete log packet structure for logging bitstream data. */
struct dls_log_pkt_bitstream_data_t
{
   dls_log_hdr_type hdr;
   /**< Length of the packet in bytes including this header, the QXDM Pro log
        code, and system timestamp. Each log packet must begin with a
        member of type log_hdr_type. */

   dls_header_bitstream_data_t log_header_bs;
   /**< Audio log header for bitstream data. */

   uint8_t payload[1];
   /**< logged bitstream data. */
}
DLS_PACKED
;
typedef struct dls_log_pkt_bitstream_data_t dls_log_pkt_bitstream_data_t;

/** Complete log packet structure for logging raw data. */

struct dls_log_pkt_raw_data_t
{
   dls_log_hdr_type hdr;
   /**< Length of the packet in bytes including this header, the QXDM Pro log
        code, and system timestamp. Each log packet must begin with a
        member of type log_hdr_type. */
   uint8_t payload[1];
   /**< logged bitstream data. */
}
DLS_PACKED
;
typedef struct dls_log_pkt_raw_data_t dls_log_pkt_raw_data_t;

/** Log container tags */
/** Container tag for the log header. */
#define DLS_AUDIOLOG_CONTAINER_LOG_HEADER        0x0001

/** Container tag for the user session. */
#define DLS_AUDIOLOG_CONTAINER_USER_SESSION      0x0002

/** Container tag for the PCM data format. */
#define DLS_AUDIOLOG_CONTAINER_PCM_DATA_FORMAT   0x0004

/** Container tag for the bitstream data format. */
#define DLS_AUDIOLOG_CONTAINER_BS_DATA_FORMAT    0x0005

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif // #ifdef _DLS_LOG_PKT_HDR_API_H_
