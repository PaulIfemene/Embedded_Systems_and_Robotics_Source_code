// --- Buzzer Pin Configuration ---
// **IMPORTANT**: Change this if you connect the buzzer to a different pin
const int BUZZER_PIN = 2; 

void setup() {
  Serial.begin(115200);
  Serial.println("Buzzer Test Starting (using tone())...");

  // No ledcSetup or ledcAttachPin needed!
  // The 'tone' function handles the PWM setup internally.

  // --- Test the Tones ---
  Serial.println("\nPlaying SUCCESS Tone...");
  playSuccessTone();

  delay(2000); // Wait 2 seconds between tests

  Serial.println("\nPlaying FAILURE Tone...");
  playFailureTone();
  
  Serial.println("\nBuzzer Test Complete.");
}

void loop() {
  // Nothing needed in the loop for this test
}

// --- SUCCESS Tone Implementation (High-Low-High ascending sound) ---
void playSuccessTone() {
  // tone(pin, frequency, duration)
  
  // Tone 1: C5 (523 Hz)
  tone(BUZZER_PIN, 523, 100); 
  delay(150); // Pause includes the tone duration + short break
  
  // Tone 2: E5 (659 Hz)
  tone(BUZZER_PIN, 659, 150); 
  delay(150); // Wait for the tone to finish
  noTone(BUZZER_PIN); // Stop the tone after the sequence
}

// --- FAILURE Tone Implementation (Low, repetitive 'buzz') ---
void playFailureTone() {
  // Play a low, repetitive note with short, sharp pulses: A4 (440 Hz)
  
  for (int i = 0; i < 3; i++) { // Repeat the short tone 3 times
    // Tone: A4
    tone(BUZZER_PIN, 440, 80); 
    delay(180); // Longer pause between pulses to sound choppy (80ms tone + 100ms pause)
    noTone(BUZZER_PIN); // Explicitly stop the tone
    delay(100); 
  }
}