/**
 * @file cwake.h
 * @brief CWAKE implementation
 * @author Qvafir <qvafir@outlook.com>
 * @copyright MIT License, see repository LICENSE file
 */

#ifndef CWAKE_H
#define CWAKE_H
#include <stdint.h>

typedef enum cwake_error {
    CWAKE_ERROR_NONE         = 0,
    CWAKE_ERROR_TIMEOUT      = -1,
    CWAKE_ERROR_CRC          = -2,
    CWAKE_ERROR_INVALID_DATA = -3,
    CWAKE_ERROR_OVERFLOW     = -4,
    CWAKE_ERROR_BUSY         = -5
} cwake_error;

struct cwake_service {
    uint32_t start_pending_time;
    //new line buffers
    uint8_t buffer_rxenc[256*2];    // encoded received data (raw)
    uint8_t buffer_rxdec[256];      // decoded received data
    uint8_t buffer_txenc[256*2];    // encoded transmitting data
    uint8_t buffer_txdec[256];      // decoded transmitting data (raw)
    //uint8_t* buffer_rxenc_fstart;    //
    uint8_t* buffer_rxenc_dstart;       // stored data start
    uint8_t* buffer_rxenc_dend;         // stored data end
    uint8_t* buffer_rxdec_dend;         // stored data end

    uint8_t uncomplete_fesc_is_reserved;
};

typedef struct cwake_platform {
    uint8_t     addr;
    uint32_t    timeout_ms;
    uint8_t     (*read) (uint8_t* buf, uint32_t count);
    uint8_t     (*write) (uint8_t* buf, uint32_t count);
    uint32_t    (*current_time_ms) ();
    int32_t     (*handle) (uint8_t cmd,
                           uint8_t* data, uint8_t size,
                           uint8_t** rdata, uint8_t* rsize
                           );
    struct cwake_service service;
} cwake_platform;


/**
 * @brief Initialize cWAKE protocol platform
 *
 * @param platform Pointer to cwake_platform structure object
 * @return cwake_error Error code (CWAKE_ERROR_NONE == 0).
 */
cwake_error cwake_init(cwake_platform* platform);

/**
 * @brief Polling data transfer interface (using read/write callbacks)
 *
 * @param platform Pointer to cwake_platform structure object
 * @return cwake_error Error code (CWAKE_ERROR_NONE == 0).
 */
cwake_error cwake_poll(cwake_platform* platform);

/**
 * @brief Send command with potential data to server
 *
 * @param addr Server address
 * @param cmd Command code
 * @param data Pointer to data
 * @param size Size of data array
 * @param platform Pointer to cwake_platform structure object
 * @return cwake_error Error code (CWAKE_ERROR_NONE == 0).
 */
cwake_error cwake_call(uint8_t addr, uint8_t cmd,
                       uint8_t* data, uint8_t size,
                       cwake_platform* platform);



//make internal implementations public for test
#ifdef CWAKE_TEST
#include <string.h>
// WAKE protocol specific codes
extern const uint8_t FEND;
extern const uint8_t FESC;
extern const uint8_t TFEND;
extern const uint8_t TFESC;
extern const uint8_t PREAMBLE;
extern const uint8_t CRC8_POLYNOMIAL;

// sizes
extern const size_t WORK_BUFFER_SIZE;
extern const size_t STUFFER_BUFFER_SIZE;
extern const size_t PREAMBLE_SIZE;
extern const size_t HEADER_SIZE;
extern const size_t CRC_SIZE;

// positions
extern const int16_t PREAMBLE_POS; // !!! PREAMBLE is not saved in buffer
extern const int16_t ADDR_POS;
extern const int16_t CMD_POS;
extern const int16_t SIZE_POS;
extern const int16_t DATA_POS;

void reset_buffers(cwake_platform* platform);
void reset_state(cwake_platform* platform);
uint8_t is_timeout(cwake_platform* platform);
void generate_crc8_table(uint8_t polynomial);
uint8_t get_crc8(uint8_t* data, uint8_t size, uint8_t crc);
size_t stuff(const uint8_t* src, uint8_t src_len, uint8_t* dst);
size_t destuff(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_max_len);
cwake_error read_and_destuff(cwake_platform* platform);

#endif
#endif // CWAKE_H
