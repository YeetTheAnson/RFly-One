// Written with Gemini. Firmware time not counted.
// --- Pin Definitions ---
#define LED_PIN    PA1
#define MOSFET_PIN PC1

#define BTN_RIGHT  PD4
#define BTN_LEFT   PD2
#define BTN_OK     PD3
#define BTN_PWR    PD0

// --- Button Structure ---
struct Button {
  uint8_t pin;
  const char* name;
  bool state;
  bool lastReading;
  unsigned long lastDebounceTime;
};

// Initialize buttons (assuming default unpressed state is HIGH due to pull-ups)
Button buttons[4] = {
  {BTN_RIGHT, "RIGHT_BTN", HIGH, HIGH, 0},
  {BTN_LEFT,  "LEFT_BTN",  HIGH, HIGH, 0},
  {BTN_OK,    "OK_BTN",    HIGH, HIGH, 0},
  {BTN_PWR,   "PWR_BTN",   HIGH, HIGH, 0}
};

// --- State Variables ---
bool mosfetOn = false;        // MOSFET state track
bool ledState = LOW;          // LED state track
unsigned long lastBlink = 0;  // Blink timing

const unsigned long debounceDelay = 50; // 50ms debounce time

void setup() {
  // Initialize UART at 115200 baud (Defaults to PD5 TX, PD6 RX)
  Serial.begin(115200);

  // Initialize LED
  pinMode(LED_PIN, OUTPUT);

  // Initialize P-Channel MOSFET
  // PC1 goes LOW to turn on, so we initialize it to HIGH (OFF)
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, HIGH);

  // Initialize Buttons with internal pull-ups
  for (int i = 0; i < 4; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].state = digitalRead(buttons[i].pin);
    buttons[i].lastReading = buttons[i].state;
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Non-blocking LED Blink (Toggle every 500ms)
  if (currentMillis - lastBlink >= 500) {
    lastBlink = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }

  // 2. UART Message Relay (Echo RX to TX)
  while (Serial.available() > 0) {
    Serial.write(Serial.read());
  }

  // 3. Button Reading & Debouncing
  for (int i = 0; i < 4; i++) {
    bool reading = digitalRead(buttons[i].pin);

    // If the switch changed (due to noise or pressing)
    if (reading != buttons[i].lastReading) {
      buttons[i].lastDebounceTime = currentMillis;
    }

    // If the reading has been stable longer than the debounce delay
    if ((currentMillis - buttons[i].lastDebounceTime) > debounceDelay) {
      // If the actual button state has changed
      if (reading != buttons[i].state) {
        buttons[i].state = reading;

        // Send state change over UART
        Serial.print(buttons[i].name);
        Serial.print(" ");
        Serial.println(buttons[i].state); // 0 when pressed, 1 when released

        // 4. Special handling for PWR Button toggle logic
        // Check if this is the PWR button and it was just pressed (went LOW)
        if (buttons[i].pin == BTN_PWR && buttons[i].state == LOW) {
          mosfetOn = !mosfetOn;
          
          if (mosfetOn) {
            digitalWrite(MOSFET_PIN, LOW);  // Turn P-MOSFET ON
          } else {
            digitalWrite(MOSFET_PIN, HIGH); // Turn P-MOSFET OFF
          }
        }
      }
    }
    // Save the reading for the next loop comparison
    buttons[i].lastReading = reading;
  }
}