#include <OneWire.h>

// IO pin definitions
#define ONE_WIRE_PIN      10
#define RELAY_PIN         13
#define MOTOR_ENABLE       9
#define MOTOR_INPUT        8

// Serial settings
#define BAUD_RATE         9600

// System states
#define SCALD       0
#define COOL        1
#define DISPENSE    2
#define INOCULATE   3
#define DONE        4

// Yogurt parameters
#define SCALD_TEMP_F          180.0
#define DISPENSE_TIME_MIN     2.0
#define INOCULATE_TEMP_F      110.0
#define INOCULATE_HYST_F      0.5
#define INOCULATE_TIME_HR     12.0 

// Temperature sensor parameters
#define ERROR_TEMP_C      10000.0  // defined large enough to ensure the heater will be disabled in case of an error

OneWire  ds(ONE_WIRE_PIN);

// Persistent variables
int State;
unsigned long inoculateStart;
unsigned long dispenseStart;

// the setup function runs once when you press reset or power the board
void setup() {
  
  // Initialize pin modes
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MOTOR_ENABLE, OUTPUT);
  pinMode(MOTOR_INPUT, OUTPUT);

  // Initialize serial port
  Serial.begin(BAUD_RATE);

  // Initialize yogurt state machine
  State=SCALD;

  // Initialize pin states
  enable_heater();
  motor_off();
}

void loop(void) {
  float inoculateDurationHr;
  float dispenseDurationMin;
  float temp_f;

  temp_f = get_temp_f();
  Serial.print("Temperature: ");
  Serial.print(temp_f);
  Serial.print(" (degF), State=");
  
  if (State==SCALD){
    Serial.println("SCALD");
    if (temp_f >= SCALD_TEMP_F){
      disable_heater();
      State=COOL;
    }
  } else if (State==COOL) {
    Serial.println("COOL");
    if (temp_f <= INOCULATE_TEMP_F){
      dispenseStart = millis();
      motor_on(); 
      State=DISPENSE;
    }
  } else if (State==DISPENSE) {
    dispenseDurationMin = ((float)(millis()-dispenseStart))/60000.0;
    Serial.print("DISPENSE, Duration=");
    Serial.print(dispenseDurationMin);
    Serial.println(" (min)");
    if (dispenseDurationMin >= DISPENSE_TIME_MIN) {
      motor_off();
      inoculateStart = millis();
      State=INOCULATE;
    }
  } else if (State==INOCULATE) {
    inoculateDurationHr = ((float)(millis()-inoculateStart))/3600000.0;
    Serial.print("INOCULATE, Duration=");
    Serial.print(inoculateDurationHr);
    Serial.println(" (hr)");
    if (inoculateDurationHr < INOCULATE_TIME_HR){
      if (temp_f > (INOCULATE_TEMP_F+INOCULATE_HYST_F)){
        disable_heater();
      } else if (temp_f < (INOCULATE_TEMP_F-INOCULATE_HYST_F)){
        enable_heater();
      }
    } else {
      disable_heater();
      State=DONE;      
    }
  } else {
    Serial.println("DONE");
    State=DONE;
  }
}

void enable_heater(){
  digitalWrite(RELAY_PIN, HIGH);
}

void disable_heater(){
  digitalWrite(RELAY_PIN, LOW);
}

void motor_on(){
  digitalWrite(MOTOR_ENABLE, HIGH);
  digitalWrite(MOTOR_INPUT, HIGH);
}

void motor_off(){
  digitalWrite(MOTOR_ENABLE, LOW);
  digitalWrite(MOTOR_INPUT, LOW);
}

//
// Code below adapted from OneWire example code DS18x20_Temperature
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//

float get_temp_f(){
  float fahrenheit;
  
  fahrenheit = get_temp_c() * 1.8 + 32.0;
  return fahrenheit;
}

float get_temp_c(){
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius;
  
  if (!ds.search(addr)) {
    Serial.println("No more addresses.");
    ds.reset_search();
    return ERROR_TEMP_C;
  } else {
    ds.reset_search();
  }
  
  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return ERROR_TEMP_C;
  }
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return ERROR_TEMP_C;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  
  celsius = (float)raw / 16.0;
  
  return celsius;
}
