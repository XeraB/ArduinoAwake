#include <ArduinoBLE.h>
#include <Adafruit_NeoPixel.h>
#include <DFMiniMp3.h>
#include <WiFiNINA.h>
#include "arduino_secrets.h"
#include "settings.h"
#include <Timezone_Generic.h>  // https://github.com/khoih-prog/Timezone_Generic
#include <RP2040_RTC.h>

// Wifi settings
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;  // the WiFi radio's status

// US Eastern Time Zone (New York, Detroit)
TimeChangeRule myDST = { "MESZ", Last, Sun, Mar, 2, +120 };  //Daylight time = UTC + 2 hours
TimeChangeRule mySTD = { "MEZ", Last, Wed, Oct, 3, +60 };    //Standard time = UTC + 1 hour
Timezone* myTZ;
TimeChangeRule* tcr;  //pointer to the time change rule, use to get TZ abbrev

// NTP settings
char timeServer[] = "time.nist.gov";  // NTP server
unsigned int localPort = 2390;        // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;       // NTP timestamp is in the first 48 bytes of the message
const int UDP_TIMEOUT = 2000;         // timeout in miliseconds to wait for an UDP packet to arrive
byte packetBuffer[NTP_PACKET_SIZE];   // buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
char datetime_buf[256];
datetime_t currTime;
datetime_t alarm;

Adafruit_NeoPixel strip1(LED_COUNT, LED1_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel strip2(LED_COUNT, LED2_PIN, NEO_RGB + NEO_KHZ800);

// DF Player
class Mp3Notify;
typedef DFMiniMp3<HardwareSerial, Mp3Notify> DfMp3;
DfMp3 dfmp3(Serial1);
uint16_t count;

// actual value of millis() for loop
long ms_actual = 0;

// Timout to stop alarm
const long ALARM_TIMEOUT = 30000;  // 5min: 300 000

// alarm properties
bool rtcActive = 0;
byte alarmAktive = 0;
long ms_last = 0;
long timeout = 0;
long step = 0;
long randNumber;

// alarm settings
int duration = 1;  // minutes
int maxVolume = 20;

// night light properties
byte nightLightActive = 0;
long ms_night_last = 0;
long timeout_nightLight = 0;
long step_nightLight = 0;

// night light settings
long nightLightTimer = 10;  // minutes
int nightLightBright = 255;
float brightness_adjust = 1.25;

BLEService timerService(TIMER_SERVICE_UUID);
BLEIntCharacteristic timestampCharacteristic(TIMESTAMP_CHAR_UUID, BLERead | BLEWrite);
BLEIntCharacteristic durationCharacteristic(DUR_CHAR_UUID, BLERead | BLEWrite);
BLEIntCharacteristic volumeCharacteristic(VOL_CHAR_UUID, BLERead | BLEWrite);
BLEByteCharacteristic alarmCharacteristic(ALARM_CHAR_UUID, BLERead | BLEWrite);
BLEByteCharacteristic nightCharacteristic(NIGHT_CHAR_UUID, BLERead | BLEWrite);
BLEByteCharacteristic nightTimerCharacteristic(TNIGHT_CHAR_UUID, BLERead | BLEWrite);
BLEByteCharacteristic nightBrightCharacteristic(NBRIGHT_CHAR_UUID, BLERead | BLEWrite);

void setup() {
  delay(1000);
  // initialization LED strips
  strip1.begin();
  strip2.begin();
  updateStrips();
  delay(100);
  colorWipe(strip1.Color(255, 51, 51), 5); // red

  // initialization Lamps
  pinMode(LAMP_1, OUTPUT);
  pinMode(LAMP_2, OUTPUT);
  digitalWrite(LAMP_1, LOW);
  digitalWrite(LAMP_2, LOW);
  // initialization DF Player
  dfmp3.begin();
  dfmp3.reset();
  count = dfmp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  dfmp3.setVolume(1);
  colorWipe(strip1.Color(255, 200, 0), 5); // yellow

  myTZ = new Timezone(myDST, mySTD);
  rtc_init();
  connectToWifi();
  Udp.begin(localPort);
  getNTPTime();

  // initialization RTC
  delay(1000);
  startBle();
  colorWipe(strip1.Color(0, 255, 20), 5); // green
  delay(10000);
  strip1.clear();
  strip2.clear();
  updateStrips();
}
void rtcCallback() {
  rtcActive = 1;
}
void set_RTC_Alarm(datetime_t* alarmTime) {
  rtc_set_alarm(alarmTime, rtcCallback);
}

void loop() {

  if (BLE.central().connected()) {
    BLE.poll();  // poll for BLE events
  }
  if (rtcActive == 1) {
    rtcActive = 0;
    startAlarm();
  }

  ms_actual = millis();
  /*Alarm: 
    After the timeout between each step, new values are set for the output devices
  */
  if (alarmAktive == 1 && (ms_actual - ms_last) > timeout) {
    // calling dfmp3.loop() periodically allows for notifications
    // to be handled without interrupts
    dfmp3.loop();

    step++;

    // alarm is stopped after time defined in ALARM_TIMEOUT
    if (step == 101) {
      stopAlarm();
    } else {
      // adjusting brightness LED Strips
      int brightness = step * 2.55;
      strip1.setBrightness(brightness);
      strip2.setBrightness(brightness);

      // adjusting color LED Strips
      int colorG = round(40 + step * 2.1);
      colorWipe(strip1.Color(255, colorG, 0), 5);

      // adjusting volume DF Player
      int volume_new = round(1 + step * 0.2);
      if (volume_new <= maxVolume) {
        dfmp3.setVolume(volume_new);
      }

      // last step of alarm
      if (step == 100) {
        aktivateLamps();
        colorWipe(strip1.Color(255, 230, 0), 5);
        timeout = ALARM_TIMEOUT;
      }
      // restart timeout
      ms_last = millis();
    }
  }
  /*
    After the timeout between each step, new values are set for the output devices
  */
  if (nightLightActive == 1 && (ms_actual - ms_night_last) > timeout_nightLight) {
    step_nightLight++;
    if (step_nightLight == 100) {
      stopNightLight();
    } else {

      // adjusting brightness LED Strips
      int brightness = round(nightLightBright - step_nightLight * brightness_adjust);
      strip1.setBrightness(brightness);
      strip2.setBrightness(brightness);

      // adjusting color LED Strips
      int colorR = 230 - step_nightLight * 2;
      int colorB = round(255 - step_nightLight * 1.25);
      colorWipe(strip1.Color(colorR, 0, colorB), 5);

      // restart timeout
      ms_night_last = millis();
    }
  }
}

void startBle() {
  // initialization BLE (as peripheral)
  if (!BLE.begin()) {
    while (1)
      ;
  }
  BLE.setLocalName(BLE_NAME);                                 // Set name for connection
  BLE.setAdvertisedService(timerService);                     // Advertise service
  timerService.addCharacteristic(timestampCharacteristic);    // Add characteristic to service
  timerService.addCharacteristic(durationCharacteristic);     // Add characteristic to service
  timerService.addCharacteristic(volumeCharacteristic);       // Add characteristic to service
  timerService.addCharacteristic(alarmCharacteristic);        // Add characteristic to service
  timerService.addCharacteristic(nightCharacteristic);        // Add characteristic to service
  timerService.addCharacteristic(nightTimerCharacteristic);   // Add characteristic to service
  timerService.addCharacteristic(nightBrightCharacteristic);  // Add characteristic to service
  BLE.addService(timerService);                               // Add service

  //hourCharacteristic.setEventHandler(BLEWritten, timeCharacteristicWritten);
  //minuteCharacteristic.setEventHandler(BLEWritten, timeCharacteristicWritten);
  timestampCharacteristic.setEventHandler(BLEWritten, timeStampCharacteristicWritten);
  durationCharacteristic.setEventHandler(BLEWritten, durationCharacteristicWritten);
  volumeCharacteristic.setEventHandler(BLEWritten, volumeCharacteristicWritten);
  alarmCharacteristic.setEventHandler(BLEWritten, alarmCharacteristicWritten);
  nightCharacteristic.setEventHandler(BLEWritten, nightCharacteristicWritten);
  nightTimerCharacteristic.setEventHandler(BLEWritten, nightTimerCharacteristicWritten);
  nightBrightCharacteristic.setEventHandler(BLEWritten, nightBrightCharacteristicWritten);

  BLE.advertise();  // Start advertising
}

void startAlarm() {
  alarmAktive = 1;
  step = 0;
  ms_last = millis();
  timeout = (duration * 60 * 1000) / 100;  // calc alarm timeout
  strip1.setBrightness(1);
  strip2.setBrightness(1);
  colorWipe(strip1.Color(255, 43, 0), 5);
  updateStrips();
  randNumber = random(1, count + 1);
  dfmp3.setVolume(1);
  dfmp3.playGlobalTrack(randNumber);
}
void stopAlarm() {
  alarmAktive = 0;
  step = 0;
  deaktivateLamps();
  strip1.clear();
  strip2.clear();
  updateStrips();
  dfmp3.stop();
  dfmp3.setVolume(1);
}

void startNightLight() {
  nightLightActive = 1;
  step_nightLight = 0;
  timeout_nightLight = (nightLightTimer * 60 * 1000) / 100;  // calc nightLight timeout
  brightness_adjust = nightLightBright / 100.00;
  ms_night_last = millis();
  strip1.setBrightness(nightLightBright);
  strip2.setBrightness(nightLightBright);
  colorWipe(strip1.Color(230, 0, 255), 5);
  updateStrips();
}
void stopNightLight() {
  nightLightActive = 0;
  step_nightLight = 0;
  strip1.clear();
  strip2.clear();
  updateStrips();
  nightCharacteristic.writeValue(0); 
}
void aktivateLamps() {
  digitalWrite(LAMP_1, HIGH);
  digitalWrite(LAMP_2, HIGH);
}
void deaktivateLamps() {
  digitalWrite(LAMP_1, LOW);
  digitalWrite(LAMP_2, LOW);
}
void updateStrips() {  //write new values to strips
  strip1.show();
  strip2.show();
}
// fill LED Strip with Color
void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip1.numPixels(); i++) {  // For each pixel in strip...
    strip1.setPixelColor(i, color);               //  Set pixel's color (in RAM)
    strip2.setPixelColor(i, color);               //  Set pixel's color (in RAM)
    updateStrips();                               //  Update strip to match
    delay(wait);                                  //  Pause for a moment
  }
}

void timeStampCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  Serial.println("* Characteristic Time");
  Serial.println(timestampCharacteristic.value());
  // time_t timestamp = timestampCharacteristic.value();
  // Serial.println("Recieved TS: " + timestamp);
  time_t timestamp = timestampCharacteristic.value();
  time_t local_t = myTZ->toLocal(timestamp, &tcr);
  printDateTime(local_t, tcr->abbrev);

  /* Weekday time_t:     1-7, 1 is Sunday
     Weekday datetime_t: 0-6, 0 is Sunday
  */
  datetime_t alarmTimer = { year(local_t), month(local_t), day(local_t), weekday(local_t) - 1, hour(local_t), minute(local_t), second(local_t) };
  Serial.println("----------------");
  Serial.println(alarmTimer.year);
  Serial.println(alarmTimer.month);
  Serial.println(alarmTimer.day);
  Serial.println(alarmTimer.dotw);
  Serial.println(alarmTimer.hour);
  Serial.println(alarmTimer.min);
  Serial.println(alarmTimer.sec);
  Serial.println("----------------");
  set_RTC_Alarm(&alarmTimer);
}
void durationCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  if (durationCharacteristic.value() > 0 && durationCharacteristic.value() <= 30) {
    duration = durationCharacteristic.value();
  }
}
void volumeCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  if (volumeCharacteristic.value() > 5 && volumeCharacteristic.value() <= 25) {
    maxVolume = volumeCharacteristic.value();
  }
}
void alarmCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  if (alarmCharacteristic.value() == 1) {
    startAlarm();
  }
  if (alarmCharacteristic.value() == 0) {
    stopAlarm();
  }
}
void nightCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  if (nightCharacteristic.value() == 1) {
    startNightLight();
  }
  if (nightCharacteristic.value() == 0) {
    stopNightLight();
  }
}
void nightTimerCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  if (nightTimerCharacteristic.value() > 0 && nightTimerCharacteristic.value() <= 60) {
    nightLightTimer = nightTimerCharacteristic.value();
  }
}
void nightBrightCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  if (nightBrightCharacteristic.value() > 0 && nightBrightCharacteristic.value() < 256) {
    nightLightBright = nightBrightCharacteristic.value();
  }
}

class Mp3Notify {
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action) {
    if (source & DfMp3_PlaySources_Sd) {
    }
    if (source & DfMp3_PlaySources_Usb) {
    }
    if (source & DfMp3_PlaySources_Flash) {
    }
  }
  static void OnError([[maybe_unused]] DfMp3& mp3, uint16_t errorCode) {
    // see DfMp3_Error for code meaning
  }
  static void OnPlayFinished([[maybe_unused]] DfMp3& mp3, [[maybe_unused]] DfMp3_PlaySources source, uint16_t track) {
  }
  static void OnPlaySourceOnline([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
  }
  static void OnPlaySourceInserted([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
  }
  static void OnPlaySourceRemoved([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
  }
};

void connectToWifi() {
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
}
void sendNTPpacket(char* ntpSrv) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  Udp.beginPacket(ntpSrv, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
void getNTPTime() {
  sendNTPpacket(timeServer);
  delay(1000);

  if (Udp.parsePacket()) {
    Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // convert NTP time into everyday time:
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;
    time_t epoch_t = epoch;
    time_t local_t = myTZ->toLocal(epoch_t, &tcr);
    setTime(local_t);
    datetime_t currentTime = { year(local_t), month(local_t), day(local_t), weekday(local_t) - 1, hour(local_t), minute(local_t), second(local_t) };
    // Update RTC
    rtc_set_datetime(&currentTime);
  } else {
    while (true)
      ;
  }
}