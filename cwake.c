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
const uint8_t FEND  = 0xC0;
const uint8_t FESC  = 0xDB;
const uint8_t TFEND = 0xDC;
const uint8_t TFESC = 0xDD;
const uint8_t PREAMBLE = FEND;

const uint8_t CRC8_POLYNOMIAL = 0x31;

// sizes
const size_t WORK_BUFFER_SIZE    = 256;
const size_t STUFFER_BUFFER_SIZE = WORK_BUFFER_SIZE*2;

const size_t PREAMBLE_SIZE   = 1;
const size_t HEADER_SIZE     = 3;
const size_t CRC_SIZE        = 1;

// positions
const int16_t PREAMBLE_POS    = -1; // !!! PREAMBLE is not saved in buffer
const int16_t ADDR_POS        = 0;
const int16_t CMD_POS         = 1;
const int16_t SIZE_POS        = 2;
const int16_t DATA_POS        = 3;

// Global structures and variebles
DSTATIC uint8_t crc8_table       [256];


// ========================================================= Service functional
DSTATIC void reset_buffers(cwake_platform* platform)
{
    platform->service.stuffer_buffer_tail = 0;
    platform->service.work_buffer_tail = 0;
}

DSTATIC void set_next_state(cwake_state new_state, cwake_platform* platform)
{
    cwake_state current_state = platform->service.state;
    if(current_state != new_state)DEBUG_PRINT("change state %d to %d", current_state, new_state);

    switch (new_state) {
    case CWAKE_STATE_PENDING:
        if (current_state != CWAKE_STATE_PENDING){
            reset_buffers(platform);
            platform->service.start_pending_time = 0;
            platform->service.pending_size = PREAMBLE_SIZE;
        }
        break;
    case CWAKE_STATE_HEADER_RECEIVING:
        if ( platform->service.work_buffer_tail >= HEADER_SIZE)
            platform->service.pending_size = 0;
        else
            platform->service.pending_size = HEADER_SIZE
                - platform->service.work_buffer_tail;

        if (current_state != CWAKE_STATE_HEADER_RECEIVING){
            platform->service.start_pending_time = platform->current_time_ms();
        }
        break;
    case CWAKE_STATE_COMPLETE_RECEIVING:
        platform->service.pending_size = platform->service.work_buffer[SIZE_POS]
            + HEADER_SIZE + CRC_SIZE
            - platform->service.work_buffer_tail;
        break;
    }

    platform->service.state = new_state;
}

DSTATIC void reset_state(cwake_platform* platform)
{
    set_next_state(CWAKE_STATE_PENDING, platform);
}

DSTATIC uint8_t is_timeout(cwake_platform* platform) {
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

DSTATIC uint8_t get_crc8(uint8_t* data, uint8_t size, uint8_t crc)
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
            if (i + 1 >= src_len) return 0;// Check for complete sequence
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

DSTATIC cwake_error read_and_destuff(cwake_platform* platform)
{
    uint8_t* stuffer_buffer = platform->service.stuffer_buffer;
    uint32_t* stuffer_buffer_tail = &(platform->service.stuffer_buffer_tail);
    uint8_t prepare_incomplete_stuffing = 0;

    //read 2 x pending_size (full stuffed potential)
    uint8_t readed = platform->read(stuffer_buffer + *stuffer_buffer_tail,
                                    platform->service.pending_size);
    if (readed < 1) return CWAKE_ERROR_NONE;
    DEBUG_PRINT("Rx: %s", format_hex_ascii(stuffer_buffer + *stuffer_buffer_tail, readed));
    *stuffer_buffer_tail += readed;
    // check for PREAMBLE
    for (int i = 0; i < *stuffer_buffer_tail; ++i) {
        if ( stuffer_buffer[i] == PREAMBLE )
            return CWAKE_ERROR_INVALID_DATA;
    }

    // check for complete stuffing. discard last byte if incomplete
    if ( stuffer_buffer[*stuffer_buffer_tail-1] == FESC ) {
        prepare_incomplete_stuffing = 1;
        *stuffer_buffer_tail -= 1;
    }

    //destuff
    uint8_t* work_buffer = platform->service.work_buffer;
    uint32_t* work_buffer_tail = &(platform->service.work_buffer_tail);
    uint8_t destuffed = destuff(stuffer_buffer, *stuffer_buffer_tail,
                                work_buffer + *work_buffer_tail,
                                WORK_BUFFER_SIZE - *work_buffer_tail
                                );

    if ( destuffed == 0 ) {
        return CWAKE_ERROR_INVALID_DATA;
    }

    *work_buffer_tail += destuffed;

    //reset stuffer buffer
    if ( prepare_incomplete_stuffing == 1 ) {
        *stuffer_buffer_tail = 1;
        stuffer_buffer[0] = FESC;
    }
    else {
        *stuffer_buffer_tail = 0;
    }

    return CWAKE_ERROR_NONE;
}

// ========================================================== Public functional
cwake_error cwake_init(cwake_platform* platform)
{
    generate_crc8_table(CRC8_POLYNOMIAL);
    platform->service.state = 0;
    reset_state(platform);

    return CWAKE_ERROR_NONE;
}

cwake_error cwake_poll(cwake_platform* platform)
{
    cwake_error err = CWAKE_ERROR_NONE;

    if ( is_timeout(platform) ) {
        reset_state(platform);
        return CWAKE_ERROR_TIMEOUT;
    }

    switch (platform->service.state) {
    case CWAKE_STATE_PENDING:
        if (platform->read(platform->service.work_buffer,
                           platform->service.pending_size)
            ){
            DEBUG_PRINT("Rx: %s", format_hex_ascii(platform->service.work_buffer, platform->service.pending_size));
            if ( platform->service.work_buffer[0] == PREAMBLE ) {
                set_next_state(CWAKE_STATE_HEADER_RECEIVING, platform);
                return CWAKE_ERROR_NONE;
            }
            return CWAKE_ERROR_INVALID_DATA;
        }
        break;
    case CWAKE_STATE_HEADER_RECEIVING:
        err = read_and_destuff(platform);
        if ( err ) {
            reset_state(platform);
            return err;
        }

        set_next_state(CWAKE_STATE_HEADER_RECEIVING, platform); //update pending size
        // check for state complete
        if ( platform->service.pending_size == 0 ){
            if ( platform->service.work_buffer[SIZE_POS] >
                (WORK_BUFFER_SIZE - HEADER_SIZE - CRC_SIZE)
                ){
                reset_state(platform);
                return CWAKE_ERROR_INVALID_DATA;
            }
            set_next_state(CWAKE_STATE_COMPLETE_RECEIVING, platform);
        }

        break;
    case CWAKE_STATE_COMPLETE_RECEIVING:
        err = read_and_destuff(platform);
        if ( err ) {
            reset_state(platform);
            return err;
        }

        set_next_state(CWAKE_STATE_COMPLETE_RECEIVING, platform); //update pending size

        // check for state complete
        if ( platform->service.pending_size == 0 ) {
            // checking CRC
            uint8_t data[] = {FEND};
            if ( get_crc8(platform->service.work_buffer,
                          platform->service.work_buffer_tail,
                          get_crc8(data, 1, 0))
                ){
                reset_state(platform);
                return CWAKE_ERROR_CRC;
            }

            // filtering by ADDR
            if ( platform->addr != 0 &&
                platform->service.work_buffer[ADDR_POS] != 0 &&
                platform->service.work_buffer[ADDR_POS] != platform->addr
                ){
                reset_state(platform);
                return CWAKE_ERROR_NONE;
            }

            set_next_state(CWAKE_STATE_PENDING, platform);

            // call user handler
            uint8_t* return_buffer = NULL;
            uint8_t return_size = 0;
            platform->handle(platform->service.work_buffer[CMD_POS],
                             platform->service.work_buffer + DATA_POS,
                             platform->service.work_buffer[SIZE_POS],
                             &return_buffer,
                             &return_size
                            );

            // return user data to cwake_call if exist
            if (return_buffer && return_size > 0) {
                err = cwake_call(platform->addr,
                                 platform->service.work_buffer[CMD_POS],
                                 return_buffer, return_size, platform
                                 );
                if ( err ) {
                    reset_state(platform);
                    return err;
                }
            }
        }
        else {
            return CWAKE_ERROR_NONE;
        }
        break;
    }

    return CWAKE_ERROR_NONE;
}

cwake_error cwake_call(uint8_t addr, uint8_t cmd,
                       uint8_t* data, uint8_t size,
                       cwake_platform* platform)
{
    if ( platform->service.state != CWAKE_STATE_PENDING) {
        return CWAKE_ERROR_BUSY;
    }

    uint8_t* work_buffer = platform->service.work_buffer;
    uint32_t* work_buffer_tail = &(platform->service.work_buffer_tail);

    if (size > WORK_BUFFER_SIZE - HEADER_SIZE - CRC_SIZE) {
        return CWAKE_ERROR_INVALID_DATA;
    }

    work_buffer[(*work_buffer_tail)++] = FEND;
    work_buffer[(*work_buffer_tail)++] = addr;
    work_buffer[(*work_buffer_tail)++] = cmd;
    work_buffer[(*work_buffer_tail)++] = size;
    memcpy(work_buffer + *work_buffer_tail, data, size);
    *work_buffer_tail += size;
    work_buffer[*work_buffer_tail] = get_crc8(work_buffer, *work_buffer_tail, 0);
    *work_buffer_tail += 1;


    uint8_t* stuff_buffer = platform->service.stuffer_buffer;
    uint32_t* stuff_buffer_tail = &(platform->service.stuffer_buffer_tail);

    *stuff_buffer_tail = stuff(work_buffer, *work_buffer_tail, stuff_buffer);
    if ( *stuff_buffer_tail == 0 ) {
        return CWAKE_ERROR_INVALID_DATA;
    }
    DEBUG_PRINT("Tx: %s", format_hex_ascii(stuff_buffer, *stuff_buffer_tail));
    platform->write(stuff_buffer, *stuff_buffer_tail);

    reset_buffers(platform);

    return CWAKE_ERROR_NONE;
}

#undef DEBUG_PRINT
