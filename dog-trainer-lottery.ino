
#include <Servo.h>

// =====================================================
// CUSTOM STRUCTURES
// =====================================================

struct RewardStage {
  unsigned long minIntervalMs;
  unsigned long maxIntervalMs;
};

struct SoundReading {
  int minimum;
  int maximum;
  int soundLevel;
};

// =====================================================
// MANUAL FUNCTION PROTOTYPES
// =====================================================

// These prevent the Arduino IDE from generating
// incorrect prototypes before the structs are declared.

RewardStage getCurrentStage();

SoundReading measureSound();

void printSoundInfo(const SoundReading& sound);

void failCurrentTrial(const SoundReading& sound);


// =====================================================
// USER CONFIGURATION
// =====================================================

// Select training stage (1–14).
const int TRAINING_STAGE = 1;

// Session duration in minutes.
// 0 = run indefinitely.
const unsigned long SESSION_MINUTES = 5;

bool waitingForQuietAfterFailure = false;
bool eatingPeriodPending = false;
bool eatingPeriodActive = false;
unsigned long eatingPeriodStartTime = 0;

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

// Post-reward waiting times (SECONDS)
const unsigned long TREAT_DISPENSE_WAIT_SEC = 1;
const unsigned long TREAT_EATING_WAIT_SEC = 5;
const unsigned long POST_REWARD_SETTLE_SEC = 2;

// Automatically start the next trial?
const bool AUTO_START_NEXT_TRIAL = true;


// Sound measurement timing (SECONDS)
const float SOUND_SAMPLE_WINDOW_SEC = 0.05;
const float SOUND_CHECK_INTERVAL_SEC = 0.25;


// =====================================================
// TIME CONVERSIONS
// =====================================================

const unsigned long SESSION_DURATION_MS =
  SESSION_MINUTES * 60UL * 1000UL;

const unsigned long TREAT_DISPENSE_WAIT_MS =
  TREAT_DISPENSE_WAIT_SEC * 1000UL;

const unsigned long TREAT_EATING_WAIT_MS =
  TREAT_EATING_WAIT_SEC * 1000UL;

const unsigned long POST_REWARD_SETTLE_MS =
  POST_REWARD_SETTLE_SEC * 1000UL;

const unsigned long SOUND_SAMPLE_WINDOW_MS =
  (unsigned long)(SOUND_SAMPLE_WINDOW_SEC * 1000.0);

const unsigned long SOUND_CHECK_INTERVAL_MS =
  (unsigned long)(SOUND_CHECK_INTERVAL_SEC * 1000.0);

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
// DEBUG CONFIGURATION
// =====================================================

// Set false to disable sound monitoring output.
const bool PRINT_SOUND_READINGS = true;

// How often to print sound readings (SECONDS).
const float SOUND_DEBUG_INTERVAL_SEC = 0.5;

// Enable/disable countdown displays.
const bool SHOW_COUNTDOWNS = true;

// =====================================================
// DEBUG TIME CONVERSIONS
// =====================================================

const unsigned long SOUND_DEBUG_INTERVAL_MS =
  (unsigned long)(SOUND_DEBUG_INTERVAL_SEC * 1000.0);

unsigned long lastSoundDebugTime = 0;
int lastCountdownBucket = -1;

// =====================================================
// COUNTDOWN DISPLAY
// =====================================================

void resetCountdownDisplay() {
  lastCountdownBucket = -1;
}

void printCountdown(const char* label, unsigned long remainingMs) {

  if (!SHOW_COUNTDOWNS) return;

  // Round up so we don't prematurely show zero.
  unsigned long remainingSec =
    (remainingMs + 999UL) / 1000UL;

  int displayBucket;

  // Under 60 seconds: update every second.
  if (remainingSec < 60) {
    displayBucket = remainingSec;
  }

  // 1 minute or more: update every 10 seconds.
  else {
    displayBucket = ((remainingSec + 9) / 10) * 10;
  }

  // Only redraw when the displayed time changes.
  if (displayBucket == lastCountdownBucket) return;

  lastCountdownBucket = displayBucket;

  Serial.print('\r');
  Serial.print('[');
  Serial.print(label);
  Serial.print(F("] "));

  if (displayBucket < 60) {

    Serial.print(displayBucket);
    Serial.print(F(" seconds remaining"));
  }

  else {

    Serial.print(displayBucket / 60);
    Serial.print(F("m "));

    if (displayBucket % 60 < 10) {
      Serial.print('0');
    }

    Serial.print(displayBucket % 60);
    Serial.print(F("s remaining"));
  }

  // Clear leftover characters from longer previous messages.
  Serial.print(F("                    "));
}


// =====================================================
// TRAINING STAGES
// =====================================================

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

int totalTrials = 0;
int successfulTrials = 0;
int failedTrials = 0;


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

  while (millis() - sampleStart < SOUND_SAMPLE_WINDOW_MS) {

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

      Serial.print('\r');
      Serial.print(F("[DISPENSING] "));
      Serial.print(portionsRemaining);
      Serial.println(F(" cycles remaining"));

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
  waitingForQuietAfterFailure = false;
  generateNextQuietDuration();

  quietStartTime = millis();
  trialActive = true;

  totalTrials++;

  resetCountdownDisplay();

  Serial.println();
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
  eatingPeriodPending = true;

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

  // Dispense the reward.
  dispensePellets(reward);

  // Do not start another trial yet.
  // Wait for dispensing to finish first.
}


// =====================================================
// UPDATING EATING PAUSE
// =====================================================

void updateEatingPeriod() {

  if (!sessionActive) return;

  // Start eating period once dispensing is complete.
  if (eatingPeriodPending &&
      !eatingPeriodActive &&
      !servoDispensing) {

    eatingPeriodPending = false;
    eatingPeriodActive = true;

    eatingPeriodStartTime = millis();

    resetCountdownDisplay();

    Serial.println();
    Serial.println(F("[EATING] Treat dispensed."));
  }

  // Update eating countdown.
  if (eatingPeriodActive) {

    unsigned long elapsed =
      millis() - eatingPeriodStartTime;

    if (elapsed >= TREAT_EATING_WAIT_MS) {

      eatingPeriodActive = false;

      Serial.println();
      Serial.println(F("[EATING COMPLETE]"));

      // Begin another quiet trial.
      startQuietTrial();
    }

    else {

      unsigned long remaining =
        TREAT_EATING_WAIT_MS - elapsed;

      printCountdown("EATING", remaining);
    }
  }
}

// =====================================================
// FAIL CURRENT TRIAL
// =====================================================

void failCurrentTrial(const SoundReading& sound) {

  if (!trialActive) return;

  unsigned long now = millis();

  unsigned long previousQuietTime =
    now - quietStartTime;

  // End the current trial.
  trialActive = false;

  // Record the failure.
  failedTrials++;

  // Wait until Coco becomes quiet before starting again.
  waitingForQuietAfterFailure = true;

  resetCountdownDisplay();

  Serial.println();
  Serial.println(F("========== TRIAL FAILED =========="));

  Serial.print(F("Failed trials: "));
  Serial.println(failedTrials);

  Serial.println(F("Reason: Vocalization detected"));

  printSoundInfo(sound);

  Serial.print(F("Previous quiet time: "));
  Serial.print(previousQuietTime / 1000.0, 1);
  Serial.println(F(" seconds"));

  Serial.println(F("----------------------------------"));
  Serial.println(F("[WAITING] Waiting for quiet..."));
  Serial.println(F("=================================="));
  Serial.println();
}


// =====================================================
// CHECK SOUND
// =====================================================

void checkSound() {

  if (!sessionActive) return;

  // Do not monitor while Coco is eating or the
  // dispenser is operating.
  if (eatingPeriodActive || servoDispensing) return;

  // Only monitor during an active trial or while
  // waiting for quiet after a failed trial.
  if (!trialActive && !waitingForQuietAfterFailure) return;

  unsigned long now = millis();

  if (now - lastSoundCheck < SOUND_CHECK_INTERVAL_MS) {
    return;
  }

  lastSoundCheck = now;

  SoundReading sound = measureSound();

  // Optional sound monitoring.
  if (PRINT_SOUND_READINGS &&
      now - lastSoundDebugTime >= SOUND_DEBUG_INTERVAL_MS) {

    printSoundInfo(sound);

    lastSoundDebugTime = now;
  }

  // ==========================================
  // ACTIVE TRIAL
  // ==========================================

  if (trialActive) {

    // Any vocalization fails the current trial.
    if (sound.soundLevel >= WHIMPER_THRESHOLD) {

      failCurrentTrial(sound);
    }
  }

  // ==========================================
  // WAITING FOR QUIET AFTER FAILURE
  // ==========================================

  else if (waitingForQuietAfterFailure) {

    // Start a new trial once Coco becomes quiet.
    if (sound.soundLevel < WHIMPER_THRESHOLD) {

      waitingForQuietAfterFailure = false;

      Serial.println();
      Serial.println(F("[QUIET DETECTED]"));

      Serial.println(F("[NEXT TRIAL] Starting new trial."));
      Serial.println();

      startQuietTrial();
    }
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
  else {

    unsigned long remaining =
      quietDurationMs - quietElapsed;

    printCountdown("QUIET", remaining);
  }
}


// =====================================================
// START SESSION
// =====================================================

void startSession() {

  if (sessionActive) return;

  sessionActive = true;
  sessionStartTime = millis();
  waitingForQuietAfterFailure = false;
  eatingPeriodActive = false;
  eatingPeriodPending = false;
  eatingPeriodStartTime = 0;
  sessionDurationMs = SESSION_DURATION_MS;

  totalTrials = 0;
  successfulTrials = 0;
  failedTrials = 0;

  eatingPeriodActive = false;
  eatingPeriodStartTime = 0;

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
  waitingForQuietAfterFailure = false;
  eatingPeriodActive = false;
  eatingPeriodPending = false;

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

  updateServo();

  updateEatingPeriod();

  checkSound();

  updateQuietTimer();

  updateSession();
}
