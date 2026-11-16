
#include <Stepper.h>

// ==========================================
// 1. CONFIGURATION
// ==========================================

// Hardware pins
const int SOUND_PIN = A0;
const int SWITCH_PIN = 2;

// Stepper motor
const int STEPS_PER_REVOLUTION = 2048;
const int MOTOR_SPEED_RPM = 10;

// Sound detection
const int SOUND_THRESHOLD = 20;
const unsigned long SOUND_SAMPLE_MS = 50;

// Reward timing
const unsigned long REWARD_PAUSE_MS = 2000;

// Reward schedule (seconds)
const int rewardSchedule[] = {
  3, 4, 5, 6, 7, 8, 9, 10,
  9, 8, 7, 6, 5, 4, 3
};

const int SCHEDULE_LENGTH =
  sizeof(rewardSchedule) / sizeof(rewardSchedule[0]);

// Button debounce
const unsigned long DEBOUNCE_DELAY_MS = 50;

// Logging
const unsigned long LOG_INTERVAL_MS = 1000;


// ==========================================
// 2. HARDWARE OBJECTS
// ==========================================

// Keep this pin order for the ULN2003 driver
Stepper motor(
  STEPS_PER_REVOLUTION,
  8, 10, 9, 11
);


// ==========================================
// 3. SYSTEM VARIABLES
// ==========================================

// System state
bool systemOn = false;

// Reward schedule
int scheduleIndex = 0;

// Quietness timer
bool quietTimerRunning = false;
unsigned long quietStart = 0;
unsigned long lastLog = 0;

// Reward pause
bool inRewardPause = false;
unsigned long rewardPauseStart = 0;

// Button state
bool lastButtonState = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;


// ==========================================
// 4. SETUP
// ==========================================

void setup() {

  Serial.begin(9600);

  pinMode(SWITCH_PIN, INPUT_PULLUP);

  motor.setSpeed(MOTOR_SPEED_RPM);

  Serial.println();
  Serial.println("DOG QUIETNESS TRAINING");
  Serial.println("SYSTEM OFF");
}


// ==========================================
// 5. MAIN LOOP
// ==========================================

void loop() {

  // Check button and toggle system if necessary
  checkButton();

  // Do nothing while system is off
  if (!systemOn) {
    return;
  }

  // Handle eating/dispensing pause
  if (handleRewardPause()) {
    return;
  }

  // Measure ambient sound
  int soundLevel = measureSound();

  // Check whether noise was detected
  if (soundLevel >= SOUND_THRESHOLD) {
    resetQuietTimer();
    return;
  }

  // Start quietness timer if necessary
  startQuietTimer();

  // Calculate elapsed quiet time
  unsigned long now = millis();
  unsigned long quietDuration = now - quietStart;

  // Print quietness progress
  logQuietProgress(now, quietDuration);

  // Check whether reward target has been reached
  checkRewardTarget(quietDuration);
}


// ==========================================
// 6. BUTTON HANDLING
// ==========================================

void checkButton() {

  bool reading = digitalRead(SWITCH_PIN);

  // Detect a change in button state
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // Wait for stable button state
  if (millis() - lastDebounceTime > DEBOUNCE_DELAY_MS) {

    if (reading != buttonState) {

      buttonState = reading;

      // Button pressed (INPUT_PULLUP means LOW)
      if (buttonState == LOW) {

        systemOn = !systemOn;

        if (systemOn) {
          startSystem();
        } else {
          stopSystem();
        }
      }
    }
  }

  lastButtonState = reading;
}


// ==========================================
// 7. START / STOP SYSTEM
// ==========================================

void startSystem() {

  // Reset timers and reward schedule
  scheduleIndex = 0;

  quietTimerRunning = false;
  quietStart = 0;
  lastLog = 0;

  inRewardPause = false;
  rewardPauseStart = 0;

  Serial.println();
  Serial.println("SYSTEM ON");

  // Dispense initial treat
  Serial.println("DISPENSING INITIAL TREAT");

  dispenseTreat();

  Serial.print("First reward target: ");
  Serial.print(rewardSchedule[scheduleIndex]);
  Serial.println(" seconds");
}


void stopSystem() {

  // Reset everything
  scheduleIndex = 0;

  quietTimerRunning = false;
  quietStart = 0;
  lastLog = 0;

  inRewardPause = false;
  rewardPauseStart = 0;

  Serial.println();
  Serial.println("SYSTEM OFF");
  Serial.println("Reward schedule reset.");
}


// ==========================================
// 8. STEPPER MOTOR / TREAT DISPENSING
// ==========================================

void dispenseTreat() {

  Serial.println("STEPPER MOTOR ACTIVATED");

  // One full revolution
  motor.step(STEPS_PER_REVOLUTION);

  Serial.println("TREAT DISPENSED");
}

// ==========================================
// 9. SOUND MEASUREMENT
// ==========================================

int measureSound() {

  int minimum = 1023;
  int maximum = 0;

  unsigned long sampleStart = millis();

  while (millis() - sampleStart < SOUND_SAMPLE_MS) {

    int reading = analogRead(SOUND_PIN);

    if (reading < minimum) {
      minimum = reading;
    }

    if (reading > maximum) {
      maximum = reading;
    }
  }

  int soundLevel = maximum - minimum;

  // Print actual sound measurements
  Serial.print("Sound Level: ");
  Serial.print(soundLevel);

  Serial.print(" | Threshold: ");
  Serial.print(SOUND_THRESHOLD);

  if (soundLevel >= SOUND_THRESHOLD) {
    Serial.println(" | SOUND DETECTED!");
  } else {
    Serial.println(" | QUIET");
  }

  return soundLevel;
}

// ==========================================
// 10. QUIETNESS TIMER
// ==========================================

void startQuietTimer() {

  if (!quietTimerRunning) {

    quietTimerRunning = true;

    quietStart = millis();
    lastLog = quietStart;

    Serial.print("QUIET START - Target: ");

    Serial.print(rewardSchedule[scheduleIndex]);

    Serial.println(" seconds");
  }
}


void resetQuietTimer() {

  if (quietTimerRunning) {
    Serial.println("SOUND DETECTED - TIMER RESET");
  }

  quietTimerRunning = false;

  quietStart = 0;
  lastLog = 0;
}


// ==========================================
// 11. QUIETNESS PROGRESS LOGGING
// ==========================================

void logQuietProgress(
  unsigned long now,
  unsigned long quietDuration
) {

  if (now - lastLog >= LOG_INTERVAL_MS) {

    lastLog += LOG_INTERVAL_MS;

    Serial.print(quietDuration / 1000);

    Serial.println(",QUIET");
  }
}


// ==========================================
// 12. REWARD TARGET CHECK
// ==========================================

void checkRewardTarget(unsigned long quietDuration) {

  unsigned long targetDuration =
    (unsigned long)rewardSchedule[scheduleIndex] * 1000UL;

  // Has the dog remained quiet long enough?
  if (quietDuration >= targetDuration) {

    Serial.print(quietDuration / 1000);

    Serial.println(",DISPENSE TREAT");

    // Dispense actual treat
    // dispenseTreat();

    // Advance reward schedule
    advanceRewardSchedule();

    // Start eating pause AFTER dispensing
    inRewardPause = true;
    rewardPauseStart = millis();

    // Reset quietness timer
    quietTimerRunning = false;

    quietStart = 0;
    lastLog = 0;

    Serial.print("PAUSING FOR ");

    Serial.print(REWARD_PAUSE_MS / 1000);

    Serial.println(" SECONDS");

    Serial.print("NEXT REWARD TARGET: ");

    Serial.print(rewardSchedule[scheduleIndex]);

    Serial.println(" seconds");
  }
}


// ==========================================
// 13. REWARD SCHEDULE
// ==========================================

void advanceRewardSchedule() {

  scheduleIndex++;

  if (scheduleIndex >= SCHEDULE_LENGTH) {

    scheduleIndex = 0;

    Serial.println("REWARD CYCLE RESTARTED");
  }
}


// ==========================================
// 14. POST-REWARD PAUSE
// ==========================================

bool handleRewardPause() {

  if (!inRewardPause) {
    return false;
  }

  // Check whether eating pause has finished
  if (millis() - rewardPauseStart >= REWARD_PAUSE_MS) {

    inRewardPause = false;

    quietTimerRunning = false;
    quietStart = 0;
    lastLog = 0;

    Serial.println("EATING PAUSE COMPLETE");
    Serial.println("STARTING NEW QUIET TIMER");

    return false;
  }

  // Still in eating pause
  return true;
}
