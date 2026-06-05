/*
 * DIY Audio Silence Detector for XIAO ESP32C3
 * Features: 2-Stage Guitar-Tuner Calibration, LED Status, Email Alerts (SMTP2GO)
 */

#include <WiFi.h>
#include <ESP_Mail_Client.h>

// =================================================================
// 1. CONFIGURATION
// =================================================================

// Wi-Fi Settings
const char* WIFI_SSID     = "Private";
const char* WIFI_PASSWORD = "Pass";

// SMTP Settings (SMTP2GO)
#define SMTP_HOST         "mail.smtp2go.com"
#define SMTP_PORT         2525
#define AUTHOR_EMAIL      "email@email.com"
#define AUTHOR_PASSWORD   "emailpass"

// Recipient Email (Who gets the alerts)
#define RECIPIENT_EMAIL   "email@email.com" 

// Timing Settings (in milliseconds)
const unsigned long SILENCE_TIME_LIMIT = 30000; // 30 seconds of silence triggers ALARM
const unsigned long RECOVERY_TIME_LIMIT = 10000; // 10 seconds of audio clears ALARM

// =================================================================
// 2. HARDWARE & VARIABLES
// =================================================================

// Pin Definitions
const int micPin    = D0;  // Audio input
const int buttonPin = D7;  // Calibration Button (Wire between D7 and GND)
const int redLed    = D8;  // Red LED
const int greenLed  = D9;  // Green LED

// Audio Variables
const int sampleWindow = 50; 
unsigned int currentThreshold = 50; // Dynamic startup threshold

// State Machine
enum SystemState {
  MONITORING,     // Audio is fine
  SILENCE_GRACE,  // Silence detected, counting to 30s
  ALARM_ACTIVE,   // Email sent, waiting for audio to return
  RECOVERING      // Audio returned, counting to 10s to clear alarm
};
SystemState currentState = MONITORING;

// Timers
unsigned long silenceStartTime = 0;
unsigned long audioReturnTime = 0;
unsigned long lastBlinkTime = 0;
bool ledToggle = false;

// SMTP Session object
SMTPSession smtp;

// Forward Declaration
void runTwoPartCalibration();

// =================================================================
// 3. SETUP
// =================================================================

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // 0 to 4095 range

  pinMode(buttonPin, INPUT_PULLUP); 
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);

  // Initial LED State (Connecting)
  digitalWrite(redLed, HIGH);
  digitalWrite(greenLed, HIGH);

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  
  smtp.debug(0); 

  // Boot sequence finished
  digitalWrite(redLed, LOW);
  digitalWrite(greenLed, LOW);
  Serial.println("System Ready. Hold button for 3 seconds to calibrate input level.");
}

// =================================================================
// 4. MAIN LOOP
// =================================================================

void loop() {
  // 1. Check for 3+ second button hold to initiate Calibration
  if (digitalRead(buttonPin) == LOW) {
    unsigned long pressStart = millis();
    bool triggered = false;
    
    while (digitalRead(buttonPin) == LOW) {
      if (millis() - pressStart >= 3000) {
        triggered = true;
        runTwoPartCalibration();
        break;
      }
      delay(10);
    }
  }

  // 2. Sample the Audio
  unsigned long startMillis = millis(); 
  unsigned int signalMax = 0;
  unsigned int signalMin = 4095;

  while (millis() - startMillis < sampleWindow) {
    unsigned int sample = analogRead(micPin);
    if (sample < 4095) {
      if (sample > signalMax) signalMax = sample;
      if (sample < signalMin) signalMin = sample;
    }
  }
  unsigned int peakToPeak = signalMax - signalMin;

  // 3. Evaluate Audio and Update State Machine
  bool isSilent = (peakToPeak < currentThreshold);

  if (isSilent) {
    if (currentState == MONITORING) {
      currentState = SILENCE_GRACE;
      silenceStartTime = millis();
      Serial.println("Warning: Silence detected. Timer started.");
    } 
    else if (currentState == SILENCE_GRACE) {
      if (millis() - silenceStartTime >= SILENCE_TIME_LIMIT) {
        Serial.println("ALARM: Silence time limit reached. Sending email!");
        sendEmail("URGENT: Audio Silence Detected", "Audio has dropped below the threshold for 30 seconds.");
        currentState = ALARM_ACTIVE;
      }
    } 
    else if (currentState == RECOVERING) {
      Serial.println("Recovery failed. Back to alarm state.");
      currentState = ALARM_ACTIVE;
    }
  } 
  else { 
    if (currentState == SILENCE_GRACE) {
      currentState = MONITORING;
      Serial.println("Audio returned. Warning cleared.");
    } 
    else if (currentState == ALARM_ACTIVE) {
      currentState = RECOVERING;
      audioReturnTime = millis();
      Serial.println("Audio detected! Starting 10s recovery timer...");
    } 
    else if (currentState == RECOVERING) {
      if (millis() - audioReturnTime >= RECOVERY_TIME_LIMIT) {
        Serial.println("RECOVERY: Audio sustained. Sending clear email!");
        sendEmail("CLEAR: Audio Restored", "Audio has been consistently active for 10 seconds. System normalized.");
        currentState = MONITORING;
      }
    }
  }

  // 4. Update LEDs based on current state
  updateLEDs();
}

// =================================================================
// 5. UPGRADED TWO-PART CALIBRATION ROUTINE
// =================================================================

void runTwoPartCalibration() {
  Serial.println("\n--- CALIBRATION MODE INITIATED ---");
  
  // Flash both lights twice to indicate start
  for (int i = 0; i < 2; i++) {
    digitalWrite(redLed, HIGH);  digitalWrite(greenLed, HIGH); delay(200);
    digitalWrite(redLed, LOW);   digitalWrite(greenLed, LOW);  delay(200);
  }
  
  // Wait for user to let go of the button before starting Stage 1
  while (digitalRead(buttonPin) == LOW) { delay(10); }
  delay(200); // Debounce delay

  // ---------------------------------------------------------------
  // STAGE 1: Input Level Calibration (Guitar Tuner Mode)
  // ---------------------------------------------------------------
  Serial.println("Stage 1: Play audio and adjust input volume source...");
  
  unsigned int maxPtpTracked = 0;
  unsigned long lastDecayTime = millis();
  unsigned long lastBlinkTime = 0;
  bool tunerLedState = false;

  // Define our target peak-to-peak sweet spot for a clean 12-bit audio input
  const unsigned int IDEAL_MIN = 2200;
  const unsigned int IDEAL_MAX = 3500;

  while (true) {
    // Check if user short-pressed the button to advance to Stage 2
    if (digitalRead(buttonPin) == LOW) {
      delay(50); // debounce
      if (digitalRead(buttonPin) == LOW) {
        while (digitalRead(buttonPin) == LOW) { delay(10); } // Wait for release
        break; // Exit Stage 1 loop
      }
    }

    // Sample Audio over standard window
    unsigned long startMillis = millis();
    unsigned int sMax = 0;
    unsigned int sMin = 4095;
    while (millis() - startMillis < sampleWindow) {
      unsigned int sample = analogRead(micPin);
      if (sample < 4095) {
        if (sample > sMax) sMax = sample;
        if (sample < sMin) sMin = sample;
      }
    }
    unsigned int ptp = sMax - sMin;

    // Envelope Filter: Catch peak volume immediately (Attack)
    if (ptp > maxPtpTracked) {
      maxPtpTracked = ptp;
    } 
    // Decay slowly every 80ms to smooth out gaps in talk radio or musical beats
    else if (millis() - lastDecayTime > 80) {
      lastDecayTime = millis();
      if (maxPtpTracked > 20) {
        maxPtpTracked -= (maxPtpTracked * 0.04); 
      }
    }

    // --- Guitar Tuner LED Logic ---
    unsigned long currentMillis = millis();

    if (maxPtpTracked < IDEAL_MIN) {
      // VOLUME TOO LOW -> Flash Red. 
      // Calculate error distance to scale blink speed (closer to ideal = slower blink)
      unsigned int error = IDEAL_MIN - maxPtpTracked;
      unsigned int blinkInterval = map(error, 0, IDEAL_MIN, 450, 60);
      blinkInterval = constrain(blinkInterval, 60, 450);

      digitalWrite(greenLed, LOW);
      if (currentMillis - lastBlinkTime >= blinkInterval) {
        lastBlinkTime = currentMillis;
        tunerLedState = !tunerLedState;
        digitalWrite(redLed, tunerLedState);
      }
    } 
    else if (maxPtpTracked > IDEAL_MAX) {
      // VOLUME TOO HIGH -> Flash Green.
      unsigned int error = maxPtpTracked - IDEAL_MAX;
      unsigned int blinkInterval = map(error, 0, 4095 - IDEAL_MAX, 450, 60);
      blinkInterval = constrain(blinkInterval, 60, 450);

      digitalWrite(redLed, LOW);
      if (currentMillis - lastBlinkTime >= blinkInterval) {
        lastBlinkTime = currentMillis;
        tunerLedState = !tunerLedState;
        digitalWrite(greenLed, tunerLedState);
      }
    } 
    else {
      // PERFECT RANGE -> Both Solid On
      digitalWrite(redLed, HIGH);
      digitalWrite(greenLed, HIGH);
    }
  }

  // ---------------------------------------------------------------
  // STAGE 2: Automated 30-Second Threshold Calculation
  // ---------------------------------------------------------------
  Serial.println("Stage 1 locked. Commencing Stage 2: Profile Threshold (30s)...");
  
  long peakSum = 0;
  int peakCount = 0;
  unsigned long stage2Start = millis();
  unsigned long stage2BlinkTimer = 0;
  bool stage2LedState = false;

  while (millis() - stage2Start < 30000) {
    // Visual feedback: Alternate flashing LEDs to indicate calculations are executing
    if (millis() - stage2BlinkTimer >= 200) {
      stage2BlinkTimer = millis();
      stage2LedState = !stage2LedState;
      digitalWrite(redLed, stage2LedState);
      digitalWrite(greenLed, !stage2LedState);
    }

    unsigned long startMillis = millis();
    unsigned int sMax = 0;
    unsigned int sMin = 4095;
    while (millis() - startMillis < sampleWindow) {
      unsigned int sample = analogRead(micPin);
      if (sample < 4095) {
        if (sample > sMax) sMax = sample;
        if (sample < sMin) sMin = sample;
      }
    }
    unsigned int ptp = sMax - sMin;

    // Ignore bottom baseline hardware noise floor spikes
    if (ptp > 15) {
      peakSum += ptp;
      peakCount++;
    }
    delay(10); 
  }

  // Final Calculations
  if (peakCount > 0) {
    int averagePeak = peakSum / peakCount;
    // Keep baseline threshold to 30% of the active tracked dynamic average
    currentThreshold = averagePeak * 0.30;
    if (currentThreshold < 25) currentThreshold = 25; 

    Serial.print("SUCCESS! Dynamic Threshold established at: ");
    Serial.println(currentThreshold);
  } else {
    currentThreshold = 40; // Safe failure fallback
    Serial.println("Warning: Low profile sampling data. Using standard fallback threshold (40).");
  }

  // Flash both lights twice to signal system is fully armed and tracking
  for (int i = 0; i < 2; i++) {
    digitalWrite(redLed, HIGH);  digitalWrite(greenLed, HIGH); delay(150);
    digitalWrite(redLed, LOW);   digitalWrite(greenLed, LOW);  delay(150);
  }

  currentState = MONITORING; // Clear any old flags and force check loops
  Serial.println("--- MONITORING ACTIVE ---\n");
}

// =================================================================
// 6. SYSTEM UTILITIES & ALERTS
// =================================================================

void updateLEDs() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastBlinkTime >= 250) {
    lastBlinkTime = currentMillis;
    ledToggle = !ledToggle;
  }

  switch (currentState) {
    case MONITORING:
      digitalWrite(greenLed, HIGH);
      digitalWrite(redLed, LOW);
      break;
      
    case SILENCE_GRACE:
      digitalWrite(greenLed, LOW);
      digitalWrite(redLed, HIGH);
      break;
      
    case ALARM_ACTIVE:
      digitalWrite(greenLed, LOW);
      digitalWrite(redLed, ledToggle ? HIGH : LOW);
      break;
      
    case RECOVERING:
      digitalWrite(greenLed, ledToggle ? HIGH : LOW);
      digitalWrite(redLed, LOW);
      break;
  }
}

void sendEmail(String subject, String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to send email: Wi-Fi disconnected.");
    return;
  }

  Session_Config config;
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = ""; 

  SMTP_Message email_msg;
  email_msg.sender.name = "ESP32 Silence Detector";
  email_msg.sender.email = AUTHOR_EMAIL;
  email_msg.subject = subject;
  email_msg.addRecipient("Broadcast Admin", RECIPIENT_EMAIL);
  email_msg.text.content = message.c_str();

  if (!smtp.connect(&config)) {
    Serial.println("SMTP Connection error");
    return;
  }

  if (!MailClient.sendMail(&smtp, &email_msg)) {
    Serial.println("Error sending Email: " + smtp.errorReason());
  } else {
    Serial.println("Email sent successfully!");
  }
  
  smtp.sendingResult.clear();
}