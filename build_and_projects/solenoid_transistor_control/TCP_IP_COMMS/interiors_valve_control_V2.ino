// ============================================================
// INTERIORS valve control
// ============================================================
//
// The following code is uploaded to a Wiznet W5500 EVB Pico2
// to handle control of the manifold valves at INTERIORS.
//
// Commands are plain ASCII, one per line (terminated by CR, LF
// or CRLF). Case-insensitive, extra spaces are ignored.
//
//   "<n> ON"    -> valve n output HIGH     (n = 1..18)
//   "<n> OFF"   -> valve n output LOW
//   "ALL OFF"   -> every valve output LOW
//   "ALL ON"    -> every valve output HIGH (cannot be used when valves are connected, 
//                  only for testing)
//   "STATUS"    -> reports all 18 states
//
// Replies (each ends with "\r\n"):
//
//   "OK 3 ON"
//   "OK ALL OFF"
//   "STATUS 010000000000000000"   (valve 1 first, 1 = ON)
//   "ERR <reason>"
//
// A command is only executed once CR or LF is received.
// Backspace is supported and telnet negotiation bytes are
// ignored, so it can be driven by hand from a telnet session.
// ============================================================


#include <SPI.h>
#include <EthernetCompat.h>

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

IPAddress ip(192, 168, 1, 2);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);


// ============================================================
// Valve outputs
//
// Valve 1..18 -> GP0..GP15, GP26, GP27
// (GP16..GP21 are used by the W5500, so they are skipped)
// ============================================================

const uint8_t VALVE_PINS[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
  10, 11, 12, 13, 14, 15, 26, 27
};

const uint8_t NUM_VALVES = sizeof(VALVE_PINS) / sizeof(VALVE_PINS[0]);

bool valveState[NUM_VALVES];

void setValve(uint8_t index, bool on)
{
  digitalWrite(VALVE_PINS[index], on ? HIGH : LOW);
  valveState[index] = on;
}

void allValvesOff()
{
  for (uint8_t i = 0; i < NUM_VALVES; i++)
  {
    setValve(i, false);
  }
}

void allValvesOn()
{
  for (uint8_t i = 0; i < NUM_VALVES; i++)
  {
    setValve(i, true);
  }
}


// ============================================================
// TCP command server
// ============================================================

#define TCP_PORT 5000

EthernetServer server(TCP_PORT);
EthernetClient client;

#define CMD_BUF_SIZE  32

char    cmdBuf[CMD_BUF_SIZE];
uint8_t cmdLen      = 0;
bool    cmdOverflow = false;

// Telnet option negotiation (IAC = 0xFF) is skipped, not
// treated as command text. iacSkip = bytes still to discard.
uint8_t iacSkip = 0;

void resetCommandBuffer()
{
  cmdLen      = 0;
  cmdOverflow = false;
  iacSkip     = 0;
}


// ============================================================
// Fatal init error: blink the onboard LED forever
//
// fast blink (100 ms) = W5500 not detected
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
// Command parsing
// ============================================================

// Upper-case in place and trim leading/trailing whitespace.
// Returns pointer to the first non-space character.
char* normaliseCommand(char* s)
{
  for (char* p = s; *p; p++)
  {
    *p = toupper((unsigned char)*p);
  }

  while (*s == ' ' || *s == '\t')
  {
    s++;
  }

  char* end = s + strlen(s);

  while (end > s && (end[-1] == ' ' || end[-1] == '\t'))
  {
    *--end = '\0';
  }

  return s;
}

// Skip spaces/tabs
char* skipSpaces(char* p)
{
  while (*p == ' ' || *p == '\t')
  {
    p++;
  }
  return p;
}

void processCommand(char* raw)
{
  char* cmd = normaliseCommand(raw);

  if (*cmd == '\0')
  {
    return;   // blank line, ignore quietly
  }

  // ---------------- STATUS ----------------
  if (strcmp(cmd, "STATUS") == 0)
  {
    client.print("STATUS ");

    for (uint8_t i = 0; i < NUM_VALVES; i++)
    {
      client.print(valveState[i] ? '1' : '0');
    }

    client.print("\r\n");
    return;
  }

  // ---------------- ALL ON / ALL OFF ----------------
  if (strncmp(cmd, "ALL", 3) == 0)
  {
    char* rest = skipSpaces(cmd + 3);

    if (strcmp(rest, "OFF") == 0)
    {
      allValvesOff();
      client.print("OK ALL OFF\r\n");
    }
    else if (strcmp(rest, "ON") == 0)
    {
      allValvesOn();
      client.print("OK ALL ON\r\n");
    }
    else
    {
      client.print("ERR only ALL ON or ALL OFF is supported\r\n");
    }
    return;
  }

  // ---------------- <n> ON / <n> OFF ----------------
  char* end;
  long  valve = strtol(cmd, &end, 10);

  if (end == cmd)
  {
    client.print("ERR unknown command\r\n");
    return;
  }

  if (valve < 1 || valve > NUM_VALVES)
  {
    client.print("ERR valve must be 1-");
    client.print(NUM_VALVES);
    client.print("\r\n");
    return;
  }

  char* action = skipSpaces(end);
  bool  on;

  if (strcmp(action, "ON") == 0)
  {
    on = true;
  }
  else if (strcmp(action, "OFF") == 0)
  {
    on = false;
  }
  else
  {
    client.print("ERR expected ON or OFF\r\n");
    return;
  }

  setValve(valve - 1, on);

  client.print("OK ");
  client.print(valve);
  client.print(on ? " ON\r\n" : " OFF\r\n");
}

void finishCommand()
{
  if (cmdOverflow)
  {
    client.print("ERR command too long\r\n");
  }
  else
  {
    cmdBuf[cmdLen] = '\0';
    processCommand(cmdBuf);
  }

  resetCommandBuffer();
}


// ============================================================
// Handle the TCP command client
//
// - Accepts one client at a time
// - Collects characters into a line, runs it only on CR/LF
// ============================================================

void handleTcpClient()
{
  // If the current client is gone, drop it and
  // check for a new incoming connection.
  if (!client.connected())
  {
    client.stop();

    client = server.accept();

    if (client)
    {
      resetCommandBuffer();
    }
  }

  if (!client)
  {
    return;
  }

  // Process any received characters
  while (client.available() > 0)
  {
    uint8_t c = client.read();

    // ---- Telnet negotiation: IAC <cmd> [<option>] ----
    if (iacSkip > 0)
    {
      // After IAC, WILL/WONT/DO/DONT (0xFB-0xFE) carry one
      // more option byte; anything else is a 2-byte sequence.
      if (iacSkip == 2 && (c < 0xFB || c > 0xFE))
      {
        iacSkip = 0;
      }
      else
      {
        iacSkip--;
      }
      continue;
    }

    if (c == 0xFF)
    {
      iacSkip = 2;
      continue;
    }

    // ---- End of line: run the command ----
    if (c == '\r' || c == '\n')
    {
      if (cmdLen > 0 || cmdOverflow)
      {
        finishCommand();
      }
      continue;
    }

    // ---- Backspace / delete ----
    if (c == 0x08 || c == 0x7F)
    {
      if (cmdLen > 0 && !cmdOverflow)
      {
        cmdLen--;
      }
      continue;
    }

    // ---- Ignore any other control or non-ASCII bytes ----
    if (c < 0x20 || c > 0x7E)
    {
      continue;
    }

    if (cmdLen < CMD_BUF_SIZE - 1)
    {
      cmdBuf[cmdLen++] = (char)c;
    }
    else
    {
      cmdOverflow = true;
    }
  }
}


// ============================================================
// Setup
// ============================================================

void setup()
{
  // ----------------------------------------------------------
  // Valve outputs first, all LOW, so nothing is left floating
  // while the network comes up
  // ----------------------------------------------------------

  for (uint8_t i = 0; i < NUM_VALVES; i++)
  {
    pinMode(VALVE_PINS[i], OUTPUT);
  }

  allValvesOff();


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
}


// ============================================================
// Main loop
// ============================================================

void loop()
{
  handleTcpClient();
}
