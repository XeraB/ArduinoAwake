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

Adafruit_NeoPixel strip1(LED_COUNT, LED1_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel strip2(LED_COUNT, LED2_PIN, NEO_RGB + NEO_KHZ800);

// DF Player
class Mp3Notify;
typedef DFMiniMp3<HardwareSerial, Mp3Notify> DfMp3;
DfMp3 dfmp3(Serial1);

// actual value of millis() for loop
long ms_actual = 0;

// Timout to stop alarm
const long ALARM_TIMEOUT = 30000;  // 5min: 300 000

// alarm properties
byte alarmAktive = 0;
long ms_last = 0;
long timeout = 0;
long step = 0;

// alarm settings
long timer;
int duration = 1;  // minutes
int maxVolume = 25;

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
BLEIntCharacteristic timeCharacteristic(TIME_CHAR_UUID, BLERead | BLEWrite);
BLEIntCharacteristic durationCharacteristic(DUR_CHAR_UUID, BLERead | BLEWrite);
BLEIntCharacteristic volumeCharacteristic(VOL_CHAR_UUID, BLERead | BLEWrite);
BLEByteCharacteristic alarmCharacteristic(ALARM_CHAR_UUID, BLERead | BLEWrite);
BLEByteCharacteristic nightCharacteristic(NIGHT_CHAR_UUID, BLERead | BLEWrite);
BLEByteCharacteristic nightTimerCharacteristic(TNIGHT_CHAR_UUID, BLERead | BLEWrite);
BLEByteCharacteristic nightBrightCharacteristic(NBRIGHT_CHAR_UUID, BLERead | BLEWrite);

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  delay(1000);
  myTZ = new Timezone(myDST, mySTD);
  rtc_init();
  connectToWifi();
  Udp.begin(localPort);
  getNTPTime();

  // initialization Lamps
  pinMode(LAMP_1, OUTPUT);
  pinMode(LAMP_2, OUTPUT);
  digitalWrite(LAMP_1, LOW);
  digitalWrite(LAMP_2, LOW);
  // initialization LED strips
  strip1.begin();
  strip2.begin();
  updateStrips();
  // initialization DF Player
  dfmp3.begin();
  // initialization RTC

  displayTime();
  delay(5000);
  displayTime();
  startBle();
}
void rtcCallback() {
  startAlarm();
}
void set_RTC_Alarm(datetime_t* alarmTime) {
  rtc_set_alarm(alarmTime, rtcCallback);
}
void displayTime() {
  rtc_get_datetime(&currTime);
  // Display time from RTC
  DateTime now = DateTime(currTime);
  time_t local = now.get_time_t();
  printDateTime(local, tcr->abbrev);
}
// format and print a time_t value, with a time zone appended.
void printDateTime(time_t t, const char* tz) {
  char buf[32];
  char m[4];  // temporary storage for month string (DateStrings.cpp uses shared buffer)
  strcpy(m, monthShortStr(month(t)));
  sprintf(buf, "%.2d:%.2d:%.2d %s %.2d %s %d %s",
          hour(t), minute(t), second(t), dayShortStr(weekday(t)), day(t), m, year(t), tz);
  Serial.println(buf);
}

void loop() {
  //displayTime();

  if (BLE.central().connected()) {
    BLE.poll();  // poll for BLE events
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
    Serial.println("... Next Step: ");
    Serial.println(step);

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
  timerService.addCharacteristic(timeCharacteristic);         // Add characteristic to service
  timerService.addCharacteristic(durationCharacteristic);     // Add characteristic to service
  timerService.addCharacteristic(volumeCharacteristic);       // Add characteristic to service
  timerService.addCharacteristic(alarmCharacteristic);        // Add characteristic to service
  timerService.addCharacteristic(nightCharacteristic);        // Add characteristic to service
  timerService.addCharacteristic(nightTimerCharacteristic);   // Add characteristic to service
  timerService.addCharacteristic(nightBrightCharacteristic);  // Add characteristic to service
  BLE.addService(timerService);                               // Add service

  timeCharacteristic.setEventHandler(BLEWritten, timeCharacteristicWritten);
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
  strip1.setBrightness(30);
  strip2.setBrightness(30);
  colorWipe(strip1.Color(255, 43, 0), 5);
  updateStrips();
  dfmp3.setVolume(1);
  dfmp3.playRandomTrackFromAll();  // random of all folders on sd
}
void stopAlarm() {
  alarmAktive = 0;
  step = 0;
  deaktivateLamps();
  strip1.clear();
  strip2.clear();
  updateStrips();
  dfmp3.stop();
  Serial.println("-- Alarm Stopped --");
}

void startNightLight() {
  Serial.println("-- Starting Night Light --");
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
  Serial.println("-- Night Light Stopped --");
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

/* Callback Methods for BLE Events */
void timeCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  Serial.println("* Characteristic event, written: ");
  timer = timeCharacteristic.value();
}
void durationCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  Serial.println("* Characteristic event, written: ");
  if (durationCharacteristic.value() > 0 && durationCharacteristic.value() <= 30) {
    duration = durationCharacteristic.value();
  }
}
void volumeCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  Serial.println("* Characteristic event, written: ");
  if (volumeCharacteristic.value() > 5 && volumeCharacteristic.value() <= 25) {
    maxVolume = volumeCharacteristic.value();
  }
}
void alarmCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  Serial.println("* Characteristic event, written: ");
  if (alarmCharacteristic.value() == 1) {
    startAlarm();
  }
  if (alarmCharacteristic.value() == 0) {
    stopAlarm();
  }
}
void nightCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  Serial.println("* Characteristic event, written: ");
  if (nightCharacteristic.value() == 1) {
    startNightLight();
  }
  if (nightCharacteristic.value() == 0) {
    stopNightLight();
  }
}
void nightTimerCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  Serial.println("* Characteristic event, written: ");
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
      Serial.print("SD Card, ");
    }
    if (source & DfMp3_PlaySources_Usb) {
      Serial.print("USB Disk, ");
    }
    if (source & DfMp3_PlaySources_Flash) {
      Serial.print("Flash, ");
    }
    Serial.println(action);
  }
  static void OnError([[maybe_unused]] DfMp3& mp3, uint16_t errorCode) {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }
  static void OnPlayFinished([[maybe_unused]] DfMp3& mp3, [[maybe_unused]] DfMp3_PlaySources source, uint16_t track) {
    Serial.print("Play finished for #");
    Serial.println(track);
  }
  static void OnPlaySourceOnline([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
    PrintlnSourceAction(source, "inserted");
  }
  static void OnPlaySourceRemoved([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
    PrintlnSourceAction(source, "removed");
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

    // Update RTC
    rtc_set_datetime(DateTime(local_t));
  } else {
    while(true) ;
  }
}