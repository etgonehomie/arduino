
#include <Servo.h>

// =====================================================
// CUSTOM DATA STRUCTURES
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
// USER CONFIGURATION
// Change these settings most often.
// =====================================================

// Delay before training begins (SECONDS).
// Gives you time to leave after pressing the button.
// 0 = start immediately.
const unsigned long STARTUP_DELAY_SEC = 10;

// -----------------------------------------------------
// TRAINING SESSION
// -----------------------------------------------------

// Select training stage (1–14).
const int TRAINING_STAGE = 1;

// Session duration in minutes.
// 0 = run indefinitely.
const unsigned long SESSION_MINUTES = 5;


// -----------------------------------------------------
// LOTTERY / REWARD CONFIGURATION
// -----------------------------------------------------

// Lottery begins at this stage.
// Before this stage, successful trials always get 1 reward.
const int LOTTERY_START_STAGE = 9;

// Reward amounts are servo dispensing cycles.
const int NORMAL_REWARD = 1;
const int SMALL_BONUS_REWARD = 2;
const int BIG_BONUS_REWARD = 3;
const int JACKPOT_REWARD = 5;

// Probability of each bonus reward.
// Remaining probability automatically goes to NORMAL_REWARD.

constexpr float SMALL_BONUS_CHANCE = 0.20;  // 20%
constexpr float BIG_BONUS_CHANCE   = 0.08;  //  8%
constexpr float JACKPOT_CHANCE     = 0.02;  //  2%

// Compile-time validation: bonus probabilities cannot exceed 100%.
static_assert(
  SMALL_BONUS_CHANCE + BIG_BONUS_CHANCE + JACKPOT_CHANCE <= 1.0f,
  "ERROR: Combined bonus probabilities exceed 100%!"
);

// -----------------------------------------------------
// ADAPTIVE TRAINING CONFIGURATION
// -----------------------------------------------------

// Number of consecutive passes at the highest stage
// required to advance to the next stage.
constexpr int PASSES_TO_ADVANCE = 5;

// Probability of selecting each trial difficulty.
constexpr int CURRENT_STAGE_CHANCE = 85;   // 85%
constexpr int PREVIOUS_STAGE_CHANCE = 10;  // 10%
constexpr int OLDER_STAGE_CHANCE = 5;      // 5%

// Ensure trial probabilities total exactly 100%.
static_assert(
  CURRENT_STAGE_CHANCE +
  PREVIOUS_STAGE_CHANCE +
  OLDER_STAGE_CHANCE == 100,
  "ERROR: Trial stage probabilities must total 100%!"
);

static_assert(
  PASSES_TO_ADVANCE >= 1,
  "ERROR: PASSES_TO_ADVANCE must be at least 1!"
);


// -----------------------------------------------------
// SOUND CONFIGURATION
// -----------------------------------------------------

// Peak-to-peak sound thresholds.
const int BARKING_THRESHOLD = 10;
const int WHIMPER_THRESHOLD = 5;

// Sound measurement timing (SECONDS).
const float SOUND_SAMPLE_WINDOW_SEC = 0.05;
const float SOUND_CHECK_INTERVAL_SEC = 0.25;

// -----------------------------------------------------
// SERVO AND EATING TIMING
// -----------------------------------------------------

// Pause after dispensing has completely finished.
const unsigned long TREAT_EATING_WAIT_SEC = 5;


// Delay between individual dispensing cycles (SECONDS).
const unsigned long SERVO_CYCLE_DELAY_SEC = 3;

// Physical servo movement timing (MILLISECONDS).
const unsigned long SERVO_OPEN_MS = 500;
const unsigned long SERVO_CLOSED_MS = 300;

// Servo positions.
const int SERVO_CLOSED = 0;
const int SERVO_OPEN = 90;

// Ensure eating time is at least as long as the
// delay between servo dispensing cycles.
static_assert(
  TREAT_EATING_WAIT_SEC >= SERVO_CYCLE_DELAY_SEC,
  "ERROR: Eating time must be >= servo cycle delay!"
);

// -----------------------------------------------------
// HARDWARE PIN CONFIGURATION
// -----------------------------------------------------

const int SERVO_PIN = 9;
const int BUTTON_PIN = 2;
const int SOUND_PIN = A0;


// -----------------------------------------------------
// BUTTON CONFIGURATION
// -----------------------------------------------------

// Short press: start session.
// Long press (1.5 seconds): stop session.

const unsigned long BUTTON_DEBOUNCE_MS = 40;
const unsigned long BUTTON_LONG_PRESS_MS = 1500;

// -----------------------------------------------------
// DEBUG CONFIGURATION
// -----------------------------------------------------

// Enable/disable periodic sound output while waiting
// for quiet after a failed trial.
//
// Failure records still include their sound reading.
const bool PRINT_SOUND_READINGS = true;

// How often to print sound readings while waiting (SECONDS).
const float SOUND_DEBUG_INTERVAL_SEC = 0.5;

// Enable/disable countdown displays.
const bool SHOW_COUNTDOWNS = true;


// =====================================================
// TRAINING STAGE DEFINITIONS
// =====================================================

// Intervals are quiet durations required for a reward.
// Time units (internally stored as milliseconds).
const unsigned long SECOND = 1000UL;
const unsigned long MINUTE = 60UL * SECOND;
const unsigned long HOUR   = 60UL * MINUTE;

const RewardStage stages[] = {

  // Stage 1: Foundation [1–5 seconds]
  {1UL * SECOND, 5UL * SECOND},

  // Stage 2: Early confidence [5–8 seconds]
  {5UL * SECOND, 8UL * SECOND},

  // Stage 3: Confidence [8–12 seconds]
  {8UL * SECOND, 12UL * SECOND},

  // Stage 4: Building [12–20 seconds]
  {12UL * SECOND, 20UL * SECOND},

  // Stage 5: Intermediate [20–30 seconds]
  {20UL * SECOND, 30UL * SECOND},

  // Stage 6: Extended [30–60 seconds]
  {30UL * SECOND, 60UL * SECOND},

  // Stage 7 [1–2 minutes]
  {1UL * MINUTE, 2UL * MINUTE},

  // Stage 8 [2–4 minutes]
  {2UL * MINUTE, 4UL * MINUTE},

  // Stage 9: Lottery begins [4–8 minutes]
  {4UL * MINUTE, 8UL * MINUTE},

  // Stage 10 [8–15 minutes]
  {8UL * MINUTE, 15UL * MINUTE},

  // Stage 11 [15–30 minutes]
  {15UL * MINUTE, 30UL * MINUTE},

  // Stage 12 [30–60 minutes]
  {30UL * MINUTE, 60UL * MINUTE},

  // Stage 13 [1–2 hours]
  {1UL * HOUR, 2UL * HOUR},

  // Stage 14: Long-term goal [2–4 hours]
  {2UL * HOUR, 4UL * HOUR}
};

const int TOTAL_STAGES =
  sizeof(stages) / sizeof(stages[0]);


// =====================================================
// TIME CONVERSIONS
// Configuration above uses seconds where practical.
// =====================================================

const unsigned long SESSION_DURATION_MS =
  SESSION_MINUTES * 60UL * 1000UL;

const unsigned long TREAT_EATING_WAIT_MS =
  TREAT_EATING_WAIT_SEC * 1000UL;

const unsigned long SERVO_CYCLE_DELAY_MS =
  SERVO_CYCLE_DELAY_SEC * 1000UL;

const unsigned long SOUND_SAMPLE_WINDOW_MS =
  (unsigned long)(SOUND_SAMPLE_WINDOW_SEC * 1000.0);

const unsigned long SOUND_CHECK_INTERVAL_MS =
  (unsigned long)(SOUND_CHECK_INTERVAL_SEC * 1000.0);

const unsigned long SOUND_DEBUG_INTERVAL_MS =
  (unsigned long)(SOUND_DEBUG_INTERVAL_SEC * 1000.0);

const unsigned long STARTUP_DELAY_MS =
  STARTUP_DELAY_SEC * 1000UL;

// =====================================================
// FUNCTION PROTOTYPES
// =====================================================

// Keep these declarations after the custom structures.
// This avoids Arduino IDE prototype-generation issues.

SoundReading measureSound();

void printSoundInfo(const SoundReading& sound);

void failCurrentTrial(const SoundReading& sound);


// =====================================================
// RUNTIME STATE
// =====================================================

// -----------------------------------------------------
// Session and trial state
// -----------------------------------------------------
bool startupDelayActive = false;
unsigned long startupDelayStartTime = 0;

bool sessionActive = false;
bool trialActive = false;

unsigned long sessionStartTime = 0;

unsigned long quietStartTime = 0;
unsigned long quietDurationMs = 0;

unsigned long lastSoundCheck = 0;

int totalTrials = 0;
int successfulTrials = 0;
int failedTrials = 0;

// -----------------------------------------------------
// ADAPTIVE TRAINING STATE
// -----------------------------------------------------

// Highest currently achieved training stage.
int currentTrainingStage = TRAINING_STAGE;

// Actual stage selected for the current trial.
int currentTrialStage = TRAINING_STAGE;

// Consecutive successful trials at the highest stage.
int consecutivePasses = 0;

// -----------------------------------------------------
// Post-failure and eating state
// -----------------------------------------------------

bool waitingForQuietAfterFailure = false;

bool eatingPeriodPending = false;
bool eatingPeriodActive = false;

unsigned long eatingPeriodStartTime = 0;


// -----------------------------------------------------
// Servo state
// -----------------------------------------------------

Servo pelletServo;

bool servoDispensing = false;
bool servoIsOpen = false;
bool servoWaitingForNextCycle = false;

int portionsRemaining = 0;
int portionsCompleted = 0;
int portionsTotal = 0;

unsigned long servoStateStart = 0;


// -----------------------------------------------------
// Button state
// -----------------------------------------------------

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;
unsigned long buttonDownTime = 0;

bool longPressHandled = false;


// -----------------------------------------------------
// Countdown display state
// -----------------------------------------------------

bool countdownStarted = false;

int lastLoggedSecond = -1;
int countdownEntriesOnLine = 0;

unsigned long nextLongCheckpoint = 1;


// -----------------------------------------------------
// Sound debug state
// -----------------------------------------------------

unsigned long lastStatusPrintTime = 0;


// =====================================================
// COUNTDOWN DISPLAY
// =====================================================

void resetCountdownDisplay() {

  countdownStarted = false;
  lastLoggedSecond = -1;
  countdownEntriesOnLine = 0;
  nextLongCheckpoint = 1;
}


void printCountdown(
  const char* label,
  unsigned long remainingMs,
  unsigned long totalMs
) {

  if (!SHOW_COUNTDOWNS) return;

  unsigned long remainingSec =
    (remainingMs + 999UL) / 1000UL;

  unsigned long elapsedSec =
    (totalMs - remainingMs) / 1000UL;

  bool shortTimer = totalMs <= 10000UL;

  // Start a new countdown line.
  if (!countdownStarted) {

    countdownStarted = true;

    Serial.println(F("---------------------------"));

    Serial.print('[');
    Serial.print(label);
    Serial.print(F("] "));

    if (remainingSec < 10) Serial.print('0');

    Serial.print(remainingSec);
    Serial.print(F("s | "));

    // Short timers log their remaining seconds.
    if (shortTimer) {

      Serial.print(remainingSec);
      Serial.print('s');

      lastLoggedSecond = remainingSec;
      countdownEntriesOnLine = 1;
    }

    return;
  }

  bool shouldLog = false;
  unsigned long checkpoint = 0;

  // -----------------------------------------
  // SHORT TIMERS: LOG EVERY REMAINING SECOND
  // -----------------------------------------

  if (shortTimer) {

    if (remainingSec != (unsigned long)lastLoggedSecond &&
        remainingSec > 0) {

      checkpoint = remainingSec;
      shouldLog = true;
    }
  }

  // -----------------------------------------
  // LONG TIMERS: 1s, 10s, 20s, 30s...
  // -----------------------------------------

  else {

    if (elapsedSec >= nextLongCheckpoint) {

      checkpoint = nextLongCheckpoint;
      shouldLog = true;

      if (nextLongCheckpoint == 1) {
        nextLongCheckpoint = 10;
      }
      else {
        nextLongCheckpoint += 10;
      }
    }
  }

  if (!shouldLog) return;

  // Start another line after 10 entries.
  if (countdownEntriesOnLine >= 10) {

    Serial.println();

    Serial.print('[');
    Serial.print(label);
    Serial.print(F("] "));

    if (remainingSec < 10) Serial.print('0');

    Serial.print(remainingSec);
    Serial.print(F("s | "));

    countdownEntriesOnLine = 0;
  }

  else {

    Serial.print(F(", "));
  }

  Serial.print(checkpoint);
  Serial.print('s');

  countdownEntriesOnLine++;

  lastLoggedSecond = checkpoint;
}


// =====================================================
// GENERATE NEXT QUIET DURATION
// =====================================================

void generateNextQuietDuration() {

  // Stage 1 has no easier stages.
  if (currentTrainingStage == 1) {

    currentTrialStage = 1;
  }

  else {

    int roll = random(100);

    // -----------------------------------------
    // CURRENT STAGE
    // -----------------------------------------

    if (roll < CURRENT_STAGE_CHANCE) {

      currentTrialStage = currentTrainingStage;
    }

    // -----------------------------------------
    // PREVIOUS STAGE
    // -----------------------------------------

    else if (
      currentTrainingStage == 2 ||
      roll < CURRENT_STAGE_CHANCE + PREVIOUS_STAGE_CHANCE
    ) {

      // At Stage 2, both easier categories
      // effectively select Stage 1.
      currentTrialStage = currentTrainingStage - 1;
    }

    // -----------------------------------------
    // ANY OLDER STAGE
    // -----------------------------------------

    else {

      // Randomly select any stage except:
      // - Current stage
      // - Immediately previous stage

      currentTrialStage = random(
        1,
        currentTrainingStage - 1
      );
    }
  }

  // Generate duration using the selected trial stage.
  RewardStage stage = stages[currentTrialStage - 1];

  quietDurationMs = random(
    stage.minIntervalMs,
    stage.maxIntervalMs + 1UL
  );
}


// =====================================================
// SOUND DETECTION
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


const char* getSoundType(int soundLevel) {

  if (soundLevel >= BARKING_THRESHOLD) {
    return "BARKING";
  }

  if (soundLevel >= WHIMPER_THRESHOLD) {
    return "WHIMPERING";
  }

  return "QUIET";
}


void printSoundInfo(const SoundReading& sound) {

  Serial.print(F(" | "));
  Serial.print(getSoundType(sound.soundLevel));

  Serial.print(F(" level="));
  Serial.print(sound.soundLevel);

  Serial.print(F(" ["));
  Serial.print(sound.minimum);
  Serial.print('-');
  Serial.print(sound.maximum);
  Serial.print(']');
}


// =====================================================
// LOTTERY REWARD SELECTION
// =====================================================

int getLotteryReward() {

  // Early stages always receive the normal reward.
  if (currentTrainingStage < LOTTERY_START_STAGE) {
    return NORMAL_REWARD;
  }

  // Generate random number between 0.0000 and 0.9999.
  float roll = random(10000) / 10000.0;

  // Check largest reward first.
  if (roll < JACKPOT_CHANCE) {
    return JACKPOT_REWARD;
  }

  if (roll < JACKPOT_CHANCE + BIG_BONUS_CHANCE) {
    return BIG_BONUS_REWARD;
  }

  if (roll < JACKPOT_CHANCE + BIG_BONUS_CHANCE + SMALL_BONUS_CHANCE) {
    return SMALL_BONUS_REWARD;
  }

  // Everything remaining receives the normal reward.
  return NORMAL_REWARD;
}

const char* getRewardName(int reward) {

  if (reward == JACKPOT_REWARD) {
    return "JACKPOT";
  }

  if (reward == BIG_BONUS_REWARD) {
    return "BIG BONUS";
  }

  if (reward == SMALL_BONUS_REWARD) {
    return "SMALL BONUS";
  }

  return "NORMAL";
}

// =====================================================
// SERVO DISPENSER
// =====================================================

void dispensePellets(int portions) {

  if (portions <= 0) return;

  // Start a new dispensing batch.
  if (!servoDispensing) {

    portionsCompleted = 0;
    portionsTotal = 0;
  }

  portionsRemaining += portions;
  portionsTotal += portions;

  if (servoDispensing) return;

  servoDispensing = true;
  servoIsOpen = true;

  pelletServo.write(SERVO_OPEN);

  servoStateStart = millis();
}


void updateServo() {

  if (!servoDispensing) return;

  unsigned long now = millis();

  // ==========================================
  // WAITING BETWEEN DISPENSING CYCLES
  // ==========================================

  if (servoWaitingForNextCycle) {

    unsigned long elapsed = now - servoStateStart;

    if (elapsed >= SERVO_CYCLE_DELAY_MS) {

      Serial.println();
      resetCountdownDisplay();

      servoWaitingForNextCycle = false;

      pelletServo.write(SERVO_OPEN);

      servoIsOpen = true;
      servoStateStart = now;
    }
    else {

      unsigned long remaining =
        SERVO_CYCLE_DELAY_MS - elapsed;

      printCountdown(
        "SERVO WAIT",
        remaining,
        SERVO_CYCLE_DELAY_MS
      );
    }

    return;
  }

  // ==========================================
  // SERVO IS CURRENTLY OPEN
  // ==========================================

  if (servoIsOpen) {

    if (now - servoStateStart >= SERVO_OPEN_MS) {

      pelletServo.write(SERVO_CLOSED);

      servoIsOpen = false;
      servoStateStart = now;
    }
  }

  // ==========================================
  // SERVO IS CURRENTLY CLOSED
  // ==========================================

  else {

    if (now - servoStateStart >= SERVO_CLOSED_MS) {

      portionsRemaining--;
      portionsCompleted++;

      Serial.print(F("[DISPENSE] Completed "));
      Serial.print(portionsCompleted);
      Serial.print('/');
      Serial.print(portionsTotal);
      Serial.println(F(" cycles"));

      if (portionsRemaining > 0) {

        servoWaitingForNextCycle = true;
        servoStateStart = now;

        resetCountdownDisplay();
      }

      else {
        servoDispensing = false;
      }
    }
  }
}

// =====================================================
// ADAPTIVE TRAINING PROGRESSION
// =====================================================

void updateAdaptiveProgression(bool passed) {

  // ==========================================
  // SUCCESSFUL TRIAL
  // ==========================================

  if (passed) {

    // Only count successes at the highest stage.
    if (currentTrialStage == currentTrainingStage) {

      consecutivePasses++;

      Serial.print(F("[PROGRESS] Highest-stage passes: "));
      Serial.print(consecutivePasses);
      Serial.print('/');
      Serial.println(PASSES_TO_ADVANCE);

      // Advance after enough consecutive successes.
      if (consecutivePasses >= PASSES_TO_ADVANCE) {

        consecutivePasses = 0;

        if (currentTrainingStage < TOTAL_STAGES) {

          int previousStage = currentTrainingStage;

          currentTrainingStage++;

          Serial.print(F("[ADVANCE] Stage "));
          Serial.print(previousStage);
          Serial.print(F(" -> Stage "));
          Serial.println(currentTrainingStage);
        }

        else {

          Serial.println(F("[ADVANCE] Maximum stage reached"));
        }
      }
    }

    else {

      // Easier-stage successes do not count toward
      // advancement and do not reset the streak.

      Serial.print(F("[PROGRESS] Easier-stage pass | "));
      Serial.print(consecutivePasses);
      Serial.print('/');
      Serial.println(PASSES_TO_ADVANCE);
    }
  }

  // ==========================================
  // FAILED TRIAL
  // ==========================================

  else {

    // Any failure, regardless of trial difficulty,
    // resets the streak and lowers the training level.

    consecutivePasses = 0;

    if (currentTrainingStage > 1) {

      int previousStage = currentTrainingStage;

      currentTrainingStage--;

      Serial.print(F("[REGRESS] Stage "));
      Serial.print(previousStage);
      Serial.print(F(" -> Stage "));
      Serial.println(currentTrainingStage);
    }

    else {
      Serial.println(F("[REGRESS] Already at Stage 1"));
    }
  }
}

// =====================================================
// TRIAL MANAGEMENT
// =====================================================

// -----------------------------------------------------
// Start a new quiet trial
// -----------------------------------------------------

void startQuietTrial() {

  if (!sessionActive) return;

  waitingForQuietAfterFailure = false;

  generateNextQuietDuration();

  quietStartTime = millis();

  trialActive = true;
  totalTrials++;

  resetCountdownDisplay();

  Serial.println();
  Serial.println(F("==========================="));

  Serial.print(F("[TRIAL "));
  Serial.print(totalTrials);
  Serial.print(F(" START] stage="));
  Serial.print(currentTrialStage);

  Serial.print(F(" | goal="));
  Serial.print(currentTrainingStage);

  Serial.print(F(" | target="));

  Serial.print(quietDurationMs / 1000.0, 1);
  Serial.println('s');
}


// -----------------------------------------------------
// Successful trial
// -----------------------------------------------------

void successfulTrial() {

  if (!trialActive) return;

  trialActive = false;

  successfulTrials++;

  eatingPeriodPending = true;

  int reward = getLotteryReward();

  Serial.println();
  Serial.print(F("[PASS] #"));
  Serial.print(totalTrials);

  Serial.print(F(" | trial_stage="));
  Serial.print(currentTrialStage);

  Serial.print(F(" | training_stage="));
  Serial.print(currentTrainingStage);

  Serial.print(F(" | "));
  Serial.print(quietDurationMs / 1000.0, 1);
  Serial.print(F("s"));

  Serial.print(F(" | reward="));
  Serial.print(reward);
  Serial.print(F("x ("));
  Serial.print(getRewardName(reward));
  Serial.println(')');

  // Update training difficulty for the next trial.
  updateAdaptiveProgression(true);

  dispensePellets(reward);
}


// -----------------------------------------------------
// Failed trial
// -----------------------------------------------------

void failCurrentTrial(const SoundReading& sound) {

  if (!trialActive) return;

  unsigned long now = millis();

  unsigned long previousQuietTime =
    now - quietStartTime;

  trialActive = false;

  failedTrials++;

  waitingForQuietAfterFailure = true;

  resetCountdownDisplay();

  Serial.println();
  Serial.print(F("[FAIL] #"));
  Serial.print(totalTrials);

  Serial.print(F(" | trial_stage="));
  Serial.print(currentTrialStage);

  Serial.print(F(" | training_stage="));
  Serial.print(currentTrainingStage);

  Serial.print(F(" | quiet="));
  Serial.print(previousQuietTime / 1000.0, 1);
  Serial.print('s');

  printSoundInfo(sound);

  Serial.println();

  // Failure immediately lowers the difficulty.
  updateAdaptiveProgression(false);

  Serial.println(F("[WAIT] Waiting for quiet"));
}


// =====================================================
// EATING PERIOD
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
  }

  if (eatingPeriodActive) {

    unsigned long elapsed =
      millis() - eatingPeriodStartTime;

    if (elapsed >= TREAT_EATING_WAIT_MS) {

      eatingPeriodActive = false;

      Serial.println();

      startQuietTrial();
    }

    else {

      unsigned long remaining =
        TREAT_EATING_WAIT_MS - elapsed;

      printCountdown(
        "EAT",
        remaining,
        TREAT_EATING_WAIT_MS
      );
    }
  }
}


// =====================================================
// SOUND CHECK / TRIAL FAILURE DETECTION
// =====================================================

void checkSound() {

  if (!sessionActive) return;

  // Never monitor sound while Coco is receiving or eating
  // a reward. This prevents dispensing noises from failing
  // a trial.
  if (eatingPeriodActive || servoDispensing) return;

  if (!trialActive && !waitingForQuietAfterFailure) return;

  unsigned long now = millis();

  if (now - lastSoundCheck < SOUND_CHECK_INTERVAL_MS) {
    return;
  }

  lastSoundCheck = now;

  SoundReading sound = measureSound();

  // -----------------------------------------
  // ACTIVE TRIAL
  // -----------------------------------------

  if (trialActive) {

    if (sound.soundLevel >= WHIMPER_THRESHOLD) {

      failCurrentTrial(sound);
    }
  }

  // -----------------------------------------
  // WAITING FOR QUIET AFTER FAILURE
  // -----------------------------------------

  else if (waitingForQuietAfterFailure) {

    if (sound.soundLevel < WHIMPER_THRESHOLD) {

      waitingForQuietAfterFailure = false;

      Serial.println(
        F("[QUIET] Detected | Starting next trial")
      );

      startQuietTrial();
    }

    else {

      // Periodic sound output while waiting for quiet.
      if (PRINT_SOUND_READINGS &&
          now - lastStatusPrintTime >= SOUND_DEBUG_INTERVAL_MS) {

        lastStatusPrintTime = now;

        Serial.print(F("[WAIT] S="));
        Serial.print(sound.soundLevel);

        Serial.print(' ');
        Serial.println(getSoundType(sound.soundLevel));
      }
    }
  }
}


// =====================================================
// QUIET TIMER
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

    printCountdown(
      "QUIET",
      remaining,
      quietDurationMs
    );
  }
}


// =====================================================
// SESSION MANAGEMENT
// =====================================================


// -----------------------------------------------------
// Initialize startup delay
// -----------------------------------------------------

void updateStartupDelay() {

  if (!sessionActive || !startupDelayActive) return;

  unsigned long elapsed =
    millis() - startupDelayStartTime;

  if (elapsed >= STARTUP_DELAY_MS) {

    startupDelayActive = false;

    // Actual training session begins now.
    sessionStartTime = millis();

    Serial.println();
    Serial.println(F("[STARTUP] Complete | Beginning training"));

    startQuietTrial();
  }

  else {

    unsigned long remaining =
      STARTUP_DELAY_MS - elapsed;

    printCountdown(
      "START",
      remaining,
      STARTUP_DELAY_MS
    );
  }
}

// -----------------------------------------------------
// Start session
// -----------------------------------------------------

void startSession() {

  if (sessionActive) return;

  sessionActive = true;

  startupDelayActive = STARTUP_DELAY_MS > 0;
  startupDelayStartTime = millis();

  // Temporary start time; reset when startup finishes.
  sessionStartTime = millis();

  waitingForQuietAfterFailure = false;

  eatingPeriodActive = false;
  eatingPeriodPending = false;

  lastStatusPrintTime = 0;
  lastSoundCheck = 0;

  currentTrainingStage = TRAINING_STAGE;
  currentTrialStage = TRAINING_STAGE;
  consecutivePasses = 0;

  totalTrials = 0;
  successfulTrials = 0;
  failedTrials = 0;

  Serial.println();
  Serial.println(F("=== COCO REWARD SYSTEM ==="));

  Serial.print(F("Stage "));
  Serial.print(TRAINING_STAGE);

  Serial.print(F(" | Session "));

  if (SESSION_MINUTES == 0) {
    Serial.println(F("Unlimited"));
  }
  else {
    Serial.print(SESSION_MINUTES);
    Serial.println(F(" min"));
  }

  Serial.println(
    F("[STARTED] Short press=start | Hold=stop")
  );

  if (startupDelayActive) {

    Serial.print(F("[STARTUP] "));
    Serial.print(STARTUP_DELAY_SEC);
    Serial.println(F("s delay | Leave now!"));

    resetCountdownDisplay();

    printCountdown(
      "START",
      STARTUP_DELAY_MS,
      STARTUP_DELAY_MS
    );
  }

  else {

    sessionStartTime = millis();
    startQuietTrial();
  }
}

// -----------------------------------------------------
// Stop session
// -----------------------------------------------------

void stopSession() {

  if (!sessionActive) return;

  unsigned long elapsedSessionMs =
  startupDelayActive ? 0 : millis() - sessionStartTime;
  startupDelayActive = false;
  sessionActive = false;

  trialActive = false;

  waitingForQuietAfterFailure = false;
  
  eatingPeriodActive = false;
  eatingPeriodPending = false;

  // Stop any queued dispensing.
  portionsRemaining = 0;

  servoIsOpen = false;
  servoDispensing = false;
  servoWaitingForNextCycle = false;

  pelletServo.write(SERVO_CLOSED);

  Serial.println();
  Serial.println();

  Serial.println(F("======= SESSION SUMMARY ======="));

  Serial.print(F("Pass: "));
  Serial.print(successfulTrials);

  Serial.print(F(" | Fail: "));
  Serial.println(failedTrials);

  // Total completed trials = passes + failures.
  // An unfinished trial is intentionally excluded.
  int completedTrials =
    successfulTrials + failedTrials;

  float passRate = 0.0;

  if (completedTrials > 0) {

    passRate =
      successfulTrials * 100.0 / completedTrials;
  }

  Serial.print(F("Completed: "));
  Serial.println(completedTrials);

  Serial.print(F("Pass rate: "));
  Serial.print(passRate, 1);
  Serial.println('%');

  Serial.print(F("Duration: "));
  Serial.print(elapsedSessionMs / 60000.0, 1);
  Serial.println(F(" min"));

  Serial.println(F("==============================="));
}


// -----------------------------------------------------
// Automatic session timeout
// -----------------------------------------------------

void updateSession() {

  if (!sessionActive || startupDelayActive) return;
  
  // Zero means unlimited session.
  if (SESSION_MINUTES == 0) return;

  if (millis() - sessionStartTime >= SESSION_DURATION_MS) {

    stopSession();
  }
}


// =====================================================
// BUTTON MANAGEMENT
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

  // -----------------------------------------
  // Detect button state change
  // -----------------------------------------

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

  // -----------------------------------------
  // Detect long press
  // -----------------------------------------

  if (stableButtonState == LOW &&
      !longPressHandled &&
      millis() - buttonDownTime >= BUTTON_LONG_PRESS_MS) {

    longPressHandled = true;

    handleLongPress();
  }
}


// -----------------------------------------------------
// Short button press
// -----------------------------------------------------

void handleShortPress() {

  if (!sessionActive) {

    startSession();
  }

  else {

    Serial.println(F("[INFO] Session already running."));
  }
}


// -----------------------------------------------------
// Long button press
// -----------------------------------------------------

void handleLongPress() {

  if (sessionActive) {

    stopSession();
  }

  else {

    Serial.println(F("[INFO] No active session."));
  }
}


// =====================================================
// ARDUINO SETUP
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

  updateStartupDelay();

  updateEatingPeriod();

  checkSound();

  updateQuietTimer();

  updateSession();
}
