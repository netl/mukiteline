#define F_CPU 8000000UL

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
*/
#define	DIRECTION	0	//B
#define	ENABLE		2	//B
#define	LED		7	//A
#define	BUTTON		0	//A
#define	TRAY_OUT	1	//A
#define	TRAY_IN		2	//A
#define	TRIMMER		3	//A
#define	RED		4	//A
#define	GREEN		5	//A
#define	BLUE		6	//A
#define	TRAY_SPEED	100

int moving;
int move(int dir);
void motorfade(int spd);
void pwm(int en);
void set_hue(uint8_t hue);

volatile uint8_t pwm_RED, pwm_GREEN, pwm_BLUE;

int main(void)
{
	//port configuration

	DDRA=(1<<LED)|(1<<RED)|(1<<GREEN)|(1<<BLUE);		//leds as output
	DDRB=(1<<DIRECTION)|(1<<ENABLE);			//motor direction and enable as output
	PORTA=(1<<BUTTON)|(1<<TRAY_OUT)|(1<<TRAY_IN)|(1<<LED);	//pull-ups and LED on for debug

	//pwm setup for motor and front led

	pwm(0);							//configure motor and front led pwm
	TCCR0B = (1<<CS00);
	ADMUX = (1<<MUX0)|(1<<MUX1); 				//select PA3 for adc
	ADCSRB = (1<<ACME)|(1<<ADLAR);				// enable as adc source and select to show the 8 highest bits
	ADCSRA = (1<<ADEN)|(1<<ADSC)|(1<<ADATE);		//enable with auto trigger

	//configure software pwm
	TIMSK0 = (1<<TOIE0);
	pwm_RED = 0;
	pwm_GREEN = 0;
	pwm_BLUE = 0;
	sei();							//interrupts on

	int i=0,cnt=0,dir=1,afk=0;				//dir 1=in 2=out 0=don't care
	OCR0B=0;						//front led brightness

	while(1)
	{
		//move tray if the button is pressed

		if(bit_is_clear(PINA,BUTTON))	//if front button is pressed, move the tray
		{
			while(bit_is_clear(PINA,BUTTON)); //wait for button to be released
			if(dir==2)	//if moving outwards, blink the led for a while
			{
				for(i=6;i>0;i--)
				{
					OCR0B=0xff*(i%2);
					_delay_ms(100);
				}
			}
			else _delay_ms(300);
			move(dir);
		}

		//stop tray if it is at the limits

		if(bit_is_clear(PINA,TRAY_OUT)&&dir==2)	//moving out and allready at max reach
		{
			move(0);
			dir=1;
		}

		if(bit_is_clear(PINA,TRAY_IN)&&dir==1)	//moving in and closed
		{
			_delay_ms(100);		//make sure it has reached the end
			OCR0A=0;		//stop the motor immediately fade will cause the tray to bump out
			pwm(0);			//motor pwm off for sake of silence
			dir=2;
		}

		//idle

		if(bit_is_clear(PINA,TRAY_IN))	//fade led in and out if tray is in
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
			pwm_RED = 0;		//rgb leds off
			pwm_GREEN = 0;
			pwm_BLUE = 0;
			_delay_ms(10);		//slow the program :(
		}
		else				
		{
			afk=0;			//turn off the front led
			OCR0B=afk;
			
			if(cnt>(ADCH*50)-130)		//mess with the rgb leds
			{
				cnt = 0;
				set_hue(i);
				if(i<255)
					i++;
				else
					i = 0;
			}
			else
				cnt++;
		}

		if(bit_is_clear(PINA,TRAY_OUT))	
		{
		}

	}
}

void set_hue(uint8_t hue)
{
	//red
	if(hue<255*1/3)
		pwm_RED = 255-hue*3;
	else
	if(hue>=255*2/3)
		pwm_RED = (hue-255*2/3)*3;
	else
		pwm_RED = 0;

	//green
	if(hue<255*1/3)
		pwm_GREEN = hue*3;
	else
	if(hue>=255*1/3 && hue<255*2/3)
		pwm_GREEN = 255-(hue-255*1/3)*3;
	else
		pwm_GREEN = 0;

	//blue
	if(hue>255*1/3 && hue<255*2/3)
		pwm_BLUE = (hue-255*1/3)*3;
	else
	if(hue>=255*2/3)
		pwm_BLUE = 255-(hue-255*2/3)*3;
	else
		pwm_BLUE = 0;
}

SIGNAL(TIM0_OVF_vect)
{
	static uint8_t counter = 0;
	//counter &= 0xf;						//mask counter to 4 bits
	uint8_t rgb = (((counter)<pwm_RED)<<RED)		//compare pwm to counter
			|(((counter)<pwm_GREEN)<<GREEN)
			|(((counter)<pwm_BLUE)<<BLUE);	
	PORTA=(PORTA&~((1<<RED)|(1<<GREEN)|(1<<BLUE)))|rgb;	//mask PORTA with rgb pins and set the correct values
	counter ++;
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
					motorfade(TRAY_SPEED);
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
					motorfade(TRAY_SPEED);
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
