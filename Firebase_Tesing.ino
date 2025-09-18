#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Wi-Fi Credentials
#define WIFI_SSID "GalaxyM21"
#define WIFI_PASSWORD "8765432112"

// Firebase Credentials
#define DATABASE_URL "https://esp32testing-b35d1-default-rtdb.firebaseio.com/"
#define API_KEY "AIzaSyBEPOAA8vdEUx7bihiw49MLaJoIwmO1in4"

FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

// Pins
#define LED_PIN 2
#define MOISTURE_PIN 34

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWi-Fi Connected!");

  // Firebase configuration
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;

  // Anonymous login (leave email/password empty)
  if(Firebase.signUp(&config, &auth, "", "")){
    Serial.println("SignUp OK");
    signupOK = true;
  }else{
    Serial.print("Signup Error: ");
    Serial.println(config.signer.signupError.message.c_str());

  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

 

  Serial.println("Firebase initialized!");
}

void loop() {
  if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    int moisture = analogRead(MOISTURE_PIN);
  int moisturePercent = map(moisture, 4095, 0, 0, 100);

    if (Firebase.RTDB.setInt(&fbdo, "sensor/moisture", moisturePercent)) {
    Serial.println("Moisture sent: " + String(moisturePercent) + "%");

  } else {
    Serial.println("Error sending moisture: " + fbdo.errorReason());
  }

   // ----------reading data
  
    if (Firebase.RTDB.getInt(&fbdo, "LED/led")) {
    int ledState = fbdo.intData();
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    Serial.println("LED state: " + String(ledState));
  } else {
    Serial.println("Error reading LED: " + fbdo.errorReason());
  }

  }

  
}
