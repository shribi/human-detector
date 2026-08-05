#pragma once
#include <cstddef>

// UART (LD2410C)
#define LD2410_RX_PIN 16
#define LD2410_TX_PIN 17

// I2S (INMP441)
#define I2S_WS      25
#define I2S_SCK     26
#define I2S_SD      22

#define I2S_PORT I2S_NUM_0

static const char *WS_HOST = "192.168.1.34";   // Laptop IP
static const int   WS_PORT = 12000;

static constexpr size_t LOG_SIZE = 100;