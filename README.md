# cWAKE - cross-platform C library

cWAKE is simple implementation of the cross-platform C library of the WAKE protocol.

features:

- platform independent and embeddable

- less than 500 code lines

- no dynamic memory allocations

- half-duplex communication mode

minimal requirements:

- C99 standard

- C standard library (for using stdint, stdio, string)

## How to install

Just copy `cwake.h` and `cwake.c` to your project directory and include it.



## How to use

To prepare, you only need to implement 4 functions and initialize a special structure

```c
#include <stdio.h>
#include "cwake.h"


// 1. Implement callback functions for your platform

/**
 * @brief read data from communication port
 * @param buf buffer into which data should be written
 * @param count the maximum number of bytes that need to be written to the buffer
 * @return number of bytes written to buffer (0 for no data)
 */
int8_t on_cwake_read(uint8_t* buf, uint8_t count)
{
// ! do not use timeouts and pauses
// just write to 'buf' no more than requested 'count' bytes of data from communication port
// and return writed data size or 0 if no data
    return read(port_fd, buf, count);
}

/**
 * @brief write data to communication port
 * @param buf data to be sent to the port
 * @param count size of data
 * @return number of bytes written to port (error if return != count)
 */
uint8_t on_cwake_write(uint8_t* buf, uint8_t count)
{
// ! use blocking write function for send all data
    size_t tl = 0;
    size_t ret = 0;
    int err_cnt = 0;
    while ( tl < size ) {
        ret = write(x->port_fd, data + tl, size - tl);

        if( ret == 0 ) {
            err_cnt += 1;
            printf("Error attempting write %d bytes (try %d)", size, err_cnt);
        }
        if ( err_cnt >= 3 ) {
            printf("Writing %d bytes error on byte %d", size, tl);
            return tl;
        }
        tl += ret;
    }
    return tl;
}

/**
 * @brief return current system time in ms
 * @return current system time in ms
 */
uint32_t on_cwake_get_time()
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000 + time.tv_nsec / 1000000;
}

/**
 * @brief user command handler
 * @param cmd command code
 * @param data received data array
 * @param size received data size
 * @param rdata pointer to returning data array
 * @param rsize pointer to variable of size of returning data
 * @return 0 for success, any other code for error (user classified)
 */
int32_t on_cwake_handle( uint8_t cmd,
                        uint8_t* data, uint8_t size,
                        uint8_t** rdata, uint8_t* rsize)
{
    // read and handling data
    printf("Received CMD 0x%02X with %d byte DATA", cmd, size);
    if (size) print_hex_table(data,size); //abstract function

    // (optional) return data
    if ( cmd == 0x0F) {
        *rdata = (uint8_t*)my_data_array; //abstract external data array
        *rsize = sizeof(my_data_array);   //no more than 252 bytes
    }
    return 0;
}

in main() {
    // 2. Init special structure
    cwake_platform cwake;
    cwake_init(&cwake);
    cwake.addr = 0x01;                          // 0 for client, any other for server
    cwake.current_time_ms = on_cwake_get_time;
    cwake.read = on_cwake_read;
    cwake.write = on_cwake_write;
    cwake.handle = on_cwake_handle;
    cwake.timeout_ms = 1500;

    // 3. Send and receive commands
    char* data = "Hello cwake!";
    cwake_call(0x00, 0x01, data, strlen(data), &cwake);    //send broadcast packet

    while ( 1 ) {
        cwake_error err = cwake_poll(&cwake);              //receiving and handling packets
        if (err) {
            printf("CWAKE error code: %d", err);
        }
    }

    return 0;
}
```

## Different between client and server

Server uses a unique address other than 0x00, while client always uses the address 0x00.

Client sends a packet to the server with server address in the header and a command code.

Server, in response to a command, sends a packet with its own address in the header and the code of the command to which it responds.

Client and server can send broadcast packets with the address 0x00 in the header.
