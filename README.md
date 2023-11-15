# ArduinoAwake
AWAKE is an alarm clock which simulates the sunrise. 

## Arduino
Arduino Nano Connect RP2040

## Libraries
- ArduinoBLE by Arduino (Version: 1.3.6)
    https://www.arduino.cc/reference/en/libraries/arduinoble/
- Adafruit NeoPixel by Adafruit (Version: 1.12.0)
    https://github.com/adafruit/Adafruit_NeoPixel
- DFPlayer Mini Mp3 by Makuna by Michael C. Miller (Version: 1.2.2)
    https://github.com/Makuna/DFMiniMp3/wiki
- WifiNINA by Arduino (Version: 1.8.14)
    https://www.arduino.cc/reference/en/libraries/wifinina/
- Time by Michael Margolis (Version: 1.6.1)
    https://github.com/PaulStoffregen/Time
- Timezone_Generic by Jack Christensen, Khoi Hoang (Version: 1.10.1)
    https://github.com/khoih-prog/Timezone_Generic
- RP2040_RTC by Khoi Hoang (Version: 1.1.1)
    https://github.com/khoih-prog/RP2040_RTC

## Wifi
Save your Wifi-Name and Password in the file arduino_secrets.h

## BLE Characeristics
UUIDS can be found and changed in settings.h

### Service: 
TimerService UUID: "19B10010-E8F2-537E-4F6C-D104768A1214"

### Characeristics:
Hour        UUID: "b7d06720-3cb7-40dc-94da-61b4af8a2759"
Value: 0-23
(hour the alarm starts)

Minute      UUID: "57dd48d8-ba50-4df4-ab6b-9e2349cac529"
Value: 0-59
(hour the alarm starts)

Duration    UUID: "f246785d-5c35-4e77-be65-81d711fff24a"
Value: 1-30 (minutes)
(time from start of the alarm to full brightness)

volume      UUID: "eaefd17d-24cf-4021-afb7-06c7d9f221f9"
Value: 6-25
(max volume of the sounds)

alarm       UUID: "33611222-e286-4835-b760-4adbcad8770b"
Value: 0 | 1
(to manually start the alarm)

night       UUID: "a805442b-63a8-4f7e-8f4e-59d0dcafba98"
Value: 0 | 1
(to manually start the night light)

nightTimer  UUID: "9dcdea3b-2a3c-4662-9eba-2e0bee9ffcf7"
Value: 1-60
(nightlight switches off after the set time)

nightBright UUID: "b34278fc-4756-45dc-b7d5-22a35412dea1"
Value: 1-255
(brightness of the nightlight)

## Real time clock (rtc)
When connecting the arduino to the power, the current time will be fetched via Wifi.

## Functions
### Alarm
The alarm simulates the sunrise by gradually adjusting the brightness and color of the led-strips. 
During the alarm a random song from the SD card is played.

### Nightlight 
The Nightlight can be started manually and shuts off the light after the set timeout.