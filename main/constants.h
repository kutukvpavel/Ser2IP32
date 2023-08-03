#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

#include "driver/gpio.h"
#include "driver/uart.h"

// Storage
#define STORAGE_NAMESPACE "storage"

// Console
#define CONSOLE_ACTIVATE_PIN GPIO_NUM_4

// UART

#define UART_DEFAULT_BAUDS 115200
#define UART_DEFAULT_BUFFER 2048
#define UART_DEFAULT_DATA_BITS UART_DATA_8_BITS
#define UART_DEFAULT_STOP_BITS UART_STOP_BITS_1
#define UART_DEFAULT_PARITY UART_PARITY_DISABLE

// WIFI
#define WIFI_MODE_AP 0
#define WIFI_MODE_STATION 1
#define WIFI_SSID_MAX_LENGTH 31
#define WIFI_PASSWD_MAX_LENGTH 61

#define WIFI_AP_DEFAULT_SSID "Ser2IP32"
#define WIFI_AP_DEFAULT_PASSWD "12345678"
#define WIFI_AP_DEFAULT_CHANNEL 6

//Pins available on WT32-ETH01: 1 (UART0 TX), 2, 3 (UART0 RX), 4, 5, 12, 14, 15, 17, 32, 33, 34, 36, 37, 38, 39
//Total = 16
//UART w/flow control (RTS/CTS) takes 4 pins, x3 instances = 12 pins for UART
// 4 left as GPIO (console jumper, )

const int UART_DEFAULT_ENABLE[3] = {1, 1, 1};
const int UART_DEFAULT_TCP_PORT[3] = {2220, 2221, 2222};

const gpio_num_t UART_DEFAULT_TX_PIN[3] = {GPIO_NUM_1, GPIO_NUM_17, GPIO_NUM_15};
const gpio_num_t UART_DEFAULT_RX_PIN[3] = {GPIO_NUM_3, GPIO_NUM_35, GPIO_NUM_14};
const gpio_num_t UART_DEFAULT_RTS_PIN[3] = {GPIO_NUM_5, GPIO_NUM_33, GPIO_NUM_37};
const gpio_num_t UART_DEFAULT_CTS_PIN[3] = {GPIO_NUM_4, GPIO_NUM_32, GPIO_NUM_34};

#endif