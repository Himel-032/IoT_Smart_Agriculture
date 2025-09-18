#include <Arduino.h>
#include "DHT.h"

// ------------------- Pin Definitions -------------------
#define SOIL_PIN1 34
#define SOIL_PIN2 35
#define LED_SOIL_PIN 2      // LED indicates watering decision
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define FLOAT_PIN 27

DHT dht(DHT_PIN, DHT_TYPE);

// ------------------- Calibration -------------------
const int dryValue = 3500;   // raw ADC when soil is dry
const int wetValue = 1130;   // raw ADC when soil is wet
const int numSamples = 11;   // median filter
const float alpha = 0.1;     // smoothing factor

float smoothed1 = 0;
float smoothed2 = 0;

// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  pinMode(LED_SOIL_PIN, OUTPUT);
  pinMode(FLOAT_PIN, INPUT_PULLUP); // Use internal pull-up

  dht.begin();

  Serial.println("Smart Dual Soil + DHT22 system initializing...");
}

// ------------------- Loop -------------------
void loop() {
  int soilAvg = getSoilAvg();
  float temp = getTemperature();
  float hum = getHumidity();

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

  if (soilAvg < 40) {  // soil dry
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
} else {
    digitalWrite(LED_SOIL_PIN, LOW);   // LED OFF = do not water
}


  delay(2000); // update every 2 seconds
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
