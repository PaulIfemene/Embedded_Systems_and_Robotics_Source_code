/***************************************************
  Trail Attendance Fingerprint System - ESP32
  PROFESSIONAL GRADE
  Registers and verifies fingerprints linked to
  Student Name and Registration Number
  Tracks attendance with timestamps
  INDUSTRY-GRADE ERROR HANDLING
  WI-FI ENABLED
****************************************************/

#include <WiFi.h>
#include <Adafruit_Fingerprint.h>
#include <time.h>

// ------------------- Wi-Fi CONFIG -------------------
const char* ssid = "hi_stranger";         // replace with your Wi-Fi SSID
const char* password = "hi_stranger"; // replace with your Wi-Fi password

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
  } else {
    Serial.println("\nFailed to connect to Wi-Fi. Check credentials.");
  }
}

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

// ------------------- Setup -------------------
void setup() {
  Serial.begin(9600);
  while (!Serial);  
  delay(100);
  Serial.println("\n===== Trail Attendance Fingerprint System =====");

  // Connect to Wi-Fi first
  connectWiFi();

  // Initialize NTP after Wi-Fi
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

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
  } else {
    Serial.println("Enrollment failed after multiple attempts. Try again later.");
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
          break;
        }
      }
      if (!found) Serial.println("User not found in database!");
      return;
    } else {
      Serial.println("User not found. Adjust finger and try again.");
      retries++;
    }
  }
  Serial.println("Verification failed after multiple attempts.");
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

// Get current timestamp
String getTimeStamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time unavailable";
  }
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}
