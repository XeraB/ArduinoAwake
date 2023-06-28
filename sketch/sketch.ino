#include <ArduinoBLE.h>

// TODO: set correct ports
const int LAMP_1 = 1;
const int LAMP_2 = 2;

// Timout nach dem der Alarm gestoppt wird
const ALARM_TIMEOUT = 300000;  // 5min: 300000

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
BLEStringCharacteristic timerCharacteristic("19B10010-E8F2-537E-4F6C-D104768A1214", BLERead, 13);

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;

  pinMode(LAMP_1, OUTPUT);
  pinMode(LAMP_2, OUTPUT);
  digitalWrite(LAMP_1, LOW);
  digitalWrite(LAMP_1, LOW);

  // initialize the built-in LED pin
  pinMode(LED_BUILTIN, OUTPUT);

  if (!BLE.begin()) {  // initialize BLE
    Serial.println("starting BLE failed!");
    while (1)
      ;
  }

  BLE.setLocalName("Nano33BLE");                        // Set name for connection
  BLE.setAdvertisedService(timerService);               // Advertise service
  timerService.addCharacteristic(timerCharacteristic);  // Add characteristic to service
  BLE.addService(timerService);                         // Add service

  BLE.advertise();  // Start advertising
  Serial.println("Waiting for connections...");

  delay(2000);
  startAlarm();
}

void loop() {
  ms_actual = millis();
  
  /*
    After the timeout between each step, new values are set for the output devices
  */
  if (alarmAktive == 1 && (ms_actual - ms_last) > timeout) {

    Serial.println("... setting new values");

    // setzen der neuen Werte
    // ...

    step++;
    Serial.println("Next Step: ");
    Serial.println(step);
    ms_last = millis();
    // Alarm auf höchster stufe
    if (step == 9) {
      aktivateLamps();
      timeout = ALARM_TIMEOUT;  // timeout auf 5 Min
    }
    // nach 5 Minuten wird der Alarm gestoppt
    if (step == 10) {
      stopAlarm();
    }
  }
}

void startAlarm() {
  Serial.println("-- Starting Alarm --");
  alarmAktive = 1;
  step = 0;
  ms_last = millis();
  calcTimeout();
}
void stopAlarm() {
  alarmAktive = 0;
  step = 0;
  deaktivateLamps();
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
  digitalWrite(LAMP_1, HIGH);
  Serial.println("LED on");
}
void deaktivateLamps() {
  digitalWrite(LAMP_1, LOW);
  digitalWrite(LAMP_1, LOW);
  Serial.println("LED off");
}