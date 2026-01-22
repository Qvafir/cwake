/**
 * @file cwake.c
 * @brief CWAKE implementation
 * @author Qvafir <qvafir@outlook.com>
 * @copyright MIT License, see repository LICENSE file
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cwake.h"

#ifndef CWAKE_TEST
#define DSTATIC static
#else
#define DSTATIC
#endif

#ifdef CWAKE_DEBUG_OUTPUT
#include <ctype.h>
extern void cwake_debug_print(const char *format, ...);
#define DEBUG_PRINT(msg, ...) cwake_debug_print(msg, ##__VA_ARGS__)
static char* format_hex_ascii(const unsigned char *data, size_t size) {
    static char out_str[2042];
    const size_t out_max = sizeof(out_str);

    size_t pos = 0;
    for (size_t i = 0; i < size; i++)
        pos += snprintf(out_str + pos, out_max - pos, "%02X ", data[i]);
    pos += snprintf(out_str + pos, out_max - pos, "| ");
    for (size_t i = 0; i < size; i++) {
        char c = isprint(data[i]) ? data[i] : '.';
        pos += snprintf(out_str + pos, out_max - pos, "%c", c);
    }
    out_str[pos] = 0;
    return out_str;
}
#else
#define DEBUG_PRINT(msg, ...)
#endif

// =============================================================== Declarations
// WAKE protocol specific codes
DSTATIC const uint8_t FEND  = 0xC0;
DSTATIC const uint8_t FESC  = 0xDB;
DSTATIC const uint8_t TFEND = 0xDC;
DSTATIC const uint8_t TFESC = 0xDD;
DSTATIC const uint8_t PREAMBLE = FEND;

DSTATIC const uint8_t CRC8_POLYNOMIAL = 0x31;

// sizes
DSTATIC const size_t WORK_BUFFER_SIZE    = 256;
DSTATIC const size_t STUFFER_BUFFER_SIZE = WORK_BUFFER_SIZE*2;

DSTATIC const size_t PREAMBLE_SIZE   = 1;
DSTATIC const size_t HEADER_SIZE     = 3;
DSTATIC const size_t CRC_SIZE        = 1;

// positions
DSTATIC const int16_t PREAMBLE_POS    = -1; // !!! PREAMBLE is not saved in buffer
DSTATIC const int16_t ADDR_POS        = 0;
DSTATIC const int16_t CMD_POS         = 1;
DSTATIC const int16_t SIZE_POS        = 2;
DSTATIC const int16_t DATA_POS        = 3;

// Global structures and variebles
DSTATIC uint8_t crc8_table       [256];


// ========================================================= Service functional
static inline void reset_buffer_rxenc(cwake_platform* platform)
{
    platform->service.buffer_rxenc_dend = platform->service.buffer_rxenc;
    platform->service.buffer_rxenc_dstart = platform->service.buffer_rxenc;
}
static inline  void reset_buffer_rxdec(cwake_platform* platform)
{
    platform->service.buffer_rxdec_dend = platform->service.buffer_rxdec;
}

static inline  void start_timeout_timer(cwake_platform* platform)
{
    platform->service.start_pending_time = platform->current_time_ms();
}

static inline  void stop_timeout_timer(cwake_platform* platform)
{
    platform->service.start_pending_time = 0;
}

DSTATIC uint8_t is_timeout(cwake_platform* platform)
{
    uint32_t start = platform->service.start_pending_time;

    if ( start == 0) return 0; //timer is not started

    uint32_t current = platform->current_time_ms();
    uint32_t passed = 0;

    //overflow checking
    if (current < start) { passed = UINT32_MAX - start + current; }
    else                 { passed = current - start; }

    if ( passed > platform->timeout_ms) return 1;

    return 0;
}

// CRC-8
DSTATIC void generate_crc8_table(uint8_t polynomial)
{
    for (int i = 0; i < 256; i++) {
        uint8_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
        crc8_table[i] = crc;
    }
}

DSTATIC inline uint8_t get_crc8(uint8_t* data, uint8_t size, uint8_t crc)
{
    for (size_t i = 0; i < size; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

// BYTESTUFFING
DSTATIC size_t stuff(const uint8_t* src, uint8_t src_len, uint8_t* dst) {
    size_t dst_len = 0;

    if(src_len > 0) dst[dst_len++] = src[0];

    for (uint8_t i = 1; i < src_len; i++) {
        uint8_t current_byte = src[i];

        if (current_byte == FEND) {
            dst[dst_len++] = FESC;
            dst[dst_len++] = TFEND; // 0xDC
        } else if (current_byte == FESC) {
            dst[dst_len++] = FESC;
            dst[dst_len++] = TFESC; // 0xDD
        } else {
            dst[dst_len++] = current_byte;
        }
    }

    return dst_len;
}

DSTATIC size_t destuff(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_max_len) {
    size_t dst_len = 0;

    for (size_t i = 0; i < src_len; i++) {
        uint8_t current_byte = src[i];

        if (current_byte == FESC) {
            if (i + 1 >= src_len)
              return 0;// Check for complete sequence
            uint8_t next_byte = src[++i]; // Advance index past FESC

            if      (next_byte == TFEND) current_byte = FEND; // Restore the original FEND
            else if (next_byte == TFESC) current_byte = FESC; // Restore the original FESC
            else    return 0;            // Invalid sequence after FESC, handle error
        }
        // Destination buffer overflow check
        if   (dst_len < dst_max_len) dst[dst_len++] = current_byte;
        else return 0;
    }
    return dst_len;
}

DSTATIC cwake_error _poll_handle_return(
    cwake_error err_if_complete,
    cwake_error err_if_continue,
    cwake_platform* platform, uint32_t str_number)
{
    if(platform->service.buffer_rxenc_dstart >=   //if rx encoded buffer is empty
        platform->service.buffer_rxenc_dend) {

        reset_buffer_rxenc(platform);

        if (platform->service.uncomplete_fesc_is_reserved) {
            *platform->service.buffer_rxenc_dend = FESC;
            platform->service.buffer_rxenc_dend += 1;
        }

        if (err_if_complete != CWAKE_ERROR_NONE) {
            DEBUG_PRINT("Error in complete rx buffer handling: %d/%d", str_number, err_if_complete );
        }
        return err_if_complete;
    }
    else {                                        //if rx encoded buffer contains data
        if (err_if_continue != CWAKE_ERROR_NONE) {
            DEBUG_PRINT("Error in continue rx buffer handling: %d/%d", str_number, err_if_continue );
        }
        return err_if_continue;
    }
}

#define poll_handle_return(err1, err2, p) _poll_handle_return(err1, err2, p, __LINE__)

// ========================================================== Public functional
cwake_error cwake_init(cwake_platform* platform)
{
    generate_crc8_table(CRC8_POLYNOMIAL);
    reset_buffer_rxenc(platform);
    reset_buffer_rxdec(platform);


    return CWAKE_ERROR_NONE;
}

cwake_error cwake_poll(cwake_platform* platform)
{
    struct cwake_service* ps = &platform->service;

    // ==== RECEIVING ====
    // read external rx buffer if internal rxenc buffer is empty
    if( (ps->buffer_rxenc + ps->uncomplete_fesc_is_reserved) == ps->buffer_rxenc_dend ) {//rxenc buffer is empty
        if ( is_timeout(platform) ) {
            reset_buffer_rxdec(platform);
            return CWAKE_ERROR_TIMEOUT;
        }

        uint32_t received = platform->read(ps->buffer_rxenc_dend, UINT8_MAX); //TODO use uint32_t for read max size

        if (received){
            DEBUG_PRINT("Rx: %s", format_hex_ascii(platform->service.buffer_rxenc_dend, received));
            ps->buffer_rxenc_dend += received;
            stop_timeout_timer(platform);
        }
        else return CWAKE_ERROR_NONE;


        //incomplete escape sequence skip and reservation
        if(*(ps->buffer_rxenc_dend-1) == FESC ) {
            ps->buffer_rxenc_dend -= 1;
            ps->uncomplete_fesc_is_reserved = 1;
        }
        else {
            ps->uncomplete_fesc_is_reserved = 0;
        }
    }

    // ==== FRAMING ====
    uint8_t* buffer_rxenc_fstart = ps->buffer_rxenc_dstart;//frame start
    uint8_t* buffer_rxenc_fend = 0;  //frame end


    //skip first bytes if preamble (search msg frame start)
    while(*buffer_rxenc_fstart == PREAMBLE) buffer_rxenc_fstart += 1;

    buffer_rxenc_fend = buffer_rxenc_fstart;

    //search msg frame end (next preamble or global head)
    while(*buffer_rxenc_fend != PREAMBLE &&
           buffer_rxenc_fend < ps->buffer_rxenc_dend){
        buffer_rxenc_fend += 1;
    }

    ps->buffer_rxenc_dstart = buffer_rxenc_fend;

    //check for first byte in frame is preamble
    if(ps->buffer_rxdec_dend == ps->buffer_rxdec &&
            (*(buffer_rxenc_fstart-1) != PREAMBLE)
        ) {
        return poll_handle_return(CWAKE_ERROR_INVALID_DATA, CWAKE_ERROR_INVALID_DATA, platform);
    }

    // ==== DESTUFFING ====
    uint32_t frame_size = buffer_rxenc_fend - buffer_rxenc_fstart;
    uint32_t rxdec_buffer_size = 256 - (ps->buffer_rxdec_dend - ps->buffer_rxdec);

    uint8_t destuffed = destuff(buffer_rxenc_fstart, frame_size,
                                ps->buffer_rxdec_dend, rxdec_buffer_size
                                );
    if (destuffed == 0) {
        return poll_handle_return(CWAKE_ERROR_INVALID_DATA, CWAKE_ERROR_INVALID_DATA, platform);
    }
    ps->buffer_rxdec_dend += destuffed;

    // ==== VALIDATING ====
    uint32_t buffer_rxdec_stored = ps->buffer_rxdec_dend - ps->buffer_rxdec;
    //check complete header
    if ( buffer_rxdec_stored < HEADER_SIZE ) {
        if ( ps->buffer_rxenc_dstart >= ps->buffer_rxenc_dend ) start_timeout_timer(platform);      //TODO refactor
        return  poll_handle_return(CWAKE_ERROR_NONE, CWAKE_ERROR_INVALID_DATA, platform);
    }
    //check correct size code
    if (ps->buffer_rxdec[SIZE_POS] > (WORK_BUFFER_SIZE - PREAMBLE_SIZE - HEADER_SIZE - CRC_SIZE) ) {
        reset_buffer_rxdec(platform);
        return poll_handle_return(CWAKE_ERROR_INVALID_DATA, CWAKE_ERROR_INVALID_DATA, platform);
    }

    //check complete request
    if ( buffer_rxdec_stored < (ps->buffer_rxdec[SIZE_POS] + HEADER_SIZE + CRC_SIZE) ){
        if ( ps->buffer_rxenc_dstart >= ps->buffer_rxenc_dend ) start_timeout_timer(platform);      //TODO refactor
        return poll_handle_return(CWAKE_ERROR_NONE, CWAKE_ERROR_INVALID_DATA, platform);
    }

    //check crc
    uint8_t data[] = {FEND};
    if ( get_crc8(ps->buffer_rxdec, buffer_rxdec_stored, get_crc8(data, 1, 0)) ){
        reset_buffer_rxdec(platform);
        return poll_handle_return(CWAKE_ERROR_CRC, CWAKE_ERROR_CRC, platform);
    }

    //check addr (address filtering)
    if ( platform->addr != 0 &&
        platform->service.buffer_rxdec[ADDR_POS] != 0 &&
        platform->service.buffer_rxdec[ADDR_POS] != platform->addr
        ){
        reset_buffer_rxdec(platform);
        return poll_handle_return(CWAKE_ERROR_NONE, CWAKE_ERROR_NONE, platform);
    }

    // ==== HANDLING ====
    // call user handler
    uint8_t* return_buffer = NULL;
    uint8_t return_size = 0;
    platform->handle(platform->service.buffer_rxdec[CMD_POS],
                     platform->service.buffer_rxdec + DATA_POS,
                     platform->service.buffer_rxdec[SIZE_POS],
                     &return_buffer,
                     &return_size
                     );

    // return user data to cwake_call if exist
    if (return_buffer && return_size > 0) {
        cwake_error err = cwake_call(platform->addr,
                                     platform->service.buffer_rxdec[CMD_POS],
                                     return_buffer, return_size, platform
                                     );
        if ( err ){
            return poll_handle_return(err, err, platform);
        }
    }

    reset_buffer_rxdec(platform);
    return poll_handle_return(CWAKE_ERROR_NONE, CWAKE_ERROR_NONE, platform);
}

cwake_error cwake_call(uint8_t addr, uint8_t cmd,
                       uint8_t* data, uint8_t size,
                       cwake_platform* platform)
{
    //uint8_t* work_buffer = platform->service.work_buffer;
    uint8_t* work_buffer = platform->service.buffer_txdec;
    uint32_t work_buffer_tail = 0;

    if (size > WORK_BUFFER_SIZE - HEADER_SIZE - CRC_SIZE) {
        return CWAKE_ERROR_INVALID_DATA;
    }

    work_buffer[(work_buffer_tail)++] = FEND;
    work_buffer[(work_buffer_tail)++] = addr;
    work_buffer[(work_buffer_tail)++] = cmd;
    work_buffer[(work_buffer_tail)++] = size;
    memcpy(work_buffer + work_buffer_tail, data, size);
    work_buffer_tail += size;
    work_buffer[work_buffer_tail] = get_crc8(work_buffer, work_buffer_tail, 0);
    work_buffer_tail += 1;


    //uint8_t* stuff_buffer = platform->service.stuffer_buffer;
    uint8_t* stuff_buffer = platform->service.buffer_txenc;
    uint32_t stuff_buffer_tail = 0;

    stuff_buffer_tail = stuff(work_buffer, work_buffer_tail, stuff_buffer);
    if ( stuff_buffer_tail == 0 ) {
        return CWAKE_ERROR_INVALID_DATA;
    }
    DEBUG_PRINT("Tx: %s", format_hex_ascii(stuff_buffer, stuff_buffer_tail));
    platform->write(stuff_buffer, stuff_buffer_tail);

    return CWAKE_ERROR_NONE;
}

#undef DEBUG_PRINT
