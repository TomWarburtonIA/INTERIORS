#include <SPI.h>
#include <Ethernet.h>

// ================= NETWORK CONFIG =================
byte mac[] = { 0x02, 0xA0, 0x01, 0x00, 0x00, 0x01 };  // locally administered
IPAddress ip(192, 168, 0, 177);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

EthernetServer server(5025);  // match DAQFactory initTCP()

// ================= DUMMY MEASUREMENTS =================
float temperatureC = 22.75;
float humidityPct  = 48.20;
float irradiance   = 615.0;

// ================= SETUP =================
void setup() {
  Ethernet.init(10);  // CS pin (safe for Ethernet Shield 2 / W5500)
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

  // Dispatch commands (DAQFactory paramNames)
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

  // Close connection (DAQFactory locks per poll)
  client.stop();
}
