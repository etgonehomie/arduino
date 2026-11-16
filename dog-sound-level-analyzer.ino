
// ==========================================
// SOUND SENSOR + START/STOP BUTTON
// ==========================================

const int SOUND_PIN = A2;
const int SWITCH_PIN = 2;

const unsigned long SAMPLE_WINDOW_MS = 50;
const unsigned long OUTPUT_INTERVAL_MS = 250;
const unsigned long DEBOUNCE_DELAY_MS = 50;

// Button variables
bool recording = false;
bool lastButtonReading = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;

void setup() {
  Serial.begin(9600);

  pinMode(SWITCH_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("SOUND SENSOR TESTER");
  Serial.println("Press button to START recording");
}

// ==========================================
// MAIN LOOP
// ==========================================


void loop() {

  checkButton();

  if (!recording) {
    return;
  }

  static unsigned long lastMeasurement = 0;

  if (millis() - lastMeasurement >= OUTPUT_INTERVAL_MS) {
    lastMeasurement = millis();
    measureSound();
  }
}

// ==========================================
// BUTTON HANDLING
// ==========================================

void checkButton() {

  bool reading = digitalRead(SWITCH_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > DEBOUNCE_DELAY_MS) {

    if (reading != buttonState) {

      buttonState = reading;

      // Button pressed
      if (buttonState == LOW) {

        recording = !recording;

        if (recording) {
          Serial.println();
          Serial.println("RECORDING STARTED");
          Serial.println("Min,Max,SoundLevel");
        } else {
          Serial.println("RECORDING STOPPED");
          Serial.println();
        }
      }
    }
  }

  lastButtonReading = reading;
}

// ==========================================
// SOUND MEASUREMENT
// ==========================================

void measureSound() {

  int minimum = 1023;
  int maximum = 0;

  unsigned long startTime = millis();

  while (millis() - startTime < SAMPLE_WINDOW_MS) {

    // Check button during sampling too
    checkButton();

    if (!recording) {
      return;
    }

    int reading = analogRead(SOUND_PIN);

    minimum = min(minimum, reading);
    maximum = max(maximum, reading);
  }

  int soundLevel = maximum - minimum;

  Serial.print(minimum);
  Serial.print(",");
  Serial.print(maximum);
  Serial.print(",");
  Serial.println(soundLevel);
}
