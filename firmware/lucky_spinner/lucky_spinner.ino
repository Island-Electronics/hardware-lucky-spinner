#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// Hardware pin mapping for the current board revision.
static const uint8_t PIN_BTN_UP = 6;
static const uint8_t PIN_BTN_LEFT = 7;
static const uint8_t PIN_BTN_RIGHT = 4;
static const uint8_t PIN_BTN_DOWN = 2;

static const uint8_t PIN_NEOPIXEL = 8;
static const uint8_t PIN_BUZZER = 9;

static const uint16_t NEOPIXEL_COUNT = 3;
static const uint8_t LED_BRIGHTNESS_DIVIDER = 18;  // 1 = full brightness.

// SSD1306 128x32 on standard I2C pins, no dedicated reset pin.
static const int SCREEN_W = 128;
static const int SCREEN_H = 32;
static const int OLED_RESET = -1;
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

Adafruit_NeoPixel strip(NEOPIXEL_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// App limits and display constraints.
static const uint16_t N_MIN = 1;
static const uint16_t N_MAX = 9999;
static const uint16_t MAX_WINNERS_STORED = 120;
static const uint8_t VISIBLE_WINNER_ROWS = 3;

struct Button {
  uint8_t pin;
  bool lastStable = true;   // INPUT_PULLUP: HIGH means released.
  bool lastRead = true;
  uint32_t lastChangeMs = 0;
  bool fellEvent = false;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
    lastStable = digitalRead(pin);
    lastRead = lastStable;
  }

  void update() {
    fellEvent = false;
    const bool readNow = digitalRead(pin);
    if (readNow != lastRead) {
      lastRead = readNow;
      lastChangeMs = millis();
    }

    if ((millis() - lastChangeMs) > 25) {
      if (lastStable != lastRead) {
        lastStable = lastRead;
        if (lastStable == LOW) {
          fellEvent = true;
        }
      }
    }
  }

  bool pressed() const { return fellEvent; }
};

Button btnUp, btnLeft, btnRight, btnDown;

// PRNG: xorshift32 + rejection sampling to avoid modulo bias.
static uint32_t rngState = 0x12345678u;

static inline uint32_t xorshift32() {
  uint32_t x = rngState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rngState = x;
  return x;
}

static uint32_t uniform_u32(uint32_t n) {
  if (n == 0) return 0;
  const uint32_t limit = (uint32_t)(0xFFFFFFFFu / n) * n;
  while (true) {
    const uint32_t r = xorshift32();
    if (r < limit) return (r % n);
  }
}

static inline void mixEntropy(uint32_t v) {
  rngState ^= v + 0x9E3779B9u + (rngState << 6) + (rngState >> 2);
}

static void gatherEntropyFromPress(uint8_t whichBtn) {
  mixEntropy((uint32_t)micros());
  mixEntropy(((uint32_t)millis() << 8) ^ whichBtn);
}

static inline void buzzerTone(uint16_t freq, uint16_t ms) {
  tone(PIN_BUZZER, freq, ms);
}

static void playStartupJingle() {
  buzzerTone(1400, 60);
  delay(70);
  buzzerTone(1800, 70);
}

static void playAdjustTone(bool increasing) {
  buzzerTone(increasing ? 2200 : 1700, 20);
}

static void playStartTone() {
  buzzerTone(1200, 40);
  delay(50);
  buzzerTone(1600, 45);
  delay(55);
  buzzerTone(2200, 70);
}

static void playCatchTone() {
  buzzerTone(2600, 40);
  delay(50);
  buzzerTone(3200, 90);
}

static void playNextRoundTone() {
  buzzerTone(1850, 40);
}

static void playFullTone() {
  buzzerTone(1000, 90);
  delay(110);
  buzzerTone(760, 160);
}

static uint8_t pulse8(uint16_t periodMs) {
  if (periodMs < 2) return 255;
  const uint16_t half = periodMs / 2;
  const uint16_t t = millis() % periodMs;
  const uint16_t ramp = (t < half) ? t : (periodMs - t);
  return (uint8_t)((ramp * 255UL) / half);
}

static uint8_t limitLedLevel(uint8_t value) {
  if (LED_BRIGHTNESS_DIVIDER <= 1) return value;
  return (uint8_t)(value / LED_BRIGHTNESS_DIVIDER);
}

static uint32_t ledColor(uint8_t r, uint8_t g, uint8_t b) {
  return strip.Color(limitLedLevel(r), limitLedLevel(g), limitLedLevel(b));
}

static void setAllPixels(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < NEOPIXEL_COUNT; i++) {
    strip.setPixelColor(i, ledColor(r, g, b));
  }
  strip.show();
}

static void showSetupPixels() {
  const uint8_t p = pulse8(1200);
  const uint8_t blue = (uint8_t)(35 + (p / 3));
  const uint8_t green = (uint8_t)(8 + (p / 10));
  setAllPixels(0, green, blue);
}

static void showSpinPixels() {
  const uint8_t idx = (uint8_t)((millis() / 90) % NEOPIXEL_COUNT);
  for (uint8_t i = 0; i < NEOPIXEL_COUNT; i++) {
    if (i == idx) {
      strip.setPixelColor(i, ledColor(30, 170, 255));
    } else if (i == (uint8_t)((idx + 1) % NEOPIXEL_COUNT)) {
      strip.setPixelColor(i, ledColor(10, 55, 95));
    } else {
      strip.setPixelColor(i, ledColor(0, 5, 12));
    }
  }
  strip.show();
}

static void showWinnerPixels() {
  const uint8_t p = pulse8(700);
  const uint8_t red = (uint8_t)(120 + (p / 2));
  const uint8_t green = (uint8_t)(60 + (p / 5));
  setAllPixels(red, green, 0);
}

static void showDonePixels() {
  const uint8_t p = pulse8(1000);
  const uint8_t red = (uint8_t)(18 + (p / 4));
  setAllPixels(red, 0, 0);
}

static void flashWinnerPixels() {
  for (uint8_t i = 0; i < 2; i++) {
    setAllPixels(255, 170, 0);
    delay(60);
    setAllPixels(0, 0, 0);
    delay(40);
  }
}

static uint16_t N = 100;
static uint16_t winnersTarget = 6;
static uint16_t winnersCount = 0;
static uint16_t winners[MAX_WINNERS_STORED];

enum State {
  SETUP_N,
  SETUP_WINNERS,
  SPIN,
  SHOW_WINNER,
  DONE_FULL
};

static State state = SETUP_N;

static uint32_t spinLastMs = 0;
static uint16_t spinValue = 1;
static uint16_t pendingWinner = 0;
static bool pendingWinnerReady = false;

static uint16_t maxSelectableWinners() {
  return (N < MAX_WINNERS_STORED) ? N : MAX_WINNERS_STORED;
}

static void clampWinnersTarget() {
  const uint16_t maxWinners = maxSelectableWinners();
  if (winnersTarget < 1) winnersTarget = 1;
  if (winnersTarget > maxWinners) winnersTarget = maxWinners;
}

static void drawSetupParticipants() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Step1 Participants N");

  display.setTextSize(1);
  display.setCursor(0, 12);
  display.print(N);

  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print("U/D +/-  L -10  R >>>");
  display.display();
}

static void drawSetupWinners() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Step2 Winners target");

  display.setTextSize(1);
  display.setCursor(0, 12);
  display.print(winnersTarget);

  display.setTextSize(1);
  display.setCursor(72, 12);
  display.print("max: ");
  //display.setCursor(72, 20);
  display.print(maxSelectableWinners());

  display.setCursor(0, 24);
  display.print("U/D +/-  L -10  R GO");
  display.display();
}

static void drawSpin() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Spin ");
  display.print(winnersCount + 1);
  display.print("/");
  display.print(winnersTarget);
  display.print("  R catch");

  display.setTextSize(2);
  display.setCursor(0, 10);
  display.print(spinValue);

  const uint8_t barW = (uint8_t)((millis() / 12) % (SCREEN_W - 2));
  display.drawRect(0, 25, SCREEN_W, 7, SSD1306_WHITE);
  display.fillRect(1, 26, barW, 5, SSD1306_WHITE);

  display.display();
}

static void drawWinnersList() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("W ");
  display.print(winnersCount);
  display.print("/");
  display.print(winnersTarget);
  if (winnersCount < winnersTarget) {
    display.print(" R:NEXT");
  } else {
    display.print(" DONE");
  }

  const uint8_t firstVisible = (winnersCount > VISIBLE_WINNER_ROWS)
                                 ? (winnersCount - VISIBLE_WINNER_ROWS)
                                 : 0;

  for (uint8_t row = 0; row < VISIBLE_WINNER_ROWS; row++) {
    const uint8_t i = firstVisible + row;
    display.setCursor(0, 8 + row * 8);
    if (i >= winnersCount) continue;
    display.print(i + 1);
    display.print(") #");
    display.print(winners[i]);
  }

  display.display();
}

static bool alreadyWinner(uint16_t id) {
  for (uint8_t i = 0; i < winnersCount; i++) {
    if (winners[i] == id) return true;
  }
  return false;
}

static uint16_t pickWinnerFair() {
  while (true) {
    const uint16_t id = (uint16_t)(uniform_u32(N) + 1);
    if (!alreadyWinner(id)) return id;
  }
}

static void preparePendingWinnerIfNeeded() {
  if (!pendingWinnerReady) {
    pendingWinner = pickWinnerFair();
    pendingWinnerReady = true;
  }
}

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);

  btnUp.begin(PIN_BTN_UP);
  btnLeft.begin(PIN_BTN_LEFT);
  btnRight.begin(PIN_BTN_RIGHT);
  btnDown.begin(PIN_BTN_DOWN);

  strip.begin();
  setAllPixels(0, 0, 0);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Fall back to visible error pattern if OLED is not detected.
    while (true) {
      setAllPixels(60, 0, 0);
      buzzerTone(420, 120);
      delay(220);
      setAllPixels(0, 0, 0);
      delay(180);
    }
  }

  rngState = (uint32_t)micros() ^ ((uint32_t)millis() << 16) ^ 0xA5A5A5A5u;
  clampWinnersTarget();

  drawSetupParticipants();
  showSetupPixels();
  playStartupJingle();
}

void loop() {
  btnUp.update();
  btnLeft.update();
  btnRight.update();
  btnDown.update();

  if (btnUp.pressed()) gatherEntropyFromPress(1);
  if (btnLeft.pressed()) gatherEntropyFromPress(2);
  if (btnRight.pressed()) gatherEntropyFromPress(3);
  if (btnDown.pressed()) gatherEntropyFromPress(4);

  switch (state) {
    case SETUP_N: {
      showSetupPixels();

      if (btnUp.pressed()) {
        if (N < N_MAX) N++;
        clampWinnersTarget();
        drawSetupParticipants();
        playAdjustTone(true);
      }

      if (btnDown.pressed()) {
        if (N > N_MIN) N--;
        clampWinnersTarget();
        drawSetupParticipants();
        playAdjustTone(false);
      }

      if (btnLeft.pressed()) {
        if (N > (N_MIN + 9)) {
          N -= 10;
        } else {
          N = N_MIN;
        }
        clampWinnersTarget();
        drawSetupParticipants();
        buzzerTone(1450, 25);
      }

      if (btnRight.pressed()) {
        clampWinnersTarget();
        state = SETUP_WINNERS;
        buzzerTone(1800, 30);
        drawSetupWinners();
      }
    } break;

    case SETUP_WINNERS: {
      showSetupPixels();
      const uint16_t maxWinners = maxSelectableWinners();

      if (btnUp.pressed()) {
        if (winnersTarget < maxWinners) winnersTarget++;
        drawSetupWinners();
        playAdjustTone(true);
      }

      if (btnDown.pressed()) {
        if (winnersTarget > 1) winnersTarget--;
        drawSetupWinners();
        playAdjustTone(false);
      }

      if (btnLeft.pressed()) {
        if (winnersTarget > 10) {
          winnersTarget -= 10;
        } else {
          winnersTarget = 1;
        }
        drawSetupWinners();
        buzzerTone(1450, 25);
      }

      if (btnRight.pressed()) {
        winnersCount = 0;
        pendingWinnerReady = false;
        preparePendingWinnerIfNeeded();
        state = SPIN;
        spinLastMs = 0;
        playStartTone();
        drawSpin();
      }
    } break;

    case SPIN: {
      showSpinPixels();
      mixEntropy(xorshift32() ^ micros());

      if (millis() - spinLastMs >= 40) {
        spinLastMs = millis();
        spinValue = (uint16_t)(uniform_u32(N) + 1);
        drawSpin();
      }

      if (btnRight.pressed()) {
        if (winnersCount < winnersTarget) {
          winners[winnersCount++] = pendingWinner;
          pendingWinnerReady = false;
          drawWinnersList();
          flashWinnerPixels();
          if (winnersCount >= winnersTarget) {
            state = DONE_FULL;
            playFullTone();
          } else {
            state = SHOW_WINNER;
            playCatchTone();
          }
        } else {
          state = DONE_FULL;
          drawWinnersList();
          playFullTone();
        }
      }
    } break;

    case SHOW_WINNER: {
      showWinnerPixels();
      if (btnRight.pressed()) {
        if (winnersCount >= winnersTarget) {
          state = DONE_FULL;
          drawWinnersList();
          playFullTone();
        } else {
          preparePendingWinnerIfNeeded();
          state = SPIN;
          playNextRoundTone();
          drawSpin();
        }
      }
    } break;

    case DONE_FULL: {
      showDonePixels();
    } break;
  }
}
