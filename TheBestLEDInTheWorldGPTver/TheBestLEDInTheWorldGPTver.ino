// --- Pins ---
const uint8_t buttonPin = 2;  // push button (to GND, uses INPUT_PULLUP)
const uint8_t RledPin   = 3;  // R (PWM)
const uint8_t GledPin   = 5;  // G (PWM)
const uint8_t BledPin   = 6;  // B (PWM)

const uint8_t LEDS[3] = {BledPin, GledPin, RledPin}; // effect order like your code

// --- Mode/state ---
enum ModeType : uint8_t { MODE_BREATH = 0, MODE_CLUB = 1 };
volatile uint8_t Mode = MODE_BREATH;

// --- Button (edge + debounce) ---
bool lastBtn = HIGH;                   // using INPUT_PULLUP, HIGH = not pressed
unsigned long lastBtnChange = 0;
const unsigned long debounceMs = 35;

// --- Timing for effects ---
unsigned long lastStep = 0;

// Breath effect state
int breathLevel = 255;                 // 255 -> 0 -> 255
int breathDir   = -1;                  // -1 down, +1 up
uint8_t breathColorIdx = 0;            // 0:B,1:G,2:R
const unsigned long breathStepMs = 10; // pace

// Night club effect state
int clubLevel = 255;                   // ramp 255 -> 0 -> 225, then next color
int clubPhase = 0;                     // 0:down, 1:up
uint8_t clubColorIdx = 0;
const unsigned long clubStepMs = 4;    // faster pace

// Helper: turn all off (common-anode style in your original: HIGH = off)
void allOff() {
  digitalWrite(RledPin, HIGH);
  digitalWrite(GledPin, HIGH);
  digitalWrite(BledPin, HIGH);
}

// Helper: write PWM to one channel, others off
void writeOne(uint8_t pin, int val) {
  // Your sketch appears to use common-anode wiring:
  // HIGH = off; PWM smaller value = brighter.
  // Since your original "worked", keep the same mapping.
  for (uint8_t i = 0; i < 3; ++i) {
    if (LEDS[i] == pin) {
      analogWrite(pin, constrain(val, 0, 255));
    } else {
      digitalWrite(LEDS[i], HIGH);
    }
  }
}

// ===== wiring/config =====
const bool COMMON_ANODE = true;  // set true if HIGH=off (typical RGB module with common anode)

// Optional: perceptual brightness shaping (1.0 = linear)
const float GAMMA = 1.0f;        // try 2.2 later if you want smoother lows

// ===== helpers =====
uint8_t applyGamma(uint8_t x) {
  if (GAMMA == 1.0f) return x;
  float yf = powf(x / 255.0f, GAMMA) * 255.0f;
  if (yf < 0) yf = 0; if (yf > 255) yf = 255;
  return (uint8_t)(yf + 0.5f);
}

uint8_t intensityToPwm(uint8_t intensity /*0..255*/) {
  // intensity: 0=off, 255=full brightness (human thinking)
  uint8_t v = applyGamma(intensity);
  return COMMON_ANODE ? (uint8_t)(255 - v) : v;
}

void writeRGB(uint8_t rI, uint8_t gI, uint8_t bI) {
  analogWrite(RledPin, intensityToPwm(rI));
  analogWrite(GledPin, intensityToPwm(gI));
  analogWrite(BledPin, intensityToPwm(bI));
}

// ===== BREATH: red, yellow, cyan, green, blue, purple (change at valley) =====
void breathUpdate() {
  struct RGB { uint8_t r,g,b; };

  // Human intensities (0=off, 255=full). Mapping to PWM is handled by writeRGB().
  static const RGB COLORS[] = {
    {255,   0,   0}, // Red
    {255, 255,   0}, // Yellow
    {  0, 255, 255}, // Cyan
    {  0, 255,   0}, // Green
    {  0,   0, 255}, // Blue
    {255,   0, 255}  // Purple
  };
  static const uint8_t NUM_COLORS = sizeof(COLORS) / sizeof(COLORS[0]);

  static uint8_t idx = 0;                 // which color we're breathing
  static int breath = 255;                // 0..255 overall brightness factor
  static int dir = -1;                    // fade out, then fade in
  static unsigned long last = 0;
  const unsigned long stepMs = 2;        // breathing speed (higher = slower)

  // Optional: tiny pause at the dark valley so the color switch is noticeable
  static unsigned long valleyHoldUntil = 0;
  const unsigned long valleyHoldMs = 60;  // set to 0 to disable

  unsigned long now = millis();
  if (now - last < stepMs) return;
  last = now;

  if (valleyHoldUntil && now < valleyHoldUntil) return;
  valleyHoldUntil = 0;

  // Scale fixed color by one brightness factor -> proper color mixing
  const RGB &c = COLORS[idx];
  uint16_t rI = (uint16_t)c.r * breath / 255;
  uint16_t gI = (uint16_t)c.g * breath / 255;
  uint16_t bI = (uint16_t)c.b * breath / 255;
  writeRGB((uint8_t)rI, (uint8_t)gI, (uint8_t)bI);

  // advance breathing
  breath += dir;

  if (breath <= 0) {
    // Dark valley: switch to next color here
    breath = 0;
    dir = +1;
    idx = (idx + 1) % NUM_COLORS;
    if (valleyHoldMs) valleyHoldUntil = now + valleyHoldMs;
  } else if (breath >= 255) {
    // Bright peak: reverse without changing color
    breath = 255;
    dir = -1;
  }
}

// ===== NIGHT CLUB (Bounce): red → yellow → cyan → green → blue → purple =====
// Fades up to peak, down to a floor, repeats a few times, then advances color.
void clubUpdate() {
  struct RGB { uint8_t r,g,b; };
  static const RGB COLORS[] = {
    {255,   0,   0}, // Red
    {255, 255,   0}, // Yellow
    {  0, 255, 255}, // Cyan
    {  0, 255,   0}, // Green
    {  0,   0, 255}, // Blue
    {255,   0, 255}  // Purple
  };
  static const uint8_t NUM_COLORS = sizeof(COLORS)/sizeof(COLORS[0]);

  // --------- TUNING ----------
  const uint8_t flashesPerColor = 2;      // up/down cycles before next color
  const uint8_t highI = 255;              // peak intensity (0..255)
  const uint8_t lowI  = 40;               // floor intensity (not full black for bounce feel)
  const uint8_t step  = 18;               // fade step per tick (bigger = faster ramps)
  const unsigned long stepMs = 9;        // time per ramp step (smaller = faster)
  const unsigned long gapMs  = 120;       // pause (black) between colors
  // ---------------------------

  enum Phase : uint8_t { PH_RAMP, PH_GAP };
  static Phase phase = PH_RAMP;
  static uint8_t colorIdx = 0;
  static uint8_t flashCount = 0;
  static int level = lowI;                // current intensity 0..255
  static int dir = +1;                    // +1 up, -1 down
  static unsigned long lastStep = 0;
  static unsigned long gapUntil = 0;

  unsigned long now = millis();

  if (phase == PH_GAP) {
    if (now >= gapUntil) {
      // next color
      colorIdx = (colorIdx + 1) % NUM_COLORS;
      flashCount = 0;
      level = lowI;
      dir = +1;
      phase = PH_RAMP;
    } else {
      return;
    }
  }

  if (now - lastStep < stepMs) return;
  lastStep = now;

  // Write current color at "level"
  const RGB &c = COLORS[colorIdx];
  uint16_t rI = (uint16_t)c.r * level / 255;
  uint16_t gI = (uint16_t)c.g * level / 255;
  uint16_t bI = (uint16_t)c.b * level / 255;
  writeRGB((uint8_t)rI, (uint8_t)gI, (uint8_t)bI);

  // Advance level in the current ramp direction
  level += dir * step;

  // Hit peaks/floors?
  if (dir > 0 && level >= highI) {
    level = highI;
    dir = -1;                 // start falling
  } else if (dir < 0 && level <= lowI) {
    level = lowI;
    dir = +1;                 // start rising
    flashCount++;             // one full bounce completed
    if (flashCount >= flashesPerColor) {
      // Finish this color: brief blackout gap, then advance color
      writeRGB(0,0,0);
      phase = PH_GAP;
      gapUntil = now + gapMs;
    }
  }
}

void setup() {
  pinMode(RledPin, OUTPUT);
  pinMode(GledPin, OUTPUT);
  pinMode(BledPin, OUTPUT);

  pinMode(buttonPin, INPUT_PULLUP);    // important for stable input
  allOff();
  Serial.begin(9600);
}

void handleButton() {
  bool cur = digitalRead(buttonPin);
  unsigned long now = millis();

  if (cur != lastBtn && (now - lastBtnChange) > debounceMs) {
    lastBtnChange = now;
    lastBtn = cur;

    // falling edge: pressed (goes from HIGH to LOW)
    if (cur == LOW) {
      Mode = (Mode == MODE_BREATH) ? MODE_CLUB : MODE_BREATH;
      Serial.print("Mode -> ");
      Serial.println(Mode == MODE_BREATH ? "BREATH" : "CLUB");

      // reset effect state so switch is immediate and clean
      breathLevel = 255; breathDir = -1; breathColorIdx = 0;
      clubLevel = 255;  clubPhase = 0;   clubColorIdx = 0;
      lastStep = 0;
      allOff();
    }
  }
}

void loop() {
  handleButton();                    // always responsive

  if (Mode == MODE_BREATH) {
    breathUpdate();                  // does at most one tiny step
  } else {
    clubUpdate();
  }
}