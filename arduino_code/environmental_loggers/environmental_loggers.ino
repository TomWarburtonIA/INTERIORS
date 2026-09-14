// ============================================================
// INTERIORS temperature and relative humidity sensor loggers
// ============================================================
//
// The following code is uploaded to each Wiznet W5500 EVB Pico2
// to handle data processing and polling for MetOne 085a 
// temperature and relative humidity probes. PCB designs are 
// held in the INTERIORS GitHub repo.
//
// To run this code yourself on a Wiznet W5500 EVB Pico2 in the
// Arduino IDE, you must first install the Wiznet boards. 
// Instructions for this can be found here:
// https://maker.wiznet.io/matthew/projects/how-to-use-wiznet-evb-pico2-in-arduino-ide/

#include <SPI.h>
#include <Wire.h>
#include <EthernetCompat.h>   
#include <Adafruit_ADS1X15.h>

#define W5500_MISO   16
#define W5500_CS     17
#define W5500_SCLK   18
#define W5500_MOSI   19
#define W5500_RESET  20
#define W5500_INT    21

Wiznet5500lwIP eth(W5500_CS, SPI, W5500_INT);

// ============================================================
// Ethernet static IP configuration
// ============================================================

IPAddress ip(192, 168, 1, 190);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

// ============================================================
// TCP command server
//
// This is configured to receive an 'A' over ASCII, upon which
// it will then transmit the calculated temperature (2 d.p) 
// and the relative humidity (1 d.p), separated by a comma.
// ============================================================

#define TCP_PORT 5000

EthernetServer server(TCP_PORT);
EthernetClient client;

// ============================================================
// ADS1115 I2C configuration
// ============================================================

#define I2C_SDA 4
#define I2C_SCL 5

#define ADS1115_ADDRESS 0x48

Adafruit_ADS1115 ads;

// ============================================================
// Thermistor calibration
// ADS1115 A3 pin
// ============================================================
// This uses a Met One 085a T/RH sensor, the temperature
// signal being divided through an 18k7 Ohm precision resistor,
// itself fed by a LM4040 2.5V shunt acting as a reference
// voltage. Calibration values are:
//
// -30 C = 2.137 V
// +50 C = 0.778 V

const float TEMP_LOW_C = -30.0;
const float TEMP_LOW_V = 2.137;

const float TEMP_HIGH_C = 50.0;
const float TEMP_HIGH_V = 0.778;

// ============================================================
// Latest readings (updated by readSensors)
// ============================================================

float temperatureC     = 0.0;
float relativeHumidity = 0.0;


// ============================================================
// Fatal init error: blink the onboard LED forever
//
// fast blink (100 ms) = W5500 not detected
// slow blink (500 ms) = ADS1115 not detected
// ============================================================

void haltBlink(unsigned long periodMs)
{
  pinMode(LED_BUILTIN, OUTPUT);

  while (true)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(periodMs);

    digitalWrite(LED_BUILTIN, LOW);
    delay(periodMs);
  }
}


// ============================================================
// Convert thermistor voltage to temperature
// ============================================================

float thermistorVoltageToTemperature(float voltage)
{
  float temperature =
      TEMP_LOW_C +
      ((voltage - TEMP_LOW_V) *
      (TEMP_HIGH_C - TEMP_LOW_C) /
      (TEMP_HIGH_V - TEMP_LOW_V));

  return temperature;
}


// ============================================================
// Convert humidity sensor voltage to %RH
//
// 0.0 V = 0 %RH
// 1.0 V = 100 %RH
// ============================================================

float humidityVoltageToRH(float voltage)
{
  float rh = voltage * 100.0;

  // Limit value to 0-100 %
  if (rh < 0.0)
    rh = 0.0;

  if (rh > 100.0)
    rh = 100.0;

  return rh;
}


// ============================================================
// Read both sensor channels and update the global values
// ============================================================

void readSensors()
{
  // ----------------------------------------------------------
  // Thermistor on ADS1115 A3.
  // Reaches at least 2.137 V, so GAIN_ONE = +/- 4.096 V.
  // ----------------------------------------------------------

  ads.setGain(GAIN_ONE);

  int16_t rawTemperature = ads.readADC_SingleEnded(3);

  float thermistorVoltage = ads.computeVolts(rawTemperature);

  temperatureC = thermistorVoltageToTemperature(thermistorVoltage);


  // ----------------------------------------------------------
  // Humidity sensor on ADS1115 A2.
  // Output is 0-1 V, so GAIN_TWO = +/- 2.048 V for better
  // resolution.
  // ----------------------------------------------------------

  ads.setGain(GAIN_TWO);

  int16_t rawHumidity = ads.readADC_SingleEnded(2);

  float humidityVoltage = ads.computeVolts(rawHumidity);

  relativeHumidity = humidityVoltageToRH(humidityVoltage);
}


// ============================================================
// Handle the TCP command client
//
// - Accepts one client at a time
// - On receiving 'A', takes a fresh reading and replies:
//   "<temp C>,<RH %>\r\n"
// ============================================================

void handleTcpClient()
{
  // If the current client is gone, drop it and
  // check for a new incoming connection.
  if (!client.connected())
  {
    client.stop();

    client = server.accept();
  }

  if (!client)
  {
    return;
  }

  // Process any received characters
  while (client.available() > 0)
  {
    char c = client.read();

    if (c == 'A')
    {
      // Take a fresh reading so the reply is current
      readSensors();

      client.print(temperatureC, 2);
      client.print(',');
      client.print(relativeHumidity, 1);
      client.print("\r\n");
    }

    // Everything else (CR, LF, other characters) is ignored
  }
}


// ============================================================
// Setup
// ============================================================

void setup()
{
  // ----------------------------------------------------------
  // Reset W5500
  // ----------------------------------------------------------

  pinMode(W5500_RESET, OUTPUT);

  digitalWrite(W5500_RESET, LOW);
  delay(10);

  digitalWrite(W5500_RESET, HIGH);
  delay(100);


  // ----------------------------------------------------------
  // Configure W5500 SPI pins
  // ----------------------------------------------------------

  SPI.setRX(W5500_MISO);
  SPI.setCS(W5500_CS);
  SPI.setSCK(W5500_SCLK);
  SPI.setTX(W5500_MOSI);


  // ----------------------------------------------------------
  // Configure static IP and start Ethernet
  // ----------------------------------------------------------

  eth.config(ip, gateway, subnet, dns);

  if (!eth.begin())
  {
    haltBlink(100);
  }


  // ----------------------------------------------------------
  // Start TCP command server
  // ----------------------------------------------------------

  server.begin();


  // ----------------------------------------------------------
  // Configure I2C and start ADS1115
  // ----------------------------------------------------------

  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  if (!ads.begin(ADS1115_ADDRESS, &Wire))
  {
    haltBlink(500);
  }
}


// ============================================================
// Main loop
// ============================================================

void loop()
{
  handleTcpClient();
}
