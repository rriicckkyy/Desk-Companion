// Desk Companion - Arduino firmware
// Inputs:  touch (tap = play/pause, hold = mic mute), 2 buttons (prev/next), knob (volume 0-9)
// Outputs: OLED (song, time, status), 7-segment (volume), LED (mic muted)
//
// Serial protocol (9600 baud, one message per line):
//   Arduino -> PC:  HELLO, PLAY_PAUSE, MUTE, PREV, NEXT, VOL:<0-9>
//   PC -> Arduino:  SONG:<title>|<artist>, PLAY:<0|1>, MUTE:<0|1>, TIME:<HH:MM>

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

// "_2_" = page buffer mode: uses 256 bytes of RAM instead of 1 KB (the Uno only has 2 KB)
U8G2_SH1107_64X128_2_4W_HW_SPI display(U8G2_R1, /* cs=*/ 10, /* dc=*/ 7, /* reset=*/ 8);

// ---------- Pins ----------
const int TOUCH_PIN = 2;
const int POT_PIN   = A2;
const int PREV_PIN  = A3;
const int NEXT_PIN  = A4;
const int LED_PIN   = A5;

// 7-segment: a, b, c, d, e, f, g (common anode -> LOW = on)
const int SEG_PINS[7] = {3, 4, 5, 6, 9, A0, A1};
const byte DIGITS[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// ---------- Timing ----------
const unsigned long HOLD_MS       = 600;   // touch longer than this = mute instead of play/pause
const unsigned long DEBOUNCE_MS   = 30;
const unsigned long PC_TIMEOUT_MS = 5000;  // no message for this long = "Waiting for PC"

// ---------- State received from the PC ----------
char title[41]     = "";
char artist[31]    = "";
char clockText[6]  = "--:--";
bool micMuted      = false;
bool playing       = false;
unsigned long lastPcMessage = 0;
bool pcSeen        = false;

// ---------- Local state ----------
int  volume      = -1;
bool needsRedraw = true;
bool wasConnected = false;

char lineBuf[80];
byte lineLen = 0;

// ---------- 7-segment ----------
void showDigit(int n) {
  for (int i = 0; i < 7; i++) {
    bool on = (DIGITS[n] >> i) & 1;
    digitalWrite(SEG_PINS[i], on ? LOW : HIGH);
  }
}

void sendVolume() {
  Serial.print("VOL:");
  Serial.println(volume);
}

// ---------- Knob (with hysteresis so the value doesn't flicker between two steps) ----------
void handleKnob() {
  int raw = analogRead(POT_PIN);
  int v = raw * 10L / 1024;
  if (v == volume) return;

  int low  = v * 1024L / 10;
  int high = (v + 1) * 1024L / 10;
  bool insideLow  = (v == 0) || raw > low + 15;
  bool insideHigh = (v == 9) || raw < high - 15;

  if (volume == -1 || (insideLow && insideHigh)) {
    volume = v;
    showDigit(volume);
    sendVolume();
    needsRedraw = true;
  }
}

// ---------- Touch: tap vs. hold ----------
bool touchDown = false;
bool holdFired = false;
unsigned long touchStart = 0;

void handleTouch() {
  bool touched = digitalRead(TOUCH_PIN) == HIGH;
  unsigned long now = millis();

  if (touched && !touchDown) {
    touchDown = true;
    holdFired = false;
    touchStart = now;
  } else if (touched && touchDown && !holdFired && now - touchStart >= HOLD_MS) {
    Serial.println("MUTE");
    holdFired = true;
  } else if (!touched && touchDown) {
    touchDown = false;
    if (!holdFired && now - touchStart >= DEBOUNCE_MS) {
      Serial.println("PLAY_PAUSE");
    }
  }
}

// ---------- Buttons (debounced, fire once per press) ----------
struct Button {
  int pin;
  const char* message;
  bool stable;
  bool lastRead;
  unsigned long changedAt;
};

Button buttons[2] = {
  {PREV_PIN, "PREV", HIGH, HIGH, 0},
  {NEXT_PIN, "NEXT", HIGH, HIGH, 0},
};

void handleButtons() {
  for (auto &b : buttons) {
    bool reading = digitalRead(b.pin);
    if (reading != b.lastRead) {
      b.lastRead = reading;
      b.changedAt = millis();
    }
    if (millis() - b.changedAt > DEBOUNCE_MS && reading != b.stable) {
      b.stable = reading;
      if (b.stable == LOW) {          // INPUT_PULLUP: pressed = LOW
        Serial.println(b.message);
      }
    }
  }
}

// ---------- Messages from the PC ----------
// copies src into dest; returns true if the content changed
bool setText(char* dest, size_t size, const char* src) {
  if (strncmp(dest, src, size - 1) == 0) return false;
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
  return true;
}

void processLine(char* line) {
  lastPcMessage = millis();
  pcSeen = true;

  if (strncmp(line, "SONG:", 5) == 0) {
    char* data = line + 5;
    char* sep = strchr(data, '|');
    const char* newArtist = "";
    if (sep) {
      *sep = '\0';
      newArtist = sep + 1;
    }
    if (setText(title, sizeof(title), data)) needsRedraw = true;
    if (setText(artist, sizeof(artist), newArtist)) needsRedraw = true;
  } else if (strncmp(line, "TIME:", 5) == 0) {
    if (setText(clockText, sizeof(clockText), line + 5)) needsRedraw = true;
  } else if (strncmp(line, "MUTE:", 5) == 0) {
    bool m = line[5] == '1';
    if (m != micMuted) {
      micMuted = m;
      digitalWrite(LED_PIN, micMuted ? HIGH : LOW);
      needsRedraw = true;
    }
  } else if (strncmp(line, "PLAY:", 5) == 0) {
    bool p = line[5] == '1';
    if (p != playing) {
      playing = p;
      needsRedraw = true;
    }
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      processLine(lineBuf);
      lineLen = 0;
    } else if (c != '\r' && lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}

// ---------- OLED ----------
void drawScreen(bool connected) {
  display.firstPage();
  do {
    display.setFont(u8g2_font_6x10_tr);

    if (!connected) {
      display.drawStr(0, 12, "Desk Companion");
      display.drawStr(0, 34, "Waiting for PC...");
    } else {
      // status line + clock
      if (micMuted) {
        display.drawBox(0, 0, 58, 12);
        display.setDrawColor(0);
        display.drawStr(2, 10, "MIC MUTED");
        display.setDrawColor(1);
      } else {
        display.drawStr(0, 10, playing ? "Playing" : "Paused");
      }
      display.drawStr(98, 10, clockText);
      display.drawHLine(0, 14, 128);

      // song
      display.setFont(u8g2_font_7x14B_tr);
      display.drawStr(0, 31, title[0] ? title : "Nothing playing");
      display.setFont(u8g2_font_6x10_tr);
      display.drawStr(0, 45, artist);
    }

    // volume bar
    display.drawFrame(0, 54, 128, 10);
    if (volume > 0) {
      display.drawBox(0, 54, volume * 128 / 9, 10);
    }
  } while (display.nextPage());
}

// ---------- Setup & loop ----------
void setup() {
  Serial.begin(9600);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(PREV_PIN, INPUT_PULLUP);
  pinMode(NEXT_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < 7; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
  }

  display.begin();
  Serial.println("HELLO");
  handleKnob();  // sends the starting volume
}

void loop() {
  handleSerial();
  handleTouch();
  handleButtons();
  handleKnob();

  bool connected = pcSeen && millis() - lastPcMessage < PC_TIMEOUT_MS;
  if (connected != wasConnected) {
    wasConnected = connected;
    needsRedraw = true;
    if (connected) {
      sendVolume();                 // make sure the PC knows the knob position
    } else {
      digitalWrite(LED_PIN, LOW);   // PC gone: don't show a stale mute state
      micMuted = false;
    }
  }

  if (needsRedraw) {
    needsRedraw = false;
    drawScreen(connected);
  }
}