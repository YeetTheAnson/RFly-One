> [!CAUTION]
> The current firmware is for validation only, and cannnot decode ADS-B signal yet

### ESP32S3
- Initialize 2.4" display and display test results
- Test and return BMI270 IMU & MMC5983 magnetometer raw values
- Test and initialize E80-900M2212S LoRa module
- Test and initialize ATGM332D-5N31 GNSS module
- Test whether CH32V003 can be reached with serial loopback
- Test whether RP2040 can be reached with serial loopback
- Measure frequency of RP2040 ADS-B extracted clock
- Test if SD card can be detected and accessed

### RP2040
- Extract and output a clock signal from the PPM signal with manchester encoding
- Relay serial data

### Ch32V003
- Blink LED
- Power button toggle power on and off to the rest of the system
- Relay serial data
- Send button state over serial