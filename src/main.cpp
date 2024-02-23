#include <iostream>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>               
#include <FS.h>               

#include <Adafruit_ADS1X15.h>
#include <Adafruit_SHT31.h>

#include "mux.hpp"
#include "aqm_envcity_config.h"
#include "Alphasense_GasSensors.hpp"

// CONFIGS --------------------------------------------------------------------------------------------------------------

//SD SPI
#define _SD_MOSI_ 7
#define _SD_MISO_ 5
#define _SD_SCK_ 6
#define _SD_CS_ 15

//I2C
#define _SDA_ 10
#define _SCL_ 11

//Sensor analog pins
enum sensor_analog_pins{
  CO_WE_PIN,CO_AE_PIN,
  NO2_WE_PIN, NO2_AE_PIN, 
  OX_WE_PIN, OX_AE_PIN, 
  NH3_WE_PIN, NH3_AE_PIN,
  TOTAL_ANALOG_PINS,
};


typedef struct _sensors_readings{
  float co_ppb[4];
  float nh3_ppb[4];
  float no2_ppb[4];
  float no2_best_value;
  //float so2_ppb[4];
  float ox_ppb[4];
  //float h2s_ppb;
  float analog[TOTAL_ANALOG_PINS];

  float temp;            // 28, 29, 30, 31
  float humidity;        // 32,
} SensorsReadings;

SensorsReadings readings;

// CONFIGS --------------------------------------------------------------------------------------------------------------

//ALPHASENSE ------------------------------------------------------------------------------------------------------------
// NO2 -> ---
AlphasenseSensorParam param5 = {"NO2", NO2B43F_n, -0.73, 222, 212, -424, 0.31, 230, 220, 0};
Alphasense_NO2 no2(param5);

// COB4 -> 354
AlphasenseSensorParam param1 = {"CO-B4", COB4_n, 0.8, 330, 316, 510, 0.408, 336, 321, 0};
Alphasense_COB4 cob4_s1(param1);

// OX -> ---
AlphasenseSensorParam param4 = {"0X", OXB431_n, -0.73, 229, 234, -506, 0.369, 237, 242, -587};
Alphasense_OX ox(param4);

AlphasenseSensorParam param2 = {"NH3-B1", COB4_n, 0.8, 775, 277, 59, 0.047, 277, 278, 0};
Alphasense_NH3 nh3(param2);

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
//ALPHASENSE ------------------------------------------------------------------------------------------------------------

//MUX -------------------------------------------------------------------------------------------------------------------
//enum {S0 = 2, S1 = 1, S2 = 17, S3 = 18}; // MUX PINS
gpio_num_t pins[] = {GPIO_NUM_2, GPIO_NUM_1, GPIO_NUM_17, GPIO_NUM_18};
mux Mux(pins, 4);
//MUX -------------------------------------------------------------------------------------------------------------------

//ADS -------------------------------------------------------------------------------------------------------------------
Adafruit_ADS1115 ads;
uint16_t adc = 0;

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
  float readed_voltage;
  Mux.selectOutput(mux_output);                         //LEITURA
  //delay(2000);
  adc = ads.readADC_SingleEnded(1);                     //pin aio1
  readed_voltage = ads.computeVolts(adc);
  //delay(741);
  ets_delay_us(10);
  if(print)
    printf("\nReaded voltage on port %i: %f \n", (int)mux_output, readed_voltage);
  return readed_voltage;
}

void ads_all_read(float analog[TOTAL_ANALOG_PINS])
{
  for (uint8_t i = 0; i < TOTAL_ANALOG_PINS; i++)
  {
    Mux.selectOutput(i);
    delayMicroseconds(5);
    adc = ads.readADC_SingleEnded(1); 
    analog[i] = ads.computeVolts(adc);
  }
}
//ADS -------------------------------------------------------------------------------------------------------------------

//SD --------------------------------------------------------------------------------------------------------------------
SPIClass mySPI = SPIClass(HSPI); //SPI virtual

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  digitalWrite(_SD_CS_, HIGH);
  digitalWrite(18, LOW);
  Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
  digitalWrite(18, HIGH);
  digitalWrite(_SD_CS_, LOW);
}

void setup_sd() 
{
  mySPI.begin(_SD_SCK_, _SD_MISO_, _SD_MOSI_);
  if(!SD.begin(_SD_CS_, mySPI, 10000000))
  {
    Serial.println("Erro na leitura do arquivo não existe um cartão SD ou o módulo está conectado incorretamente...");
    return;
  }

  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("Nenhum cartao SD encontrado");
    return;
  }

  Serial.println("Inicializando cartao SD...");
  //if (!SD.begin(SD_CS, spi1)) 
  if (!SD.begin(_SD_CS_)) 
  {
    Serial.println("ERRO - SD nao inicializado!");
    return; 
  }
  File file = SD.open("/data.csv");
  if(!file) 
  {
    Serial.println("SD: arquivo data.csv nao existe");
    Serial.println("SD: Criando arquivo...");
    writeFile(SD, "/data.csv", "TEMPERATURA; HUMIDADE; CO_PPB; CO_WE; CO_AE; NO2_PPB; NO2_WE; NO2_AE; OX_PPB; OX_WE; OX_AE; NH3_PPB; NH3_WE; NH3_AE; \r\n");
  }
  else {
    Serial.println("SD: arquivo ja existe");  
  }
  file.close();
}
//SD --------------------------------------------------------------------------------------------------------------------


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

//PACK ------------------------------------------------------------------------------------------------------------------
/*
0 - temperatura    2 - co_ppb    5 - no2_ppb    8 - ox_ppb   11 - am_ppb
1 - humidade       3 - co_we     6 - no2_we     9 - ox_we    12 - am_we
                   4 - co_ae     7 - no2_ae    10 - ox_ae    13 - am_ae
*/

void print_packet()
{
  printf("Temperatura: %.2f \nHumidade: %.2f\n", readings.temp, readings.humidity);
  printf("CO_PPB: %.5f\nCO_WE: %.5f\nCO_AE: %.5f\n", readings.co_ppb[0], readings.analog[CO_WE_PIN], readings.analog[CO_AE_PIN]);
  printf("NO2_PPB: %.5f\nNO2_WE: %.5f\nNO2_AE: %.5f\n", readings.no2_best_value, readings.analog[NO2_WE_PIN], readings.analog[NO2_AE_PIN]);
  printf("OX_PPB: %.5f\nOX_WE: %.5f\nOX_AE: %.5f\n", readings.ox_ppb[0], readings.analog[OX_WE_PIN], readings.analog[OX_AE_PIN]);
  printf("NH3_PPB: %.5f\nNH3_WE: %.5f\nNH3_AE: %.5f\n\n", readings.nh3_ppb[0], readings.analog[NH3_WE_PIN], readings.analog[NH3_AE_PIN]);
}

//Contrução de pacote dos dados para anexação no SD
void build_packet_to_SD(bool print)
{
  String leitura;
  leitura.concat(String(readings.temp, 2)); leitura.concat(";");
  leitura.concat(String(readings.humidity, 2)); leitura.concat(";");
  leitura.concat(String(readings.co_ppb[0], 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[CO_WE_PIN], 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[CO_AE_PIN], 5)); leitura.concat(";");
  leitura.concat(String(readings.no2_best_value, 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[NO2_WE_PIN], 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[NO2_AE_PIN], 5)); leitura.concat(";");
  leitura.concat(String(readings.ox_ppb[0], 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[OX_WE_PIN], 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[OX_AE_PIN], 5)); leitura.concat(";");
  leitura.concat(String(readings.nh3_ppb[0], 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[NH3_WE_PIN], 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[NH3_AE_PIN], 5)); leitura.concat("; \r\n");
  
  if(print)
    Serial.println(leitura);
  appendFile(SD, "/data.csv", leitura.c_str());
}

void build_packet(bool print)
{
  //leitura de todos os pinos analógicos
  ads_all_read(readings.analog);

  //temperatura e humidade
  readings.temp = sht_read(true);
  readings.humidity = sht_read(false);
  cob4_s1.fourAlgorithms(1000*readings.analog[CO_WE_PIN], 1000*readings.analog[CO_AE_PIN], readings.co_ppb, readings.temp);
  no2.fourAlgorithms(1000*readings.analog[NO2_WE_PIN], 1000*readings.analog[NO2_AE_PIN], readings.no2_ppb, readings.temp);
  readings.no2_best_value = getBestNO2Value(readings.no2_ppb);
  ox.fourAlgorithms(1000*readings.analog[OX_WE_PIN], 1000*readings.analog[OX_AE_PIN], readings.ox_ppb, readings.no2_best_value, readings.temp);
  nh3.fourAlgorithms(1000*readings.analog[NH3_WE_PIN], 1000*readings.analog[NH3_AE_PIN], readings.nh3_ppb ,readings.temp);

  build_packet_to_SD(print);
  if(print)
    print_packet();
}
//PACK ------------------------------------------------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);
  delay(100);
  Wire.begin(_SDA_, _SCL_, 100000);

  ads_setup();
  setup_sd();
  sht_setup();
}

/*void loop()
{
  float readed_voltage_we;
  float readed_voltage_ae;

  Serial.println("========= NO2 =========");
  readed_voltage_we = ads_read(0, true); //lendo mux porta 0
  readed_voltage_ae = ads_read(1, true); //lendo mux porta 1
  //readed_voltage_we = 0.218; //lido por multímetro
  //readed_voltage_ae = 0.187; //lido por multímetro

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

}*/

void loop()
{
  build_packet(false);
  delay(5000);
}