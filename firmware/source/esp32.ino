// Written with Gemini. Firmware time not counted.
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_BMI270.h>
#include <Adafruit_MMC5983.h>
#include <SD_MMC.h>
#include <RadioLib.h>

// --- Pin Definitions ---
// TFT LCD
#define LCD_MOSI   42
#define LCD_MISO   39
#define LCD_SCK    41
#define LCD_CMD    43
#define LCD_CS     44
#define LCD_LED    40

// I2C
#define I2C_SDA    36
#define I2C_SCL    35

// SDIO SD Card
#define SDIO_DAT0  9
#define SDIO_DAT1  8
#define SDIO_DAT2  12
#define SDIO_DAT3  13
#define SDIO_CMD   11
#define SDIO_CLK   10
#define SDIO_CD    7

// LoRa (LR2021 / LR1121)
#define LORA_BUSY  16
#define LORA_CE    5
#define LORA_SCK   4
#define LORA_MOSI  2
#define LORA_MISO  1

// GNSS
#define GNSS_TX_ESP_RX  15
#define GNSS_RX_ESP_TX  14
#define GNSS_PPS        17

// Coprocessors & ADSB
#define CH32_RX_PIN     21 // ESP RX
#define CH32_TX_PIN     18 // ESP TX
#define RP2040_RX_PIN   47 // ESP RX
#define RP2040_TX_PIN   48 // ESP TX
#define ADSB_CLK        34

// --- Peripherals & Objects ---
SPIClass tftSPI(FSPI);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&tftSPI, LCD_CMD, LCD_CS, -1);

SPIClass loraSPI(HSPI);
LR1121 radio = new Module(LORA_CE, RADIOLIB_NC, RADIOLIB_NC, LORA_BUSY, loraSPI);

Adafruit_BMI270 bmi;
Adafruit_MMC5983MA mmc;

// Hardware UARTS (ESP32-S3 has 3: Serial0, Serial1, Serial2)
// Assumes USB CDC is enabled for the main `Serial` monitor.
HardwareSerial SerialRP(0);   
HardwareSerial SerialGNSS(1); 
HardwareSerial SerialCH(2);   

// --- Global State ---
bool sdOk = false;
bool loraOk = false;
bool bmiOk = false;
bool mmcOk = false;

volatile uint32_t adsb_pulse_count = 0;
uint32_t last_adsb_count = 0;
uint32_t last_update_ms = 0;

// ADSB Clock Interrupt
void IRAM_ATTR adsb_isr() {
    adsb_pulse_count++;
}

void setup() {
    Serial.begin(115200);

    // 1. Initialize TFT
    pinMode(LCD_LED, OUTPUT);
    digitalWrite(LCD_LED, HIGH); // Turn on backlight
    tftSPI.begin(LCD_SCK, LCD_MISO, LCD_MOSI, LCD_CS);
    tft.begin();
    tft.setRotation(3); // Landscape
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.println("Initializing System...");

    // 2. Initialize I2C & Sensors
    Wire.begin(I2C_SDA, I2C_SCL);
    bmiOk = bmi.begin(0x68, &Wire);
    mmcOk = mmc.begin();

    // 3. Initialize SDIO SD Card
    pinMode(SDIO_CD, INPUT_PULLUP);
    SD_MMC.setPins(SDIO_CLK, SDIO_CMD, SDIO_DAT0, SDIO_DAT1, SDIO_DAT2, SDIO_DAT3);
    if (digitalRead(SDIO_CD) == LOW) { // Card Detected
        sdOk = SD_MMC.begin();
    }

    // 4. Initialize LoRa
    loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CE);
    int state = radio.begin();
    loraOk = (state == RADIOLIB_ERR_NONE);

    // 5. Initialize UARTS
    SerialGNSS.begin(9600, SERIAL_8N1, GNSS_TX_ESP_RX, GNSS_RX_ESP_TX);
    SerialCH.begin(115200, SERIAL_8N1, CH32_RX_PIN, CH32_TX_PIN);
    SerialRP.begin(115200, SERIAL_8N1, RP2040_RX_PIN, RP2040_TX_PIN);

    // 6. Initialize ADSB Clock Interrupt
    pinMode(ADSB_CLK, INPUT);
    attachInterrupt(digitalPinToInterrupt(ADSB_CLK), adsb_isr, RISING);
}

void loop() {
    uint32_t now = millis();
    
    // Update screen and run checks every 500ms
    if (now - last_update_ms >= 500) {
        last_update_ms = now;
        
        tft.setCursor(0, 0);

        // --- I2C Sensors ---
        if (bmiOk) {
            sensors_event_t accel, gyro, temp;
            bmi.getEvent(&accel, &gyro, &temp);
            tft.printf("BMI: X:%.1f Y:%.1f  \n", accel.acceleration.x, accel.acceleration.y);
        } else {
            tft.println("BMI270: FAIL       ");
        }

        if (mmcOk) {
            sensors_event_t magEvent;
            mmc.getEvent(&magEvent);
            tft.printf("MMC: X:%.0f Y:%.0f  \n", magEvent.magnetic.x, magEvent.magnetic.y);
        } else {
            tft.println("MMC5983: FAIL      ");
        }

        // --- SD Card ---
        tft.printf("SD Card: %s \n", (sdOk && digitalRead(SDIO_CD) == LOW) ? "OK      " : "NOT MNT ");

        // --- LoRa ---
        tft.printf("LR2021: %s \n", loraOk ? "OK      " : "FAIL    ");

        // --- ADSB Clock ---
        uint32_t current_count = adsb_pulse_count;
        uint32_t hz = (current_count - last_adsb_count) * 2; // *2 because loop is 500ms
        last_adsb_count = current_count;
        tft.printf("ADSB CLK: %lu Hz   \n", hz);

        // --- GNSS ---
        bool gnssData = false;
        while (SerialGNSS.available()) {
            SerialGNSS.read(); // Flush buffer
            gnssData = true;
        }
        tft.printf("GNSS UART: %s \n", gnssData ? "ACTIVE  " : "WAITING ");

        // --- CH32V003 Coprocessor ---
        SerialCH.print("C"); // Send test char
        delay(10); // Brief wait for loopback
        bool ch32Ok = false;
        while (SerialCH.available()) {
            if (SerialCH.read() == 'C') ch32Ok = true;
        }
        tft.printf("CH32 UART: %s \n", ch32Ok ? "LOOP OK " : "FAIL    ");

        // --- RP2040 Coprocessor ---
        SerialRP.print("R"); // Send test char
        delay(10);
        bool rpOk = false;
        while (SerialRP.available()) {
            if (SerialRP.read() == 'R') rpOk = true;
        }
        tft.printf("RP20 UART: %s \n", rpOk ? "LOOP OK " : "FAIL    ");
    }
}