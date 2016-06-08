#include "ClockControl.h"

void setPLL0Frequency(unsigned char clkSrc, unsigned short cfg_m, unsigned char cfg_n)
{
      LPC_SC->CLKSRCSEL = clkSrc;
      LPC_SC->PLL0CFG   = (((unsigned int)cfg_n-1) << 16) | cfg_m-1;
      LPC_SC->PLL0CON   = 0x01;             
      LPC_SC->PLL0FEED  = 0xAA;
      LPC_SC->PLL0FEED  = 0x55;
      while (!(LPC_SC->PLL0STAT & (1<<26)));
    
      LPC_SC->PLL0CON   = 0x03;
      LPC_SC->PLL0FEED  = 0xAA;
      LPC_SC->PLL0FEED  = 0x55;
}

void setPLL1Frequency(unsigned char clkSrc, unsigned short cfg_m, unsigned char cfg_n)
{
      LPC_SC->CLKSRCSEL = clkSrc;
      LPC_SC->PLL1CFG   = (((unsigned int)cfg_n-1) << 16) | cfg_m-1;
      LPC_SC->PLL1CON   = 0x01;             
      LPC_SC->PLL1FEED  = 0xAA;
      LPC_SC->PLL1FEED  = 0x55;
      while (!(LPC_SC->PLL1STAT & (1<<26)));
    
      LPC_SC->PLL1CON   = 0x03;
      LPC_SC->PLL1FEED  = 0xAA;
      LPC_SC->PLL1FEED  = 0x55;
}

unsigned int setSystemFrequency(unsigned char clkDivider, unsigned char clkSrc, unsigned short cfg_m, unsigned char cfg_n)
{
    setPLL0Frequency(clkSrc, cfg_m, cfg_n);
    LPC_SC->CCLKCFG = clkDivider - 1;
    SystemCoreClockUpdate();
    return SystemCoreClock;
}