#include <SPI.h>
#include <Ethernet.h>

// ================= NETWORK CONFIG =================
byte mac[] = { 0x02, 0xA0, 0x01, 0x00, 0x00, 0x01 };  // locally administered
IPAddress ip(192, 168, 0, 177); // change to match your gateway etc.
IPAddress gateway(192, 168, 0, 1); // change to match your gateway etc.
IPAddress subnet(255, 255, 255, 0); // change to match your gateway etc.

EthernetServer server(5025);  // match the port in the DAQFactory code

// ================= MEASUREMENTS =================
float temperatureC = 22.75; // dummy measurements, replace with your own, these are here just to test
float humidityPct  = 48.20; // dummy measurements, replace with your own, these are here just to test
float irradiance   = 615.0; // dummy measurements, replace with your own, these are here just to test

// ================= SETUP =================
void setup() {
  Ethernet.init(10);  // CS pin
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  server.begin();
}

// ================= MAIN LOOP =================
void loop() {
  EthernetClient client = server.available();
  if (!client) return;

  // Read command until CR (ASCII 13)
  String cmd = client.readStringUntil('\r');
  cmd.trim();

  // set your data acquisition system (DAS) commands here - make sure these are matched in DAQFactory
  if (cmd == "TEMP?") {
    client.print(temperatureC, 2);
    client.print('\r');
  }
  else if (cmd == "RH?") {
    client.print(humidityPct, 2);
    client.print('\r');
  }
  else if (cmd == "IRRAD?") {
    client.print(irradiance, 1);
    client.print('\r');
  }
  else {
    // Unknown command → return NaN so DAQFactory logs bad value cleanly
    client.print("NaN\r");
  }
  
  client.stop();
}
