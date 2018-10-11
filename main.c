#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include "nokia5110.h"

#define REF_AVCC (1<<REFS0)  // reference = AVCC = 5 V
uint16_t adc_x, adc_y;

uint16_t adc_read(uint8_t channel)
{
	ADMUX = REF_AVCC | channel;  // set reference and channel
	ADCSRA |= (1<<ADSC);         // start conversion
	loop_until_bit_is_clear(ADCSRA,ADSC); // wait for conversion complete
	return ADC;
}


void play_music(uint8_t type)
{
	DDRB |= (1 << PB3);	
	PORTB |= (1 << PB3);
   	TCCR0A |= (1 << WGM01) | (1 << WGM00) | (1 << COM0A1) | (1 << COM0A0); //toggle fast pwm mode
	
	switch(type)
	{
	case 1: //simple chime, when moving
	{
	OCR0A = 0xC0; //sets the intensity of sound, the closer to 0xFF, the louder it is.
	_delay_ms(25);
	OCR0A = 0x00; //stops the sound
	break;
	}
	
	case 2: //dual tone, when died
	{
	OCR0A = 0x70;
	_delay_ms(300);
	OCR0A = 0xC0;
	_delay_ms(200);
	OCR0A = 0x00;
	break;
	}
	
	case 3: //when grabbing star
	{
		OCR0A = 0xC0;
		_delay_ms(100);
		OCR0A = 0x70;
		_delay_ms(100);
		OCR0A = 0x00;
		break;
	}
	}
	TCCR0B |= (1 << CS00); //enable timer, with no  prescaling
	DDRB =0x00; //to stop making noise
}

int main(void)
{
	
	 ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); //enable adc, clock is divided by 128 , 16000/128=125 khz, between specified 50...200 khz specification
	 play_music(1);
	 //set a0 and a1 pins as input
	 DDRA &= ~(1 << PA0);
	 DDRA &= ~(1 << PA1);
	 
	 uint8_t lives= 3;
	 uint16_t score= 999;
	 uint8_t dir=0;
	 uint8_t collected=0;
	 uint8_t result = 0;
	 uint8_t speed = 20;
	 nokia_lcd_init();
	 nokia_lcd_clear();
	 draw_borders();
	 initialize_arena();
	 refresh_arena();
	 move_frog(0); // draw the frog without moving it anywhere
   
   
   
    while(collected<6 && lives!=0) { //while still playing
       redraw_arena();
       draw_hud(lives,score);
       if(score>0) score--;
	  
	  //keep reading joystick input. in normal position, x and y are about 450. 
	  
	adc_x = adc_read(0);
	adc_y = adc_read(1);
	  if(adc_x > 850) dir=1; //up
	  if(adc_x < 100) dir=2; //down
	  if(adc_y < 100) dir=3; //left
	  if(adc_y > 850) dir=4; //right
	  
      if(dir!=0) play_music(1);
	  result = move_frog(dir); //move_Frog returns various events related to frog state
	  
	 if(result==1) {lives--; play_music(2);} //if hit by obstacle
	 if(result == 2) {collected++; play_music(3);} //if collected star
	 if(result == 2 && collected==3)  // level up. 
	 {  lives=3; play_music(3);
	    
	    nokia_lcd_clear();
	    nokia_lcd_set_cursor(15,15);
	    nokia_lcd_write_string("LEVEL UP!",1);
	    nokia_lcd_render();
	    _delay_ms(2000);
	    nokia_lcd_clear();
	    draw_borders();
	    initialize_arena(); //draw obstacles in initial position, see game_map.png
	    refresh_arena();  
	    speed =10;
	 }
	 
      if(score%speed ==0) //moves obstacle, when "score" decreases by "speed" amount
	 refresh_arena(); //moves the obstacles
	 dir=0; //reset direction
	 result=move_frog(dir); //check whether player died by moving obstacles (while idle)
	 if(result==1 ) {lives--; play_music(2);} 
	 result=0;
	 
	 nokia_lcd_render(); //update the information on the lcd
        _delay_ms(50);
        
       
    }
	
    
    if(collected == 6 ) //3 stars per level, 2 levels, player won
    { play_music(3);
       nokia_lcd_clear();
	   draw_hud(lives,score);
       nokia_lcd_set_cursor(15,15);
       nokia_lcd_write_string("YOU WON! :)",1);
       nokia_lcd_render();
   }
   
       if(lives == 0 ) //if player died
    { play_music(2);
       nokia_lcd_clear();
       nokia_lcd_set_cursor(15,15);
       nokia_lcd_write_string("YOU LOST :(",1);
       nokia_lcd_render();
   }
   
   
   
   
}


   