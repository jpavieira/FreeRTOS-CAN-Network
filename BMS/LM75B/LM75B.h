#ifndef LM75B_H
#define LM75B_H

#include "mbed.h"

// LM75B Adresse
#define LM75B_ADDR 0x90

// LM75B Register
#define LM75B_CONF 0x01
#define LM75B_TEMP 0x00

class LM75B
{
public:
  LM75B(PinName sda, PinName scl);  // I2C Pins übergeben p28, p27
  ~LM75B();
  float read(); 
  
private:
  char cmd[2];
  I2C i2c;
};
#endif