#define SIGNAL_PIN 2   // from ESP32
#define RELAY_PIN 4    // relay module
#define SIGNAL_PIN_LIGHT 7  // from esp32 for night light
#define LIGHT_PIN 6

void setup() {
  Serial.begin(9600);
  pinMode(SIGNAL_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // OFF (for active LOW relay)
  pinMode(SIGNAL_PIN_LIGHT, INPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
  Serial.println("Arduino Ready: Waiting for ESP32 signal...");
}

void loop() {
  int signalState = digitalRead(SIGNAL_PIN);
  int signalStateLight = digitalRead(SIGNAL_PIN_LIGHT);

  if (signalState == HIGH) {
    // Turn relay ON
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Relay ON (signal HIGH)");
  } else {
    // Turn relay OFF
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Relay OFF (signal LOW)");
  }
  if(signalStateLight == HIGH){
    digitalWrite(LIGHT_PIN, LOW); // light on
    Serial.println("LIGHT ON");
  } else{
    digitalWrite(LIGHT_PIN, HIGH); // light off
    Serial.println("LIGHT OFF");
  }

  delay(2000);
}
