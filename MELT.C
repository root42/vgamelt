#include <stdio.h>
#include <stdlib.h>
#include <alloc.h>
#include <conio.h>
#include <dos.h>
#include <math.h>
#include <mem.h>

#include "gif.h"
#include "types.h"
#include "vga.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

byte far *framebuf;
byte pal[768];
struct image *img = NULL;

#define SETPIX(x,y,c) *(framebuf + (dword)SCREEN_WIDTH * (y) + (x)) = c
#define GETPIX(x,y) *(framebuf + (dword)SCREEN_WIDTH * (y) + (x))
#define GETIMG(x,y) *(img->data + (dword)img->width * ((y) % img->height) + ((x) % img->width))

void draw_melt( const int t )
{
   byte orig, msl;

   /* wait for 2 retraces to make it slow */
   wait_for_retrace();
   wait_for_retrace();

   /* get original MSL data */
   outportb( CRTC_INDEX, MAXIMUM_SCAN_LINE );
   orig = inportb( CRTC_DATA );

   /* only set MSL portion of register */
   msl = (orig & 0xE0) | (t & 0x1F);
   outportb( CRTC_INDEX, MAXIMUM_SCAN_LINE );
   outportb( CRTC_DATA, t );
}

void draw_melt2( const int t )
{
   int i;
   byte lo;

   /* disable interrupts, since we count scanlines */
   disable();

   /* store original value of line offset register */
   outportb( CRTC_INDEX, LINE_OFFSET );
   lo = inportb( CRTC_DATA );

   /* wait for the vertical retrace to finish */
   while( inp( INPUT_STATUS ) & VRETRACE );

   /* count the first t scanlines */
   for( i = 0; i < t; ++i ) {
      while( inportb( INPUT_STATUS ) & HRETRACE );
      while( !(inportb( INPUT_STATUS ) & HRETRACE) );
   }

   /* from here on, repeat every scanline's contents */
   outportb( CRTC_INDEX, LINE_OFFSET );
   outportb( CRTC_DATA, 0 );

   /* wait for the vertical retrace to start */
   while( !(inp( INPUT_STATUS ) & VRETRACE) );

   /* restore the line offset register */
   outportb( CRTC_INDEX, LINE_OFFSET );
   outportb( CRTC_DATA, lo );

   /* interrupts are allowed again */
   enable();
}


int main()
{
   byte kc = 0;
   int t = 0;
   int dt = 1;
   int draw_mode = 1;
   int max = 31;

   img = load_gif("pic.gif", 0);
   if(img == NULL ) {
      printf("couldn't load image\n");
      return 1;
   }

   /* initialize graphics and copy image */
   set_mode_y();
   set_palette(&img->palette[0][0]);
   copy2page(img->data,0,0,0,320,200);

   /* loop until ESC is pressed */
   while( kc != 0x1b ) {
      if( kbhit() ) {
         kc = getch();
         switch( kc ) {
	 case ' ':
	    /* space switches between the two effects */
	    t = 0;
            if( draw_mode == 0 ) {
               draw_melt( 1 );
               draw_mode = 1;
            } else {
               draw_mode = 0;
            }
            break;
         default:
            break;
         }
      }
      switch( draw_mode ) {
      case 0:
	 /* 31 is the max value of the MSL register */
	 max = 31;
         draw_melt(t);
         break;
      case 1:
	 /* VGA screen has 400 scan lines */
	 max = 400;
	 draw_melt2(t);
	 break;
      default:
         break;
      }
      /* do the ping-pong */
      t += dt;
      if( t < 0 || t > max ) {
         dt = -dt;
         t += dt;
      }
   }
   set_text_mode();
   return 0;
}
