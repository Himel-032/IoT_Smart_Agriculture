#include <Arduino.h>
#include "DHT.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ------------------- Pin Definitions -------------------
#define SOIL_PIN1 34
#define SOIL_PIN2 35
#define DHT_PIN 33
#define DHT_TYPE DHT22
#define FLOAT_PIN 27
#define LDR_D_PIN 19
#define PEST_LIGHT 15   // LDR output LED
#define relay 16
// --- manually calculating relay active time
unsigned long startTime = 0;    // Time when motor turns ON
unsigned long activeTime = 0;   // Total active time
bool motorState = false; 
double waterPerSecond = 24.3; // ML
int light=0;
double totalVolumeSession = 0.0;

DHT dht(DHT_PIN, DHT_TYPE);

// // ------ Firebase ------
#define WIFI_SSID "Shahporan"
#define WIFI_PASSWORD "Shahporan"

// // Firebase Credentials
#define DATABASE_URL "https://smart-agriculture-system-25807-default-rtdb.firebaseio.com"
#define API_KEY "AIzaSyAOl9cvZ-T7w5-5wtYCGMPbA6oBQbHhtec"

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
void sendDataFirebase(int moisture, float temp, float hum, int pump, int light, double water = 0.0){
  if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();



    if (Firebase.RTDB.setInt(&fbdo, "weather/moisture", moisture)) {
      Serial.println("Moisture sent: " + String(moisture) + "%");

    } else {
      Serial.println("Error sending moisture: " + fbdo.errorReason());
    }
    if (Firebase.RTDB.setInt(&fbdo, "weather/humidity", hum)) {
      Serial.println("Humidity sent: " + String(hum) + "%");
    } else {
      Serial.println("Error sending moisture: " + fbdo.errorReason());
    }
    if (Firebase.RTDB.setInt(&fbdo, "weather/temperature", temp)) {
      Serial.println("Humidity sent: " + String(temp) + "°C");
    } else {
      Serial.println("Error sending moisture: " + fbdo.errorReason());
    }
    if (Firebase.RTDB.setInt(&fbdo, "systemStatus/motor", pump)) {
      Serial.println("Pump status sent: " + String(pump));
    } else {
      Serial.println("Error sending moisture: " + fbdo.errorReason());
    }
    if (Firebase.RTDB.setInt(&fbdo, "systemStatus/nightLight", light)) {
      Serial.println("Night Light status sent: " + String(light));
    } else {
      Serial.println("Error sending moisture: " + fbdo.errorReason());
    }
    if (Firebase.RTDB.setInt(&fbdo, "water/lastSession", water)) {
      Serial.println("Water status sent: " + String(light));
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

// // ------------------- Calibration -------------------
const int dryValue = 3500;   // raw ADC when soil is dry
const int wetValue = 1130;   // raw ADC when soil is wet
const int numSamples = 4;   // median filter
const float alpha = 0.1;     // smoothing factor

float smoothed1 = 0;
float smoothed2 = 0;
// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  firebaseSetUp();

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // pinMode(LED_SOIL_PIN, OUTPUT);
  pinMode(FLOAT_PIN, INPUT_PULLUP); // Use internal pull-up

  dht.begin();

  pinMode(relay, OUTPUT);

  pinMode(LDR_D_PIN, INPUT);
  pinMode(PEST_LIGHT, OUTPUT);
  digitalWrite(PEST_LIGHT, LOW);

  Serial.println("Smart Dual Soil + DHT22 system initializing...");
}

// ------------------- Loop -------------------
void loop() {



  int soilAvg = getSoilAvg();
  float temp = getTemperature();
  float hum = getHumidity();
  int state = digitalRead(LDR_D_PIN);
  // delay(1000);
 

  if (state == LOW) {
    // Depending on module, HIGH might mean bright
    Serial.println("🌞 Daytime detected");                                                                                                          
    digitalWrite(PEST_LIGHT, LOW);   // LED OFF at day
    light= 0;
  } else {
    Serial.println("🌙 Nighttime detected");
    digitalWrite(PEST_LIGHT, HIGH);  // LED ON at night
    light=1;
  }

  // // Print readings
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

  if (soilAvg <= 60) {  // soil dry
    if (!isnan(hum) && !isnan(temp)) {
      if (hum <= 70 && temp >= 25) {    // air not too humid, temp reasonable
        needWater = true;
      }
      else {
        needWater = false;
      }
    }
  }


  // Add float switch check
if (needWater && isWaterPresent()) { 
    digitalWrite(relay, HIGH); // Relay(pump) ON = water can be given
    Serial.println("Motor on");
    if(!motorState){
      startTime = millis();
      motorState = true;
    }
} else {  
    digitalWrite(relay, LOW);  // Relay(pump) OFF = do not water
    Serial.println("Relay off");
    if(motorState){
      activeTime = millis() - startTime;
      motorState = false;
    Serial.print("Motor ran for: ");
    Serial.print(activeTime / 1000.0); // Convert to seconds
    Serial.print(" seconds| Water this session: ");
    totalVolumeSession = (waterPerSecond * (activeTime/1000.0))/1000.0;
    Serial.print(totalVolumeSession);
    Serial.println("L");
    }
}
  if(motorState == false){
    sendDataFirebase(soilAvg, temp, hum, motorState, light, totalVolumeSession);
  } else {
    sendDataFirebase(soilAvg, temp, hum, motorState, light);
  }
  




  delay(3000); // update every 3 seconds
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
