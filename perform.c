/**
 * @file perform.c
 * @brief CWAKE performance tests implementation
 * @author Qvafir <qvafir@outlook.com>
 * @copyright MIT License, see repository LICENSE file
 */
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "cwake.h"
#include "mock.h"
#include "common.h"


#define PACKET_SIZE 250 // Size of each packet in bytes
#define NUM_PACKETS 10000 // Number of packets to send

static uint8_t packet[PACKET_SIZE];
static cwake_platform platform;

// Function to measure the time taken to send packets
static double measure_time(void (*operation)(void)) {
    clock_t start, end;
    double duration;

    start = clock();
    operation(); // Call the function that sends packets
    end = clock();

    duration = (double)(end - start) / CLOCKS_PER_SEC; // Calculate the time taken in seconds

    return duration;
}

// Function to create and send packets using cwake library
void send_packets(void) {
    for (int i = 0; i < NUM_PACKETS; i++) {
        cwake_call(FEND, FEND, packet, PACKET_SIZE, &platform);
    }
}

// Function to read and handle packets using cwake library
void handle_packets(void) {
    while (handle_counter < NUM_PACKETS) {
        int err = cwake_poll(&platform);
        if ( err ) {
            log("cwake_poll err: %d", err);
            return;
        }
    }
}

void cwake_lib_performance(void)
{
    log("PERFORMANCE TEST...");
    memset(packet, FEND, PACKET_SIZE); // Fill the packet with dummy data

    // sending test
    platform = mock_create_cwake_platform(0x01, 5);
    platform.read = mock_dummy_rw;
    platform.write = mock_dummy_rw;
    cwake_init(&platform);

    double send_duration;
    double send_speed;
    send_duration = measure_time(send_packets);
    send_speed = (NUM_PACKETS * PACKET_SIZE) / send_duration;

    // handling test
    handle_counter = 0;
    platform = mock_create_cwake_platform(FEND, 5);
    platform.read = mock_reread;
    platform.handle = mock_dummy_handle;
    cwake_init(&platform);

    //prepend packet for handling
    cwake_call(FEND, FEND, packet, PACKET_SIZE, &platform);
    memcpy(mock_rx_buffer, mock_tx_buffer, mock_tx_index);
    mock_rx_index = mock_tx_index;
    mock_tx_index = 0;

    double handle_duration;
    double handle_speed;
    handle_duration = measure_time(handle_packets);
    handle_speed = (NUM_PACKETS * PACKET_SIZE) / handle_duration;


    log("PERFORMANCE TEST COMPLETE");

    // Convert to MB/s and Mb/s
    double speed_MBps = send_speed / 1048576.0; // 1 MB = 2^20 bytes
    double speed_Mbps = (send_speed * 8) / 1048576.0; // 1 bit = 1/8 byte
    log("Packet creation speed: %.2f B/s, %.2f MB/s, %.2f Mb/s\n", send_speed, speed_MBps, speed_Mbps);
    speed_MBps = handle_speed / 1048576.0; // 1 MB = 2^20 bytes
    speed_Mbps = (handle_speed * 8) / 1048576.0; // 1 bit = 1/8 byte
    log("Packet handling speed: %.2f B/s, %.2f MB/s, %.2f Mb/s\n", handle_speed, speed_MBps, speed_Mbps);
}


