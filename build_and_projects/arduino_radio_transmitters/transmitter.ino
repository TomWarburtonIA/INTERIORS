// ===================== TRANSMITTER (remote) =====================
// Button A: timed mode -> 1/2/3 presses = 5/10/15 s
//           manual mode -> toggle solenoid on/off
// Button B: switch between timed and manual mode
#include <M5Unified.h>
#include <RadioLib.h>

// ---------- Pins ----------
#define LORA_CS_PIN    23
#define LORA_IRQ_PIN   15
#define LORA_BUSY_PIN  19

// ---------- Must match the receiver ----------
const float LORA_FREQ = 868.0;
const char* NET_ID    = "N1:";

// ---------- Transmitter settings ----------
const unsigned long HELLO_INTERVAL_MS = 2000;  // how often to look for the receiver
const unsigned long ACK_TIMEOUT_MS    = 1000;  // wait this long for a reply
const int           CMD_TRIES         = 3;     // resend a command up to this many times
const unsigned long PRESS_WINDOW_MS   = 2000;
const int           SECS_PER_PRESS    = 5;
const int           MAX_PRESSES       = 3;

SX1262 radio = new Module(LORA_CS_PIN, LORA_IRQ_PIN, RADIOLIB_NC, LORA_BUSY_PIN);
M5Canvas canvas(&M5.Display);

volatile bool radioFlag = false;
void IRAM_ATTR onRadio() { radioFlag = true; }

bool linked = false;
bool manualMode = false;
unsigned long lastHello = 0;

int presses = 0;
unsigned long firstPress = 0;

// What the receiver last told us about its solenoid
bool remoteOn = false;
unsigned long remoteSince = 0;
unsigned long remoteDurMs = 0;   // 0 = no timer

String note = "";
bool redraw = true;
unsigned long lastDraw = 0;

// ---------------------------------------------------------------------
void drawStatus() {
  canvas.fillScreen(TFT_BLACK);
  canvas.setCursor(0, 0);

  canvas.setTextColor(TFT_WHITE);
  canvas.println("TRANSMITTER");

  canvas.setTextColor(linked ? TFT_GREEN : TFT_YELLOW);
  canvas.println(linked ? "LINKED" : "SEARCHING");

  canvas.setTextColor(TFT_CYAN);
  canvas.println(manualMode ? "MODE MANUAL" : "MODE TIMED");

  canvas.setTextColor(remoteOn ? TFT_RED : TFT_WHITE);
  if (!remoteOn) {
    canvas.println("SOL OFF");
  } else if (!remoteDurMs) {
    canvas.println("SOL ON");
  } else {
    unsigned long elapsed = millis() - remoteSince;
    unsigned long left = elapsed >= remoteDurMs ? 0 : (remoteDurMs - elapsed + 999) / 1000;
    canvas.printf("SOL ON %lus\n", left);
  }

  if (presses > 0) {
    canvas.setTextColor(TFT_WHITE);
    canvas.printf("PRESSES %d\n", presses);
  }

  canvas.setTextColor(TFT_DARKGREY);
  canvas.println(note);

  canvas.pushSprite(0, 0);
  redraw = false;
  lastDraw = millis();
}

// Send a packet and wait for a reply starting with `expect`.
// Returns the reply body, or "" if nothing came back.
String sendCmd(const String& cmd, const char* expect, int tries) {
  String pkt = String(NET_ID) + cmd;

  for (int i = 0; i < tries; i++) {
    radio.transmit(pkt.c_str());
    radioFlag = false;            // finishing a transmit also fires the IRQ - ignore it
    radio.startReceive();

    unsigned long t0 = millis();
    while (millis() - t0 < ACK_TIMEOUT_MS) {
      if (radioFlag) {
        radioFlag = false;
        String rx;
        bool ok = radio.readData(rx) == RADIOLIB_ERR_NONE;
        radio.startReceive();
        if (ok && rx.startsWith(NET_ID)) {
          String body = rx.substring(strlen(NET_ID));
          if (body.startsWith(expect)) return body;
        }
      }
      delay(1);
    }
  }
  return "";
}

// Send a solenoid command with retries and update our view of the receiver
void command(const String& cmd, const String& label) {
  note = "SENDING..";
  drawStatus();

  String ack = sendCmd(cmd, "ACK", CMD_TRIES);
  if (ack.length()) {
    long secs = ack.substring(3).toInt();        // 0 = off, -1 = on, N = N secs left
    remoteOn    = (secs != 0);
    remoteSince = millis();
    remoteDurMs = secs > 0 ? secs * 1000UL : 0;
    note = "OK " + label;
  } else {
    linked = false;                              // go back to searching
    note = "NO REPLY";
  }
  redraw = true;
}

void sendTimed() {
  int secs = presses * SECS_PER_PRESS;
  presses = 0;
  command("T" + String(secs), String(secs) + "s");
}

void fail(int code) {
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_RED);
  M5.Display.printf("LoRa FAIL\ncode %d", code);
  while (true) delay(1000);
}

// ---------------------------------------------------------------------
void setup() {
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

  // Look for the receiver until it answers
  if (!linked && millis() - lastHello >= HELLO_INTERVAL_MS) {
    if (sendCmd("HI", "RDY", 1).length()) {
      linked = true;
      note = "LINKED";
    }
    lastHello = millis();
    redraw = true;
  }

  // Button B: switch mode
  if (M5.BtnB.wasPressed()) {
    manualMode = !manualMode;
    presses = 0;
    redraw = true;
  }

  // Button A: send / count presses
  if (M5.BtnA.wasPressed()) {
    if (!linked) {
      note = "NOT LINKED";
      redraw = true;
    } else if (manualMode) {
      command(remoteOn ? "OFF" : "ON", remoteOn ? "OFF" : "ON");
    } else {
      if (presses == 0) firstPress = millis();
      presses++;
      redraw = true;
      if (presses >= MAX_PRESSES) sendTimed();
    }
  }

  // Press window closed: send what we have
  if (presses > 0 && millis() - firstPress >= PRESS_WINDOW_MS) sendTimed();

  // The receiver's timer will have run out by now
  if (remoteOn && remoteDurMs && millis() - remoteSince >= remoteDurMs) {
    remoteOn = false;
    redraw = true;
  }

  // Redraw on change, or once a second while counting down
  if (redraw || (remoteOn && remoteDurMs && millis() - lastDraw >= 1000)) {
    drawStatus();
  }
}
