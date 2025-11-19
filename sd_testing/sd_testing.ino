#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Define the Chip Select (CS) pin for the SD card module
// **IMPORTANT**: You MUST change this to match your actual wiring!
// GPIO 5 is a common default for many modules.
#define SD_CS_PIN 5 

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Initializing SD card...");
  
  // Initialize the SD card using the CS pin defined above
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("❌ ERROR: Card Mount Failed or NO SD Card detected.");
    Serial.println("Check wiring, power, and that an SD card is inserted.");
    return; // Stop here if initialization fails
  }
  
  // If we reach here, the card is mounted successfully.
  Serial.println("✅ SUCCESS: SD Card mounted successfully.");
  
  // --- Check Card Type and Size ---
  
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached.");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC or SDXC (High Capacity)");
  } else {
    Serial.println("UNKNOWN");
  }

  // Calculate and print the total size of the SD card
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %llu MB\n", cardSize);
  
  // --- Basic Read/Write Test ---
  
  // We'll write a simple test string to a file, then read it back.
  const char* path = "/test.txt";
  const char* message = "Congratulations, You've Successfully connected your ESP32 SD Card module!";

  // 1. Write the test message to the file
  Serial.printf("\nWriting to file: %s\n", path);
  writeFile(SD, path, message);

  // 2. Read the content of the file
  Serial.printf("Reading file: %s\n", path);
  readFile(SD, path);
}

void loop() {
  // Nothing needed in the loop for this basic check
}

// --- Helper Functions for Read/Write ---

void readFile(fs::FS &fs, const char * path){
  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  
  Serial.print("File Content: ");
  while(file.available()){
    Serial.write(file.read());
  }
  Serial.println(); // Newline after reading the file
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  
  if(file.print(message)){
    Serial.println("File written successfully");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}