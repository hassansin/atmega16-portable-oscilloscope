/* This source has been formatted by an unregistered SourceFormatX */
/* If you want to remove this info, please register this shareware */
/* Please visit http://www.textrush.com to get more information    */

#include "glcd.h"
#include "font3x6.h"
#include <util/delay.h>
#include <string.h>
void glcdInit(void)
  {
  data_port_ddr = 0xff; //Make DATA port output
  ctrl_port_ddr = 1 << lcddi | 1 << lcdrw | 1 << lcde | 1 << CS1 | 1 << CS2 | 1 << lcdrst; //Make CONTROL port outputs
  ctrl_port |= (1 << lcdrst); //set reset pin to high
  setBOTH; //select both chip
  setMode(WriteIns);
  displayOff;
  eStrobe();
  data_port = 0b11000000; //Display start line = 0 (0-63)
  eStrobe();
  data_port = 0b01000000; //Set address = 0 (0-63)
  eStrobe();
  data_port = 0b10111000; //Set page = 0 (0-7)
  eStrobe();
  displayOn;
  eStrobe();
  }
//-------------------------------------------------------------------------
void drawPic(const char *pic, unsigned char x, unsigned char y)
  {
  unsigned int i;
  const char *data = pic;
  column = x;
  line = y;
  gLCDgotoXY(line, column);
  for (i = 0; i < 1024; i++)
    sendData(pgm_read_byte(data++));
  }
//-------------------------------------------------------------------------
void gLCDgotoXY(unsigned char lineData, unsigned char columnData)
  {
  if (columnData < 64)
  // If column is at address 0-63...
    setCS1;
  // ...select the first half of LCD
  else
    {
    // ...or else...
    setCS2; // ...select the second half of LCD.
    columnData -= 64;
    }
  setMode(WriteIns);
  lineData |= 0b10111000;
  data_port = lineData; //Select page (0-7)
  eStrobe();
  columnData |= 0b01000000;
  data_port = columnData; //Set column address (0-63)
  eStrobe();
  }
//-------------------------------------------------------------------------
void fillDataLcdBuffer(unsigned char address, unsigned char data)
  {
  samples[address] = data;
  }
//-------------------------------------------------------------------------
unsigned char readData(void)
  {
  unsigned char data;
  if (column < 64)
    setCS1;
  if (column >= 64)
    gLCDgotoXY(line, column);
  data_port_ddr = 0;
  ctrl_port |= ((1 << lcdrw) | (1 << lcddi)); // "DATA SEND" mode
  _delay_us(5);
  ctrl_port |= (1 << lcde);
  _delay_us(5);
  ctrl_port &= ~(1 << lcde);
  _delay_us(5);
  ctrl_port |= (1 << lcde);
  _delay_us(5);
  data = data_port_pins;
  ctrl_port &= ~(1 << lcde);
  _delay_us(5);
  data_port_ddr = 0xff;
  gLCDgotoXY(line, column);
  return data;
  }
//-------------------------------------------------------------------------
void sendData(unsigned char data)
  {
  if (column == 128)
    {
    column = 0;
    line++;
    if (line == 8)
      line = 0;
    }
  gLCDgotoXY(line, column);
  _delay_loop_1(1);
  setMode(WriteData);
  data_port = data;
  eStrobe();
  column++; // increase column (maximum 128).
  }
//-------------------------------------------------------------------------
void glcdWrite(unsigned char data)
  {
  setMode(WriteData);
  data_port = data;
  eStrobe();
  }
//-------------------------------------------------------------------------
unsigned char glcdRead(void)
  {
  unsigned char data = 0;
  setMode(ReadData);
  //DUMMY READ
  data_port_ddr = 0;
  _delay_us(2);
  ctrl_port |= (1 << lcde);
  _delay_us(2);
  ctrl_port &= ~(1 << lcde);
  _delay_us(2);
  ctrl_port |= (1 << lcde);
  _delay_us(2);
  data = data_port_pins;
  ctrl_port &= ~(1 << lcde);
  _delay_us(2);
  data_port_ddr = 0xff;
  return data;
  }
//-------------------------------------------------------------------------
void restoreRaster(void)
  {
  unsigned char data, i;
  for (i = 0; i < 99; i++)
    {
    column = i + 1;
    line = blank[i *2];
    data = blank[(i *2) + 1];
    gLCDgotoXY(line, column);
    sendData(data);
    }
  }
//-------------------------------------------------------------------------
void eStrobe(void)
  {
  ctrl_port |= (1 << lcde); //lcd 'E' pin high
  _delay_us(En_delay);
  ctrl_port &= ~(1 << lcde); //lcd 'E' pin low
  _delay_us(En_delay);
  }
//-------------------------------------------------------------------------
void putChar(char charToWrite)
  {
  unsigned char i = 0;
  for (i = 0; i < 3; i++)
    sendData(pgm_read_byte(&font3x6[((charToWrite - 0x20) *3) + i]));
  sendData(0x00);
  }
//-------------------------------------------------------------------------
void putStr(char *str,unsigned char x,unsigned char y)
  {
	line=x;
	column=y;
	gLCDgotoXY(line,column);
  while (*str)
    {
    putChar(*str++);
    }
  }
//-------------------------------------------------------------------------
void setMode(unsigned char m)
  {
  switch (m)
    {
    case 0:
      ctrl_port &= ~(1 << lcdrw); //DI=0; RW=0
      ctrl_port &= ~(1 << lcddi);
      break;
    case 1:
      ctrl_port |= (1 << lcdrw); //DI=0; RW=1 
      ctrl_port &= ~(1 << lcddi);
      break;
    case 2:
      ctrl_port &= ~(1 << lcdrw); //Clear RW. 
      ctrl_port |= (1 << lcddi); //Set RS. write data mode				
      break;
    case 3:
      ctrl_port |= (1 << lcdrw); //Clear RW. 
      ctrl_port |= (1 << lcddi); //Set RS. write data mode				
      break;
    }
  }
//-------------------------------------------------------------------------
void clearScreen(void)
  {
  for (unsigned char i = 0; i < 8; i++)
    {
    for (unsigned char j = 0; j < 128; j++)
      {
      gLCDgotoXY(i, j);
      glcdWrite(0);
      }
    }
  }
//-------------------------------------------------------------------------
void v_line(unsigned char x, unsigned char y1, unsigned char y2)
  {
  unsigned char t = 0;
  for (char i = y1; i <= y2; i++)
    {
    if (i % 8 > 0 || i == 0)
      {
      t |= 1 << i % 8;
      gLCDgotoXY(i / 8, x);
      }
    else
      {
      gLCDgotoXY(i / 8-1, x);
      t |= glcdRead();
      gLCDgotoXY(i / 8-1, x);
      glcdWrite(t);
      t = 1;
      }
    }
  glcdWrite(t);
  }
//-------------------------------------------------------------------------
void h_line(unsigned char x1, unsigned char x2, unsigned char y)
  {
  unsigned char t = 0;
  for (char i = x1; i <= x2; i++)
    {
    gLCDgotoXY(y / 8, i);
    t = glcdRead();
    t |= 1 << y % 8;
    gLCDgotoXY(y / 8, i);
    glcdWrite(t);
    }
  }
//-------------------------------------------------------------------------
void drawRect(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2)
  {
  h_line(x1, x2, y1);
  h_line(x1, x2, y2);
  v_line(x1, y1, y2);
  v_line(x2, y1, y2);
  }
//-------------------------------------------------------------------------
void drawWave(void)
  {
  unsigned char temp = 0;
  unsigned char index = xPos;
  column = 1;
  while (column < 100)
    {
    line = (samples[index] + 1) / 8;
    temp = (pgm_read_byte(&LcdRaster[128 *line + column]));
    blank[(column - 1) *2] = line; //Backup the line position (0-7)
    blank[(column - 1) *2+1] = temp; //Backup the data which are on the current LCD possition.
    temp |= 1 << ((samples[index] + 1) % 8);
    //gLCDgotoXY(line, column);
    //glcdWrite(temp);
		sendData(temp);
    //column++;
    index++;
    }
  }
void drawStem(void)
	{
	unsigned char temp=0;
	unsigned char index=xPos;
	unsigned char stemp=0;
	column=1;
	unsigned char j=0;
	unsigned char k=0;
	while(column<100)
		{

		stemp=samples[index] +1;	
		if(stemp<30)
			{j=0;
			k=30-stemp+1;
			}
		else 
			{
			k=stemp-30;
			j=1;
			}
		while(k)
			{
			line=stemp/8;	
  	  //temp = (pgm_read_byte(&LcdRaster[128 *line + column]));
			gLCDgotoXY(line,column);
			temp=glcdRead();
 	   	temp |= 1 << ( stemp % 8);
    	gLCDgotoXY(line, column);
    	glcdWrite(temp);
			if(!j)
				stemp++;
			else 
				stemp--;
			k--;
			}

		column+=2;
		index++;
		}
	}

			

void clearLine(unsigned char l)
{
line=l;
column=0;
gLCDgotoXY(line,column);
for(unsigned char i=0;i<128;i++)
	{
	sendData(0b00000000);
	}
}

