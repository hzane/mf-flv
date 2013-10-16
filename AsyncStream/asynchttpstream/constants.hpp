#pragma once
static const uint32_t make_sure_negativei32 = 1ui32 << 31;
static const uint64_t make_sure_negative = 1ui64 << 63 ;
static const int64_t ok                       = 0;
static const int64_t e_tar_eof = -1;  // same EOF
static const int64_t e_tar_eos = -9;
static const int64_t e_tar_http_error = -10;
static const int64_t e_tar_aborted = -11;
static const int64_t e_tar_unknown = -12;
static const int64_t e_tar_null_content_range = -13;
static const int64_t e_tar_http_eos = -14;
static const int64_t e_tar_stream_range = -15;
static const int64_t e_tar_reading_overlapped = -16;
static const int64_t e_tar_httpstream_ex = -17;

static const int64_t http_request_timeout = 10; // seconds

static const uint32_t byte_per_ui64       = sizeof(uint64_t);
static const uint32_t bits_per_byte       = 8ui32;
static const uint32_t bits_container_size = byte_per_ui64 * bits_per_byte;


static const uint32_t _4k            = 4 * 1024ui32;
static const uint32_t _1m = 1024ui32 * 1024;
static const uint32_t _128m = 128 * 1024 * 1024ui32;
static const uint32_t tar_page_size  = _128m;
static const uint32_t tar_slice_size = _4k;

const static uint32_t _64k                 = 64 * 1024ui32;
const static uint32_t tar_max_downloadings = 2;
const static uint32_t tar_max_chunk_size   = _64k;
const static uint32_t tar_max_cache_size = _1m;

const static uint8_t content_fixed_length = 1ui8;
const static uint8_t content_undetermined_length = 0ui8;
const static uint8_t max_failed_times = 3;