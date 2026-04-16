// RGB LED Pin Test for Wemos D1 Mini
// Tests each pin individually to verify wiring
// Red=D6 (GPIO12), Green=D7 (GPIO13), Blue=D1 (GPIO5)

#define RED_PIN   12  // D6
#define GREEN_PIN 13  // D7
#define BLUE_PIN   5  // D1

void setup() {
  Serial.begin(115200);
  Serial.println("\nRGB Pin Test - testing each pin one at a time");

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  // Force everything off
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
}

void loop() {
  // All OFF
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
  Serial.println("ALL OFF");
  delay(2000);

  // Only D6 (should be RED)
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
  Serial.println("D6 only (expect RED)");
  delay(2000);

  // Only D7 (should be GREEN)
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, HIGH);
  digitalWrite(BLUE_PIN, LOW);
  Serial.println("D7 only (expect GREEN)");
  delay(2000);

  // Only D1 (should be BLUE)
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, HIGH);
  Serial.println("D1 only (expect BLUE)");
  delay(2000);
}
