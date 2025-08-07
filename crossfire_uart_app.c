#include <furi.h>
#include <furi_hal_uart.h>
#include <furi_hal_uart_config.h>
#include <stdio.h>

#define CRSF_FRAME_SIZE 25 // header(1)+len(1)+data(22)+crc(1)
#define CRSF_HEADER 0xC8

static uint8_t crsf_buffer[CRSF_FRAME_SIZE];
static uint8_t crsf_index = 0;
static uint16_t channels[16];

static FuriHalUart* uart;

void crsf_parse_channels(const uint8_t* data) {
    uint32_t buffer = 0;
    uint8_t bits = 0;
    uint8_t ch = 0;
    for(uint8_t i = 0; i < 22; i++) {
        buffer |= ((uint32_t)data[i]) << bits;
        bits += 8;
        while(bits >= 11 && ch < 16) {
            channels[ch++] = buffer & 0x7FF;
            buffer >>= 11;
            bits -= 11;
        }
    }
}

uint8_t crsf_crc8(const uint8_t* data, uint8_t length) {
    uint8_t crc = 0;
    for(uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for(uint8_t b = 0; b < 8; b++) {
            if(crc & 0x80) crc = (crc << 1) ^ 0xD5;
            else crc <<= 1;
        }
    }
    return crc;
}

void uart_callback(void* context) {
    FuriHalUart* uart = (FuriHalUart*)context;
    while(furi_hal_uart_rx_available(uart)) {
        uint8_t byte = 0;
        furi_hal_uart_read(uart, &byte, 1);

        if(crsf_index == 0) {
            if(byte == CRSF_HEADER) {
                crsf_buffer[crsf_index++] = byte;
            }
        } else {
            crsf_buffer[crsf_index++] = byte;
            if(crsf_index == CRSF_FRAME_SIZE) {
                // Check length byte
                if(crsf_buffer[1] == 22) {
                    // Check CRC
                    uint8_t crc = crsf_crc8(&crsf_buffer[2], 22);
                    if(crc == crsf_buffer[24]) {
                        crsf_parse_channels(&crsf_buffer[2]);
                        printf("CRSF Channels:");
                        for(int i = 0; i < 16; i++) {
                            printf(" CH%d:%d", i+1, channels[i]);
                        }
                        printf("\n");
                    }
                }
                crsf_index = 0;
            }
        }
    }
}

int crossfire_uart_app(void) {
    furi_hal_uart_init(UART_ID_1, 420000);
    uart = furi_hal_uart_get(UART_ID_1);

    furi_hal_uart_config_t config = {
        .baud_rate = 420000,
        .data_bits = UART_DATA_BITS_8,
        .parity = UART_PARITY_NONE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_control = UART_FLOW_CONTROL_NONE,
    };
    furi_hal_uart_set_config(uart, &config);

    furi_hal_uart_set_rx_callback(uart, uart_callback, uart);

    while(1) {
        furi_delay_ms(100);
    }
    return 0;
}
