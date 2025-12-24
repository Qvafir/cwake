/**
 * @file tests.c
 * @brief CWAKE tests implementation
 * @author Qvafir <qvafir@outlook.com>
 * @copyright MIT License, see repository LICENSE file
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cwake.h"
#include "mock.h"
#include "common.h"


static int pass_counter = 0;
static int total_counter = 0;

static cwake_platform create_test_platform(uint8_t addr, uint32_t timeout) {
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

void cwake_debug_print(const char *format, ...){
    va_list args;
    va_start(args, format);
    char msg[512];
    vsnprintf(msg, 512, format, args);
    log(msg);
    va_end(args);
}

static void test_packet_formation() {
    log("TEST packet formation...");
    total_counter+=1;


    cwake_platform platform = create_test_platform(0x01, 1000);
    cwake_init(&platform);



    uint8_t sample[] = {FEND, 0x01, FEND, 0x03, 0x23, FESC, 0x7F, 0};
    uint8_t expect[] = {FEND, 0x01, FESC, TFEND, 0x03, 0x23, FESC, TFESC, 0x7F, 0};
    sample[sizeof(sample)-1] = get_crc8(sample, sizeof(sample)-1, 0);
    expect[sizeof(expect)-1] = sample[sizeof(sample)-1];

    mock_reset_buffers();
    cwake_error err = cwake_call(sample[ADDR_POS+1],
                                 sample[CMD_POS+1],
                                 sample+DATA_POS+1,
                                 sample[SIZE_POS+1],
                                 &platform);
    ASSERT(err == CWAKE_ERROR_NONE);
    ASSERT(sizeof(expect) == mock_tx_index);
    ASSERT(!memcmp(expect, mock_tx_buffer, sizeof(expect)));

    pass_counter+=1;
    log("PASSED");
}

static void test_packet_reception() {
    log("TEST packet reception...");
    total_counter+=1;

    cwake_platform platform = create_test_platform(0x01, 10);
    cwake_init(&platform);

    //create packet sample for assertion
    mock_reset_buffers();
    uint8_t data[] = {0x23, FESC, 0x7F, 0x3F};
    cwake_error err = cwake_call(0x01,
                                 FEND,
                                 data,
                                 sizeof(data),
                                 &platform);
    ASSERT(err == CWAKE_ERROR_NONE);

    //put sample to mock_rx_buffer and set mock_rx_index
    memcpy(mock_rx_buffer, mock_tx_buffer, mock_tx_index);
    mock_rx_index = mock_tx_index;

    int count = 5;
    while (count) {
        count -= 1;
        err = cwake_poll(&platform);
        ASSERT(err == CWAKE_ERROR_NONE);
    }
    ASSERT(mock_called_cmd == FEND);
    mock_called_cmd = 0;

    //=== use another addr ===
    mock_reset_buffers();
    err = cwake_call(0xC5,
                     FEND,
                     data,
                     sizeof(data),
                     &platform);
    ASSERT(err == CWAKE_ERROR_NONE);

    //put sample to mock_rx_buffer and set mock_rx_index
    memcpy(mock_rx_buffer, mock_tx_buffer, mock_tx_index);
    mock_rx_index = mock_tx_index;

    count = 10;
    while (count) {
        count -= 1;
        err = cwake_poll(&platform);
        ASSERT(err == CWAKE_ERROR_NONE);
    }
    ASSERT(mock_called_cmd == 0);

    //=== use bad crc or data ===
    mock_reset_buffers();
    err = cwake_call(0x01,
                     FEND,
                     data,
                     sizeof(data),
                     &platform);
    ASSERT(err == CWAKE_ERROR_NONE);

    //put sample to mock_rx_buffer and set mock_rx_index
    memcpy(mock_rx_buffer, mock_tx_buffer, mock_tx_index);
    mock_rx_index = mock_tx_index;
    mock_rx_buffer[mock_rx_index-2] = 0xA3;

    count = 10;
    while (count) {
        count -= 1;
        err = cwake_poll(&platform);
        if (err) count = 0;
    }
    ASSERT(err == CWAKE_ERROR_CRC);
    ASSERT(mock_called_cmd == 0);

    //=== use bad stuffing ===
    mock_reset_buffers();
    err = cwake_call(0x01,
                     FEND,
                     data,
                     sizeof(data),
                     &platform);
    ASSERT(err == CWAKE_ERROR_NONE);

    //put sample to mock_rx_buffer and set mock_rx_index
    memcpy(mock_rx_buffer, mock_tx_buffer, mock_tx_index);
    mock_rx_index = mock_tx_index;
    mock_rx_buffer[mock_rx_index-3] = FESC;

    count = 10;
    while (count) {
        count -= 1;
        err = cwake_poll(&platform);
        if (err) count = 0;
    }
    ASSERT(err == CWAKE_ERROR_INVALID_DATA);
    ASSERT(mock_called_cmd == 0);

    //=== use bad preamble ===
    mock_reset_buffers();
    err = cwake_call(0x01,
                     FEND,
                     data,
                     sizeof(data),
                     &platform);
    ASSERT(err == CWAKE_ERROR_NONE);

    memcpy(mock_rx_buffer, mock_tx_buffer, mock_tx_index);
    mock_rx_index = mock_tx_index;
    mock_rx_buffer[0] = 0;

    count = 10;
    while (count) {
        count -= 1;
        err = cwake_poll(&platform);
        if (err) count = 0;
    }
    ASSERT(err == CWAKE_ERROR_INVALID_DATA);
    ASSERT(mock_called_cmd == 0);



    //=== filling a buffer with multiple packets with different CMD===
    mock_reset_buffers();
    err = cwake_call(0x01,
                     0x05,
                     data,
                     sizeof(data),
                     &platform);
    ASSERT(err == CWAKE_ERROR_NONE);

    memcpy(mock_rx_buffer, mock_tx_buffer, mock_tx_index);
    mock_rx_index = mock_tx_index;
    mock_tx_index = 0;

    err = cwake_call(0x01,
                     0x15,
                     data,
                     sizeof(data),
                     &platform);
    ASSERT(err == CWAKE_ERROR_NONE);

    memcpy(mock_rx_buffer + mock_rx_index, mock_tx_buffer, mock_tx_index);
    mock_rx_index += mock_tx_index;

    count = 10;
    while (count) {
        count -= 1;
        err = cwake_poll(&platform);
        if (err) count = 0;
    }
    ASSERT(err == CWAKE_ERROR_NONE);
    ASSERT(mock_called_cmd == 0x15);

    //=== break down by stuffed code ===
    mock_reset_buffers();
    err = cwake_call(0x01,
                     0xFF,
                     data,
                     sizeof(data),
                     &platform);
    ASSERT(err == CWAKE_ERROR_NONE);
    uint8_t tail = 4;
    memcpy(mock_rx_buffer, mock_tx_buffer, mock_tx_index-tail);
    mock_rx_index = mock_tx_index-tail;
    count = 10;
    while (count) {
        count -= 1;
        err = cwake_poll(&platform);
        if (err) count = 0;
    }
    ASSERT(err == CWAKE_ERROR_NONE);
    memcpy(mock_rx_buffer+mock_rx_index, mock_tx_buffer + mock_tx_index-tail, mock_tx_index-tail);
    mock_rx_index += tail;
    count = 10;
    while (count) {
        count -= 1;
        err = cwake_poll(&platform);
        if (err) count = 0;
    }
    ASSERT(err == CWAKE_ERROR_NONE);
    ASSERT(mock_called_cmd == 0xFF);

    pass_counter+=1;
    log("PASSED");
}

static void test_handler_return() {
    log("TEST handler return...");
    total_counter+=1;

    cwake_platform platform = create_test_platform(0x01, 10);
    cwake_init(&platform);

    //create packet sample for assertion
    mock_reset_buffers();
    uint8_t data[] = {0x23, FESC, 0x7F, 0x3F};

    uint8_t expect[] = {FEND, 0x01, 0xCF, strlen("Hello world!"),
                        'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', 0};
    expect[sizeof(expect)-1] = get_crc8(expect, sizeof(expect)-1, 0);

    cwake_error err = cwake_call(0x01,
                                 0xCF,
                                 data,
                                 sizeof(data),
                                 &platform);
    ASSERT(err == CWAKE_ERROR_NONE);

    //put sample to mock_rx_buffer and set mock_rx_index
    memcpy(mock_rx_buffer, mock_tx_buffer, mock_tx_index);
    mock_rx_index = mock_tx_index;
    mock_tx_index = 0;

    mock_called_cmd = 0;
    int count = 5;
    while (count) {
        count -= 1;
        err = cwake_poll(&platform);
        ASSERT(err == CWAKE_ERROR_NONE);
    }
    ASSERT(mock_called_cmd == 0xCF);
    ASSERT(!memcmp(mock_tx_buffer, expect, sizeof(expect)));

    pass_counter+=1;
    log("PASSED");
}

static void test_timeout() {
    log("TEST timeout...");
    total_counter+=1;

    cwake_platform platform = create_test_platform(0x01, 5);
    cwake_init(&platform);

    mock_reset_buffers();
    mock_rx_buffer[mock_rx_index++] = FEND;
    mock_rx_buffer[mock_rx_index++] = 0x01;
    mock_rx_buffer[mock_rx_index++] = 0xCC;

    cwake_error err = CWAKE_ERROR_NONE;
    int count = 15;
    while (count) {
        count -= 1;
        mock_time_ms += 1;
        err = cwake_poll(&platform);
        if (err) count = 0;
    }
    ASSERT(err == CWAKE_ERROR_TIMEOUT);

    pass_counter+=1;
    log("PASSED");
}

static void test_perform() {
    log("TEST perform...");
    total_counter+=1;
    //packet creation speed test (1000 times)
    uint64_t creation_time_start = 0;
    uint64_t creation_time = 0;
    uint8_t data[252] = {FEND,};

    cwake_platform platform = create_test_platform(0x01, 5);
    platform.read = mock_dummy_rw;
    platform.write = mock_dummy_rw;
    cwake_init(&platform);

    creation_time_start = time_now_ns();
    for (int var = 0; var < 1000; ++var) {
        cwake_call(FEND, FEND, data, 252, &platform);
    }
    creation_time = time_now_ns() - creation_time_start;
    log("packet creation time (1000 samples): %ds = %dms = %dus",
        creation_time/1000000000,
        creation_time/1000000,
        creation_time/1000
        );
    log("packet creation speed: %d B/s", ((uint64_t)1000000 * (uint64_t)1000) / (creation_time / 1000) * (uint64_t)252);


    //polling speed test (1000 times)
    //all process speedtest (1000 times)
    pass_counter+=1;
    log("PASSED");
}

void cwake_lib_test(void) {
    log("=== Starting CWAKE library tests ===");


    test_packet_formation();
    test_packet_reception();
    test_handler_return();
    test_timeout();
    //test_perform();

    log("=== All CWAKE library tests complete ===");
    log("PASSED %d / %d", pass_counter, total_counter);
}
