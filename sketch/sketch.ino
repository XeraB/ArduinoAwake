#include <ArduinoBLE.h>
#include <Adafruit_NeoPixel.h>
#include <DFMiniMp3.h>

// PIN Layout Lamps
const int LAMP_1 = 14;
const int LAMP_2 = 15;

// LED Strips
const int LED1_PIN = 15;
const int LED2_PIN = 25;
const int LED_COUNT = 30;
Adafruit_NeoPixel strip1(LED_COUNT, LED1_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel strip2(LED_COUNT, LED2_PIN, NEO_RGB + NEO_KHZ800);

// DF Player
class Mp3Notify;
typedef DFMiniMp3<HardwareSerial, Mp3Notify> DfMp3;
DfMp3 dfmp3(Serial1);

// Timout nach dem der Alarm gestoppt wird
const long ALARM_TIMEOUT = 5000;  // 5min: 300000

long alarmAktive = 0;
long ms_actual = 0;
long ms_last = 0;
long timeout = 0;
long step = 0;

// Timer Eigenschaften
long timer;
int duration = 1;
int volume;

BLEService timerService("19B10010-E8F2-537E-4F6C-D104768A1214");
BLEByteCharacteristic timeCharacteristic("b7d06720-3cb7-40dc-94da-61b4af8a2759", BLERead | BLEWrite);
BLEByteCharacteristic durationCharacteristic("f246785d-5c35-4e77-be65-81d711fff24a", BLERead | BLEWrite);
BLEByteCharacteristic volumeCharacteristic("eaefd17d-24cf-4021-afb7-06c7d9f221f9", BLERead | BLEWrite);
BLEByteCharacteristic alarmCharacteristic("33611222-e286-4835-b760-4adbcad8770b", BLERead | BLEWrite);

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;

  pinMode(LAMP_1, OUTPUT);
  pinMode(LAMP_2, OUTPUT);
  digitalWrite(LAMP_1, LOW);
  digitalWrite(LAMP_2, LOW);

  // Initialize LED strips
  strip1.begin();
  strip2.begin();
  updateStrips();

  // Initialize DF Player
  dfmp3.begin();
  // if you hear popping when starting, remove this call to reset()
  dfmp3.reset();

  // Initialize the built-in LED pin (for BLE)
  pinMode(LED_BUILTIN, OUTPUT);

  if (!BLE.begin()) {  // initialize BLE
    Serial.println("starting BLE failed!");
    while (1)
      ;
  }

  BLE.setLocalName("Nano33BLE");                           // Set name for connection
  BLE.setAdvertisedService(timerService);                  // Advertise service
  timerService.addCharacteristic(timeCharacteristic);      // Add characteristic to service
  timerService.addCharacteristic(durationCharacteristic);  // Add characteristic to service
  timerService.addCharacteristic(volumeCharacteristic);    // Add characteristic to service
  timerService.addCharacteristic(alarmCharacteristic);    // Add characteristic to service
  BLE.addService(timerService);                            // Add service

  // assign event handlers for characteristic
  timeCharacteristic.setEventHandler(BLEWritten, timeCharacteristicWritten);
  durationCharacteristic.setEventHandler(BLEWritten, durationCharacteristicWritten);
  volumeCharacteristic.setEventHandler(BLEWritten, volumeCharacteristicWritten);
  alarmCharacteristic.setEventHandler(BLEWritten, alarmCharacteristicWritten);

  BLE.advertise();  // Start advertising
  Serial.println("Waiting for connections...");
}

void loop() {
    if (BLE.central().connected()) {
      // poll for Bluetooth® Low Energy events
      BLE.poll();
    }
  
  ms_actual = millis();
  /*
    After the timeout between each step, new values are set for the output devices
  */
  if (alarmAktive == 1 && (ms_actual - ms_last) > timeout) {

    // calling dfmp3.loop() periodically allows for notifications
    // to be handled without interrupts
    dfmp3.loop();

    step++;
    Serial.println("... Next Step: ");
    Serial.println(step);

    Serial.println("setting new values");

    // Brightness LED Strips
    int brightness = 30 + step * 25;
    strip1.setBrightness(brightness);
    strip2.setBrightness(brightness);
    updateStrips();
    Serial.println("brightness");
    Serial.println(brightness);

    // Color LED Strips
    int color = 43 + step * 17;
    colorWipe(strip1.Color(255, color, 0), 5);
    Serial.println("color");
    Serial.println(color);

    // Volume DF Player
    int volume = 10 + step * 2;
    dfmp3.setVolume(volume);
    
    // Alarm auf höchster stufe
    if (step == 9) {
      aktivateLamps();
      colorWipe(strip1.Color(255, 230, 0), 5);
      timeout = ALARM_TIMEOUT;
    }
    // nach 5 Minuten wird der Alarm gestoppt
    if (step == 10) {
      stopAlarm();
    }

    // restart timeout
    ms_last = millis();
  }
}

void startAlarm() {
  Serial.println("-- Starting Alarm --");
  alarmAktive = 1;
  step = 0;
  ms_last = millis();
  calcTimeout();

  strip1.setBrightness(30);
  strip2.setBrightness(30);
  colorWipe(strip1.Color(255, 43, 0), 5);
  updateStrips();
  dfmp3.setVolume(10);
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

//  Sets the timeout between the different steps during the alarm
void calcTimeout() {
  timeout = duration * 60 * 100;
  Serial.println("New Timeout: ");
  Serial.println(timeout);
}

void aktivateLamps() {
  digitalWrite(LAMP_1, HIGH);
  digitalWrite(LAMP_2, HIGH);
  Serial.println("LED on");
}
void deaktivateLamps() {
  digitalWrite(LAMP_1, LOW);
  digitalWrite(LAMP_2, LOW);
  Serial.println("LED off");
}

void updateStrips() {
  strip1.show();
  strip2.show();
}


// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
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
  // central wrote new value to characteristic, update time
  Serial.println("* Characteristic event, written: ");

  timer = timeCharacteristic.value();
  Serial.println("Timer updated: ");
  Serial.println(timer);
}
void durationCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // central wrote new value to characteristic, update time
  Serial.println("* Characteristic event, written: ");
  Serial.println(durationCharacteristic.value());

  if (durationCharacteristic.value() > 0 && durationCharacteristic.value() <= 30) {
    duration = durationCharacteristic.value();
    Serial.println("Duration updated: ");
    Serial.println(duration);
  }
}
void volumeCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // central wrote new value to characteristic, update time
  Serial.println("* Characteristic event, written: ");

  if (volumeCharacteristic.value() > 5 && volumeCharacteristic.value() <= 30) {
    volume = volumeCharacteristic.value();
    Serial.println("Volume updated: ");
    Serial.println(volume);
  }
}
void alarmCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // central wrote new value to characteristic, update time
  Serial.println("* Characteristic event, written: ");

  if (alarmCharacteristic.value() == 1) {
    startAlarm();
  }
  if (alarmCharacteristic.value() == 0) {
    stopAlarm();
  }
}

// implement a notification class,
// its member methods will get called
//
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