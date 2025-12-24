/**
 * @file mock.h
 * @brief mock implementation for tests
 * @author Qvafir <qvafir@outlook.com>
 * @copyright MIT License, see repository LICENSE file
 */

#ifndef MOCK_H
#define MOCK_H
#include <stdint.h>


extern uint8_t mock_tx_buffer[];
extern uint8_t mock_rx_buffer[];
extern uint32_t mock_tx_index;
extern uint32_t mock_rx_index;
extern uint32_t mock_rx_start;
extern uint32_t mock_time_ms;
extern uint8_t mock_called_cmd;
extern uint8_t mock_rd_buffer[];

uint8_t mock_dummy_rw(uint8_t* buf, uint8_t count);
uint8_t mock_read(uint8_t* buf, uint8_t count);
uint8_t mock_write(uint8_t* buf, uint8_t count);
uint32_t mock_time_ms_func();
int32_t mock_handle(uint8_t cmd, uint8_t* data, uint8_t size,
                    uint8_t** rdata, uint8_t* rsize);
void mock_reset_buffers();

#endif
