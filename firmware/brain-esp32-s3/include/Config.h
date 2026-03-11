#pragma once

#define BOT_TOKEN ""
#define CHAT_ID ""
#define SMART_BULB_IP "192.168.1.XXX"

#define I2C_SDA 8
#define I2C_SCL 9
#define RX2_PIN 16
#define TX2_PIN 17
#define LASER_PIN 5
#define BUZZER_PIN 6
#define LOCAL_SS_PIN 21
#define LOCAL_RST_PIN 14
#define SPI_SCK 12
#define SPI_MISO 13
#define SPI_MOSI 11
#define WIFI_LED 42
#define RESET_BUTTON 47
#define SOUND_PIN 4 
#define POT_PIN 1
#define SOUND_LED_PIN 41
#define CSI_DATA_LEN 128
#define CSI_CHANNELS 64

#define MAX_CARDS 50
#define MAX_ZONES 10
#define MAX_LOGS 20
#define MAX_SENSING_APS 5

const int armingDelaySeconds = 10;
extern int silenceOffset;
extern int currentSensitivity;
extern bool isSirenDetected;