#include <SPI.h>
#include <BLEPeripheral.h>
#include <EEPROM.h>

#define UUID      "6ABC61D8-17A4-11E6-B6BA-3E1D05DEFE78"

#define BLE_REQ   6
#define BLE_RDY   7
#define BLE_RST   4

#define byte_min_tem        0
#define byte_max_tem        1
#define byte_min_humidity   2
#define byte_max_humidity   3
#define byte_min_light      4
#define byte_max_light      5

#define temperaturePin  A0
#define lightPin        A1
#define humidityPin     A2
#define batteryPin      A8
#define ledRGB_red      9
#define ledRGB_green    10
#define ledRGB_blue     11

#define interval_read_sensors  3000

enum State{
  OK,
  TEMPERATURE_WRONG,
  HUMIDITY_WRONG,
  LIGHT_WRONG
};

State state;

int currentTemperature = 0, currentHumidity = 0, currentLight = 0;
int minTemperature = 10, maxTemperature = 40, minHumidity = 25, maxHumidity = 75, minLight = 25, maxLight = 7;

BLEPeripheral blePeripheral = BLEPeripheral(BLE_REQ, BLE_RDY, BLE_RST);

BLEService sensorsService = BLEService("0000ccc0-0000-1000-8000-00805f9b34fb");
BLEIntCharacteristic tempCharacteristic = BLEIntCharacteristic("0000ccc1-0000-1000-8000-00805f9b34fb", BLERead | BLENotify);
BLEIntCharacteristic humidityCharacteristic = BLEIntCharacteristic("0000ccc2-0000-1000-8000-00805f9b34fb", BLERead | BLENotify);
BLEIntCharacteristic lightCharacteristic = BLEIntCharacteristic("0000ccc3-0000-1000-8000-00805f9b34fb", BLERead | BLENotify);
BLEIntCharacteristic batteryCharacteristic = BLEIntCharacteristic("0000ccc4-0000-1000-8000-00805f9b34fb", BLERead | BLENotify);

BLEService sensorsConfigService = BLEService("0000ddd0-0000-1000-8000-00805f9b34fb");
BLEIntCharacteristic tempMinCharacteristic = BLEIntCharacteristic("0000ddd1-0000-1000-8000-00805f9b34fb", BLERead | BLEWrite);
BLEIntCharacteristic tempMaxCharacteristic = BLEIntCharacteristic("0000ddd2-0000-1000-8000-00805f9b34fb", BLERead | BLEWrite);
BLEIntCharacteristic humidityMinCharacteristic = BLEIntCharacteristic("0000ddd3-0000-1000-8000-00805f9b34fb", BLERead | BLEWrite);
BLEIntCharacteristic humidityMaxCharacteristic = BLEIntCharacteristic("0000ddd4-0000-1000-8000-00805f9b34fb", BLERead | BLEWrite);
BLEIntCharacteristic lightMinCharacteristic = BLEIntCharacteristic("0000ddd5-0000-1000-8000-00805f9b34fb", BLERead | BLEWrite);
BLEIntCharacteristic lightMaxCharacteristic = BLEIntCharacteristic("0000ddd6-0000-1000-8000-00805f9b34fb", BLERead | BLEWrite);

unsigned long lastRead;

void setup() {
  Serial.begin(9600);

  blePeripheral.setLocalName("SmartPlant");
  blePeripheral.setDeviceName("SmartPlant");

  blePeripheral.setAdvertisedServiceUuid(UUID);
  blePeripheral.addAttribute(sensorsService);
  blePeripheral.addAttribute(tempCharacteristic);
  blePeripheral.addAttribute(humidityCharacteristic);
  blePeripheral.addAttribute(lightCharacteristic);
  blePeripheral.addAttribute(batteryCharacteristic);
  blePeripheral.addAttribute(sensorsConfigService);
  blePeripheral.addAttribute(tempMinCharacteristic);
  blePeripheral.addAttribute(tempMaxCharacteristic);
  blePeripheral.addAttribute(humidityMinCharacteristic);
  blePeripheral.addAttribute(humidityMaxCharacteristic);
  blePeripheral.addAttribute(lightMinCharacteristic);
  blePeripheral.addAttribute(lightMaxCharacteristic);

  tempMinCharacteristic.setEventHandler(BLEWritten, tempMinCharacteristicWritten);
  tempMaxCharacteristic.setEventHandler(BLEWritten, tempMaxCharacteristicWritten);
  humidityMinCharacteristic.setEventHandler(BLEWritten, humidityMinCharacteristicWritten);
  humidityMaxCharacteristic.setEventHandler(BLEWritten, humidityMaxCharacteristicWritten);
  lightMinCharacteristic.setEventHandler(BLEWritten, lightMinCharacteristicWritten);
  lightMaxCharacteristic.setEventHandler(BLEWritten, lightMaxCharacteristicWritten);
  
  minTemperature = EEPROM.read(byte_min_tem);
  maxTemperature = EEPROM.read(byte_max_tem);
  minHumidity = EEPROM.read(byte_min_humidity);
  maxHumidity = EEPROM.read(byte_max_humidity);
  minLight = EEPROM.read(byte_min_light);
  maxLight = EEPROM.read(byte_max_light);
  
  blePeripheral.begin();
  lastRead = 0;
  
  pinMode(ledRGB_red, OUTPUT);
  pinMode(ledRGB_green, OUTPUT);
  pinMode(ledRGB_blue, OUTPUT);
}

void loop() {
  blePeripheral.poll();

  if((millis() - lastRead) > interval_read_sensors) {
    lastRead = millis();
    updateTempCharacteristicValue();
    updateHumidityCharacteristicValue();
    updateLightCharacteristicValue();
    updateBatteryCharacteristicValue();
  
    checkPlantState(currentTemperature, currentHumidity, currentLight);
  
    switch (state) {
      case OK:
        digitalWrite(ledRGB_red, LOW);
        digitalWrite(ledRGB_green, HIGH);
        digitalWrite(ledRGB_blue, LOW);
        break;
      case TEMPERATURE_WRONG:
        digitalWrite(ledRGB_red, HIGH);
        digitalWrite(ledRGB_green, LOW);
        digitalWrite(ledRGB_blue, LOW);
        break;
      case HUMIDITY_WRONG:
        digitalWrite(ledRGB_red, LOW);
        digitalWrite(ledRGB_green, LOW);
        digitalWrite(ledRGB_blue, HIGH);
        break;
      case LIGHT_WRONG:
        digitalWrite(ledRGB_red, HIGH);
        digitalWrite(ledRGB_green, HIGH);
        digitalWrite(ledRGB_blue, LOW);
        break;
    }
  }
}

void checkPlantState(int temperature, int humidity, int light) {
  if((minTemperature <= currentTemperature && currentTemperature <= maxTemperature) && (minHumidity <= currentHumidity && currentHumidity <= maxHumidity) && (minLight <= currentLight && currentLight <= maxLight)) {
    state = OK;
  } else if(minTemperature > temperature || maxTemperature < temperature) {
    state = TEMPERATURE_WRONG;
  } else if(minHumidity > humidity || maxHumidity < humidity) {
    state = HUMIDITY_WRONG;
  } else if(minLight > light || maxLight < light) {
    state = LIGHT_WRONG;
  }
}

void updateTempCharacteristicValue() {
  float voltage = analogRead(temperaturePin) * (3300/1024);
  currentTemperature = (voltage - 500) / 10;
  tempCharacteristic.setValue(currentTemperature);
}

void updateHumidityCharacteristicValue() {
  float voltage = analogRead(humidityPin);
  voltage = constrain(voltage, 45, 680);
  currentHumidity = map(voltage, 45, 680, 0, 100);
  humidityCharacteristic.setValue(currentHumidity);
}

void updateLightCharacteristicValue() {
  int lightLevel = analogRead(lightPin);
  lightLevel = constrain(lightLevel, 0, 1024);
  currentLight = map(lightLevel, 0, 1024, 0, 100);
  lightCharacteristic.setValue(currentLight);
}

void updateBatteryCharacteristicValue() {
  int voltage = analogRead(batteryPin);
  voltage = map(voltage, 0, 1024, 0, 100);
  batteryCharacteristic.setValue(voltage);
}

void tempMinCharacteristicWritten(BLECentral& central, BLECharacteristic& characteristic) {
  minTemperature = tempMinCharacteristic.value();
  EEPROM.write(byte_min_tem, tempMinCharacteristic.value());
}

void tempMaxCharacteristicWritten(BLECentral& central, BLECharacteristic& characteristic) {
  maxTemperature = tempMaxCharacteristic.value();
  EEPROM.write(byte_max_tem, tempMaxCharacteristic.value());
}

void humidityMinCharacteristicWritten(BLECentral& central, BLECharacteristic& characteristic) {
  minHumidity = humidityMinCharacteristic.value();
  EEPROM.write(byte_min_humidity, humidityMinCharacteristic.value());
}

void humidityMaxCharacteristicWritten(BLECentral& central, BLECharacteristic& characteristic) {
  maxHumidity = humidityMaxCharacteristic.value();
  EEPROM.write(byte_max_humidity, humidityMaxCharacteristic.value());
}

void lightMinCharacteristicWritten(BLECentral& central, BLECharacteristic& characteristic) {
  minLight = lightMinCharacteristic.value();
  EEPROM.write(byte_min_light, lightMinCharacteristic.value());
}

void lightMaxCharacteristicWritten(BLECentral& central, BLECharacteristic& characteristic) {
  maxLight = lightMaxCharacteristic.value();
  EEPROM.write(byte_max_light, lightMaxCharacteristic.value());
}
