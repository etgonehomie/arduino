#include <Servo.h>

Servo pelletServo;

// ==========================================
// 1. CONFIGURATION
// ==========================================

// Hardware pins
const int SOUND_PIN = A2;
const int SWITCH_PIN = 2;
const int SERVO_PIN = 9;


// Sound detection
const int BARKING_THRESHOLD = 10;
const int WHIMPER_THRESHOLD = 5;
const unsigned long SOUND_SAMPLE_MS = 50;

// Which sounds reset the quietness timer?
const char* SOUND_WHIMPER = "[!] WHIMPER";
const char* SOUND_BARKING = "[!!!] BARKING";
const bool BARKING_RESETS_TIMER = true;
const bool WHIMPER_RESETS_TIMER = true;

// Reward timing
const unsigned long REWARD_PAUSE_MS = 3000;

// Reward schedule (seconds)
const int rewardSchedule[] = {60, 61};

// const int rewardSchedule[] = {
//   3, 4, 5, 6, 7, 8, 9, 10,
//   9, 8, 7, 6, 5, 4, 3
// };

const int SCHEDULE_LENGTH =
  sizeof(rewardSchedule) / sizeof(rewardSchedule[0]);

// Button debounce
const unsigned long DEBOUNCE_DELAY_MS = 50;

// Logging
const unsigned long LOG_INTERVAL_MS = 1000;


// ==========================================
// 2. MOTOR OBJECT
// ==========================================
const int SERVO_CLOSED = 0;
const int SERVO_OPEN = 90;

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
  pelletServo.attach(SERVO_PIN);
  pelletServo.write(SERVO_CLOSED);
  Serial.println();
  Serial.println("***Dog Crate Training***");
  Serial.println("System OFF");
  Serial.println("Press button to start.");
}


// ==========================================
// 5. MAIN LOOP
// ==========================================

void loop() {

  // Check button
  checkButton();

  // Do nothing while system is off
  if (!systemOn) {
    return;
  }

  // Handle eating pause
  if (handleRewardPause()) {
    return;
  }

  // Measure sound
  int minimum;
  int maximum;

  int soundLevel = measureSound(minimum, maximum);

  // ----------------------------------------
  // BARKING DETECTED
  // ----------------------------------------
  if (soundLevel >= BARKING_THRESHOLD && BARKING_RESETS_TIMER){
    
    printSoundInfo(SOUND_BARKING, minimum, maximum, soundLevel);
    resetQuietTimer();
    return;
  }
  else if (soundLevel >= WHIMPER_THRESHOLD && WHIMPER_RESETS_TIMER) {
    printSoundInfo(SOUND_WHIMPER, minimum, maximum, soundLevel);
    resetQuietTimer();
    return;
  }

  // ----------------------------------------
  // QUIETNESS TIMER
  // ----------------------------------------

  startQuietTimer();
  unsigned long now = millis();
  unsigned long quietDuration = now - quietStart;
  logQuietProgress(now, quietDuration);
  checkRewardTarget(quietDuration);
}


// ==========================================
// 6. BUTTON HANDLING
// ==========================================

void checkButton() {

  bool reading = digitalRead(SWITCH_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > DEBOUNCE_DELAY_MS) {
    if (reading != buttonState) {
      buttonState = reading;
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

  scheduleIndex = 0;
  quietTimerRunning = false;
  quietStart = 0;
  lastLog = 0;
  inRewardPause = false;
  rewardPauseStart = 0;
  Serial.println();
  Serial.println("System ON");
  printReward(rewardSchedule[scheduleIndex]);
}


void stopSystem() {

  scheduleIndex = 0;
  quietTimerRunning = false;
  quietStart = 0;
  lastLog = 0;
  inRewardPause = false;
  rewardPauseStart = 0;
  Serial.println();
  Serial.println("System OFF");
  Serial.println("Reward schedule reset.");
}


// ==========================================
// 8. MOTOR / TREAT DISPENSING
// ==========================================

void dispenseTreat() {
  pelletServo.write(SERVO_OPEN);
  delay(500);
  Serial.println("Treat dispensed.");
  pelletServo.write(SERVO_CLOSED);
  delay(300);
}


// ==========================================
// 9. SOUND MEASUREMENT
// ==========================================

int measureSound(int &minimum, int &maximum) {

  minimum = 1023;
  maximum = 0;
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
  return maximum - minimum;
}

void printSoundInfo(const char* soundType, int minimum, int maximum, int soundLevel) {
  Serial.print(soundType);
  Serial.print(" | Min: ");
  Serial.print(minimum);
  Serial.print(" | Max: ");
  Serial.print(maximum);
  Serial.print(" | Sound Level: ");
  Serial.println(soundLevel);
}

// ==========================================
// 10. QUIETNESS TIMER
// ==========================================

void startQuietTimer() {

  if (!quietTimerRunning) {
    quietTimerRunning = true;
    quietStart = millis();
    lastLog = quietStart;
    Serial.println("Quiet timer started...");
  }
}

void resetQuietTimer() {

  if (quietTimerRunning) {
    Serial.println("**********TIMER RESET**********");
  }
  quietTimerRunning = false;
  quietStart = 0;
  lastLog = 0;

  // Reset reward schedule
  if (scheduleIndex != 0) {
    scheduleIndex = 0;
    Serial.println("***REWARD SCHEDULE RESET***");
    printReward(3);
  }
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
    int seconds = quietDuration / 1000;
    if (seconds < 10) {
      Serial.print("000");
    }
    else if (seconds < 100) {
      Serial.print("00");
    }
    else if (seconds < 1000) {
      Serial.print("0");
    }
    Serial.print(seconds);
    Serial.print("s, ");
    if (seconds % 10 == 0) {
      Serial.println(""); 
    }
  }
}

// ==========================================
// 12. REWARD TARGET CHECK
// ==========================================

void checkRewardTarget(unsigned long quietDuration) {

  unsigned long targetDuration = (unsigned long)rewardSchedule[scheduleIndex] * 1000UL;

  if (quietDuration >= targetDuration) {
    // Uncomment to activate automatic treat dispensing
    dispenseTreat();

    advanceRewardSchedule();

    inRewardPause = true;
    rewardPauseStart = millis();
    quietTimerRunning = false;
    quietStart = 0;
    lastLog = 0;
    printEatingPause(REWARD_PAUSE_MS / 1000);
    printReward(rewardSchedule[scheduleIndex]);
  }
}

// ==========================================
// 13. REWARD SCHEDULE
// ==========================================

void advanceRewardSchedule() {

  scheduleIndex++;
  if (scheduleIndex >= SCHEDULE_LENGTH) {
    scheduleIndex = 0;
    Serial.println("***REWARD CYCLE RESTARTED***");
  }
}

// ==========================================
// 14. POST-REWARD PAUSE
// ==========================================

bool handleRewardPause() {

  if (!inRewardPause) {
    return false;
  }

  if (millis() - rewardPauseStart >= REWARD_PAUSE_MS) {
    inRewardPause = false;
    quietTimerRunning = false;
    quietStart = 0;
    lastLog = 0;
    printEatingPause(REWARD_PAUSE_MS / 1000);
    return false;
  }

  return true;
}

void printReward(int seconds) {
  Serial.print("***NEXT REWARD TARGET: ");
  Serial.print(seconds);
  Serial.println(" seconds***");
}

void printEatingPause(int seconds) {
    Serial.print("Pausing for eating completed (");
    Serial.print(seconds);
    Serial.println("s)");
}