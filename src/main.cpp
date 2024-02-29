#include <iostream>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>               
#include <FS.h>        

#include <lmic.h>
#include <hal/hal.h>

#include <Adafruit_ADS1X15.h>
#include <Adafruit_SHT31.h>
#include <RTClib.h>

#include "mux.hpp"
#include "aqm_envcity_config.h"
#include "Alphasense_GasSensors.hpp"

// CONFIGS --------------------------------------------------------------------------------------------------------------

/*ordem sensores
CO
NO2
OX
SO2
*/

//Serial
#define _SERIAL_BAUND_ 115200

//SD SPI
#define _SD_MOSI_ 7
#define _SD_MISO_ 5
#define _SD_SCK_ 6
#define _SD_CS_ 15

//RFM SPI
#define _RFM_NSS_ 36
#define _RFM_RST_ 14
#define _RFM_DIO0_ 40
#define _RFM_DIO1_ 41
#define _RFM_DIO2_ 42

//chaves de autenticação OTAA
#define _APPEUI_KEY_ 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12                                                          //lsb
#define _DEVEUI_KEY_ 0xC8, 0x56, 0x06, 0xD0, 0x7E, 0xD5, 0xB3, 0x70                                                         //lsb
#define _APPKEY_KEY_ 0xA5, 0x8B, 0xEC, 0x77, 0x06, 0xCE, 0x14, 0x05, 0x6E, 0xFF, 0xFC, 0x05, 0xAD, 0x71, 0xF2, 0xAB          //msb

//Tempos em segundos
#define _INTERVALO_ENVIO_ 180 
//#define _INTERVALO_LEITURA_ 50

//I2C
#define _SDA_ 10
#define _SCL_ 11

//Sensor analog pins
enum sensor_analog_pins{
  CO_WE_PIN,CO_AE_PIN,
  NO2_WE_PIN, NO2_AE_PIN, 
  OX_WE_PIN, OX_AE_PIN, 
  SO2_WE_PIN, SO2_AE_PIN,
  TOTAL_ANALOG_PINS,
};

enum data{
  DAY, MONTH, YEAR,
  HOUR, MIN, SEC,
  TOTAL_DATA
};

typedef struct _sensors_readings{
  float co_ppb[4];
  //float nh3_ppb[4];
  float no2_ppb[4];
  float no2_best_value;
  //float so2_ppb[4];
  float ox_ppb[4];
  float so2_ppb[4];
  //float h2s_ppb;
  float analog[TOTAL_ANALOG_PINS];
  uint8_t data[TOTAL_DATA]; // dia/mes/ano/hora/minuto/segundo

  float temp;            // 28, 29, 30, 31
  float humidity;        // 32,
} SensorsReadings;

SensorsReadings readings;

//unsigned long tempo_zero = millis();
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

//AlphasenseSensorParam param2 = {"NH3-B1", COB4_n, 0.8, 775, 277, 59, 0.047, 277, 278, 0};
//Alphasense_NH3 nh3(param2);

AlphasenseSensorParam param6 = {"SO2", SO2B4_n, 0.8, 361, 350, 363, 0.29, 335, 343, 0};
Alphasense_SO2 so2(param6);

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

//RTC -------------------------------------------------------------------------------------------------------------------
RTC_DS1307 rtc;

void rtc_setup()
{
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
}

void rtc_read()
{
  DateTime now = rtc.now();

  readings.data[DAY] = now.day();
  readings.data[MONTH] = now.month();
  readings.data[YEAR] = now.year() % 100;
  readings.data[HOUR] = now.hour();
  readings.data[MIN] = now.minute();
  readings.data[SEC] = now.second();
}

//RTC -------------------------------------------------------------------------------------------------------------------

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
    writeFile(SD, "/data.csv", "DATA; HORA; TEMPERATURA; HUMIDADE; CO_PPB; CO_WE; CO_AE; NO2_PPB; NO2_WE; NO2_AE; OX_PPB; OX_WE; OX_AE; NH3_PPB; NH3_WE; NH3_AE; \r\n");
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

void print_sensors()
{
  printf("%d/%d/%d %d:%d:%d\n", readings.data[DAY], readings.data[MONTH], readings.data[YEAR], readings.data[HOUR], readings.data[MIN], readings.data[SEC]);
  printf("Temperatura: %.2f \nHumidade: %.2f\n", readings.temp, readings.humidity);
  printf("CO_PPB: %.5f\nCO_WE: %.5f\nCO_AE: %.5f\n", readings.co_ppb[0], readings.analog[CO_WE_PIN], readings.analog[CO_AE_PIN]);
  printf("NO2_PPB: %.5f\nNO2_WE: %.5f\nNO2_AE: %.5f\n", readings.no2_best_value, readings.analog[NO2_WE_PIN], readings.analog[NO2_AE_PIN]);
  printf("OX_PPB: %.5f\nOX_WE: %.5f\nOX_AE: %.5f\n", readings.ox_ppb[0], readings.analog[OX_WE_PIN], readings.analog[OX_AE_PIN]);
  printf("SO2_PPB: %.5f\nNH3_WE: %.5f\nSO2_AE: %.5f\n\n", readings.so2_ppb[0], readings.analog[SO2_WE_PIN], readings.analog[SO2_AE_PIN]);
}

//Contrução de pacote dos dados para anexação no SD
void build_packet_to_SD(bool print)
{
  String leitura;
  leitura.concat(String(readings.data[DAY])); leitura.concat("/");
  leitura.concat(String(readings.data[MONTH])); leitura.concat("/");
  leitura.concat(String(readings.data[YEAR])); leitura.concat(";");
  leitura.concat(String(readings.data[HOUR])); leitura.concat(":");
  leitura.concat(String(readings.data[MIN])); leitura.concat(":");
  leitura.concat(String(readings.data[SEC])); leitura.concat(";");
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
  leitura.concat(String(readings.so2_ppb[0], 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[SO2_WE_PIN], 5)); leitura.concat(";");
  leitura.concat(String(readings.analog[SO2_AE_PIN], 5)); leitura.concat("; \r\n");
  
  if(print)
    Serial.println(leitura);
  appendFile(SD, "/data.csv", leitura.c_str());
}

bool flag_reading = true;

void read_sensors(bool print)
{
  //leitura de todos os pinos analógicos
  ads_all_read(readings.analog);
  rtc_read();

  //temperatura e humidade
  readings.temp = sht_read(true);
  readings.humidity = sht_read(false);
  cob4_s1.fourAlgorithms(1000*readings.analog[CO_WE_PIN], 1000*readings.analog[CO_AE_PIN], readings.co_ppb, readings.temp);
  no2.fourAlgorithms(1000*readings.analog[NO2_WE_PIN], 1000*readings.analog[NO2_AE_PIN], readings.no2_ppb, readings.temp);
  readings.no2_best_value = getBestNO2Value(readings.no2_ppb);
  ox.fourAlgorithms(1000*readings.analog[OX_WE_PIN], 1000*readings.analog[OX_AE_PIN], readings.ox_ppb, readings.no2_best_value, readings.temp);
  so2.fourAlgorithms(1000*readings.analog[SO2_WE_PIN], 1000*readings.analog[SO2_AE_PIN], readings.so2_ppb ,readings.temp);

  build_packet_to_SD(print);
  if(print)
    print_sensors();
}

void build_packet(uint8_t packet[27])
{
  //rtc_read(true);
  read_sensors(true);

  uint16_t aux = readings.temp*100;
  packet[0] = (aux >> 8) & 0xFF;
  packet[1] = aux & 0xFF;

  aux = readings.humidity*100;
  packet[2] = (aux >> 8) & 0xFF;
  packet[3] = aux & 0xFF;

  aux = readings.analog[CO_WE_PIN]*100000;
  packet[4] = (aux >> 8) & 0xFF;
  packet[5] = aux & 0xFF;
  aux = readings.analog[CO_AE_PIN]*100000;
  packet[6] = (aux >> 8) & 0xFF;
  packet[7] = aux & 0xFF;

  aux = readings.analog[NO2_WE_PIN]*100000;
  packet[8] = (aux >> 8) & 0xFF;
  packet[9] = aux & 0xFF;
  aux = readings.analog[NO2_AE_PIN]*100000;
  packet[10] = (aux >> 8) & 0xFF;
  packet[11] = aux & 0xFF;

  aux = readings.analog[OX_WE_PIN]*100000;
  packet[12] = (aux >> 8) & 0xFF;
  packet[13] = aux & 0xFF;
  aux = readings.analog[OX_AE_PIN]*100000;
  packet[14] = (aux >> 8) & 0xFF;
  packet[15] = aux & 0xFF;

  aux = readings.analog[SO2_WE_PIN]*100000;
  packet[16] = (aux >> 8) & 0xFF;
  packet[17] = aux & 0xFF;
  aux = readings.analog[SO2_AE_PIN]*100000;
  packet[18] = (aux >> 8) & 0xFF;
  packet[19] = aux & 0xFF;

  packet[20] = readings.data[DAY];
  packet[21] = readings.data[MONTH];
  packet[22] = readings.data[YEAR];
  packet[23] = readings.data[HOUR];
  packet[24] = readings.data[MIN];
  packet[25] = readings.data[SEC];
}

/*void reading_loop(bool print)
{
  if (((millis() - tempo_zero) < 200) && flag_reading)
  {
    read_sensors(print);
    flag_reading = false;
  }
  if ((millis() - tempo_zero) > (200 + (_INTERVALO_LEITURA_*1000)))
  {
    tempo_zero = millis();
    flag_reading = true;
  }
}*/
//PACK ------------------------------------------------------------------------------------------------------------------

// LMIC -----------------------------------------------------------------------------------------------------------------
static const u1_t PROGMEM APPEUI[8] = { _APPEUI_KEY_ }; //lsb format
void os_getArtEui(u1_t *buf) { memcpy_P(buf, APPEUI, 8); }
static const u1_t PROGMEM DEVEUI[8]  = { _DEVEUI_KEY_ }; // lsb format
void os_getDevEui(u1_t *buf) { memcpy_P(buf, DEVEUI, 8); }
static const u1_t PROGMEM APPKEY[16] = { _APPKEY_KEY_ }; //msb format
void os_getDevKey(u1_t *buf) { memcpy_P(buf, APPKEY, 16); }

static osjob_t sendjob;
void do_send(osjob_t *j);

static uint8_t payload[27];

const unsigned TX_INTERVAL = _INTERVALO_ENVIO_;

const lmic_pinmap lmic_pins = {
    .nss = _RFM_NSS_,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = _RFM_RST_,
    .dio = {_RFM_DIO0_, _RFM_DIO1_, _RFM_DIO2_},
};

void onEvent(ev_t ev)
{
    Serial.print(os_getTime());
    Serial.print(": ");
    switch (ev)
    {
    case EV_SCAN_TIMEOUT:
        Serial.println(F("EV_SCAN_TIMEOUT"));
        break;
    case EV_BEACON_FOUND:
        Serial.println(F("EV_BEACON_FOUND"));
        break;
    case EV_BEACON_MISSED:
        Serial.println(F("EV_BEACON_MISSED"));
        break;
    case EV_BEACON_TRACKED:
        Serial.println(F("EV_BEACON_TRACKED"));
        break;
    case EV_JOINING:
        Serial.println(F("EV_JOINING"));
        break;
    case EV_JOINED:
        Serial.println(F("EV_JOINED"));
        break;
    case EV_JOIN_FAILED:
        Serial.println(F("EV_JOIN_FAILED"));
        LMIC_setLinkCheckMode(0);
        break;
    case EV_REJOIN_FAILED:
        Serial.println(F("EV_REJOIN_FAILED"));
        break;
    case EV_TXCOMPLETE:
        Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
        if (LMIC.txrxFlags & TXRX_ACK)
            Serial.println(F("Received ack"));
        if (LMIC.dataLen)
        {
            Serial.print(F("Received "));
            Serial.print(LMIC.dataLen);
            Serial.println(F(" bytes of payload"));
        }
        if (LMIC.dataLen == 1) 
        {
            uint8_t dados_recebidos = LMIC.frame[LMIC.dataBeg + 0];
            Serial.print(F("Dados recebidos: "));
            Serial.write(dados_recebidos);
        }
        // Agenda a transmissão automática com intervalo de TX_INTERVAL
        os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
        break;
    case EV_LOST_TSYNC:
        Serial.println(F("EV_LOST_TSYNC"));
        break;
    case EV_RESET:
        Serial.println(F("EV_RESET"));
        break;
    case EV_RXCOMPLETE:
        Serial.println(F("EV_RXCOMPLETE"));
        break;
    case EV_LINK_DEAD:
        Serial.println(F("EV_LINK_DEAD"));
        break;
    case EV_LINK_ALIVE:
        Serial.println(F("EV_LINK_ALIVE"));
        break;
    case EV_TXSTART:
        Serial.println(F("EV_TXSTART"));
        break;
    case EV_TXCANCELED:
        Serial.println(F("EV_TXCANCELED"));
        break;
    case EV_RXSTART:
        break;
    case EV_JOIN_TXCOMPLETE:
        Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
        break;
    default:
        Serial.print(F("Unknown event: "));
        Serial.println((unsigned)ev);
        break;
    }
}

void do_send(osjob_t *j)
{
    // Verifica se não está ocorrendo uma transmissão no momento TX/RX
    if (LMIC.opmode & OP_TXRXPEND){
        Serial.println(F("OP_TXRXPEND, not sending"));
    }
    else{
        //envio
        build_packet(payload);
        LMIC_setTxData2(1, payload, sizeof(payload)-1, 0);
        Serial.println(F("Sended"));
    }
}
// LMIC -----------------------------------------------------------------------------------------------------------------

void setup()
{
  Serial.begin(_SERIAL_BAUND_);
  delay(100);
  Wire.begin(_SDA_, _SCL_, 100000);

  
  ads_setup();
  setup_sd();
  sht_setup();
  rtc_setup();


  os_init();
  LMIC_reset();
  do_send(&sendjob);//Start
  
}

void loop()
{
  os_runloop_once();
}