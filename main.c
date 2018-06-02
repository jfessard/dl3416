#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <util/delay.h>
#include "rtc.h"
#include "twi.h"

#define NUM_CHAR 20


static void digit(uint8_t which, uint8_t val)
{
	static const uint8_t CS[]= { 0x38, 0x34, 0x30, 0x2c, 0x1c}; //manual CS values matching the board
	volatile uint8_t del;

	if (which >= NUM_CHAR)
		return;

	val &= 0x7f;	//filter only low 7 bits please

	//SWAP DIGIT SELECT BITS BECAUSE LETTERS IN ENGLISH GO LEFT TO RIGHT
	which ^= 3;

	//output digit select bits
	//            CS big         Addr Sel        CS all
	PORTB = (PORTB & 0xc0) | (which & 0x03) | CS[which / 4];

	for (del = 0; del < 10; del++); //super short delay

	//WR active, data DNK
	PORTD = 0;
	for (del = 0; del < 10; del++);

	//data out
	PORTD = val;
	for (del = 0; del < 10; del++);

	//WR idle & wait
	PORTD = val | 0x80;
	for (del = 0; del < 10; del++);
}

void setup()
{
	//all pins low unless otherwise specified
	PORTD = 0; //Digit data + write bit (MSB)
	PORTB &=~ 0x0F; // CHIP enables for big and small digits
	PORTC = 0; //Input for DST switch

	DDRD = 0xFF;	//port D is all outputs
	DDRB |= 0x3F;	//lower 4 bits of B are also outputs
	DDRC &= 0xFE;   // C0 input
	//PORTB |= (1 << 3);	//PB3 is high
	PORTB |= (1 << 2);	//PB2 is high
	PORTB &=~ (1 << 3);	//PB2 is low
	PORTC |= 1; // C0 pullup


	// initialize Timer1
	cli();          // disable global interrupts
	TCCR1A = 0;     // set entire TCCR1A register to 0
	TCCR1B = 0;     // same for TCCR1B

	// set compare match register to desired timer count:
	OCR1A = 31249;
	// turn on CTC mode:
	TCCR1B |= (1 << WGM12);
	// Set CS12 bits for 256 prescaler:
	TCCR1B |= (1 << CS12);
	// enable timer compare interrupt:
	TIMSK1 |= (1 << OCIE1A);
	// enable global interrupts:
	sei();
}


// Timer fires this ISR every second.
ISR(TIMER1_COMPA_vect)
{
}

void fill(char letter) {
	uint8_t j;
	for (j=0; j<NUM_CHAR; j++) {
		digit(j, letter);
	}
}


#define          QUARTER_POS 25
#define HALF_POS QUARTER_POS + 1
#define PAST_POS QUARTER_POS + 2
#define TO_POS   QUARTER_POS + 3
#define AM_POS   QUARTER_POS + 4
#define PM_POS   QUARTER_POS + 5

struct tm* t =  NULL;
static uint8_t pos = 0; //keeps track of where text ends
static const char* text[] = { "MIDNITE", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "NOON",
	"13", "14", "15", "16", "17", "18", "19", "TWENTY", "21", "22", "23", "MIDNITE", "QUARTER", "HALF ", " PAST", " TO", "AM", "PM"};

void writenum(uint8_t start, uint8_t what) {
	uint8_t i;
	for (i = 0; i < strlen(text[what]); i++) {
		digit(start + i, text[what][i]); //write at offset location (pos)
	}
	pos += i;
}

void update(uint8_t hou, uint8_t min ) {
	fill(' ');
	uint8_t orig_hou = hou;
	if (PINC & (1 << 0)) hou++; //DSL toggle
	if (min > 36) hou++; //"FOO TO HOUR+1"
	//these two if cases are for noon/midnight distinctions
	if (hou>=24) hou-=24; // for 23:47 + 1 hour
	if (hou>12 && orig_hou <=23) hou-=12; // for most cases
	pos = 0;
	if(min < 5) {
		//1:00  ONE
		//done at end of function
	}
	else if(min < 10) {
		//1:05  FIVE PAST ONE
		writenum(0, 5);
		writenum(pos, PAST_POS);
	}
	else if(min < 15) {
		//1:10  TEN PAST ONE
		writenum(0, 10);
		writenum(pos, PAST_POS);
	}
	else if(min < 20) {
		//1:15  QUARTER PAST ONE
		writenum(0, QUARTER_POS);
		writenum(pos, PAST_POS);
	}
	else if(min < 25) {
		//1:20  TWENTY PAST ONE
		writenum(0, 20);
		writenum(pos, PAST_POS);
	}
	else if(min < 30) {
		//1:25  TWENTY FIVE PAST ONE
		writenum(0, 20);
		writenum(pos, 5);
		digit(pos++, ' ');
		digit(pos++, '>');
	}
	else if(min < 35) {
		//1:30  HALF PAST ONE
		writenum(0, HALF_POS);
		writenum(pos, PAST_POS);
	}
	else if(min < 40) {
		//1:35  TWENTY FIVE TO TWO
		writenum(0, 20);
		writenum(pos, 5);
		digit(pos++, ' ');
		digit(pos++, '<');
	}
	else if(min < 45) {
		//1:40  TWENTY TO TWO
		writenum(0, 20);
		digit(pos++, ' ');
		digit(pos++, ' ');
		writenum(pos, TO_POS);
	}
	else if(min < 50) {
		//1:45  QUARTER TO TWO
		writenum(0, QUARTER_POS);
		digit(pos++, ' ');
		writenum(pos, TO_POS);
	}
	else if(min < 55) {
		//1:50  TEN TO TWO
		writenum(0, 10);
		digit(pos++, ' ');
		digit(pos++, ' ');
		digit(pos++, ' ');
		digit(pos++, ' ');
		digit(pos++, ' ');
		digit(pos++, ' ');
		writenum(pos, TO_POS);
	}
	else if(min < 60) {
		//1:55  FIVE TO TWO
		writenum(0, 5);
		digit(pos++, ' ');
		writenum(pos, TO_POS);
	}
	int8_t pad = 0;
	const char *houStr = hou < sizeof(text) / sizeof(*text) ? text[hou] : "ERR";
	pad = (8 - strlen(houStr))/2; //number of spaces to pad on hour line
	for (;pos<12;pos++) digit(pos, ' '); //fill rest of 1st line with spaces
	for (;pad > 0; pad--) digit(pos++, ' '); //pad hours for centering
	writenum(pos, (hou)); //hour on second line
}

void setupi2c() {
	uint8_t ret;
	twi_init_master();
	rtc_init();
	ret = rtc_ping();
	if (ret)  { //ping failed. Show inop.
		fill('X');
		while(1);
	}
}

int main(void)
{
	setup();
	fill(' ');
	setupi2c();
#if 0
	t->hour = 1;
	t->min = 12;
	t->am = 1;
	t->sec = 0;
	t->wday=7; //day of wk
	t->mday=5;
	t->mon = 12;
	t->year = 2017;
	rtc_set_time(t); //Only uncomment when setting the RTC time
#endif

	while(1) {
		t = rtc_get_time();
		update(t->hour, t->min);
		_delay_ms(50); //avoids flicker
	}
}
