#ifndef RAW_SERIAL_H
#define RAW_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum Raw_Serial_Read_Result {
    RAW_SERIAL_READ_NONE,
    RAW_SERIAL_READ_BYTE,
    RAW_SERIAL_READ_ERROR,
} Raw_Serial_Read_Result;

typedef struct Raw_Serial {
    int fd;
    char port_path[256];
    bool had_error;
} Raw_Serial;

typedef struct Raw_Serial_Config {
    const char *port_path;
    int baud_rate;
} Raw_Serial_Config;

bool raw_serial_open(Raw_Serial *serial, const Raw_Serial_Config *config);
void raw_serial_close(Raw_Serial *serial);
const char *raw_serial_port_path(const Raw_Serial *serial);
bool raw_serial_had_error(const Raw_Serial *serial);
Raw_Serial_Read_Result raw_serial_read_byte(Raw_Serial *serial, uint8_t *out_byte);

#endif
