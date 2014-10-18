
#define F_CPU 16000000UL
#include <avr/io.h>
#include "glcd.c"
#include "24c64.c"
#include <avr/interrupt.h>
#include "menu.c"
#include <avr/pgmspace.h>
#include <math.h>

#define VREF 5.0
#define BAUD_RATE 9600UL
#define UBBR_VALUE (F_CPU/(BAUD_RATE*16))-1
#define timeUp PD5		//timeUp while play and Position left in hold mode
#define timeDown PD7	//timeDown while play and Position right in hold mode
#define hold PD3
#define voltUp PA4
#define voltDown PA5
#define menuItemN 6
#define menuBtn PD2
#define GAIN 4.4
#define samplesN 200

unsigned int pulse = 1;
unsigned int total_count = 0;
unsigned int freq = 1;
unsigned char ADCval = 0;
unsigned char timeDiv = 2;
unsigned char voltDiv = 100;
unsigned int max = 0;
//char *menuItem[]={"SAVE","READ","VIEW","SERIAL","EXIT"};


//-------------------------------------------------------------------------
void init_ADC();
int ADC_value(unsigned char bin);
void init_input_capture();
void updateTimediv(void);
void updateVoltdiv(void);
void init_PORT(void);
void paused(void);
void init_UART(void);
void sendSamples(unsigned char data);
void runningScreen(void);
void menuScreen(void);
void selectItem(unsigned char currentIndex,unsigned char pastIndex );
char actionHandle(unsigned char functionNo);
void saveEEPROM(void);
void readEEPROM(void);
void serialMode(void);
void takeSamples(void);
void About(void);
//-------------------------------------------------------------------------
ISR(TIMER1_CAPT_vect)
{
  total_count += ICR1L;
  total_count += ICR1H << 8;
  TCNT1 = 0;
  if (total_count > 300)
    {
			float temp=0;
      temp = 1000000.0 * pulse;
      temp = temp / (total_count *16);
			freq=(int)temp;
      pulse = 1;
      total_count = 0;
    }
  else
    pulse++;
}
//-------------------------------------------------------------------------
int main(void)
{
  init_PORT();
  init_ADC();
  glcdInit();
  EEOpen();
  init_input_capture();
  sei();

  drawPic(welcomeScreen,0,0);
  _delay_ms(4000);

  drawPic(LcdRaster, 0, 0);
  drawWave();
  while (1)
    {
      if ((~PIND &1 << timeUp) && (timeDiv < 16))
        {
          timeDiv++;
        }
      if ((~PIND &1 << timeDown) && (timeDiv > 1))
        {
          timeDiv--;
        }

      if ((~PINA &1 << voltUp) && (voltDiv < 200))
        {
          voltDiv+=6;
        }
      if ((~PINA &1 << voltDown) && (voltDiv > 35))
        {
          voltDiv-=6;
        }
      if (GIFR & 1<<INTF0)
				{
				_delay_ms(300);
				GIFR |= 1<<INTF0;
        menuScreen();
				}
      if(GIFR & 1<<INTF1)
			{
			_delay_ms(300);
			GIFR |=1<<INTF1;
			paused();
			}
      runningScreen();


    }
  return 0;
}
//-------------------------------------------------------------------------
void init_ADC()
{
  ADMUX |= 1 << REFS0 | 1 << ADLAR;
  ADCSRA |= 1 << ADEN | 1 << ADSC | 1<<ADPS1; /////******
}
//-------------------------------------------------------------------------
void init_input_capture()
{
  DDRD &= ~1 << PD6;
  TCCR1B |= 1 << CS12;
  TCCR1B |= 1 << ICNC1 | 1 << ICES1;
  TIMSK |= 1 << TICIE1;
}
//-------------------------------------------------------------------------
void updateTimediv(void)
{
	float temp=0;
	temp=(timeDiv/5.0)*10.0;
  line = 5; //timediv
  column = 115;
  gLCDgotoXY(line, column);
  putChar(((int)temp / 100) % 10+'0');
	putChar(((int)temp/10)%10+'0');
	sendData(0b01000000);
  putChar((int)temp % 10+'0');
}
//-------------------------------------------------------------------------
void updateVoltdiv(void)
{
	float temp;
  line = 4; //timediv
  column = 115;
	temp=(voltDiv/20.0)*100.0;
  gLCDgotoXY(line, column);
  putChar(((int)temp / 1000) % 10+'0');
	putChar(((int)temp/100)%10+'0');
	sendData(0b01000000);
  putChar(((int)temp/10) % 10+'0');
}

//-------------------------------------------------------------------------
void init_PORT(void)
{
  DDRD &= ~(1 << timeUp | 1 << timeDown  );
  PORTD |= 1 << timeUp | 1 << timeDown  ; //activate pull-up resistors
  DDRA &= ~(1 << voltUp | 1 << voltDown);
  PORTA |= 1 << voltUp | 1 << voltDown;

  MCUCR |= 1<<ISC01 | 1<<ISC11 ;
}
//-------------------------------------------------------------------------
void runningScreen(void)
{
  takeSamples();
  restoreRaster();
  drawWave();	


  line = 3; //Show peak voltage
  column = 109;
  gLCDgotoXY(line, column);
  int temp = max;
  putChar((temp / 100) % 10+'0');
  sendData(0b01000000);
  //Print one dot character on LCD (only 1 column length).
  sendData(0x00);
  putChar((temp / 10) % 10+'0');
  putChar(temp % 10+'0');
  sendData(0x00);
  putChar('V');
  line = 7; //Show frequency
  column = 103;
  gLCDgotoXY(line, column);
  temp = freq;
  putChar((temp / 10000) + '0');
  putChar((temp / 1000) % 10+'0');
  putChar((temp / 100) % 10+'0');
  putChar((temp / 10) % 10+'0');
  putChar(temp % 10+'0');

  putStr("PLAY",2,103);
  sendData(0b00000000);
  putChar('\\');

  updateTimediv();
	updateVoltdiv();


}
//-------------------------------------------------------------------------
void takeSamples(void)
{
	unsigned char max_temp=0; 

	int j=0; 
  for (int i = 0; i <15000; i++)
    {
      ADCSRA |= (1 << ADSC); // Enable ADC
      while (!(ADCSRA &1 << ADIF))
        ;
      ADCval = ADCH;
			if(ADCval>max_temp)
				max_temp=ADCval;

      if ((  (i % timeDiv) == 0))
        {
          if (j < samplesN)
            samples[j++] = ADCval;
        }
		}

  for (int i = 0; i <samplesN; i++)
    {
      float temp;

      temp = ((samples[i] -128)/ 256.0);
			temp = temp*70/voltDiv;
      temp = (temp + 0.5) *60.0;
      samples[i] = (int)temp;

    }

		  max = (max_temp *VREF/256)*100;
			max=max-250;
			max=GAIN*max;



	/*		if(freq<500 && prescale!=8)
				{ADCSRA |= 1<<ADPS1 | 1<<ADPS0 ;
				 ADCSRA &=~1<<ADPS2; 
				 prescale=8;
				 }

			else if(freq>1000 && prescale!=2)
				{ADCSRA &= ~( 1<<ADPS1 | 1<<ADPS0 | 1<<ADPS2) ; 
				prescale=2;
				}
			else if(freq>500 && freq<1000 && prescale!=4)
				{
				ADCSRA |= 1<< ADPS1 ;
				ADCSRA &=~(1<<ADPS0 | 1<<ADPS2) ;
				prescale=4;
				}

*/
}

//-------------------------------------------------------------------------
void paused(void)
{
  drawPic(LcdRaster,0,0);
  drawWave();
  putStr("HOLD",2,103);
  sendData(0b00000000);
  putChar(']');

	line = 3; //Show peak voltage
  column = 109;
  gLCDgotoXY(line, column);
  unsigned int temp =max;
  putChar((temp / 100) % 10+'0');
  sendData(0b01000000);
  //Print one dot character on LCD (only 1 column length).
  sendData(0x00);
  putChar((temp / 10) % 10+'0');
  putChar(temp % 10+'0');
  sendData(0x00);
  putChar('V');
  line = 7; //Show frequency
  column = 103;
  gLCDgotoXY(line, column);
  temp = freq;
  putChar((temp / 10000) + '0');
  putChar((temp / 1000) % 10+'0');
  putChar((temp / 100) % 10+'0');
  putChar((temp / 10) % 10+'0');
  putChar(temp % 10+'0');


  unsigned char i = 0;
  while (1)
    {
      if (~PIND &1 << timeUp && xPos < 99)
        {
					_delay_ms(100);
          xPos++;
          i = 1;
        }
      else if (~PIND &1 << timeDown && xPos > 0)
        {
					_delay_ms(100);
          xPos--;
          i = 1;
        }
      if (i)
        {
          restoreRaster();
          drawWave();
          i = 0;
        }
      if (GIFR & 1<<INTF1)
        {
					_delay_ms(300);
					GIFR |=1<<INTF1;          
          break;
        }
			if (GIFR & 1<<INTF0)
				  {
					_delay_ms(300);
					break;
					}
					
    }
}

//-------------------------------------------------------------------------
void init_UART(void)
{
  unsigned int UBBR=0;
  UBBR=(F_CPU/(BAUD_RATE*16))-1;

  UBRRH=(uint8_t) (UBBR>>8);
  UBRRL=(uint8_t) UBBR;
//set Frame rate: 8 bit, 0 stop
  UCSRC |=(1<<URSEL) | (1<<UCSZ0) | (1<<UCSZ1);// | (1<<USBS);

  UCSRB |=(1<<RXEN) | (1<<TXEN);

}
//-------------------------------------------------------------------------
void menuScreen(void)
{
  unsigned char pointer=0;
  unsigned char direction=0;
  TCCR1B &=~(1 << CS12 | 1<<CS10 | 1<<CS11) ; //disable frequecy count in menu mode
  drawPic(Menu,0,0);
	selectItem(pointer,direction);
  while (1)
    {
      if (~PINA & 1<<voltDown)
        {
          _delay_ms(300);
          pointer++;
          if (pointer==menuItemN)
            pointer=0;
          direction=0;//downward
          selectItem(pointer,direction);
        }
      else if (~PINA & 1<<voltUp)
        {
          _delay_ms(300);
          if (pointer==0)
            pointer=menuItemN-1;
          else pointer--;
          direction=1;//upward
          selectItem(pointer,direction);
        }
      if (GIFR & 1<<INTF1)
        {
          _delay_ms(300);
					GIFR |=1<<INTF1;
          if(!actionHandle(pointer))
          	selectItem(pointer,direction);
					else
					 	{
						drawPic(LcdRaster,0,0);
						break;
						}
        }

      if (GIFR & 1<<INTF0)
        {
					_delay_ms(300);
					GIFR |=1<<INTF0;
          drawPic(LcdRaster,0,0);
          break;
        }
    }
  TCCR1B |=1 << CS12 ; //enable frequecy count in running mode

}
//-------------------------------------------------------------------------
void selectItem(unsigned char currentIndex, unsigned char direction)
{
  line = currentIndex + 1;
  column = 9;
  gLCDgotoXY(line, column);
  putChar('\\');
  if (direction)
    {
      if (currentIndex != (menuItemN-1))
        {
          line++;
          column = 9;
          gLCDgotoXY(line, column);
          putChar(' ');
        }
      else
        {
          line = 1;
          column = 9;
          gLCDgotoXY(line, column);
          putChar(' ');

        }

    }
  else if (direction == 0)
    {
      if (currentIndex > 0)
        {
          line--;
          column = 9;
          gLCDgotoXY(line, column);
          putChar(' ');
        }
      if (currentIndex == 0)
        {
          line = menuItemN;
          column = 9;
          gLCDgotoXY(line, column);
          putChar(' ');


        }
    }

}
//-------------------------------------------------------------------------
char actionHandle(unsigned char functionNo)
{
  switch (functionNo)
    {
    case 0:
      saveEEPROM();
      break;
    case 1:
      readEEPROM();
      break;
    case 2:
      paused();
      break;
    case 3:
      serialMode();
      break;    
		case 4:
			About();
			break;
		case 5:
			return 1;
			break;
		}
    drawPic(Menu,0,0);
		return 0;
}
//-------------------------------------------------------------------------
void saveEEPROM(void)
{
  clearScreen();
  putStr("SAVING DATA....",2,40);
	drawRect(38,32,91,38);
	line=4;
	column=40;
	gLCDgotoXY(line,column);
	int i;
  for (i = 0; i < samplesN; i++)
	{
    if (EEWriteByte(i, samples[i]) == 0)
      break;
		if( (i%4)==0)
			{
			sendData(0b01011101);
			}

	}
  EEWriteByte(i++,(char) max);
	EEWriteByte(i++,(char) (max>>8));
	EEWriteByte(i++,(char) freq);
	EEWriteByte(i++,(char) (freq)>>8);
  clearLine(2);
  putStr("SAVING COMPLETED",2,40);
  _delay_ms(1000);
}
//-------------------------------------------------------------------------
void readEEPROM(void)
{
	int i=0;
  clearScreen();
  putStr("LOADING DATA....",2,40);
	drawRect(38,32,91,38);
	line=4;
	column=40;
	gLCDgotoXY(line,column);
  for (i = 0; i < samplesN; i++)
    {samples[i]=EEReadByte(i);
		if( (i%4)==0)
			{
			sendData(0b01011101);
			}
			}
  max = EEReadByte(i++);
	max +=EEReadByte(i++)<<8;
	freq =EEReadByte(i++);
	freq +=EEReadByte(i++)<<8;
  clearLine(2);
  putStr("LOADING COMPLETED",2,40);
  _delay_ms(1000);
}
//-------------------------------------------------------------------------
void serialMode(void)
{
 	drawPic(serial,0,0);
	putStr("SERIAL MODE",0,40);

  init_UART();

	ADCSRA |= 1<<ADPS0 | 1<< ADPS1;
//	ADCSRA &=~(1<<ADPS1 | 1<<ADPS2);
  while (1)
    {
      ADCSRA |= (1 << ADSC); // Enable ADC
      while (!(ADCSRA &1 << ADIF))
        ;
      sendSamples(ADCH);
      if (GIFR & 1<<INTF1)
        {
					_delay_ms(300);
					GIFR |= 1<<INTF1;
				  UCSRB  &=~(1<<RXEN) | (1<<TXEN);
					break;
        }
    }

}
void About(void)
{
	drawPic(about,0,0);
  while (1)
    {
     if (GIFR & 1<<INTF1)
        {
					_delay_ms(300);
					GIFR |= 1<<INTF1;
				  UCSRB  &=~(1<<RXEN) | (1<<TXEN);
					break;
        }
    }

}
//-------------------------------------------------------------------------
void sendSamples(unsigned char data)
{
      while ((UCSRA & (1<<UDRE)) ==0)
        ;
      UDR=data; //send data
    
}


