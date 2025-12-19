/**
 * @file mock.c
 * @brief mock implementation for tests
 * @author Qvafir <qvafir@outlook.com>
 * @copyright MIT License, see repository LICENSE file
 */

#include <string.h>

#include "mock.h"

uint8_t mock_tx_buffer[512];
uint8_t mock_rx_buffer[512];
uint32_t mock_tx_index = 0;
uint32_t mock_rx_index = 0;
uint32_t mock_rx_start = 0;
uint32_t mock_time_ms = 0;
uint8_t mock_called_cmd = 0;
uint8_t mock_rd_buffer[512];

uint8_t mock_read(uint8_t* buf, uint8_t count) {
    if (mock_rx_index <= mock_rx_start) {
        mock_rx_index = 0;
        mock_rx_start = 0;
        return 0;
    }

    uint8_t available = mock_rx_index - mock_rx_start;

    if ( count <= available ) {
        memcpy(buf, mock_rx_buffer + mock_rx_start, count);
        mock_rx_start += count;
        return count;
    }


    memcpy(buf, mock_rx_buffer + mock_rx_start, available);
    mock_rx_index = 0;
    mock_rx_start = 0;
    return available;
}

uint8_t mock_write(uint8_t* buf, uint8_t count) {

    memcpy(mock_tx_buffer, buf, count);
    mock_tx_index = count;
    return count;
}

uint32_t mock_time_ms_func() {
    return mock_time_ms;
}

int32_t mock_handle(uint8_t cmd, uint8_t* data, uint8_t size,
                    uint8_t** rdata, uint8_t* rsize) {
    mock_called_cmd = cmd;
    if (cmd == 0xCF) {
        char* retdata = "Hello world!";
        memcpy(mock_rd_buffer, retdata, strlen(retdata));
        *rdata = mock_rd_buffer;
        *rsize = strlen(retdata);
    }

    return 0;
}

void mock_reset_buffers() {
    mock_tx_index = 0;
    mock_rx_index = 0;
    mock_rx_start = 0;
    mock_time_ms = 0;
    memset(mock_tx_buffer, 0, sizeof(mock_tx_buffer));
    memset(mock_rx_buffer, 0, sizeof(mock_rx_buffer));
}
