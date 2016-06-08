#include "LM75B.h"

LM75B :: LM75B(PinName sda, PinName scl) : i2c(sda, scl)
{
    cmd[0] = LM75B_CONF;            // Pointerregister
    cmd[1] = 0x0;                   // Default siehe 7.4.2 Tabelle 8
    i2c.write(LM75B_ADDR, cmd, 2);  // Adr., char *, Länge   
}

float LM75B :: read()
{
    cmd[0] = LM75B_TEMP;
    
    i2c.write(LM75B_ADDR, cmd, 1);  // sende Temperatur Befehl
    i2c.read(LM75B_ADDR, cmd, 2);   // bekommmen den command string
    return (float ((cmd[0] << 8) | cmd[1])/256.0);    
}

LM75B :: ~LM75B() {}
