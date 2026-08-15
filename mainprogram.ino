#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>   
#include <TFT_eSPI.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "time.h"
#include "Image1.h" 
TFT_eSPI tft = TFT_eSPI();

//  Hardware Pins \
#define ENCODER_CLK 14
#define ENCODER_DT  27
#define ENCODER_SW  25 
#define LED_PIN      3  // Secondary Indicator LED
#define LED_AI_PIN   2  // Primary AI Indicator LED
#define BUZZER_PIN  12  

//DUAL-LINE AUTOMATED HARDWARE TRIGGER MECHANISM 
#define AI_RST_PIN  17  // Control line wired to the EN/RST pin of the second ESP32
#define AI_BOOT_PIN 16  // Control line wired to the BOOT/GPIO0 node of the second ESP32

int lastClkState;
int lastSwState = HIGH;
unsigned long buttonPressTime = 0;
const unsigned long LONG_PRESS_TIME = 1000; 

// Wi-Fi credentials
const char* ssid     = "";
const char* password = "";

// Network IoT Target IPs
const String fanIP    = "110.0.0.51"; 
const String bulbIP   = "110.0.0.52";
const String acIP     = "110.0.0.53";
const String heaterIP = "110.0.0.54";

// OpenWeatherMap
String apiKey = "06b220bf6dd53d99bf8bcab225b8b0dd";   
String city   = "Bharatpur";
String countryCode = "NP";
String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" 
                    + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";

// NTP setup
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 20700; 
const int daylightOffset_sec = 0;

// Variables for timing and weather data
unsigned long lastWeatherUpdate = 0;
float currentTemp = 0.0;
float currentHum = 0.0;
bool weatherLoaded = false;

// --- Screen & Selection Tracking ---
int currentScreen = 0;      
int selectedOption = 0;     
int subSelectedOption = 0;  
int pomoOptionIndex = 0;    
int alarmSetupPhase = 0;    
int homeApplianceIndex = 0; 
bool screenChanged = true;  

// --- Network Appliance Local States ---
bool stateFan    = false;
bool stateBulb   = false;
bool stateAC     = false;
bool stateHeater = false;

// Focus System Functional Parameters
unsigned long focusTimerStart = 0;
unsigned long targetPomoDuration = 0;
unsigned long breakDuration = 10 * 60 * 1000; 
bool pomodoroRunning = false;
bool insideBreakPhase = false;

//Global Alarm Flags 
int targetAlarmHour = 6;
int targetAlarmMin = 30;
bool alarmArmed = false;     
bool alarmTriggered = false; 
unsigned long buzzerToggleMillis = 0;
bool buzzerState = false;

//Water Reminder Configuration Parameters
unsigned long lastWaterReminderTime = 0;
const unsigned long WATER_INTERVAL = 20 * 60 * 1000; // Trigger alert pop-up every 20 Minutes
const unsigned long POPUP_DISPLAY_DURATION = 5000;  // Auto-dismiss alert box after 5 seconds
unsigned long popupStartTime = 0;
bool isWaterPopupActive = false;
int unreadWaterAlertsCount = 0;

//BLE Bluetooth Notification Storage Configuration 
#define MAX_NOTIFICATIONS 5
String notificationStorage[MAX_NOTIFICATIONS] = {
  "System: BLE Active",
  "System: No notifications",
  "System: Clear buffer",
  "System: Device online",
  "System: Standing by"
};
int totalValidNotifications = 1;
int notificationScrollIndex = 0; 

// BLE Constants
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
BLECharacteristic *pCharacteristic;

// Forward declarations
void drawStaticBackground(int screen);
void pushConvertedImage(int x, int y, const uint16_t* imageArray);
void pushLargeImage(int x, int y, const uint16_t* imageArray); 
void initHomeScreenUI();
void handleFocusAction();
void handleAIAction();
void handleHomeAssistanceAction();
void handleNotificationMenuAction();
void runActivePomodoro();
void checkGlobalAlarmEngine();
void checkWaterReminderEngine();
void sendIoTNetworkCommand(String ipAddress, String appliance, bool turnOn);
void drawGaugeArc(int cx, int cy, int r, int thickness, float startAngle, float endAngle, uint16_t color);

//BLE Inbound Packet Data Callback Pipeline Class 
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = String(pCharacteristic->getValue().c_str());
      if (value.length() > 0) {
        Serial.print("BLE Notification Captured: ");
        Serial.println(value);
        
        for (int i = MAX_NOTIFICATIONS - 1; i > 0; i--) {
          notificationStorage[i] = notificationStorage[i - 1];
        }
        notificationStorage[0] = value; 
        if (totalValidNotifications < MAX_NOTIFICATIONS) totalValidNotifications++;
      }
    }
};

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setSwapBytes(true);
  
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP); 
  pinMode(LED_PIN, OUTPUT);           
  pinMode(LED_AI_PIN, OUTPUT);        
  pinMode(BUZZER_PIN, OUTPUT);        
  
  // Initialize Automated Multi-Line Inter-IC Bootstrap Architecture
  pinMode(AI_RST_PIN, OUTPUT);
  pinMode(AI_BOOT_PIN, OUTPUT);
  digitalWrite(AI_RST_PIN, HIGH);   // Keep target reset circuit high and un-tripped
  digitalWrite(AI_BOOT_PIN, HIGH);  // Keep boot state line in high-impedance logic high state
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_AI_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  lastClkState = digitalRead(ENCODER_CLK);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Initialize BLE Peripheral Mode
  BLEDevice::init("Xiaochi_IoT_Hub");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  lastWaterReminderTime = millis();
  initHomeScreenUI();
}

void loop() {
  unsigned long currentMillis = millis();

  checkGlobalAlarmEngine();
  checkWaterReminderEngine(); 

  int currentClkState = digitalRead(ENCODER_CLK);
  if (currentClkState != lastClkState) {
    if (currentClkState == LOW) {
      bool isClockwise = (digitalRead(ENCODER_DT) != currentClkState);

      if (!alarmTriggered && !isWaterPopupActive) {
        if (currentScreen == 1) { 
          selectedOption = isClockwise ? (selectedOption + 1) % 4 : (selectedOption - 1 + 4) % 4;
          screenChanged = true;
        } 
        else if (currentScreen == 2) { 
          subSelectedOption = isClockwise ? 1 : 0;
          screenChanged = true;
        }
        else if (currentScreen == 3) { 
          pomoOptionIndex = isClockwise ? (pomoOptionIndex + 1) % 3 : (pomoOptionIndex - 1 + 3) % 3;
          screenChanged = true;
        }
        else if (currentScreen == 5) { 
          if (alarmSetupPhase == 0) { 
            targetAlarmHour = isClockwise ? (targetAlarmHour + 1) % 24 : (targetAlarmHour - 1 + 24) % 24;
          } else { 
            targetAlarmMin = isClockwise ? (targetAlarmMin + 1) % 60 : (targetAlarmMin - 1 + 60) % 60;
          }
          screenChanged = true;
        }
        else if (currentScreen == 8) {
          homeApplianceIndex = isClockwise ? (homeApplianceIndex + 1) % 4 : (homeApplianceIndex - 1 + 4) % 4;
          screenChanged = true;
        }
        else if (currentScreen == 9) {
          if (isClockwise) {
            if (notificationScrollIndex < totalValidNotifications - 1) notificationScrollIndex++;
          } else {
            if (notificationScrollIndex > 0) notificationScrollIndex--;
          }
          screenChanged = true;
        }
      }
    }
    lastClkState = currentClkState;
  }

  int currentSwState = digitalRead(ENCODER_SW);
  if (lastSwState == HIGH && currentSwState == LOW) {
    buttonPressTime = currentMillis;
  } 
  else if (lastSwState == LOW && currentSwState == HIGH) { 
    long totalPressDuration = currentMillis - buttonPressTime;
    
    if (alarmTriggered) {
      alarmTriggered = false;
      alarmArmed = false; 
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      currentScreen = 0;  
      screenChanged = true;
    } 
    else if (isWaterPopupActive) {
      isWaterPopupActive = false;
      screenChanged = true;
    }
    else {
      if (totalPressDuration >= LONG_PRESS_TIME) {
        if (currentScreen == 1) currentScreen = 0;
        else if (currentScreen == 7) currentScreen = 1; 
        else if (currentScreen == 8) currentScreen = 1; 
        else if (currentScreen == 9) currentScreen = 1; 
        else if (currentScreen == 2) currentScreen = 1;
        else if (currentScreen == 3) currentScreen = 2;
        else if (currentScreen == 4) { pomodoroRunning = false; currentScreen = 3; }
        else if (currentScreen == 5) { alarmSetupPhase = 0; currentScreen = 2; }
        screenChanged = true;
      } 
      else {
        if (currentScreen == 0) {
          currentScreen = 1;
          selectedOption = 0; 
          screenChanged = true;
        } else if (currentScreen == 1) {
          if (selectedOption == 0)      handleFocusAction();
          else if (selectedOption == 1) handleHomeAssistanceAction(); 
          else if (selectedOption == 2) handleNotificationMenuAction(); 
          else if (selectedOption == 3) handleAIAction();             
        } else if (currentScreen == 2) {
          if (subSelectedOption == 0) {
            currentScreen = 3; 
          } else {
            currentScreen = 5; 
            alarmSetupPhase = 0; 
          }
          screenChanged = true;
        } else if (currentScreen == 3) {
          if (pomoOptionIndex == 0)      targetPomoDuration = 1 * 60 * 60 * 1000;      
          else if (pomoOptionIndex == 1) targetPomoDuration = 2 * 60 * 60 * 1000; 
          else if (pomoOptionIndex == 2) targetPomoDuration = 3 * 60 * 60 * 1000; 
          
          focusTimerStart = millis();
          pomodoroRunning = true;
          insideBreakPhase = false;
          currentScreen = 4; 
          screenChanged = true;
        } else if (currentScreen == 5) {
          if (alarmSetupPhase == 0) {
            alarmSetupPhase = 1; 
            screenChanged = true;
          } else {
            alarmArmed = true; 
            currentScreen = 2;
            screenChanged = true;
          }
        } else if (currentScreen == 8) {
          switch (homeApplianceIndex) {
            case 0: stateFan = !stateFan;       sendIoTNetworkCommand(fanIP, "fan", stateFan); break;
            case 1: stateBulb = !stateBulb;     sendIoTNetworkCommand(bulbIP, "bulb", stateBulb); break;
            case 2: stateAC = !stateAC;         sendIoTNetworkCommand(acIP, "ac", stateAC); break;
            case 3: stateHeater = !stateHeater; sendIoTNetworkCommand(heaterIP, "heater", stateHeater); break;
          }
          screenChanged = true; 
        }
      }
    }
  }
  lastSwState = currentSwState;

  if (screenChanged) {
    drawStaticBackground(currentScreen);
    screenChanged = false;
  }

  if (currentScreen == 0) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      tft.fillRect(25, 25, 190, 25, TFT_WHITE);
      tft.setTextColor(TFT_RED);
      tft.setTextSize(3);
      tft.setCursor(55, 30);
      tft.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    if (weatherLoaded) {
      tft.setTextColor(TFT_RED, TFT_WHITE);
      tft.setTextSize(2);
      tft.setCursor(65, 10);
      tft.printf("%.1f*C,%.0f%%", currentTemp, currentHum);
    }
  } 
  else if (currentScreen == 4 && !isWaterPopupActive) {
    runActivePomodoro();
  }
  else if (currentScreen == 6) {
    if (currentMillis - buzzerToggleMillis > 200) {
      buzzerToggleMillis = currentMillis;
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
      digitalWrite(LED_PIN, buzzerState ? HIGH : LOW);
      
      tft.setCursor(25, 120);
      tft.setTextColor(buzzerState ? TFT_YELLOW : TFT_RED, buzzerState ? TFT_RED : TFT_YELLOW);
      tft.print(" CLICK KNOB TO STOP ");
    }
  }

  if (currentMillis - lastWeatherUpdate > 60000 || lastWeatherUpdate == 0) {
    lastWeatherUpdate = currentMillis;
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverPath);
      int httpResponseCode = http.GET();
      if (httpResponseCode == 200) {
        String payload = http.getString();
        StaticJsonDocument<1024> doc;
        if (deserializeJson(doc, payload) == DeserializationError::Ok) {
          currentTemp = doc["main"]["temp"];
          currentHum  = doc["main"]["humidity"];
          weatherLoaded = true;
        }
      }
      http.end();
    }
  }
  delay(2);
}

void sendIoTNetworkCommand(String ipAddress, String appliance, bool turnOn) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://" + ipAddress + "/control?device=" + appliance + "&state=" + (turnOn ? "1" : "0");
    http.begin(url);
    http.setTimeout(1500); 
    int httpResponseCode = http.GET();
    http.end();
  }
}

void initHomeScreenUI() {
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(0, 0, 255, 255, mercy, 255); 
}

void handleFocusAction() {
  currentScreen = 2; subSelectedOption = 0; screenChanged = true;
}

// DUAL-LINE SYNCHRONIZED HARDWARE HANDSHAKE ENGINE -
void handleAIAction() {
  digitalWrite(LED_AI_PIN, HIGH); 
  Serial.println("[!] Initiating Multi-Line Bootloader Wake Handshake Sequence...");

  // 1 Force the hidden BOOT line (GPIO 16) LOW to simulate physical button closure
  digitalWrite(AI_BOOT_PIN, LOW);  
  delay(60); 

  // 2 Pulse the Reset EN line (GPIO 17) to ground to cycle the sleep controller state
  digitalWrite(AI_RST_PIN, LOW);   
  delay(180);                      
  digitalWrite(AI_RST_PIN, HIGH);  // Release reset line to trigger chip reboot vector
  
  // 3 Retain the BOOT pin configuration to satisfy wake parameter check conditions
  delay(250); 
  
  // 4 Release the software-simulated boot switch back to logic high idle status
  digitalWrite(AI_BOOT_PIN, HIGH); 
  
  Serial.println("[+] Handshake Complete. Secondary voice engine forced awake.");
  
  delay(500); 
  digitalWrite(LED_AI_PIN, LOW);
  currentScreen = 7; 
  screenChanged = true;
}

void handleHomeAssistanceAction() {
  currentScreen = 8; homeApplianceIndex = 0; screenChanged = true;
}

void handleNotificationMenuAction() {
  currentScreen = 9; notificationScrollIndex = 0; screenChanged = true;
}

void checkWaterReminderEngine() {
  unsigned long now = millis();
  
  if (now - lastWaterReminderTime >= WATER_INTERVAL) {
    lastWaterReminderTime = now;
    isWaterPopupActive = true;
    popupStartTime = now;
    unreadWaterAlertsCount++;
    
    for (int i = MAX_NOTIFICATIONS - 1; i > 0; i--) {
      notificationStorage[i] = notificationStorage[i - 1];
    }
    notificationStorage[0] = "Reminder: Drink Water!";
    if (totalValidNotifications < MAX_NOTIFICATIONS) totalValidNotifications++;

    digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW);
    
    tft.fillRect(20, 75, 215, 100, TFT_BLUE);
    tft.drawRect(20, 75, 215, 100, TFT_WHITE);
    tft.setTextColor(TFT_YELLOW); tft.setTextSize(2);
    tft.setCursor(45, 95); tft.print("HYDRATION ALERT");
    tft.setTextColor(TFT_WHITE); tft.setTextSize(1);
    tft.setCursor(35, 130); tft.print("Time to drink some water!");
    tft.setCursor(45, 150); tft.print("[Click Knob to Close]");
  }

  if (isWaterPopupActive && (now - popupStartTime >= POPUP_DISPLAY_DURATION)) {
    isWaterPopupActive = false;
    screenChanged = true; 
  }
}

void drawGaugeArc(int cx, int cy, int r, int thickness, float startAngle, float endAngle, uint16_t color) {
  for (int t = 0; t < thickness; t++) {
    int currentRadius = r - t;
    for (float i = startAngle; i <= endAngle; i += 1.5) {
      float rad = (i - 90.0) * 3.14159 / 180.0;
      int x = cx + cos(rad) * currentRadius;
      int y = cy + sin(rad) * currentRadius;
      tft.drawPixel(x, y, color);
    }
  }
}

void runActivePomodoro() {
  if (!pomodoroRunning) return;
  
  unsigned long elapsed = millis() - focusTimerStart;
  unsigned long currentMillis = millis();
  static unsigned long lastGaugeUpdate = 0;
  static int lastProgressDegrees = -1; 

  unsigned long totalTarget = insideBreakPhase ? breakDuration : targetPomoDuration;
  if (elapsed > totalTarget) elapsed = totalTarget; 

  float percentRemaining = 1.0 - ((float)elapsed / (float)totalTarget);
  int progressDegrees = (int)(percentRemaining * 360.0);
  if (progressDegrees < 0) progressDegrees = 0;

  int centerX = 128;
  int centerY = 125;  
  int outerRadius = 85; 
  int ringThickness = 8; 

  if (currentMillis - lastGaugeUpdate >= 250 || progressDegrees != lastProgressDegrees) {
    lastGaugeUpdate = currentMillis;
    lastProgressDegrees = progressDegrees;

    uint16_t arcColor = insideBreakPhase ? TFT_BLUE : TFT_GREEN;
    uint16_t trackColor = 0x18C3; 

    drawGaugeArc(centerX, centerY, outerRadius, ringThickness, 0, progressDegrees, arcColor);
    if (progressDegrees < 360) {
      drawGaugeArc(centerX, centerY, outerRadius, ringThickness, progressDegrees, 360, trackColor);
    }

    unsigned long remainingSecs = (totalTarget - elapsed) / 1000;
    int hrs  = remainingSecs / 3600;
    int mins = (remainingSecs % 3600) / 60;
    int secs = remainingSecs % 60;

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (!insideBreakPhase) {
      tft.setTextSize(3); 
      tft.setCursor(44, 112);
      tft.printf("%02d:%02d:%02d", hrs, mins, secs);
    } else {
      tft.setTextSize(4);
      tft.setCursor(58, 112);
      tft.printf("%02d:%02d", mins, secs);
    }
  }

  static unsigned long lastClockTick = 0;
  if (currentMillis - lastClockTick >= 1000) {
    lastClockTick = currentMillis;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(68, 150); 
      tft.printf("[%02d:%02d:%02d]", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
  }

  if (!insideBreakPhase) {
    if (elapsed >= targetPomoDuration) {
      insideBreakPhase = true;
      focusTimerStart = millis(); 
      screenChanged = true; 
      return;
    }
  } 
  else {
    if (elapsed >= breakDuration) {
      pomodoroRunning = false; 
      insideBreakPhase = false;
      tft.fillScreen(TFT_RED); tft.setTextColor(TFT_WHITE); tft.setTextSize(3);
      tft.setCursor(45, 110);  tft.print("SESSION DONE");
      digitalWrite(BUZZER_PIN, HIGH); delay(1000); digitalWrite(BUZZER_PIN, LOW);
      currentScreen = 0; screenChanged = true;
      return;
    }
  }
}

void checkGlobalAlarmEngine() {
  if (!alarmArmed || alarmTriggered) return;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_hour == targetAlarmHour && timeinfo.tm_min == targetAlarmMin) {
      alarmTriggered = true; currentScreen = 6; screenChanged = true;
    }
  }
}

void pushConvertedImage(int x, int y, const uint16_t* imageArray) {
  uint16_t lineBuffer[50]; 
  for (int row = 0; row < 50; row++) {
    for (int col = 0; col < 50; col++) {
      uint16_t rawPixel = imageArray[row * 50 + col];
      uint8_t r = (rawPixel & 0x001F);        
      uint8_t g = (rawPixel >> 5) & 0x003F;   
      uint8_t b = (rawPixel >> 11) & 0x001F;  
      lineBuffer[col] = (r << 11) | (g << 5) | b; 
    }
    tft.pushImage(x, y + row, 50, 1, lineBuffer);
  }
}

void pushLargeImage(int x, int y, const uint16_t* imageArray) {
  uint16_t scaleBuffer[100]; 
  for (int row = 0; row < 50; row++) {
    for (int col = 0; col < 50; col++) {
      uint16_t rawPixel = imageArray[row * 50 + col];
      uint8_t r = (rawPixel & 0x001F);        
      uint8_t g = (rawPixel >> 5) & 0x003F;   
      uint8_t b = (rawPixel >> 11) & 0x001F;  
      uint16_t correctedPixel = (r << 11) | (g << 5) | b;
      scaleBuffer[col * 2]     = correctedPixel;
      scaleBuffer[col * 2 + 1] = correctedPixel;
    }
    tft.pushImage(x, y + (row * 2),     100, 1, scaleBuffer);
    tft.pushImage(x, y + (row * 2 + 1), 100, 1, scaleBuffer);
  }
}

void drawStaticBackground(int screen) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);

  if (screen == 0) {
    initHomeScreenUI();
  } 
  else if (screen == 1) {
    int col1X = 30; int col2X = 145; int row1Y = 45; int row2Y = 125;  
    int boxW  = 80; int boxH  = 75; 

    tft.drawRect(col1X, row1Y, boxW, boxH, (selectedOption == 0) ? TFT_RED : TFT_WHITE); 
    tft.drawRect(col2X, row1Y, boxW, boxH, (selectedOption == 1) ? TFT_RED : TFT_WHITE); 
    tft.drawRect(col1X, row2Y, boxW, boxH, (selectedOption == 2) ? TFT_RED : TFT_WHITE); 
    tft.drawRect(col2X, row2Y, boxW, boxH, (selectedOption == 3) ? TFT_RED : TFT_WHITE); 

    pushConvertedImage(col1X + 15, row1Y + 6, focus);
    tft.setCursor(col1X + 10, row1Y + 65); tft.print("FOCUS");

    pushConvertedImage(col2X + 15, row1Y + 6, home_assistance);
    tft.setCursor(col2X + 20, row1Y + 58); tft.print("HOME");

    pushConvertedImage(col1X + 15, row2Y + 6, notification);
    tft.setCursor(col1X + 10, row2Y + 58); tft.print("NOTIF");

    pushConvertedImage(col2X + 15, row2Y + 6, xiaochi_ai);
    tft.setCursor(col2X + 26, row2Y + 58); tft.print("AI");
  }
  else if (screen == 2) {
    tft.setCursor(45, 20); tft.print("SELECT FOCUS MODE");
    tft.drawRect(20, 70, 215, 45, (subSelectedOption == 0) ? TFT_RED : TFT_WHITE);
    tft.setCursor(35, 85); tft.print("1. POMODORO TIMER");

    tft.drawRect(20, 140, 215, 45, (subSelectedOption == 1) ? TFT_RED : TFT_WHITE);
    tft.setCursor(35, 155); tft.print("2. ALARM SETUP");
  }
  else if (screen == 3) {
    tft.setCursor(45, 20); tft.print("SELECT WORK PERIOD");
    tft.drawRect(20, 60, 215, 40, (pomoOptionIndex == 0) ? TFT_RED : TFT_WHITE);
    tft.setCursor(35, 73); tft.print("1 HOUR (10M BREAK)");

    tft.drawRect(20, 110, 215, 40, (pomoOptionIndex == 1) ? TFT_RED : TFT_WHITE);
    tft.setCursor(35, 123); tft.print("2 HOUR (10M BREAK)");

    tft.drawRect(20, 160, 215, 40, (pomoOptionIndex == 2) ? TFT_RED : TFT_WHITE);
    tft.setCursor(35, 173); tft.print("3 HOUR (10M BREAK)");
  }
  else if (screen == 4) {
    tft.fillScreen(TFT_BLACK); tft.setTextSize(1);
    if (!insideBreakPhase) {
      tft.setTextColor(TFT_GREEN); tft.setCursor(92, 24); tft.print("CYCLE: FOCUS");
    } else {
      tft.setTextColor(TFT_BLUE); tft.setCursor(92, 24); tft.print("CYCLE: BREAK");
    }
    tft.setTextColor(0x52AA); tft.setCursor(48, 230); tft.print("HOLD ROTARY SWITCH TO RESET");
  }
  else if (screen == 5) {
    tft.setCursor(55, 30); tft.print("CONFIGURE ALARM");
    tft.setTextSize(3);
    if (alarmSetupPhase == 0) {
      tft.setTextColor(TFT_RED); tft.setCursor(65, 110); tft.printf("%02d", targetAlarmHour);
      tft.setTextColor(TFT_WHITE); tft.print(":"); tft.printf("%02d", targetAlarmMin);
    } else {
      tft.setTextColor(TFT_WHITE); tft.setCursor(65, 110); tft.printf("%02d", targetAlarmHour);
      tft.print(":"); tft.setTextColor(TFT_RED); tft.printf("%02d", targetAlarmMin);
    }
  }
  else if (screen == 6) {
    tft.fillScreen(TFT_RED); tft.setTextColor(TFT_WHITE); tft.setTextSize(3);
    tft.setCursor(65, 50); tft.print("WAKE UP!!");
    tft.setTextSize(4); tft.setCursor(65, 110); tft.printf("%02d:%02d", targetAlarmHour, targetAlarmMin);
  }
  else if (screen == 7) {
    tft.fillScreen(TFT_BLACK); tft.setTextColor(TFT_CYAN);
    tft.setCursor(60, 25); tft.print("XIAOCHI AI ACTIVE");
    pushLargeImage(78, 70, xiaochi_ai); 
  }
  else if (screen == 8) {
    tft.fillScreen(TFT_BLACK); tft.setTextColor(TFT_GREEN);
    tft.setCursor(48, 15); tft.print(" IOT  CONTROL");

    tft.drawRect(15, 45, 225, 38, (homeApplianceIndex == 0) ? TFT_RED : TFT_WHITE);
    tft.setTextColor(TFT_WHITE); tft.setCursor(25, 56); tft.print("1. NET FAN: ");
    tft.setTextColor(stateFan ? TFT_GREEN : TFT_RED); tft.print(stateFan ? "ON" : "OFF");

    tft.drawRect(15, 92, 225, 38, (homeApplianceIndex == 1) ? TFT_RED : TFT_WHITE);
    tft.setTextColor(TFT_WHITE); tft.setCursor(25, 103); tft.print("2. NET BULB: ");
    tft.setTextColor(stateBulb ? TFT_GREEN : TFT_RED); tft.print(stateBulb ? "ON" : "OFF");

    tft.drawRect(15, 139, 225, 38, (homeApplianceIndex == 2) ? TFT_RED : TFT_WHITE);
    tft.setTextColor(TFT_WHITE); tft.setCursor(25, 150); tft.print("3. NET A/C: ");
    tft.setTextColor(stateAC ? TFT_GREEN : TFT_RED); tft.print(stateAC ? "ON" : "OFF");

    tft.drawRect(15, 186, 225, 38, (homeApplianceIndex == 3) ? TFT_RED : TFT_WHITE);
    tft.setTextColor(TFT_WHITE); tft.setCursor(28, 197); tft.print(" HEATER: ");
    tft.setTextColor(stateHeater ? TFT_GREEN : TFT_RED); tft.print(stateHeater ? "ON" : "OFF");
  }
  else if (screen == 9) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_CYAN); tft.setTextSize(2);
    tft.setCursor(20, 15); tft.print("NOTIFICATION HUB");

    int boxX = 15; int boxY = 45; int boxW = 225; int boxH = 170;
    tft.drawRect(boxX, boxY, boxW, boxH, TFT_WHITE);

    tft.setTextSize(1);
    for (int idx = 0; idx < 5; idx++) {
      int storageSlot = (notificationScrollIndex + idx) % MAX_NOTIFICATIONS;
      int lineYPosition = boxY + 12 + (idx * 30);
      
      if (lineYPosition < (boxY + boxH - 15)) {
        if (idx == 0) {
          tft.setTextC olor(TFT_YELLOW);
          tft.drawString("> ", boxX + 8, lineYPosition);
        } else {
          tft.setTextColor(TFT_WHITE);
          tft.drawString("  ", boxX + 8, lineYPosition);
        }
        tft.drawString(notificationStorage[storageSlot], boxX + 22, lineYPosition);
        tft.drawFastHLine(boxX + 5, lineYPosition + 18, boxW - 10, 0x2104); 
      }
    }
    tft.setTextColor(TFT_DARKGREY); tft.setCursor(39, 225);
    tft.print("Turn Knob to Scroll ");
  }
}
