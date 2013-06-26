#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <math.h>

/*
	wires		pins
	vcc
	gnd
	reset
	sck
	miso
	mosi
	direction	PB0
	enable		PB2
	led			PA7
	sw			PA0
	tray out	PA1
	tray in		PA2
	trimmer		PA3
*/
int moving;
int move(int dir);
void motorfade(int spd);
void pwm(int en);

int main(void)
{
	DDRA=0b10000000;	//led (PA7) as out
	DDRB=0b101;
	PORTA=0b10000111;	//led on, pull-up on tray sensors and sw
	PORTB=0b001; // disable motor control (PB2)
	pwm(1);//TCCR0A = (1<<COM0A1)|(1<<COM0B1)|(1<<WGM00)|(1<<WGM01);//
	TCCR0B = (1<<CS00);
	ADMUX = (1<<MUX0)|(1<<MUX1); //select PA3 for adc
	ADCSRB = (1<<ACME)|(1<<ADLAR);	// enable as adc source and select to show the 8 highest bits
	ADCSRA = (1<<ADEN)|(1<<ADSC)|(1<<ADATE);	//enable with auto trigger
	int i,dir=1,afk=0;	//dir 1=in 2=out 0=don't care
	OCR0B=0;	//led brightness
	move(1);	//pull tray in

	do
	{
		/*if(bit_is_clear(PINA,0))	//if sw is pressed, move the tray
		{
			while(bit_is_clear(PINA,0)); //wait for button to be released
			if(dir==2)	//if moving outwards, blink the led for a while
			{*/
				if(moving==0){
				for(i=rand()%50+3;i>0;i--)
				{
					OCR0B=0xff*(i%2);
					_delay_ms(100);
				}
					move(dir);
				}
			/*}
			else _delay_ms(300);*/
		//}
		if(!bit_is_set(PINA,1)&&dir==2)	//moving out and allready at max reach
		{
			move(0);
			dir=1;
		}
		if(!bit_is_set(PINA,2)&&dir==1)	//moving in and closed
		{
			_delay_ms(100);
			OCR0A=0;	//stop the motor immediately fade will cause the tray to bump out
			pwm(0);	//motor pwm off for sake of silence
			dir=2;
		}
		if(!bit_is_set(PINA,2))	//fade led if tray is in
		{
			afk ++;
			if(afk==0x1ff)afk=0;
			if(afk<=0xff)
			{
				OCR0B=afk;	//duty cycle
			}
			else
			{
				OCR0B=0xff-afk;	//duty cycle
			}
			_delay_ms(10);	//slow the program :(
		}
		else
		{
			afk=0;
			OCR0B=afk;
		}
		//OCR0B=ADCH;
	}while(1);
}

void pwm(int en)	//enable/disable motor pwm
{
		TCCR0A = (en<<COM0A1)|(1<<COM0B1)|(1<<WGM00)|(1<<WGM01);
		moving=en;
}

void motorfade(int spd)
{
	if(spd!=0) pwm(1);
	while(OCR0A!=spd)
	{
		if(OCR0A>spd) OCR0A --; else OCR0A ++;
		_delay_us(500);
	}
	if (spd==0) pwm(0);
}

int move(int dir)
{
	int lolspd=105+(rand()%150);
	switch (dir)
	{
		case 0:		//stop
				motorfade(0);			
				return(1);
			break;
		case 1:		//in
				if(bit_is_set(PINA,2))	//make sure the tray can move
				{
					PORTB=0b1; //set direction in
					//motorfade(128+ADCH/2);
					motorfade(lolspd);
					return(1);
				}
				else
				{
					OCR0A=0;	//stop motor immediately
					return(0);
				}
			break;
		case 2:		//out
				if(bit_is_set(PINA,1))	//make sure the tray can move
				{
					PORTB=0b0;	//set direction out
					//motorfade(128+ADCH/2);	//move the tray out acording to pot
					motorfade(lolspd);
					return(1);
				}
				else
				{
					OCR0A=0;	//stop motor immediately
					return(0);
				}
			break;
		default:
				return(2);
			break;
	}
}
