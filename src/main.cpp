#include <iostream>
#include <Wire.h>
#include <SPI.h>

#include <Adafruit_ADS1X15.h>
#include <Adafruit_SHT31.h>

#include "mux.hpp"
#include "aqm_envcity_config.h"
#include "Alphasense_GasSensors.hpp"

#define _SDA_ 10
#define _SCL_ 11
//enum {S0 = 2, S1 = 1, S2 = 17, S3 = 18}; // MUX PINS

gpio_num_t pins[] = {GPIO_NUM_2, GPIO_NUM_1, GPIO_NUM_17, GPIO_NUM_18};
mux Mux(pins, 4);

Adafruit_ADS1115 ads;

int16_t temp = 0, umid = 0;

// NO2 -> ---
AlphasenseSensorParam param5 = {"NO2", NO2B43F_n, -0.73, 222, 212, -424, 0.31, 230, 220, 0};
Alphasense_NO2 no2(param5);

bool isValid(float value) {
    const float MIN_VALID = 0.0;   
    const float MAX_VALID = 1000.0;
    return value >= MIN_VALID && value <= MAX_VALID && !std::isnan(value);
}

// Calculates the best NO2 value
float getBestNO2Value(float no2_ppb[]) {
    float sum = 0;
    int validCount = 0;

    for (int i = 0; i < 4; i++) {
        if (isValid(no2_ppb[i])) {
            sum += no2_ppb[i];
            validCount++;
        }
    }

    if (validCount > 0) { return sum / validCount; }
    else { return -1; }
}

float bestNO2Value = 0;

//SHT -------------------------------------------------------------------------------------------------------------------
Adafruit_SHT31 sht = Adafruit_SHT31();

void setup_sht()
{
  Serial.println("SHT31 test");
  if (! sht.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }
  Serial.println("Found SHT31 sensor");
}

float read_sht(bool temp_humd) // true for temperature, false for humidity
{
  if(temp_humd)
  {
    if (! isnan(sht.readTemperature()))   // check if 'is not a number'
      return sht.readTemperature();
  }
  else
  {
    if (! isnan(sht.readHumidity()))   // check if 'is not a number'
      return sht.readHumidity();
  }
  return 9999;
}
//SHT -------------------------------------------------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);
  delay(100);
  Wire.begin(_SDA_, _SCL_, 100000);
  delay(5000);

  // Configure gain for ADS1115
  ads.setGain(GAIN_TWOTHIRDS);

  // Initialize ADS1115 and check for initialization success
  if (!ads.begin(0x48)) { Serial.println("Failed to initialize ADS1115."); } 
  else { Serial.println("ADS1115 initialized successfully."); }

  setup_sht();
}

float no2_ppb[4]; 

void loop()
{
  uint16_t adc = 0;
  float readed_voltage[1];
  Mux.selectOutput(0);
  adc = ads.readADC_SingleEnded(0);
  readed_voltage[0]= ads.computeVolts(adc);
  ets_delay_us(10);

  Mux.selectOutput(1);
  adc = ads.readADC_SingleEnded(0);
  readed_voltage[1] = ads.computeVolts(adc);
  ets_delay_us(10);

  float temp = read_sht(true);

  no2.fourAlgorithms(1000*readed_voltage[0], 1000*readed_voltage[1], no2_ppb, temp);

  // Checks the values for NO2 and then process the OX sensor data
  bestNO2Value = getBestNO2Value(no2_ppb);

  Serial.print("Temp: ");  Serial.println(temp);
  Serial.print("Umid: ");  Serial.println(read_sht(false));
  Serial.print("NO2: ");   Serial.println(no2_ppb[0]);
}