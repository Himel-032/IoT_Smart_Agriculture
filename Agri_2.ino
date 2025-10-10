#include <Arduino.h>
#include "DHT.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ------------------- Pin Definitions -------------------
#define SOIL_PIN1 34
#define SOIL_PIN2 35
#define LED_SOIL_PIN 2      // LED indicates watering decision
#define DHT_PIN 33
#define DHT_TYPE DHT22
#define FLOAT_PIN 27
#define LDR_D_PIN 19
#define LED_PIN 15   // LDR output LED
#define relay 4
#define FLOW_SENSOR 32    // digital input for YF-S201

DHT dht(DHT_PIN, DHT_TYPE);

// ------ Firebase ------
#define WIFI_SSID "GalaxyM21"
#define WIFI_PASSWORD "8765432112"

// Firebase Credentials
#define DATABASE_URL "https://esp32testing-b35d1-default-rtdb.firebaseio.com/"
#define API_KEY "AIzaSyBEPOAA8vdEUx7bihiw49MLaJoIwmO1in4"

FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void firebaseSetUp()
{
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
void sendDataFirebase(int value){
  if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();



    if (Firebase.RTDB.setInt(&fbdo, "sensor/moisture", value)) {
    Serial.println("Moisture sent: " + String(value) + "%");

  } else {
    Serial.println("Error sending moisture: " + fbdo.errorReason());
  }
  }
}
double readDataFirebase(){
    if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){
       if (Firebase.RTDB.getInt(&fbdo, "sensor/moisture")) {
   double value= fbdo.intData();
   
    Serial.println("Moisture: "+ String(value));
  } else {
    Serial.println("Error reading : " + fbdo.errorReason());
  }
    }
}

// ------------------- Calibration -------------------
const int dryValue = 3500;   // raw ADC when soil is dry
const int wetValue = 1130;   // raw ADC when soil is wet
const int numSamples = 4;   // median filter
const float alpha = 0.1;     // smoothing factor

float smoothed1 = 0;
float smoothed2 = 0;
// ----------- flow sensor ----------
volatile unsigned long flowPulseCount = 0;
float flowRate = 0.0;           // L/min
unsigned long totalMilliLitres = 0;
unsigned long previousFlowMillis = 0;
const unsigned long flowInterval = 1000; // 1 second

// -------- flow sensor ------
void IRAM_ATTR flowPulseCounter() {
  flowPulseCount++;
}
float getFlowRate() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousFlowMillis >= flowInterval) {
    noInterrupts();
    unsigned long pulses = flowPulseCount;
    flowPulseCount = 0;
    interrupts();

    // Datasheet calibration: pulses per second / calibrationFactor
    const float calibrationFactor = 4.5;  // adjust if needed
    flowRate = (float)pulses / calibrationFactor;   // L/min

    // Convert L/min to mL per second
    unsigned int flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;

    previousFlowMillis = currentMillis;
  }
  return flowRate;
}

unsigned long getTotalMilliLitres() {
  return totalMilliLitres;
}


// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  firebaseSetUp();
  // ---- flow sensor --
  pinMode(FLOW_SENSOR, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR), flowPulseCounter, FALLING);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  pinMode(LED_SOIL_PIN, OUTPUT);
  pinMode(FLOAT_PIN, INPUT_PULLUP); // Use internal pull-up

  dht.begin();

  pinMode(relay, OUTPUT);

  pinMode(LDR_D_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Smart Dual Soil + DHT22 system initializing...");
}

// ------------------- Loop -------------------
void loop() {

  // -------- flow sensor-----
  // Only measure flow when watering
float currentFlow = 0;
if (digitalRead(relay) == HIGH) {
    currentFlow = getFlowRate();
} else {
    flowPulseCount = 0;  // reset pulse count when water is OFF
}

// Print flow info
Serial.print("Flow rate: "); Serial.print(currentFlow, 2); Serial.print(" L/min\t");
Serial.print("Total water dispensed: "); Serial.print(getTotalMilliLitres()/1000.0); Serial.println(" L");
  int soilAvg = getSoilAvg();
  float temp = getTemperature();
  float hum = getHumidity();
  int state = digitalRead(LDR_D_PIN);
  delay(1000);

  if (state == LOW) {
    // Depending on module, HIGH might mean bright
    Serial.println("🌞 Daytime detected");                                                                                                          
    digitalWrite(LED_PIN, LOW);   // LED OFF at day
  } else {
    Serial.println("🌙 Nighttime detected");
    digitalWrite(LED_PIN, HIGH);  // LED ON at night
  }

  // Print readings
  Serial.print("Water: "); Serial.print(isWaterPresent());
  Serial.print(" Soil Avg: "); Serial.print(soilAvg); Serial.print("%  |  ");
  if (!isnan(temp) && !isnan(hum)) {
    Serial.print("Temp: "); Serial.print(temp); Serial.print("°C  ");
    Serial.print("Humidity: "); Serial.print(hum); Serial.println("%");
    
  } else {
    Serial.println("DHT reading error!");
  }

  // ---- Smart watering decision using soil, temp, and humidity ----
  bool needWater = false;

  if (soilAvg < 50) {  // soil dry
    if (!isnan(hum) && !isnan(temp)) {
      if (hum < 85 && temp > 10) {    // air not too humid, temp reasonable
        needWater = true;
      }
      if (temp > 35 && soilAvg < 30) { // very hot + very dry soil → force watering
        needWater = true;
      }
    }
  }


  // Add float switch check
if (needWater && isWaterPresent()) {
    digitalWrite(LED_SOIL_PIN, HIGH);  // LED ON = water can be given
    digitalWrite(relay, HIGH);
} else {
    digitalWrite(LED_SOIL_PIN, LOW);   // LED OFF = do not water
    digitalWrite(relay, LOW);
}


  delay(3000); // update every 2 seconds
}




// -------------------Float switch--------------

bool isWaterPresent() {
  int floatState = digitalRead(FLOAT_PIN);

  // Adjust logic if your switch is NO or NC
  // Return true if water detected, false if no water
  if (floatState == HIGH) { // Modify if needed for your switch type
    return true; // Water present
  } else {
    return false; // No water
  }
}
// ------------------- Soil Sensor Functions -------------------
int readMedian(int pin) {
  int readings[numSamples];
  for (int i = 0; i < numSamples; i++) {
    readings[i] = analogRead(pin);
    delay(5);
  }
  // bubble sort
  for (int i = 0; i < numSamples - 1; i++) {
    for (int j = i + 1; j < numSamples; j++) {
      if (readings[i] > readings[j]) {
        int temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }
  return readings[numSamples / 2];
}

int soilPercent(int rawValue) {
  int percent = map(rawValue, dryValue, wetValue, 0, 100);
  return constrain(percent, 0, 100);
}

int getSoil1() {
  int median = readMedian(SOIL_PIN1);
  smoothed1 = smoothed1 * (1 - alpha) + median * alpha;
  return soilPercent((int)smoothed1);
}

int getSoil2() {
  int median = readMedian(SOIL_PIN2);
  smoothed2 = smoothed2 * (1 - alpha) + median * alpha;
  return soilPercent((int)smoothed2);
}

int getSoilAvg() {
  return (getSoil1() + getSoil2()) / 2;
}

// ------------------- DHT22 Functions -------------------
float getTemperature() {
  return dht.readTemperature();
}

float getHumidity() {
  return dht.readHumidity();
}
