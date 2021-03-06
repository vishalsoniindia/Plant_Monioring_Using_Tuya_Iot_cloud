/*
 * Author :- Vishal soni
 * Youtube :- http://youtube.com/vishalsoniindia 
 * @Github:https://github.com/tuya/tuya-wifi-mcu-sdk-arduino-library
 */
#include <Wire.h>                     //wire library for I2c
#include <TuyaWifi.h>                 //tuya library

TuyaWifi my_device;                  //create a object for tuya class

/* Current LED status */
unsigned char led_state = 0;          //led for wifi config
/* Connect network button pin */
int wifi_key_pin = 7;                 //wifi config button

int hulk_moisture_pin = A0;           //hulk's moisture pin
int thor_moisture_pin = A1;           //thor's moisture pin

/* SHT30 */
#define SHT30_I2C_ADDR 0x44          //address of temp & humadity sensor

/* Data point define */
#define DPID_TEMP_CURRENT     1     //data point for temperature
#define DPID_HUMIDITY_CURRENT 2     //data point for humidity
#define DPID_HULK_MOISTURE    101   //data point for hulk_moisture
#define DPID_THOR_MOISTURE    102   //data point for Thor_moisture

/* Current device DP values */
int temperature = 0;                //variable to store temperature
int humidity = 0;                   //variable to store humidity
int hulk_moisture = 0;              //variable to store hulk_moisture
int thor_moisture = 0;              //variable to store Thor_moisture

/* Stores all DPs and their types. PS: array[][0]:dpid, array[][1]:dp type. 
 *                                     dp type(TuyaDefs.h) : DP_TYPE_RAW, DP_TYPE_BOOL, DP_TYPE_VALUE, DP_TYPE_STRING, DP_TYPE_ENUM, DP_TYPE_BITMAP
*/
unsigned char dp_array[][2] =
{
  {DPID_TEMP_CURRENT, DP_TYPE_VALUE},         //data point type
  {DPID_HUMIDITY_CURRENT, DP_TYPE_VALUE},     //data point type
  {DPID_HULK_MOISTURE, DP_TYPE_VALUE},        //data point type
  {DPID_THOR_MOISTURE, DP_TYPE_VALUE},        //data point type
};

unsigned char pid[] = {"2myq644psuemw5rg"};   //PID of your tuya product
unsigned char mcu_ver[] = {"1.0.10"};         //version of Tuya board firmware

/* last time */
unsigned long last_time = 0;                  //to store milis

void setup()
{
  Serial.begin(9600);                         //begin serial communication

  // Initialise I2C communication as MASTER
  Wire.begin();                               //begin I2c communication

  //Initialize led port, turn off led.
  pinMode(LED_BUILTIN, OUTPUT);               //define led as output
  digitalWrite(LED_BUILTIN, LOW);             //make led low
  //Initialize networking keys.
  pinMode(wifi_key_pin, INPUT_PULLUP);        //define pin as pullup
  pinMode(hulk_moisture_pin,INPUT);
  pinMode(thor_moisture_pin,INPUT);

  my_device.init(pid, mcu_ver);
  //incoming all DPs and their types array, DP numbers
  my_device.set_dp_cmd_total(dp_array, 4);
  //register DP download processing callback function
  my_device.dp_process_func_register(dp_process);
  //register upload all DP callback function
  my_device.dp_update_all_func_register(dp_update_all);

  soil_moisture();

  delay(300);
  last_time = millis();
}

void loop()
{
  my_device.uart_service();                         //start tuya CBU Board server
  
  //Enter the connection network mode when Pin7 is pressed.
  if (digitalRead(wifi_key_pin) == LOW) {      
    delay(80);
    if (digitalRead(wifi_key_pin) == LOW) {
      my_device.mcu_set_wifi_mode(SMART_CONFIG);
    }
  }
  /* LED blinks when network is being connected */
  if ((my_device.mcu_get_wifi_work_state() != WIFI_LOW_POWER) && (my_device.mcu_get_wifi_work_state() != WIFI_CONN_CLOUD) && (my_device.mcu_get_wifi_work_state() != WIFI_SATE_UNKNOW)) {
    if (millis()- last_time >= 500) {
      last_time = millis();

      if (led_state == LOW) {
        led_state = HIGH;
      } else {
        led_state = LOW;
      }

      digitalWrite(LED_BUILTIN, led_state);
    }
  }

  /* get the temperature and humidity */
  get_sht30_value(&temperature, &humidity);
  
  soil_moisture();

  Serial.print("temperature :- ");       //print on serial for debuging
  Serial.println(temperature);
  Serial.print("humidity :- ");
  Serial.println(humidity);
  Serial.print("Hulk's Moisture:- ");       //print on serial for debuging
  Serial.println(analogRead(hulk_moisture_pin));
  Serial.print("Thor's Moisture:- ");
  Serial.println(analogRead(thor_moisture_pin));

  /* report the temperature and humidity */
  if ((my_device.mcu_get_wifi_work_state() == WIFI_CONNECTED) || (my_device.mcu_get_wifi_work_state() == WIFI_CONN_CLOUD)) {
    my_device.mcu_dp_update(DPID_TEMP_CURRENT, temperature, 1);
    my_device.mcu_dp_update(DPID_HUMIDITY_CURRENT, humidity, 1);
    my_device.mcu_dp_update(DPID_HULK_MOISTURE, hulk_moisture, 1);
    my_device.mcu_dp_update(DPID_THOR_MOISTURE, thor_moisture, 1);

  }

  delay(1000);
}

void get_sht30_value(int *temp_value, int *humi_value)
{
  unsigned char i2c_data[6];

  // Start I2C Transmission
  Wire.beginTransmission(SHT30_I2C_ADDR);
  // Send measurement command
  Wire.write(0x2C);
  Wire.write(0x06);
  // Stop I2C transmission
  Wire.endTransmission();
  delay(500);

  // Request 6 bytes of data
  Wire.requestFrom(SHT30_I2C_ADDR, 6);

  // Read 6 bytes of i2c_data
  // temperature msb, temperature lsb, temperature crc, humidity msb, humidity lsb, humidity crc
  if (Wire.available() == 6) {
    for (int i = 0; i < 6 ; i++) {
      i2c_data[i] = Wire.read();
    }
    
    if ((sht30_crc(i2c_data, 2) == i2c_data[2]) && (sht30_crc(i2c_data+3, 2) == i2c_data[5])) {/* crc success */
      *temp_value = ((((((i2c_data[0] * 256.0) + i2c_data[1]) * 175) / 65535.0) - 45) * 100)/10;
      *humi_value = (((((i2c_data[3] * 256.0) + i2c_data[4]) * 100) / 65535.0) * 100)/100;
    } else {
      *temp_value = 0;
      *humi_value = 0;
    }
  }
}

/**
 * @description: check sht30 temperature and humidity data
 * @param {unsigned char} *data
 * @param {unsigned int} count
 * @return {*}
 */
unsigned char sht30_crc(unsigned char *data, unsigned int count)
{
    unsigned char crc = 0xff;
    unsigned char current_byte;
    unsigned char crc_bit;

    /* calculates 8-Bit checksum with given polynomial */
    for (current_byte = 0; current_byte < count; ++current_byte)
    {
        crc ^= (data[current_byte]);
        for (crc_bit = 8; crc_bit > 0; --crc_bit)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}

/**
 * @description: DP download callback function.
 * @param {unsigned char} dpid
 * @param {const unsigned char} value
 * @param {unsigned short} length
 * @return {unsigned char}
 */
unsigned char dp_process(unsigned char dpid,const unsigned char value[], unsigned short length)
{
  /* all DP only report */
  return SUCCESS;
}

/**
 * @description: Upload all DP status of the current device.
 * @param {*}
 * @return {*}
 */
void dp_update_all(void)
{
  my_device.mcu_dp_update(DPID_TEMP_CURRENT, temperature, 1);
  my_device.mcu_dp_update(DPID_HUMIDITY_CURRENT, humidity, 1);
  my_device.mcu_dp_update(DPID_HULK_MOISTURE, hulk_moisture, 1);
  my_device.mcu_dp_update(DPID_THOR_MOISTURE, thor_moisture, 1);
}

void soil_moisture(){
 hulk_moisture = map(analogRead(hulk_moisture_pin),300,800,100,0);
 thor_moisture = map(analogRead(thor_moisture_pin),300,800,100,0);

 if(hulk_moisture < 0){
  hulk_moisture = 0;
 }
 if(hulk_moisture > 100){
  hulk_moisture = 100;
 }

 if(thor_moisture < 0){
  thor_moisture = 0;
 }
 if(thor_moisture > 100){
  thor_moisture = 100;
 }
}
