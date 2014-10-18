#ifndef _24C64_H_
#define _24C64_H_

#define FALSE 0
#define TRUE 1

void EEOpen();
uint8_t EEWriteByte(uint16_t,uint8_t);
uint8_t EEReadByte(uint16_t address);

#endif
