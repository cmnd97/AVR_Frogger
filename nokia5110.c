
#include "nokia5110.h"
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <util/delay.h>
#include "nokia5110_chars.h"


static struct {
    uint8_t screen[504]; //remember all bytes on screen. a byte stores a vertical line on the screen, which has 7 pixels.
    uint8_t active_matrix[16][7]; //remembers where the items are on the screen. an item is 5x5 pixels.
    
    // cursor position, where stuff is going to be written
    int cursor_x;
    int cursor_y;

} nokia_lcd = {
    .cursor_x = 0,
    .cursor_y = 0
};
uint8_t frog_x=0, frog_y=3; //position in active matrix
uint8_t died_idle=0; //is set to 1 if player dies without doing anything (hit by moving obstacle while idle)
/*
 * Sending data to LCD
 * @bytes: data
 * @is_data: transfer mode: 1 - data; 0 - command;
 */
static void write(uint8_t bytes, uint8_t is_data)
{
	register uint8_t i;
	PORT_LCD &= ~(1 << LCD_SCE); // Enable controller 

	/* We are sending data */
	if (is_data)
		PORT_LCD |= (1 << LCD_DC);
	/* We are sending commands */
	else
		PORT_LCD &= ~(1 << LCD_DC);

	/* Send bytes */
	for (i = 0; i < 8; i++) {
		/* Set data pin to byte state */
		if ((bytes >> (7-i)) & 0x01)
			PORT_LCD |= (1 << LCD_DIN);
		else
			PORT_LCD &= ~(1 << LCD_DIN);

		/* Blink clock */
		PORT_LCD |= (1 << LCD_CLK);
		PORT_LCD &= ~(1 << LCD_CLK);
	}

	/* Disable controller */
	PORT_LCD |= (1 << LCD_SCE);
}

static void write_cmd(uint8_t cmd)
{
	write(cmd, 0);
}

static void write_data(uint8_t data)
{
	write(data, 1);
}

/*
 * Public functions
 */

void nokia_lcd_init(void)
{
	register unsigned i;
	/* Set pins as output */
	DDR_LCD |= (1 << LCD_SCE);
	DDR_LCD |= (1 << LCD_RST);
	DDR_LCD |= (1 << LCD_DC);
	DDR_LCD |= (1 << LCD_DIN);
	DDR_LCD |= (1 << LCD_CLK);

	/* Reset display */
	PORT_LCD |= (1 << LCD_RST);
	PORT_LCD |= (1 << LCD_SCE);
	_delay_ms(10);
	PORT_LCD &= ~(1 << LCD_RST);
	_delay_ms(70);
	PORT_LCD |= (1 << LCD_RST);

	/*
	 * Initialize display
	 */
	/* Enable controller */
	PORT_LCD &= ~(1 << LCD_SCE);
	/* -LCD Extended Commands mode- */
	write_cmd(0x21);
	/* LCD bias mode 1:48 */
	write_cmd(0x13);
	/* Set temperature coefficient */
	write_cmd(0x06);
	/* Default VOP (3.06 + 66 * 0.06 = 7V) */
	write_cmd(0xC2);
	/* Standard Commands mode, powered down */
	write_cmd(0x20);
	/* LCD in normal mode */
	write_cmd(0x09);

	/* Clear LCD RAM */
	write_cmd(0x80);
	write_cmd(LCD_CONTRAST);
	for (i = 0; i < 504; i++)
		write_data(0x00);

	/* Activate LCD */
	write_cmd(0x08);
	write_cmd(0x0C);
}

void nokia_lcd_clear(void)
{
	register unsigned i;
	/* Set column and row to 0 */
	write_cmd(0x80);
	write_cmd(0x40);
	/*Cursor too */
	nokia_lcd.cursor_x = 0;
	nokia_lcd.cursor_y = 0;
	/* Clear everything (504 bytes = 84cols * 48 rows / 8 bits) */
	for(i = 0;i < 504; i++)
		nokia_lcd.screen[i] = 0x00;
}

void nokia_lcd_power(uint8_t on)
{
	write_cmd(on ? 0x20 : 0x24);
}

void nokia_lcd_set_pixel(uint8_t x, uint8_t y, uint8_t value)
{
	uint8_t *byte = &nokia_lcd.screen[y/8*84+x];
	if (value)
		*byte |= (1 << (y % 8));
	else
		*byte &= ~(1 << (y %8 ));
}
  
void nokia_lcd_write_char(char code, uint8_t scale)
{
	register uint8_t x, y;
	
if(scale !=0)   // used to write regular text. scale makes text bigger
{
	for (x = 0; x < 5*scale; x++)
	   
		for (y = 0; y < 7*scale; y++)
			if (pgm_read_byte(&CHARSET[code-32][x/scale]) & (1 << y/scale))
				nokia_lcd_set_pixel(nokia_lcd.cursor_x + x, nokia_lcd.cursor_y + y, 1);
			else
				nokia_lcd_set_pixel(nokia_lcd.cursor_x + x, nokia_lcd.cursor_y + y, 0);
		     
}
else //we use scale=0 to write symbols. not using y=7*scale like above, because objects are 5x5 pixels
{
   	for (x = 0; x < 5; x++)
		for (y = 0; y < 5; y++)
			if (pgm_read_byte(&CHARSET[code-32][x]) & (1 << y))
				nokia_lcd_set_pixel(nokia_lcd.cursor_x + x, nokia_lcd.cursor_y + y, 1);
			else
				nokia_lcd_set_pixel(nokia_lcd.cursor_x + x, nokia_lcd.cursor_y + y, 0);
		     
}

   
	
	
	if(!(code==126 || code==96 || code==94 )) //if not writing objects, go to the next cursor position after writing character, to not overlap when writing strings
	    nokia_lcd.cursor_x += 5*scale; 
	 
	 //rules for screen overlapping 
	if (nokia_lcd.cursor_x >= 84) {
		nokia_lcd.cursor_x = 0;
		nokia_lcd.cursor_y += 7*scale + 1;
	}
	if (nokia_lcd.cursor_y >= 48) {
		nokia_lcd.cursor_x = 0;
		nokia_lcd.cursor_y = 0;
	}

	
}

void nokia_lcd_write_string(const char *str, uint8_t scale) //writes more chars at once!
{
	while(*str)
		nokia_lcd_write_char(*str++, scale);
}

void nokia_lcd_set_cursor(uint8_t x, uint8_t y)
{
	nokia_lcd.cursor_x = x;
	nokia_lcd.cursor_y = y;
}

void nokia_lcd_render(void)
{
	register unsigned i;
	/* Set column and row to 0 */
	write_cmd(0x80);
	write_cmd(0x40);

	/* Write screen to display */
	for (i = 0; i < 504; i++)
		write_data(nokia_lcd.screen[i]);
}

uint8_t move_frog(uint8_t dir)
{
   uint8_t left_x=2,top_y=9;
   nokia_lcd.active_matrix[frog_x][frog_y] = 0; //before moving frog, leave a blank space behind
   nokia_lcd_set_cursor(left_x +frog_x*5,top_y+frog_y*5);
   nokia_lcd_write_string("^",0); //write space immediately, not having to wait for the active matrix to refresh.
   
   
   switch(dir) //modify frog coords according to input
   {	
      
      
      case 1: //up
      {
     if(frog_y > 0)
      frog_y--; break;
      }	
      
      case 2: //down
      { 
     if(frog_y < 6)
      frog_y++;break;
      }
      
      case 3: //left
      {
      if(frog_x > 0)
      frog_x--;break;
      }
      case 4: //right
      {
      if(frog_x < 15)
      frog_x++;break;
      }	
	  
   }

	if(died_idle == 1)  
{
	nokia_lcd.active_matrix[frog_x][frog_y] = 1; //draw the obstacle that hit the frog over the frog
	frog_x=0; frog_y=3; //set the pawn coordinates
	nokia_lcd.active_matrix[frog_x][frog_y] = 2;  died_idle=0; return 1; // draw frog at spawn, reset the died_idle, return 1 (lives--)
}

      uint8_t x,y, nofrog=1;
	 for(x=0;x<16; x++)
	 for(y=0;y<7; y++)
	 if (nokia_lcd.active_matrix[x][y]==2){ nofrog =0; break;}
		 
		 
      if(frog_x==15 && frog_y%2!=0) //respawn after collecting
      { nokia_lcd.active_matrix[frog_x][frog_y] = 0;
	      frog_x=0; frog_y=3;
	      nokia_lcd.active_matrix[frog_x][frog_y] = 2;
	      return 2; //return 2 (collected++)
      }
   
   
	 if(nokia_lcd.active_matrix[frog_x][frog_y] == 0 ) // if no obstacle, move
	 {
       nokia_lcd.active_matrix[frog_x][frog_y] = 2;
       nokia_lcd_set_cursor(left_x +frog_x*5,top_y+frog_y*5);
       nokia_lcd_write_string("~",0); return 0; //write frog immediately, not having to wait for the active matrix to refresh.
	 }
	 else if(frog_x<15 || nofrog==1)//hit obstacle, subtract lives
	 { 
		 nokia_lcd.active_matrix[frog_x][frog_y] = 1;
		 frog_x=0; frog_y=3;
      nokia_lcd.active_matrix[frog_x][frog_y] = 2;  died_idle=0; return 1;
	 }
	 

	    


   
}


void draw_hud(uint8_t lives, uint16_t score)
{//save  cursor before modifying
  uint8_t current_cursor_x=  nokia_lcd.cursor_x;
  uint8_t current_cursor_y= nokia_lcd.cursor_y;
   char buffer[3];
   nokia_lcd_set_cursor(0,0);
   
   //writing lives
   itoa(lives,buffer,10);
   nokia_lcd_write_string("L: ",1);
   nokia_lcd_write_string(buffer,1);
   
   // writing score, so it looks like 999, 099, 009 depending on amount of digits
   itoa(score,buffer,10);
   if(score ==0 ) nokia_lcd_write_string("   S: N/A",1);
      else
      {
	 if(score/100 >0) nokia_lcd_write_string("   S: ",1);
	    else if(score/10 >0) nokia_lcd_write_string("   S: 0",1);
	       else  nokia_lcd_write_string("   S: 00",1);
	nokia_lcd_write_string(buffer,1);
   
      }
   


nokia_lcd_set_cursor(current_cursor_x,current_cursor_y); //set cursor back

}

void draw_borders()
{
    uint8_t current_cursor_x= nokia_lcd.cursor_x;
    uint8_t current_cursor_y= nokia_lcd.cursor_y;
   
   //drawing the borders:
   int i=0;
   for(i;i<84;i++)
   {
   nokia_lcd_set_pixel(i, 7, 1);
   nokia_lcd_set_pixel(i, 45, 1);
   }
   for(i=7;i<45;i++)
   { 
   nokia_lcd_set_pixel(0, i, 1);
   nokia_lcd_set_pixel(83, i, 1);
   }

   nokia_lcd_set_cursor(current_cursor_x,current_cursor_y);
}

void initialize_arena()
{ //refer to game_map.png
   uint8_t x,y;
   
  for(x=1;x<16; x++)
  for(y=0;y<7; y++)
  { nokia_lcd.active_matrix[x][y]=0; /// 0 = blank space, 1=obstacle, 2=frog, 3=collectible star
     
     if(x%2!=0)  
     {
	if(y%3==0) 
	  nokia_lcd.active_matrix[x][y]=1; //draw black block
     }
     else  
     {	if(!(y%4 == 0 || y%4==3)) //draw grey block
	   nokia_lcd.active_matrix[x][y]=1;
     }
	if(x==15) 
	   if(y%2==0) 
	nokia_lcd.active_matrix[x][y]=1;
	   else
	nokia_lcd.active_matrix[x][y]=3;
	
}
  
   
}



void refresh_arena()
{
      //refer to game_map.png for further details
   uint8_t x,y;
   uint8_t buffer=0;

     
 nokia_lcd.active_matrix[frog_x][frog_y]=0; //replace frog with a blank space, so to ease matrix shifting, because frog isn't supposed to move

  for(x=1;x<15; x=x+2) //move up
  {
  for(y=0;y<6; y++)
  { 
	if(y==0)
	{
	   buffer=nokia_lcd.active_matrix[x][y];
	   nokia_lcd.active_matrix[x][y]=nokia_lcd.active_matrix[x][y+1];
	}
	else
	   nokia_lcd.active_matrix[x][y]=nokia_lcd.active_matrix[x][y+1];
	
  }
   nokia_lcd.active_matrix[x][6]=buffer;
   }
   
   
  for(x=2;x<15; x=x+2) //move down
  {
  for(y=6;y>0; y--)
  { 
	if(y==6)
	{
	   buffer=nokia_lcd.active_matrix[x][y];
	   nokia_lcd.active_matrix[x][y]=nokia_lcd.active_matrix[x][y-1];
	}
	else
	   nokia_lcd.active_matrix[x][y]=nokia_lcd.active_matrix[x][y-1];
	
  }
   nokia_lcd.active_matrix[x][0]=buffer;
   }
   
   if(nokia_lcd.active_matrix[frog_x][frog_y]== 0) // put frog back, wasn't hit
      nokia_lcd.active_matrix[frog_x][frog_y]=2;
	  
   if(nokia_lcd.active_matrix[frog_x][frog_y]==1)
	   died_idle=1;
   
}

void redraw_arena()
{
   
      uint8_t left_x=2,top_y=9; //these are not active matrix coords, but rather, the pixel coords of the 0,0 element in active matrix
      uint8_t x,y;
     for(x=0;x<16; x++)
     for(y=0;y<7; y++)
  {	
     
     nokia_lcd_set_cursor(left_x +x*5,top_y+y*5);
   	switch(nokia_lcd.active_matrix[x][y])
	{
	   
	   
	   case 0:
	   {
	   nokia_lcd_write_string("^",0); break; //space
	   }
	   case 1:
	   {
	      nokia_lcd_write_string("`",0); break; //obstacle
	   }
	   case 3:
	   {
	     nokia_lcd_write_string("*",0); break; //frog
	   }
	}
     }
     
}
  
  