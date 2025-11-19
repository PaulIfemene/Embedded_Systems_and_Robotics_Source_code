/***************************************************
  Trail Attendance Fingerprint System - ESP32
  PROFESSIONAL GRADE (full code + LED/Buzzer + SD Persist-and-Forward)
  - WiFi + NTP
  - Robust fingerprint enrollment & verification
  - Error handling & retries
  - Google Sheets logging (row-per-user, horizontal timestamps)
  - LED + Buzzer feedback (success / failure)
  - SD card offline logging + upload when reconnects
****************************************************/

#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include <driver/ledc.h>

// ------------------- Wi-Fi CONFIG -------------------
const char* ssid = "hi_stranger";         // your SSID
const char* password = "hi_stranger";     // your password

void connectWiFi(); // forward

// ------------------- Google Script URL -------------------
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbyjN0OBieNdYMTPnTRY139KvSSdwcKKeA7h8VluIv2M6F1BM2bG6YG7KGvEulL2B8cs/exec";

// ------------------- Fingerprint Sensor CONFIG -------------------
#define RX_PIN 16
#define TX_PIN 17
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);

// ------------------- Attendance Database -------------------
#define MAX_USERS 50  
#define MAX_RETRIES 5
#define FINGER_WAIT_TIMEOUT 20000 // 20 seconds per scan

struct User {
  uint8_t id;           
  String name;          
  String regNo;         
};

User users[MAX_USERS];
uint8_t userCount = 0;

// NTP Time Configuration (Uganda UTC+3)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3 * 3600; 
const int   daylightOffset_sec = 0;

// ------------------- LED / Buzzer / SD Pins -------------------
const int GREEN_LED_PIN = 14;
const int RED_LED_PIN   = 27;
const int BUZZER_PIN    = 26; // use LEDC for tone

// SD card (SPI) pins / CS
const int SD_CS_PIN     = 5;   // chip select for SD

// ------------------- SD Log File -------------------
const char* SD_LOG_FILE = "/trail_logs.csv";     // CSV lines appended
const char* SD_TEMP_FILE = "/trail_logs_pending.csv";

// ------------------- LEDC (buzzer) settings -------------------
const int BUZZER_CHANNEL = 0;   // ledc channel
const int BUZZER_FREQ_DEFAULT = 2000;
const int BUZZER_RESOLUTION = 8; // 8-bit resolution

// ------------------- Function prototypes -------------------
String getTimeStamp();
void sendToGoogleSheet(String name, String regNo, uint8_t fid, String timestamp, String status);
String escapeJson(const String &s);
bool sdInit();
void saveLogToSD(const String &csvLine);
void uploadPendingLogs();
void signalSuccess();
void signalFailure();
void playTone(unsigned int freq, unsigned long ms);
void connectWiFi();

// ------------------- Setup -------------------
void setup() {
  Serial.begin(9600);
  while (!Serial);  
  delay(100);
  Serial.println("\n===== Trail Attendance Fingerprint System =====");

  // initialize LED pins
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  // buzzer setup via LEDC
  pinMode(BUZZER_PIN, OUTPUT);
  ledcWriteTone(BUZZER_CHANNEL, 0); // off

  // init SD
  if (sdInit()) {
    Serial.println("SD card initialized.");
  } else {
    Serial.println("SD card NOT initialized. Offline logging disabled.");
  }

  // Connect to Wi-Fi first (best-effort)
  connectWiFi();

  // If connected, try upload pending logs
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // small delay to ensure NTP
    delay(2000);
    uploadPendingLogs();
  } else {
    // still initialize time (will return N/A until connected)
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }

  // Initialize fingerprint sensor
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("ERROR: Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }
}

// ------------------- Main Menu -------------------
void loop() {
  Serial.println("\nSelect Mode:");
  Serial.println("1. Register new user");
  Serial.println("2. Verify fingerprint (Attendance)");
  Serial.println("3. List all registered users");
  Serial.print("Enter choice: ");
  
  while (!Serial.available());
  int choice = Serial.parseInt();
  Serial.read(); // clear buffer

  if (choice == 1) {
    registerUser();
  } else if (choice == 2) {
    verifyUser();
  } else if (choice == 3) {
    listUsers();
  } else {
    Serial.println("Invalid choice, try again.");
  }

  delay(500);
}

// ------------------- Register User -------------------
void registerUser() {
  if (userCount >= MAX_USERS) {
    Serial.println("ERROR: User limit reached!");
    signalFailure();
    return;
  }

  Serial.print("Enter student name: ");
  String name = readString();
  Serial.print("Enter registration number: ");
  String regNo = readString();

  uint8_t id = userCount + 1; // assign next ID
  Serial.print("Enrolling fingerprint for ID "); Serial.println(id);

  if (enrollFingerprint(id)) {
    users[userCount].id = id;
    users[userCount].name = name;
    users[userCount].regNo = regNo;
    userCount++;

    Serial.println("User registered successfully!");
    Serial.print("Name: "); Serial.println(name);
    Serial.print("Reg No: "); Serial.println(regNo);

    // Send registration event to Google Sheets (or save to SD if offline)
    String ts = getTimeStamp();
    sendToGoogleSheet(name, regNo, id, ts, "Registered");

    signalSuccess();
  } else {
    Serial.println("Enrollment failed after multiple attempts. Try again later.");
    signalFailure();

    // Optional: store failed registration attempt locally
    String ts = getTimeStamp();
    String csv = ts + "," + name + "," + regNo + ",0,ENROLL_FAILED";
    saveLogToSD(csv);
  }
}

// ------------------- Verify User -------------------
void verifyUser() {
  Serial.println("Place your finger for verification...");

  int retries = 0;
  while (retries < MAX_RETRIES) {
    if (!waitForFinger()) {
      retries++;
      Serial.print("Retry "); Serial.print(retries); Serial.println(" of 5");
      continue;
    }

    if (finger.image2Tz(1) != FINGERPRINT_OK) {
      Serial.println("ERROR: Could not convert fingerprint. Adjust finger and try again.");
      retries++;
      continue;
    }

    int result = finger.fingerFastSearch();
    if (result == FINGERPRINT_OK) {
      uint8_t fid = finger.fingerID;
      bool found = false;
      for (uint8_t i = 0; i < userCount; i++) {
        if (users[i].id == fid) {
          String timestamp = getTimeStamp();
          Serial.println("Attendance recorded!");
          Serial.print("Name: "); Serial.println(users[i].name);
          Serial.print("Reg No: "); Serial.println(users[i].regNo);
          Serial.print("Time: "); Serial.println(timestamp);
          found = true;

          // Send verification to Google Sheets (or save to SD if offline)
          sendToGoogleSheet(users[i].name, users[i].regNo, fid, timestamp, "Verified");

          signalSuccess();
          break;
        }
      }
      if (!found) {
        Serial.println("User not found in database!");
        String timestamp = getTimeStamp();
        // Log unknown fingerprint recognized
        sendToGoogleSheet("Unknown", "Unknown", fid, timestamp, "Fingerprint recognized - no user");
        signalFailure();
      }
      return;
    } else {
      Serial.println("User not found. Adjust finger and try again.");
      retries++;
    }
  }
  Serial.println("Verification failed after multiple attempts.");
  signalFailure();
}

// ------------------- List Users -------------------
void listUsers() {
  if (userCount == 0) {
    Serial.println("No users registered yet.");
    return;
  }

  Serial.println("\n--- Registered Users ---");
  for (uint8_t i = 0; i < userCount; i++) {
    Serial.print("ID: "); Serial.print(users[i].id);
    Serial.print(" | Name: "); Serial.print(users[i].name);
    Serial.print(" | Reg No: "); Serial.println(users[i].regNo);
  }
  Serial.println("------------------------");
}

// ------------------- Helper Functions -------------------

// Read string input from Serial
String readString() {
  String input = "";
  while (input.length() == 0) {
    while (!Serial.available());
    input = Serial.readStringUntil('\n');
    input.trim();
  }
  return input;
}

// Enroll fingerprint with retries and feedback
bool enrollFingerprint(uint8_t id) {
  int retries = 0;

  while (retries < MAX_RETRIES) {
    Serial.println("First scan: Place finger properly...");

    if (!waitForFinger()) {
      retries++;
      Serial.print("Retry "); Serial.print(retries); Serial.println(" of 5");
      continue;
    }

    int p1 = finger.image2Tz(1);
    if (p1 != FINGERPRINT_OK) {
      handleEnrollmentError(p1);
      retries++;
      continue;
    }

    Serial.println("Remove finger and wait 2 seconds...");
    delay(2000);

    Serial.println("Second scan: Place same finger again...");
    if (!waitForFinger()) {
      retries++;
      Serial.print("Retry "); Serial.print(retries); Serial.println(" of 5");
      continue;
    }

    int p2 = finger.image2Tz(2);
    if (p2 != FINGERPRINT_OK) {
      handleEnrollmentError(p2);
      retries++;
      continue;
    }

    // Create model
    int p = finger.createModel();
    if (p != FINGERPRINT_OK) {
      Serial.println("ERROR: Fingerprints did not match. Try again.");
      retries++;
      continue;
    }

    // Store model
    p = finger.storeModel(id);
    if (p != FINGERPRINT_OK) {
      Serial.println("ERROR: Could not store fingerprint model.");
      retries++;
      continue;
    }

    Serial.println("Fingerprint enrolled successfully!");
    return true;
  }

  return false; // failed after retries
}

// Waits for a finger and provides real-time feedback
bool waitForFinger() {
  uint32_t startTime = millis();
  while (true) {
    int p = finger.getImage();

    switch(p) {
      case FINGERPRINT_OK:
        Serial.println("Fingerprint detected!");
        return true;
      case FINGERPRINT_NOFINGER:
        Serial.println("No finger detected. Place finger properly.");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Finger not fully on sensor. Adjust finger and try again.");
        break;
      case FINGERPRINT_IMAGEMESS:
        Serial.println("Finger image unclear. Clean finger and place again.");
        break;
      case FINGERPRINT_FEATUREFAIL:
        Serial.println("Cannot detect fingerprint features. Adjust finger and try again.");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error with sensor. Retry...");
        break;
      default:
        Serial.println("Unknown sensor error. Retry...");
        break;
    }

    delay(1000); // Real-time guidance every 1 sec

    if (millis() - startTime > FINGER_WAIT_TIMEOUT) {
      Serial.println("Timeout: No valid finger detected.");
      return false;
    }
  }
}

// Handle errors during enrollment
void handleEnrollmentError(int code) {
  switch(code) {
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Enrollment Error: Finger image messy. Clean finger and try again.");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Enrollment Error: Communication error with sensor.");
      break;
    case FINGERPRINT_FEATUREFAIL:
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Enrollment Error: Cannot extract fingerprint features. Adjust finger.");
      break;
    default:
      Serial.println("Enrollment Error: Unknown error occurred.");
      break;
  }
}

// Get current timestamp (ESP32 local time via NTP if available)
String getTimeStamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time unavailable";
  }
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// ------------------- Google Sheets Integration -------------------
// Send JSON payload to Google Apps Script (POST) with small retry logic
void sendToGoogleSheet(String name, String regNo, uint8_t fid, String timestamp, String status) {
  if (googleScriptURL == nullptr || strlen(googleScriptURL) < 10) {
    Serial.println("Google Script URL not configured.");
    return;
  }

  // Build JSON payload
  String payload = "{";
  payload += "\"name\":\"" + escapeJson(name) + "\","; 
  payload += "\"regNo\":\"" + escapeJson(regNo) + "\","; 
  payload += "\"fingerID\":\"" + String(fid) + "\","; 
  payload += "\"timestamp\":\"" + escapeJson(timestamp) + "\","; 
  payload += "\"status\":\"" + escapeJson(status) + "\"";
  payload += "}";

  // Attempt to POST, with retries; if offline or failure, save to SD
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected — saving log to SD.");
    String csv = timestamp + "," + name + "," + regNo + "," + String(fid) + "," + status;
    saveLogToSD(csv);
    return;
  }

  const int maxHttpRetries = 3;
  int attempt = 0;
  bool success = false;

  while (attempt < maxHttpRetries && !success) {
    attempt++;
    HTTPClient http;
    http.begin(googleScriptURL);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String resp = http.getString();
      Serial.print("Google Sheets response: ");
      Serial.println(resp);
      success = true;
    } else {
      Serial.print("Failed to POST (attempt ");
      Serial.print(attempt);
      Serial.print("). Error: ");
      Serial.println(http.errorToString(httpResponseCode));
      delay(1000 * attempt); // backoff
    }
    http.end();
  }

  if (!success) {
    Serial.println("Failed to send data to Google Sheets after retries — saving to SD.");
    String csv = timestamp + "," + name + "," + regNo + "," + String(fid) + "," + status;
    saveLogToSD(csv);
  }
}

// Minimal JSON string escaper (handles quotes and backslashes)
String escapeJson(const String &s) {
  String out = "";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\"' || c == '\\') {
      out += '\\';
      out += c;
    } else {
      out += c;
    }
  }
  return out;
}

// ------------------- SD Card Functions -------------------
bool sdInit() {
  // initialize SPI first with standard ESP32 SPI pins
  SPI.begin(18, 19, 23); // SCK, MISO, MOSI
  if (!SD.begin(SD_CS_PIN)) {
    return false;
  }
  return true;
}

// Save a CSV line to SD (append)
void saveLogToSD(const String &csvLine) {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD write failed: SD not present.");
    return;
  }

  File f = SD.open(SD_LOG_FILE, FILE_APPEND);
  if (!f) {
    Serial.println("Failed to open log file for append.");
    return;
  }
  f.println(csvLine);
  f.close();
  Serial.println("Saved log to SD.");
}

// Upload pending logs (read file, attempt to send each line; keep failures)
void uploadPendingLogs() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No Wi-Fi — cannot upload pending logs.");
    return;
  }

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD not available, skipping upload.");
    return;
  }

  if (!SD.exists(SD_LOG_FILE)) {
    Serial.println("No pending logs to upload.");
    return;
  }

  File f = SD.open(SD_LOG_FILE, FILE_READ);
  if (!f) {
    Serial.println("Failed to open log file for reading.");
    return;
  }

  File temp = SD.open(SD_TEMP_FILE, FILE_WRITE); // collect failures
  if (!temp) {
    Serial.println("Failed to open temp file for writing.");
    f.close();
    return;
  }

  char lineBuf[256];
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // CSV format: timestamp,name,regNo,fid,status
    int idx1 = line.indexOf(',');
    int idx2 = line.indexOf(',', idx1 + 1);
    int idx3 = line.indexOf(',', idx2 + 1);
    int idx4 = line.indexOf(',', idx3 + 1);
    if (idx1 < 0 || idx2 < 0 || idx3 < 0 || idx4 < 0) {
      // malformed line -> keep for manual review
      temp.println(line);
      continue;
    }
    String timestamp = line.substring(0, idx1);
    String name = line.substring(idx1 + 1, idx2);
    String regNo = line.substring(idx2 + 1, idx3);
    String fidStr = line.substring(idx3 + 1, idx4);
    String status = line.substring(idx4 + 1);

    uint8_t fid = (uint8_t) fidStr.toInt();

    // try send
    // Build JSON payload
    String payload = "{";
    payload += "\"name\":\"" + escapeJson(name) + "\","; 
    payload += "\"regNo\":\"" + escapeJson(regNo) + "\","; 
    payload += "\"fingerID\":\"" + String(fid) + "\","; 
    payload += "\"timestamp\":\"" + escapeJson(timestamp) + "\","; 
    payload += "\"status\":\"" + escapeJson(status) + "\"";
    payload += "}";

    HTTPClient http;
    http.begin(googleScriptURL);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String resp = http.getString();
      Serial.print("Uploaded log. Server response: ");
      Serial.println(resp);
    } else {
      Serial.print("Failed to upload log. Keeping in temp. Error: ");
      Serial.println(http.errorToString(httpResponseCode));
      temp.println(line); // preserve failed entry
    }
    http.end();
    delay(200); // small gap between uploads
  }

  f.close();
  temp.close();

  // replace original log with temp (temp has only failed entries)
  SD.remove(SD_LOG_FILE);
  if (SD.exists(SD_TEMP_FILE)) {
    SD.rename(SD_TEMP_FILE, SD_LOG_FILE);
    Serial.println("Pending logs processed; failed ones preserved.");
  } else {
    Serial.println("All pending logs uploaded successfully.");
  }
}

// ------------------- LED & Buzzer Feedback -------------------
void signalSuccess() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
  playTone(1500, 1000); // success tone 1.5kHz for 1s
  digitalWrite(GREEN_LED_PIN, LOW);
}

void signalFailure() {
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
  playTone(600, 1000); // failure tone 600Hz for 1s
  digitalWrite(RED_LED_PIN, LOW);
}

// Play a tone (blocking for ms). Uses LEDC on ESP32
void playTone(unsigned int freq, unsigned long ms) {
  ledcWriteTone(BUZZER_CHANNEL, freq);
  delay(ms);
  ledcWriteTone(BUZZER_CHANNEL, 0); // stop
}

// ------------------- Wi-Fi connect + auto-upload -------------------
void connectWiFi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  uint8_t retryCount = 0;
  const uint8_t maxRetries = 20;

  while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Sync time and upload pending logs
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(1500);
    uploadPendingLogs();
  } else {
    Serial.println("\nFailed to connect to Wi-Fi. Check credentials.");
  }
}
