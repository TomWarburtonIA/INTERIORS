#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// ================= NETWORK CONFIG =================
byte mac[] = { 0x02, 0xA0, 0x01, 0x00, 0x00, 0x02 }; // change to whatever you need it to be
IPAddress ip(192, 168, 1, 201); // change to whatever you need it to be
IPAddress gateway(192, 168, 1, 1); // change to whatever you need it to be
IPAddress subnet(255, 255, 255, 0); // change to whatever you need it to be

EthernetServer server(5025); // change to whatever you need it to be

// ================= ADC =================
Adafruit_ADS1115 ads;   // default I2C addr 0x48

// ================= MEASUREMENTS =================
float temperature;
float humidity;
float irradiance;

// ============== TEMP CALIBRATION ===================
const float tempV1 = 2.137;   // volts at -30C
const float tempT1 = -30.0;
const float tempV2 = 0.778;   // volts at +50C
const float tempT2 = 50.0;
float tempSlope;
float tempIntercept;

// ============== HUMIDITY CALIBRATION ===============
const float humV1 = 1.0;   // volts at 100%
const float humH1 = 100.0;
const float humV2 = 0.0;   // volts at 0%
const float humH2 = 0.0;
float humSlope;
float humIntercept;

// ================= SETUP =================
void setup() {
  Ethernet.init(10);
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  server.begin();

  // ADS1115 setup
  ads.begin();
  ads.setGain(GAIN_ONE);   // ±4.096V range

  // Precompute calibration lines
  tempSlope = (tempT2 - tempT1) / (tempV2 - tempV1);
  tempIntercept = tempT1 - tempSlope * tempV1;

  humSlope = (humH2 - humH1) / (humV2 - humV1);
  humIntercept = humH1 - humSlope * humV1;
}

// ================= MAIN LOOP =================
void loop() {

  // ---------- READ ADC ----------
  int16_t rawTemp = ads.readADC_SingleEnded(0);
  int16_t rawHum  = ads.readADC_SingleEnded(1);
  int16_t rawIrr  = ads.readADC_SingleEnded(2);

  // Convert to volts (ADS1115: 32768 counts = full scale)
  float tempV = rawTemp * (4.096 / 32768.0);
  float humV  = rawHum  * (4.096 / 32768.0);
  float irrV  = rawIrr  * (4.096 / 32768.0);

  // ---------- CONVERT ----------
  temperature = tempSlope * tempV + tempIntercept;
  humidity    = humSlope * humV + humIntercept;
  irradiance  = irrV;   // raw volts for now

  // ---------- ETHERNET ----------
  EthernetClient client = server.available();
  if (client) {
    String cmd = client.readStringUntil('\r');
    cmd.trim();

    if (cmd == "TEMP?") {
      client.print(temperature, 2);
      client.print('\r');
    }
    else if (cmd == "RH?") {
      client.print(humidity, 2);
      client.print('\r');
    }
    else if (cmd == "IRRAD?") {
      client.print(irradiance, 3);
      client.print('\r');
    }
    else {
      client.print("NaN\r");
    }
    client.stop();
  }

  delay(1000);
}
