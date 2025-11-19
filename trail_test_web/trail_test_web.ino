#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <time.h>

// ======= WIFI CONFIGURATION =======
const char* ssid = "hi_stranger";
const char* password = "hi_stranger";

// ======= GOOGLE SCRIPT URL =======
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbxiM0Wx_qi4KfQzp-Rt0mdxKFRetvzA0awfqyxAb6yMvC69i7gV3lzHxIPcrXyXX3DP/exec"; // example: https://script.google.com/macros/s/AKfycbxxxxxx/exec

// ======= FINGERPRINT SETUP =======
HardwareSerial mySerial(2); // RX2 (GPIO16), TX2 (GPIO17)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// ======= STRUCT FOR REGISTERED USERS =======
struct User {
  uint8_t id;
  String name;
  String regNo;
};
User users[20];
int totalUsers = 0;

// ======= TIME CONFIGURATION =======
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;  // Uganda GMT+3
const int daylightOffset_sec = 0;

// ======= HELPER FUNCTIONS =======
String getTimeNow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "N/A";
  }
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

void sendToGoogleSheet(String name, String regNo, String status) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(googleScriptURL) + "?name=" + name +
                 "&regno=" + regNo +
                 "&timestamp=" + getTimeNow() +
                 "&status=" + status;
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.println("✅ Data sent to Google Sheets successfully.");
    } else {
      Serial.println("⚠️ Failed to send data to Google Sheets.");
    }
    http.end();
  } else {
    Serial.println("⚠️ WiFi disconnected, cannot send data now.");
  }
}

// ======= SYSTEM SETUP =======
void setup() {
  Serial.begin(115200);
  mySerial.begin(57600);

  Serial.println("\nTRAIL - Cloud Connected Fingerprint Attendance System");

  // ======= Connect to Wi-Fi =======
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi Connected!");
  Serial.println(WiFi.localIP());

  // ======= Initialize Time =======
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("⏱ Time synced from NTP server.");

  // ======= Initialize Fingerprint Sensor =======
  finger.begin(57600);
  delay(1000);
  if (finger.verifyPassword()) {
    Serial.println("✅ Fingerprint sensor detected.");
  } else {
    Serial.println("❌ Fingerprint sensor not detected!");
    while (1) { delay(1); }
  }
}

// ======= MAIN MENU =======
void loop() {
  Serial.println("\nSelect mode:");
  Serial.println("1. Register new user");
  Serial.println("2. Verify fingerprint");
  Serial.println("3. List all registered users");

  while (!Serial.available());
  int choice = Serial.parseInt();
  Serial.read(); // clear buffer

  if (choice == 1) registerUser();
  else if (choice == 2) verifyUser();
  else if (choice == 3) listUsers();
  else Serial.println("Invalid choice. Try again.");
}

// ======= REGISTER FUNCTION =======
void registerUser() {
  if (totalUsers >= 20) {
    Serial.println("⚠️ User storage full!");
    return;
  }

  Serial.println("Enter Name:");
  while (!Serial.available());
  String name = Serial.readStringUntil('\n');
  name.trim();

  Serial.println("Enter Reg No:");
  while (!Serial.available());
  String regNo = Serial.readStringUntil('\n');
  regNo.trim();

  uint8_t id = totalUsers + 1;
  Serial.println("Place your finger to register...");

  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    else if (p == FINGERPRINT_PACKETRECIEVEERR) Serial.println("⚠️ Communication error.");
    else if (p == FINGERPRINT_IMAGEFAIL) Serial.println("⚠️ Image capture failed.");
    else if (p == FINGERPRINT_OK) break;
  }

  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("❌ Failed to convert image. Place finger properly.");
    return;
  }

  Serial.println("Remove finger and place again...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER);

  Serial.println("Place the same finger again...");
  while (finger.getImage() != FINGERPRINT_OK);

  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("❌ Second scan failed.");
    return;
  }

  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("❌ Fingerprints did not match.");
    return;
  }

  if (finger.storeModel(id) == FINGERPRINT_OK) {
    users[totalUsers] = {id, name, regNo};
    totalUsers++;
    Serial.println("✅ Registration successful!");
    sendToGoogleSheet(name, regNo, "Registered");
  } else {
    Serial.println("❌ Failed to store fingerprint.");
  }
}

// ======= VERIFY FUNCTION =======
void verifyUser() {
  Serial.println("Place your finger for verification...");

  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    else if (p == FINGERPRINT_PACKETRECIEVEERR) Serial.println("⚠️ Communication error.");
    else if (p == FINGERPRINT_IMAGEFAIL) Serial.println("⚠️ Image capture failed.");
    else if (p == FINGERPRINT_OK) break;
  }

  if (finger.image2Tz() != FINGERPRINT_OK) {
    Serial.println("❌ Failed to process fingerprint.");
    return;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    uint8_t id = finger.fingerID;
    String name = users[id - 1].name;
    String regNo = users[id - 1].regNo;
    String timeNow = getTimeNow();

    Serial.printf("✅ Match found: %s (%s)\nTime: %s\n", name.c_str(), regNo.c_str(), timeNow.c_str());
    sendToGoogleSheet(name, regNo, "Verified");
  } else {
    Serial.println("❌ User not found!");
  }
}

// ======= LIST USERS =======
void listUsers() {
  Serial.println("\nRegistered Users:");
  for (int i = 0; i < totalUsers; i++) {
    Serial.printf("%d. %s (%s)\n", users[i].id, users[i].name.c_str(), users[i].regNo.c_str());
  }
}
