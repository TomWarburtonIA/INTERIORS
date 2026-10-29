// ===================== RECEIVER (solenoid side) =====================
#include <M5Unified.h>
#include <RadioLib.h>

// ---------- Pins ----------
#define LORA_CS_PIN    23
#define LORA_IRQ_PIN   15
#define LORA_BUSY_PIN  19
#define SOLENOID_PIN   2      // G2

// ---------- Must match the transmitter ----------
const float LORA_FREQ = 868.0;
const char* NET_ID    = "N1:";   // packets without this prefix are ignored

// ---------- Receiver settings ----------
const unsigned long MANUAL_MAX_ON_MS = 60000;  // safety cut-off for manual ON (0 = never)
const int           MAX_TIMED_SECS   = 600;    // reject silly timed requests

SX1262 radio = new Module(LORA_CS_PIN, LORA_IRQ_PIN, RADIOLIB_NC, LORA_BUSY_PIN);
M5Canvas canvas(&M5.Display);    // off-screen buffer = no flicker

volatile bool radioFlag = false;
void IRAM_ATTR onRadio() { radioFlag = true; }

bool linked = false;
bool solenoidOn = false;
unsigned long onSince = 0;
unsigned long onDurationMs = 0;  // 0 = stays on until told otherwise
String lastCmd = "-";
bool redraw = true;
unsigned long lastDraw = 0;

// ---------------------------------------------------------------------
long remainingSecs() {
  if (!solenoidOn) return 0;
  if (onDurationMs == 0) return -1;              // -1 = on, no timer
  unsigned long elapsed = millis() - onSince;
  if (elapsed >= onDurationMs) return 1;
  return (onDurationMs - elapsed + 999) / 1000;
}

void setSolenoid(bool on, unsigned long durationMs = 0) {
  digitalWrite(SOLENOID_PIN, on ? HIGH : LOW);
  solenoidOn   = on;
  onSince      = millis();
  onDurationMs = on ? durationMs : 0;
  redraw = true;
}

void reply(const String& body) {
  String pkt = String(NET_ID) + body;
  radio.transmit(pkt.c_str());
  radioFlag = false;        // finishing a transmit also fires the IRQ - ignore it
  radio.startReceive();
}

void handlePacket(const String& msg) {
  if (!msg.startsWith(NET_ID)) return;           // not from our transmitter
  String cmd = msg.substring(strlen(NET_ID));

  if (cmd == "HI") {
    reply("RDY");                                // always answer, so re-linking works
  } else {
    if (cmd == "ON") {
      setSolenoid(true, MANUAL_MAX_ON_MS);
    } else if (cmd == "OFF") {
      setSolenoid(false);
    } else if (cmd.startsWith("T")) {            // e.g. "T10" = 10 seconds
      int secs = cmd.substring(1).toInt();
      if (secs <= 0 || secs > MAX_TIMED_SECS) return;
      setSolenoid(true, secs * 1000UL);
    } else {
      return;                                    // unknown command
    }
    // ACK tells the transmitter the real state: 0 = off, -1 = on, N = N secs left
    reply("ACK" + String(remainingSecs()));
  }

  linked  = true;
  lastCmd = cmd;
  redraw  = true;
}

void drawStatus() {
  canvas.fillScreen(TFT_BLACK);
  canvas.setCursor(0, 0);

  canvas.setTextColor(TFT_WHITE);
  canvas.println("RECEIVER");

  canvas.setTextColor(linked ? TFT_GREEN : TFT_YELLOW);
  canvas.println(linked ? "LINKED" : "WAITING");

  canvas.setTextColor(solenoidOn ? TFT_RED : TFT_WHITE);
  if (!solenoidOn)        canvas.println("SOL OFF");
  else if (!onDurationMs) canvas.println("SOL ON");
  else                    canvas.printf("SOL ON %lds\n", remainingSecs());

  canvas.setTextColor(TFT_DARKGREY);
  canvas.println("LAST " + lastCmd);

  canvas.pushSprite(0, 0);
  redraw = false;
  lastDraw = millis();
}

void fail(int code) {
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_RED);
  M5.Display.printf("LoRa FAIL\ncode %d", code);
  while (true) delay(1000);
}

// ---------------------------------------------------------------------
void setup() {
  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW);               // solenoid off before anything else

  M5.begin(M5.config());
  canvas.setColorDepth(8);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.setTextSize(2);

  int state = radio.begin(LORA_FREQ);
  if (state != RADIOLIB_ERR_NONE) fail(state);

  radio.setPacketReceivedAction(onRadio);
  radio.startReceive();
  drawStatus();
}

void loop() {
  M5.update();

  if (radioFlag) {
    radioFlag = false;
    String msg;
    if (radio.readData(msg) == RADIOLIB_ERR_NONE) handlePacket(msg);
    radio.startReceive();
  }

  // Timer finished (timed mode or manual safety cut-off)
  if (solenoidOn && onDurationMs && millis() - onSince >= onDurationMs) {
    setSolenoid(false);
  }

  // Redraw on change, or once a second while counting down
  if (redraw || (solenoidOn && onDurationMs && millis() - lastDraw >= 1000)) {
    drawStatus();
  }
}
