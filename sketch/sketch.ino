#include <ArduinoBLE.h>
#include <Adafruit_NeoPixel.h>
#include <DFMiniMp3.h>

// PIN Lamps
const int LAMP_1 = 14;
const int LAMP_2 = 15;

// PIN LED Strips
const int LED1_PIN = 15;
const int LED2_PIN = 25;
const int LED_COUNT = 30;
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
int duration = 1; // minutes
int volume;

// night light properties
byte nightLightActive = 0;
long ms_night_last = 0;
long timeout_nightLight = 0;
long step_nightLight = 0;

// night light settings
long nightLightTimer = 10; // minutes

BLEService timerService("19B10010-E8F2-537E-4F6C-D104768A1214");
BLEIntCharacteristic timeCharacteristic("b7d06720-3cb7-40dc-94da-61b4af8a2759", BLERead | BLEWrite);
BLEIntCharacteristic durationCharacteristic("f246785d-5c35-4e77-be65-81d711fff24a", BLERead | BLEWrite);
BLEIntCharacteristic volumeCharacteristic("eaefd17d-24cf-4021-afb7-06c7d9f221f9", BLERead | BLEWrite);
BLEByteCharacteristic alarmCharacteristic("33611222-e286-4835-b760-4adbcad8770b", BLERead | BLEWrite);
BLEByteCharacteristic nightCharacteristic("a805442b-63a8-4f7e-8f4e-59d0dcafba98", BLERead | BLEWrite);

void setup() {
  Serial.begin(9600);

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
  dfmp3.reset();  // if you hear popping when starting, remove this call to reset()

  // initialization BLE (as peripheral)
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1)
      ;
  }
  BLE.setLocalName("Nano RP2040 Connect");                 // Set name for connection
  BLE.setAdvertisedService(timerService);                  // Advertise service
  timerService.addCharacteristic(timeCharacteristic);      // Add characteristic to service
  timerService.addCharacteristic(durationCharacteristic);  // Add characteristic to service
  timerService.addCharacteristic(volumeCharacteristic);    // Add characteristic to service
  timerService.addCharacteristic(alarmCharacteristic);     // Add characteristic to service
  timerService.addCharacteristic(nightCharacteristic);     // Add characteristic to service
  BLE.addService(timerService);                            // Add service

  /* assign event handlers for characteristic
     event Type:
     - BLEWritten: calls callback function when central wrote new data to characteristic
  */
  timeCharacteristic.setEventHandler(BLEWritten, timeCharacteristicWritten);
  durationCharacteristic.setEventHandler(BLEWritten, durationCharacteristicWritten);
  volumeCharacteristic.setEventHandler(BLEWritten, volumeCharacteristicWritten);
  alarmCharacteristic.setEventHandler(BLEWritten, alarmCharacteristicWritten);
  nightCharacteristic.setEventHandler(BLEWritten, nightCharacteristicWritten);

  BLE.advertise();  // Start advertising
  Serial.println("Waiting for connections...");
}

void loop() {
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
      int volume = round(1 + step * 0.2);
      dfmp3.setVolume(volume);

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
    if (step == 100) {
      stopNightLight();
    }

    // adjusting brightness LED Strips
    int brightness = 255 - step * 2;
    strip1.setBrightness(brightness);
    strip2.setBrightness(brightness);

    // adjusting color LED Strips
    int colorR = 230 - step * 2;
    int colorB = round(255 - step * 1.25);
    colorWipe(strip1.Color(colorR, 0, colorB), 5);

    // restart timeout
    ms_night_last = millis();
  }
}

void startAlarm() {
  Serial.println("-- Starting Alarm --");
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
  ms_night_last = millis(); 
  strip1.setBrightness(255);
  strip2.setBrightness(255);
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

/* 
  Callback Methods for BLE Events
*/
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
    volume = volumeCharacteristic.value();
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

// implement a notification class,
// its member methods will get called
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