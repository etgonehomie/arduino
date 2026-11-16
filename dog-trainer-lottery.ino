
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
bool servoWaitingForNextCycle = false;

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

// Delay between individual dispensing cycles (SECONDS).
const unsigned long SERVO_CYCLE_DELAY_SEC = 3;

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

const unsigned long SERVO_CYCLE_DELAY_MS =
  SERVO_CYCLE_DELAY_SEC * 1000UL;

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

unsigned long lastStatusPrintTime = 0;

bool countdownStarted = false;
int lastLoggedSecond = -1;
int countdownEntriesOnLine = 0;
unsigned long nextLongCheckpoint = 1;

// Latest sound reading for compact status display.
int latestSoundLevel = 0;
int latestSoundMinimum = 0;
int latestSoundMaximum = 0;

bool haveSoundReading = false;


// =====================================================
// COMPACT STATUS / COUNTDOWN DISPLAY
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

  // ==========================================
  // SHORT TIMERS: LOG EVERY REMAINING SECOND
  // ==========================================

  if (shortTimer) {

    if (remainingSec != (unsigned long)lastLoggedSecond &&
        remainingSec > 0) {

      checkpoint = remainingSec;
      shouldLog = true;
    }
  }

  // ==========================================
  // LONG TIMERS: 1s, 10s, 20s, 30s...
  // ==========================================

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
// TRAINING STAGES
// =====================================================

// Intervals are quiet durations required for a reward.
// Vocalizations reset the quiet timer, not the session.

const RewardStage stages[] = {

  // Stage 1: Foundation [1–5 seconds]
  {1000UL, 5000UL},

  // Stage 2: Early confidence [5–8 seconds]
  {5000UL, 8000UL},

  // Stage 3: Confidence [8–12 seconds]
  {8000UL, 12000UL},

  // Stage 4: Building [12–20 seconds]
  {12000UL, 20000UL},

  // Stage 5: Intermediate [20–30 seconds]
  {20000UL, 30000UL},

  // Stage 6: Extended [30–60 seconds]
  {30000UL, 60000UL},

  // Stage 7 [1–2 minutes]
  {60000UL, 120000UL},

  // Stage 8 [2–4 minutes]
  {120000UL, 240000UL},

  // Stage 9: Lottery begins [4–8 minutes]
  {240000UL, 480000UL},

  // Stage 10 [8–15 minutes]
  {480000UL, 900000UL},

  // Stage 11 [15–30 minutes]
  {900000UL, 1800000UL},

  // Stage 12 [30–60 minutes]
  {1800000UL, 3600000UL},

  // Stage 13 [1–2 hours]
  {3600000UL, 7200000UL},

  // Stage 14: Long-term goal [2–4 hours]
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

  // ==========================================
  // WAITING BETWEEN DISPENSING CYCLES
  // ==========================================

  if (servoWaitingForNextCycle) {

    if (now - servoStateStart >= SERVO_CYCLE_DELAY_MS) {

      servoWaitingForNextCycle = false;

      pelletServo.write(SERVO_OPEN);

      servoIsOpen = true;
      servoStateStart = now;
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

      Serial.print(F("[DISPENSE] "));
      Serial.print(portionsRemaining);
      Serial.println(F(" cycles left"));

      if (portionsRemaining > 0) {

        // Begin configurable delay before next cycle.
        servoWaitingForNextCycle = true;
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
  Serial.println(F("==========================="));

  Serial.print(F("[TRIAL "));
  Serial.print(totalTrials);
  Serial.print(F(" START] target="));

  Serial.print(quietDurationMs / 1000.0, 1);
  Serial.println('s');
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
  Serial.print(F("[PASS] #"));
  Serial.print(totalTrials);

  Serial.print(F(" | "));
  Serial.print(quietDurationMs / 1000.0, 1);
  Serial.print(F("s"));

  Serial.print(F(" | reward="));
  Serial.print(reward);
  Serial.println('x');

  if (reward == JACKPOT_REWARD &&
      TRAINING_STAGE >= LOTTERY_START_STAGE) {
    Serial.println(F("*** JACKPOT! ***"));
  }
  else if (reward > NORMAL_REWARD) {
    Serial.println(F("*** BONUS! ***"));
  }

  dispensePellets(reward);
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

      printCountdown("EAT", remaining, TREAT_EATING_WAIT_MS);
    }
  }
}
// =====================================================
// FAIL CURRENT TRIAL
// =====================================================

void failCurrentTrial(const SoundReading& sound) {

  if (!trialActive) return;
  unsigned long now = millis();
  unsigned long previousQuietTime = now - quietStartTime;

  trialActive = false;
  failedTrials++;
  waitingForQuietAfterFailure = true;

  resetCountdownDisplay();

  Serial.println();
  Serial.print(F("[FAIL] #"));
  Serial.print(totalTrials);

  Serial.print(F(" | quiet="));
  Serial.print(previousQuietTime / 1000.0, 1);
  Serial.print('s');

  printSoundInfo(sound);

  Serial.println();

  Serial.println(F("[WAIT] Waiting for quiet"));
}

// =====================================================
// CHECK SOUND
// =====================================================

void checkSound() {

  if (!sessionActive) return;

  if (eatingPeriodActive || servoDispensing) return;

  if (!trialActive && !waitingForQuietAfterFailure) return;

  unsigned long now = millis();

  if (now - lastSoundCheck < SOUND_CHECK_INTERVAL_MS) {
    return;
  }

  lastSoundCheck = now;

  SoundReading sound = measureSound();

  // Save latest readings for the status line.
  latestSoundLevel = sound.soundLevel;
  latestSoundMinimum = sound.minimum;
  latestSoundMaximum = sound.maximum;

  haveSoundReading = true;

  // ==========================================
  // ACTIVE TRIAL
  // ==========================================

  if (trialActive) {

    if (sound.soundLevel >= WHIMPER_THRESHOLD) {

      failCurrentTrial(sound);
    }
  }

  // ==========================================
  // WAITING FOR QUIET AFTER FAILURE
  // ==========================================

  else if (waitingForQuietAfterFailure) {

    if (sound.soundLevel < WHIMPER_THRESHOLD) {

      waitingForQuietAfterFailure = false;
      Serial.println(F("[QUIET] Detected | Starting next trial"));
      startQuietTrial();
    }

    else {

      // Compact sound output while waiting.
      if (PRINT_SOUND_READINGS &&
          now - lastStatusPrintTime >= SOUND_DEBUG_INTERVAL_MS) {

        lastStatusPrintTime = now;
        Serial.print(F("[WAIT] S="));
        Serial.print(sound.soundLevel);

        Serial.print(' ');
        Serial.print(getSoundType(sound.soundLevel));
      }
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

    printCountdown("QUIET", remaining, quietDurationMs);
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
  lastStatusPrintTime = 0;
  haveSoundReading = false;
  sessionDurationMs = SESSION_DURATION_MS;

  totalTrials = 0;
  successfulTrials = 0;
  failedTrials = 0;

  eatingPeriodActive = false;
  eatingPeriodStartTime = 0;

  lastSoundCheck = 0;
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

  Serial.println(F("[STARTED] Short press=start | Hold=stop"));

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
  servoWaitingForNextCycle = false;

  // Stop any queued dispensing.
  portionsRemaining = 0;
  servoIsOpen = false;
  servoDispensing = false;

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
  int completedTrials = successfulTrials + failedTrials;

  float passRate = 0.0;

  if (completedTrials > 0) {
    passRate = successfulTrials * 100.0 / completedTrials;
  }

  Serial.print(F("Completed: "));
  Serial.println(completedTrials);

  Serial.print(F("Pass rate: "));
  Serial.print(passRate, 1);
  Serial.println('%');

  Serial.print(F("Duration: "));
  Serial.print((millis() - sessionStartTime) / 60000.0, 1);
  Serial.println(F(" min"));

  Serial.println(F("==============================="));
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
