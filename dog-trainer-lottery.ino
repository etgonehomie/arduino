
#include <Servo.h>

// =====================================================
// USER CONFIGURATION
// =====================================================

// Select training stage (1–14).
const int TRAINING_STAGE = 1;

// Session duration in minutes.
// 0 = run indefinitely.
const unsigned long SESSION_MINUTES = 5;


// =====================================================
// LOTTERY CONFIGURATION
// =====================================================

// Lottery begins at this stage.
// Before this stage, successful trials always get 1 reward.
const int LOTTERY_START_STAGE = 9;

// Reward amounts are servo dispensing cycles.
const int NORMAL_REWARD = 1;
const int SMALL_BONUS_REWARD = 2;
const int BIG_BONUS_REWARD = 3;
const int JACKPOT_REWARD = 5;

// Probability thresholds out of 100.
// 0–69: 1 cycle
// 70–89: 2 cycles
// 90–97: 3 cycles
// 98–99: 5 cycles
const int SMALL_BONUS_CHANCE = 70;
const int BIG_BONUS_CHANCE = 90;
const int JACKPOT_CHANCE = 98;


// =====================================================
// PIN CONFIGURATION
// =====================================================

const int SERVO_PIN = 9;
const int BUTTON_PIN = 2;
const int SOUND_PIN = A0;


// =====================================================
// SOUND CONFIGURATION
// =====================================================

// Peak-to-peak sound thresholds.
const int BARKING_THRESHOLD = 10;
const int WHIMPER_THRESHOLD = 5;

// Sound sampling settings.
const unsigned long SOUND_SAMPLE_MS = 40;
const unsigned long SOUND_CHECK_INTERVAL_MS = 60;

// Prevent repeated timer resets and log spam
// while the same vocalization continues.
const unsigned long SOUND_RESET_COOLDOWN_MS = 1000;


// =====================================================
// SERVO CONFIGURATION
// =====================================================

Servo pelletServo;

const int SERVO_CLOSED = 0;
const int SERVO_OPEN = 90;

const unsigned long SERVO_OPEN_MS = 500;
const unsigned long SERVO_CLOSED_MS = 300;

bool servoDispensing = false;
bool servoIsOpen = false;

int portionsRemaining = 0;

unsigned long servoStateStart = 0;


// =====================================================
// BUTTON CONFIGURATION
// =====================================================

// Short press: start session.
// Long press (1.5 seconds): stop session.

const unsigned long BUTTON_DEBOUNCE_MS = 40;
const unsigned long BUTTON_LONG_PRESS_MS = 1500;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;
unsigned long buttonDownTime = 0;

bool longPressHandled = false;


// =====================================================
// TRAINING STAGES
// =====================================================

struct RewardStage {
  unsigned long minIntervalMs;
  unsigned long maxIntervalMs;
};

// Intervals are quiet durations required for a reward.
// Vocalizations reset the quiet timer, not the session.

const RewardStage stages[] = {

  // Stage 1: Foundation
  {1000UL, 5000UL},

  // Stage 2: Early confidence
  {5000UL, 8000UL},

  // Stage 3: Confidence
  {8000UL, 12000UL},

  // Stage 4: Building
  {12000UL, 20000UL},

  // Stage 5: Intermediate
  {20000UL, 30000UL},

  // Stage 6: Extended
  {30000UL, 60000UL},

  // Stage 7
  {60000UL, 120000UL},

  // Stage 8
  {120000UL, 240000UL},

  // Stage 9
  {240000UL, 480000UL},

  // Stage 10
  {480000UL, 900000UL},

  // Stage 11
  {900000UL, 1800000UL},

  // Stage 12
  {1800000UL, 3600000UL},

  // Stage 13
  {3600000UL, 7200000UL},

  // Stage 14: Long-term goal
  {7200000UL, 14400000UL}
};

const int TOTAL_STAGES =
  sizeof(stages) / sizeof(stages[0]);


// =====================================================
// SESSION AND TRIAL VARIABLES
// =====================================================

bool sessionActive = false;
bool trialActive = false;

unsigned long sessionStartTime = 0;
unsigned long sessionDurationMs = 0;

unsigned long quietStartTime = 0;
unsigned long quietDurationMs = 0;

unsigned long lastSoundCheck = 0;
unsigned long lastSoundResetTime = 0;

bool hasSoundReset = false;

int totalTrials = 0;
int successfulTrials = 0;
int failedTrials = 0;


// =====================================================
// SOUND READING STRUCTURE
// =====================================================

struct SoundReading {
  int minimum;
  int maximum;
  int soundLevel;
};


// =====================================================
// GET CURRENT TRAINING STAGE
// =====================================================

RewardStage getCurrentStage() {
  return stages[TRAINING_STAGE - 1];
}


// =====================================================
// GENERATE RANDOM QUIET DURATION
// =====================================================

void generateNextQuietDuration() {

  RewardStage stage = getCurrentStage();

  quietDurationMs = random(
    stage.minIntervalMs,
    stage.maxIntervalMs + 1UL
  );

  Serial.print(F("[NEXT TRIAL] Quiet target: "));
  Serial.print(quietDurationMs / 1000.0, 1);
  Serial.println(F(" seconds"));
}


// =====================================================
// MEASURE SOUND
// =====================================================

SoundReading measureSound() {

  SoundReading result;

  result.minimum = 1023;
  result.maximum = 0;

  unsigned long sampleStart = millis();

  while (millis() - sampleStart < SOUND_SAMPLE_MS) {

    int reading = analogRead(SOUND_PIN);

    if (reading < result.minimum) {
      result.minimum = reading;
    }

    if (reading > result.maximum) {
      result.maximum = reading;
    }
  }

  result.soundLevel =
    result.maximum - result.minimum;

  return result;
}


// =====================================================
// GET SOUND TYPE
// =====================================================

const char* getSoundType(int soundLevel) {

  if (soundLevel >= BARKING_THRESHOLD) {
    return "BARKING";
  }

  if (soundLevel >= WHIMPER_THRESHOLD) {
    return "WHIMPERING";
  }

  return "QUIET";
}


// =====================================================
// PRINT SOUND INFORMATION
// =====================================================

void printSoundInfo(const SoundReading& sound) {

  Serial.print(F("Type: "));
  Serial.print(getSoundType(sound.soundLevel));

  Serial.print(F(" | Min: "));
  Serial.print(sound.minimum);

  Serial.print(F(" | Max: "));
  Serial.print(sound.maximum);

  Serial.print(F(" | Level: "));
  Serial.println(sound.soundLevel);
}


// =====================================================
// LOTTERY REWARD
// =====================================================

int getLotteryReward() {

  // Early stages always receive the normal reward.
  if (TRAINING_STAGE < LOTTERY_START_STAGE) {
    return NORMAL_REWARD;
  }

  int roll = random(100);

  if (roll < SMALL_BONUS_CHANCE) {
    return NORMAL_REWARD;
  }

  if (roll < BIG_BONUS_CHANCE) {
    return SMALL_BONUS_REWARD;
  }

  if (roll < JACKPOT_CHANCE) {
    return BIG_BONUS_REWARD;
  }

  return JACKPOT_REWARD;
}


// =====================================================
// SERVO DISPENSER
// =====================================================

void dispensePellets(int portions) {

  if (portions <= 0) return;

  portionsRemaining += portions;

  if (servoDispensing) return;

  servoDispensing = true;
  servoIsOpen = true;

  pelletServo.write(SERVO_OPEN);

  servoStateStart = millis();
}


void updateServo() {

  if (!servoDispensing) return;

  unsigned long now = millis();

  // Servo is currently open.
  if (servoIsOpen) {

    if (now - servoStateStart >= SERVO_OPEN_MS) {

      pelletServo.write(SERVO_CLOSED);

      servoIsOpen = false;
      servoStateStart = now;
    }
  }

  // Servo is currently closed.
  else {

    if (now - servoStateStart >= SERVO_CLOSED_MS) {

      portionsRemaining--;

      if (portionsRemaining > 0) {

        pelletServo.write(SERVO_OPEN);

        servoIsOpen = true;
        servoStateStart = now;
      }

      else {

        servoDispensing = false;
      }
    }
  }
}


// =====================================================
// START A NEW QUIET TRIAL
// =====================================================

void startQuietTrial() {

  if (!sessionActive) return;

  generateNextQuietDuration();

  quietStartTime = millis();
  trialActive = true;

  totalTrials++;

  Serial.print(F("[TRIAL "));
  Serial.print(totalTrials);
  Serial.println(F(" STARTED]"));
}


// =====================================================
// SUCCESSFUL QUIET TRIAL
// =====================================================

void successfulTrial() {

  if (!trialActive) return;

  trialActive = false;
  successfulTrials++;

  int reward = getLotteryReward();

  Serial.println();
  Serial.println(F("========== REWARD =========="));

  Serial.print(F("Successful trial: "));
  Serial.println(successfulTrials);

  Serial.print(F("Quiet duration: "));
  Serial.print(quietDurationMs / 1000.0, 1);
  Serial.println(F(" seconds"));

  Serial.print(F("Reward: "));
  Serial.print(reward);
  Serial.println(F(" dispensing cycles"));

  if (reward == JACKPOT_REWARD &&
      TRAINING_STAGE >= LOTTERY_START_STAGE) {

    Serial.println(F("*** JACKPOT! ***"));
  }
  else if (reward > NORMAL_REWARD) {

    Serial.println(F("*** BONUS REWARD! ***"));
  }

  Serial.println(F("============================"));
  Serial.println();

  dispensePellets(reward);

  // Automatically start the next quiet trial.
  startQuietTrial();
}


// =====================================================
// RESET QUIET TIMER AFTER VOCALIZATION
// =====================================================

void resetQuietTimer(const SoundReading& sound) {

  unsigned long now = millis();

  // Avoid repeatedly resetting/logging during
  // one continuous period of noise.
  if (hasSoundReset &&
      now - lastSoundResetTime < SOUND_RESET_COOLDOWN_MS) {
    return;
  }

  unsigned long previousQuietTime =
    now - quietStartTime;

  quietStartTime = now;

  lastSoundResetTime = now;
  hasSoundReset = true;

  failedTrials++;

  Serial.println();
  Serial.println(F("[QUIET TIMER RESET]"));

  printSoundInfo(sound);

  Serial.print(F("Previous quiet time: "));
  Serial.print(previousQuietTime / 1000.0, 1);
  Serial.println(F(" seconds"));

  Serial.println(F("Session continues."));
  Serial.println(F("Waiting for quiet again."));
  Serial.println();
}


// =====================================================
// CHECK SOUND
// =====================================================

void checkSound() {

  if (!sessionActive || !trialActive) return;

  unsigned long now = millis();

  if (now - lastSoundCheck < SOUND_CHECK_INTERVAL_MS) {
    return;
  }

  lastSoundCheck = now;

  SoundReading sound = measureSound();

  if (sound.soundLevel >= WHIMPER_THRESHOLD) {

    resetQuietTimer(sound);
  }
}


// =====================================================
// UPDATE QUIET TIMER
// =====================================================

void updateQuietTimer() {

  if (!sessionActive || !trialActive) return;

  unsigned long quietElapsed =
    millis() - quietStartTime;

  if (quietElapsed >= quietDurationMs) {

    successfulTrial();
  }
}


// =====================================================
// START SESSION
// =====================================================

void startSession() {

  if (sessionActive) return;

  sessionActive = true;

  sessionStartTime = millis();

  sessionDurationMs =
    SESSION_MINUTES * 60000UL;

  totalTrials = 0;
  successfulTrials = 0;
  failedTrials = 0;

  hasSoundReset = false;
  lastSoundCheck = 0;

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F("       COCO REWARD SYSTEM"));
  Serial.println(F("================================"));

  Serial.print(F("Training stage: "));
  Serial.println(TRAINING_STAGE);

  Serial.print(F("Session duration: "));

  if (SESSION_MINUTES == 0) {
    Serial.println(F("Unlimited"));
  }
  else {
    Serial.print(SESSION_MINUTES);
    Serial.println(F(" minutes"));
  }

  Serial.println(F("Session started!"));
  Serial.println(F("Press and hold button to stop."));
  Serial.println(F("--------------------------------"));

  startQuietTrial();
}


// =====================================================
// STOP SESSION
// =====================================================

void stopSession() {

  if (!sessionActive) return;

  sessionActive = false;
  trialActive = false;

  // Stop any queued dispensing.
  portionsRemaining = 0;
  servoIsOpen = false;
  servoDispensing = false;

  pelletServo.write(SERVO_CLOSED);

  unsigned long elapsed =
    millis() - sessionStartTime;

    Serial.println();
  Serial.println(F("========== SESSION SUMMARY =========="));

  Serial.print(F("Total trials: "));
  Serial.print(totalTrials);

  Serial.print(F(" | Passed: "));
  Serial.print(successfulTrials);

  Serial.print(F(" | Failed: "));
  Serial.println(failedTrials);

  float passRate = 0.0;

  if (totalTrials > 0) {
    passRate = (successfulTrials * 100.0) / totalTrials;
  }

  Serial.print(F("Pass rate: "));
  Serial.print(passRate, 1);
  Serial.println(F("%"));

  Serial.print(F("Session time: "));
  Serial.print(elapsed / 60000.0, 1);
  Serial.println(F(" minutes"));

  Serial.println(F("====================================="));
  Serial.println();
}


// =====================================================
// UPDATE SESSION TIMER
// =====================================================

void updateSession() {

  if (!sessionActive) return;

  // Zero means unlimited session.
  if (SESSION_MINUTES == 0) return;

  if (millis() - sessionStartTime >= sessionDurationMs) {

    stopSession();
  }
}


// =====================================================
// BUTTON HANDLING
// =====================================================

void updateButton() {

  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {

    lastDebounceTime = millis();
    lastButtonReading = reading;
  }

  if (millis() - lastDebounceTime < BUTTON_DEBOUNCE_MS) {
    return;
  }

  // Detect button state change.
  if (reading != stableButtonState) {

    stableButtonState = reading;

    // Button pressed.
    if (stableButtonState == LOW) {

      buttonDownTime = millis();
      longPressHandled = false;
    }

    // Button released.
    else {

      if (!longPressHandled) {
        handleShortPress();
      }
    }
  }

  // Detect long press.
  if (stableButtonState == LOW &&
      !longPressHandled &&
      millis() - buttonDownTime >= BUTTON_LONG_PRESS_MS) {

    longPressHandled = true;

    handleLongPress();
  }
}


// =====================================================
// SHORT BUTTON PRESS
// =====================================================

void handleShortPress() {

  if (!sessionActive) {

    startSession();
  }
  else {

    Serial.println(F("[INFO] Session already running."));
  }
}


// =====================================================
// LONG BUTTON PRESS
// =====================================================

void handleLongPress() {

  if (sessionActive) {

    stopSession();
  }
  else {

    Serial.println(F("[INFO] No active session."));
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  randomSeed(analogRead(A1) + micros());

  pelletServo.attach(SERVO_PIN);
  pelletServo.write(SERVO_CLOSED);

  Serial.println();
  Serial.println(F("COCO REWARD SYSTEM"));
  Serial.println(F("------------------"));

  if (TRAINING_STAGE < 1 ||
      TRAINING_STAGE > TOTAL_STAGES) {

    Serial.println(F("ERROR: Invalid training stage!"));

    while (true) {
      // Halt execution.
    }
  }

  Serial.print(F("Selected stage: "));
  Serial.println(TRAINING_STAGE);

  Serial.println(F("Short press: Start session"));
  Serial.println(F("Long press: Stop session"));
  Serial.println(F("Ready."));
  Serial.println();
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  updateButton();

  checkSound();

  updateQuietTimer();

  updateSession();

  updateServo();
}
