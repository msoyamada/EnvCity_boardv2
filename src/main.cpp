#include <iostream>
#include <Wire.h>
#include <SPI.h>

#include <Adafruit_ADS1X15.h>
#include <Adafruit_SHT31.h>

#include "mux.hpp"
#include "aqm_envcity_config.h"
#include "Alphasense_GasSensors.hpp"

// -------------------------------------------------------------------------------------------------------------------

#define _SDA_ 10
#define _SCL_ 11

#define _S0_ 2
#define _S1_ 1
#define _S2_ 17
#define _S3_ 18

// -------------------------------------------------------------------------------------------------------------------

//ALPHASENSE ------------------------------------------------------------------------------------------------------------
// NO2 -> ---
AlphasenseSensorParam param5 = {"NO2", NO2B43F_n, -0.73, 222, 212, -424, 0.31, 230, 220, 0};
Alphasense_NO2 no2(param5);

// COB4 -> 354
AlphasenseSensorParam param1 = {"CO-B4", COB4_n, 0.8, 330, 316, 510, 0.408, 336, 321, 0};
Alphasense_COB4 cob4_s1(param1);

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
//ALPHASENSE ------------------------------------------------------------------------------------------------------------

//MUX -------------------------------------------------------------------------------------------------------------------
//enum {S0 = 2, S1 = 1, S2 = 17, S3 = 18}; // MUX PINS
gpio_num_t pins[] = {GPIO_NUM_2, GPIO_NUM_1, GPIO_NUM_17, GPIO_NUM_18};
mux Mux(pins, 4);
//MUX -------------------------------------------------------------------------------------------------------------------

//ADS -------------------------------------------------------------------------------------------------------------------
Adafruit_ADS1115 ads;

void ads_setup()
{
  // Configure gain for ADS1115
  ads.setGain(GAIN_TWOTHIRDS);

  // Initialize ADS1115 and check for initialization success
  if (!ads.begin(0x48)) { Serial.println("Failed to initialize ADS1115."); } 
  else { Serial.println("ADS1115 initialized successfully."); }
  delay(100);
}

float ads_read(int mux_output, bool print)
{
  uint16_t adc = 0;
  float readed_voltage;
  Mux.selectOutput(mux_output);                         //LEITURA
  //delay(2000);
  adc = ads.readADC_SingleEnded(1);
  readed_voltage = ads.computeVolts(adc);
  //delay(741);
  ets_delay_us(10);
  if(print)
    printf("\nReaded voltage on port %i: %f \n", (int)mux_output, readed_voltage);
  return readed_voltage;
}
//ADS -------------------------------------------------------------------------------------------------------------------

//SHT -------------------------------------------------------------------------------------------------------------------
Adafruit_SHT31 sht = Adafruit_SHT31();
int16_t temp = 0, umid = 0;

void sht_setup()
{
  Serial.println("SHT31 test");
  if (! sht.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }
  Serial.println("Found SHT31 sensor");
  delay(100);
}

float sht_read(bool temp_humd) // true for temperature, false for humidity
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

void sht_print()
{
  Serial.print("Temp: ");  Serial.println(sht_read(true));
  Serial.print("Umid: ");  Serial.println(sht_read(false));
}
//SHT -------------------------------------------------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);
  delay(100);
  Wire.begin(_SDA_, _SCL_, 100000);
  delay(5000);

  sht_setup();
  ads_setup();
}

float no2_ppb[4]; 
float co_ppb[4];
uint16_t adc;

void loop()
{
  float readed_voltage_we;
  float readed_voltage_ae;

  Serial.println("========= NO2 =========");
  readed_voltage_we = ads_read(0, true); //lendo mux porta 0
  readed_voltage_ae = ads_read(1, true); //lendo mux porta 1
  //readed_voltage_we = 0.218; //lido por multímetro
  //readed_voltage_ae = 0.187; //lido por multímetro

  //Mux.selectOutput(0);                                          // ADS READ
  //adc = ads.readADC_SingleEnded(0);
  //readed_voltage_we = ads.computeVolts(adc);
  //ets_delay_us(10);
  //printf("\nReaded voltage on port 0: %f \n", readed_voltage_we);
  //delay(741);
  //Mux.selectOutput(1);
  //adc = ads.readADC_SingleEnded(0);
  //readed_voltage_ae = ads.computeVolts(adc);
  //ets_delay_us(10);
  //printf("\nReaded voltage on port 1: %f \n", readed_voltage_ae);


  float temp = sht_read(true);
  no2.fourAlgorithms(1000*readed_voltage_we, 1000*readed_voltage_ae, no2_ppb, temp);
  bestNO2Value = getBestNO2Value(no2_ppb);

  sht_print();
  Serial.print("NO2 [0]: ");   Serial.println(no2_ppb[0]);
  Serial.print("NO2 [1]: ");   Serial.println(no2_ppb[1]);
  Serial.print("NO2 [2]: ");   Serial.println(no2_ppb[2]);
  Serial.print("NO2 [3]: ");   Serial.println(no2_ppb[3]);
  Serial.print("best NO2: ");   Serial.println(bestNO2Value); Serial.println(" ");

  Serial.println("========= CO =========");
  readed_voltage_we = ads_read(2, true);
  readed_voltage_ae = ads_read(3, true);
  //readed_voltage_we = 0.556; //lido por multímetro
  //readed_voltage_ae = 0.336; //lido por multímetro
  temp = sht_read(true);
  cob4_s1.fourAlgorithms(1000*readed_voltage_we, 1000*readed_voltage_ae, co_ppb, temp);

  sht_print();
  Serial.print("COB4 [0]: ");   Serial.println(co_ppb[0]);
  Serial.print("COB4 [1]: ");   Serial.println(co_ppb[1]);
  Serial.print("COB4 [2]: ");   Serial.println(co_ppb[2]);
  Serial.print("COB4 [3]: ");   Serial.println(co_ppb[3]); Serial.println(" ");

  delay(5000);

}

/*void loop()
{
  ads_read(2, true);
  delay(1000);
}*/