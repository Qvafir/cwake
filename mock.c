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

uint32_t handle_counter = 0;

uint8_t mock_dummy_rw(uint8_t* buf, uint32_t count)
{
    return count;
}

int32_t mock_dummy_handle(uint8_t cmd, uint8_t* data, uint8_t size,
                    uint8_t** rdata, uint8_t* rsize)
{
    handle_counter += 1;
    return 0;
}

uint8_t mock_reread(uint8_t* buf, uint32_t count) {
    if (mock_rx_index <= mock_rx_start) {
        mock_rx_start = 0;
    }

    uint32_t available = mock_rx_index - mock_rx_start;

    if ( count < available ) {
        memcpy(buf, mock_rx_buffer + mock_rx_start, count);
        mock_rx_start += count;
        return count;
    }


    memcpy(buf, mock_rx_buffer + mock_rx_start, available);
    mock_rx_start += available;
    return available;
}

uint8_t mock_read(uint8_t* buf, uint32_t count) {
    if (mock_rx_index <= mock_rx_start) {
        mock_rx_index = 0;
        mock_rx_start = 0;
        return 0;
    }

    uint8_t available = mock_rx_index - mock_rx_start;

    if ( count < available ) {
        memcpy(buf, mock_rx_buffer + mock_rx_start, count);
        mock_rx_start += count;
        return count;
    }


    memcpy(buf, mock_rx_buffer + mock_rx_start, available);
    mock_rx_index = 0;
    mock_rx_start = 0;
    return available;
}

uint8_t mock_write(uint8_t* buf, uint32_t count) {

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

    handle_counter += 1;
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

cwake_platform mock_create_cwake_platform(uint8_t addr, uint32_t timeout) {
    cwake_platform platform = {
        .addr = addr,
        .timeout_ms = timeout,
        .read = mock_read,
        .write = mock_write,
        .current_time_ms = mock_time_ms_func,
        .handle = mock_handle
    };
    return platform;
}
