#include <Wire.h>
#include <ModbusRtu.h>
#include "Adafruit_HTU21DF.h"

// modbus setup
int16_t au16data[21] = {0};  // Data array
Modbus slave(1, Serial, 0);  // Slave ID 1, using Serial, no DE/RE pin, not sure if its needed

// Define pins
const int solenoid1 = 22;
const int solenoid2 = 23;
const int solenoid3 = 24;
const int solenoid4 = 25;
const int solenoid5 = 26;
const int solenoid6 = 27;
const int solenoid7 = 28;
const int solenoid8 = 29;
const int solenoid9 = 30;
const int solenoid10 = 31;
const int solenoid11 = 32;
const int solenoid12 = 33;
const int solenoid13 = 34;
const int solenoid14 = 35;
const int solenoid15 = 36;
const int solenoid16 = 37;
const int fan = 13;

Adafruit_HTU21DF htu = Adafruit_HTU21DF();

void setup() {
  slave.begin(9600);   // baud rate

  pinMode(solenoid1, OUTPUT);  
  pinMode(solenoid2, OUTPUT);
  pinMode(solenoid3, OUTPUT);  
  pinMode(solenoid4, OUTPUT);
  pinMode(solenoid5, OUTPUT);  
  pinMode(solenoid6, OUTPUT);
  pinMode(solenoid7, OUTPUT);  
  pinMode(solenoid8, OUTPUT);
  pinMode(solenoid9, OUTPUT);  
  pinMode(solenoid10, OUTPUT);
  pinMode(solenoid11, OUTPUT); 
  pinMode(solenoid12, OUTPUT);
  pinMode(solenoid13, OUTPUT); 
  pinMode(solenoid14, OUTPUT);
  pinMode(solenoid15, OUTPUT); 
  pinMode(solenoid16, OUTPUT);
  pinMode(fan, OUTPUT);
}

void loop() {
  // solenoid and fan control
  digitalWrite(solenoid1,  au16data[0]  ? HIGH : LOW);
  digitalWrite(solenoid2,  au16data[1]  ? HIGH : LOW);
  digitalWrite(solenoid3,  au16data[2]  ? HIGH : LOW);
  digitalWrite(solenoid4,  au16data[3]  ? HIGH : LOW);
  digitalWrite(solenoid5,  au16data[4]  ? HIGH : LOW);
  digitalWrite(solenoid6,  au16data[5]  ? HIGH : LOW);
  digitalWrite(solenoid7,  au16data[6]  ? HIGH : LOW);
  digitalWrite(solenoid8,  au16data[7]  ? HIGH : LOW);
  digitalWrite(solenoid9,  au16data[8]  ? HIGH : LOW);
  digitalWrite(solenoid10, au16data[9]  ? HIGH : LOW);
  digitalWrite(solenoid11, au16data[10] ? HIGH : LOW);
  digitalWrite(solenoid12, au16data[11] ? HIGH : LOW);
  digitalWrite(solenoid13, au16data[12] ? HIGH : LOW);
  digitalWrite(solenoid14, au16data[13] ? HIGH : LOW);
  digitalWrite(solenoid15, au16data[14] ? HIGH : LOW);
  digitalWrite(solenoid16, au16data[15] ? HIGH : LOW);
  digitalWrite(fan,        au16data[16] ? HIGH : LOW);

  // sensor readings
  if (htu.begin()) {
    au16data[17] = htu.readTemperature() * 1000; // divide by 1000 in DAAQFactory to get back to decimalised
    au16data[18] = htu.readHumidity() * 1000; // divide by 1000 in DAAQFactory to get back to decimalised
  }

  // comms test logic
  if (au16data[19] == 1) {
    au16data[20] = 1;        // signal DAQFactory that Arduino received the ping
  } else {
    au16data[20] = 0;        // default state
  }

  // poll Modbus
  slave.poll(au16data, 21);  // pass entire array
}
