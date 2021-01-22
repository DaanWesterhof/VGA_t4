//
// Created by daang on 1/20/2021.
//

#ifndef VGA_T4_VGA_SETTINGS_HPP
#define VGA_T4_VGA_SETTINGS_HPP

//##################### VGA Settings ############################

// Enable debug info (requires serial initialization)
//#define DEBUG

// Enable 12bits mode
// Default is 8bits RRRGGGBB (332)
// But 12bits GBB0RRRRGGGBB (444) feasible BUT NOT TESTED !!!!
//#define BITS12



#ifdef BITS12
typedef uint16_t vga_pixel;
#define VGA_RGB(r,g,b)  ( (((r>>3)&0x1f)<<11) | (((g>>2)&0x3f)<<5) | (((b>>3)&0x1f)<<0) )
#else
typedef uint8_t vga_pixel;
#define VGA_RGB(r,g,b)   ( (((r>>5)&0x07)<<5) | (((g>>5)&0x07)<<2) | (((b>>6)&0x3)<<0) )
#endif

#define MaxPolyPoint    100
#define AUDIO_SAMPLE_BUFFER_SIZE 256
#define DEFAULT_VSYNC_PIN 8

#ifndef ABS
#define ABS(X)  ((X) > 0 ? (X) : -(X))
#endif

//settings in cpp

#define TOP_BORDER    40
#define PIN_HBLANK    15

#ifdef BITS12
#define PIN_R_B3      5
#endif
#define PIN_R_B2      33
#define PIN_R_B1      4
#define PIN_R_B0      3
#define PIN_G_B2      2
#define PIN_G_B1      13
#define PIN_G_B0      11
#define PIN_B_B1      12
#define PIN_B_B0      10
#ifdef BITS12
#define PIN_B_B2      6
#define PIN_B_B3      9
#define PIN_G_B3      32
#endif

#define DMA_HACK      0x80

#define R16(rgb) ((rgb>>8)&0xf8)
#define G16(rgb) ((rgb>>3)&0xfc)
#define B16(rgb) ((rgb<<3)&0xf8)


// VGA 640x480@60Hz
// Screen refresh rate 60 Hz
// Vertical refresh    31.46875 kHz
// Pixel freq.         25.175 MHz
//
// Visible area        640  25.422045680238 us
// Front porch         16   0.63555114200596 us
// Sync pulse          96   3.8133068520357 us
// Back porch          48   1.9066534260179 us
// Whole line          800  31.777557100298 us

#define frame_freq     60.0     // Hz
#define line_freq      31.46875 // KHz
#define pix_freq       (line_freq*800) // KHz (25.175 MHz)

// pix_period = 39.7ns
// H-PULSE is 3.8133us = 3813.3ns => 96 pixels (see above for the rest)
#define frontporch_pix  20 //16
#define backporch_pix   44 //48

// Flexio Clock
// PLL3 SW CLOCK    (3) => 480 MHz
// PLL5 VIDEO CLOCK (2) => See formula for clock (we take 604200 KHz as /24 it gives 25175)
#define FLEXIO_CLK_SEL_PLL3 3
#define FLEXIO_CLK_SEL_PLL5 2


/* Set video PLL */
// There are /1, /2, /4, /8, /16 post dividers for the Video PLL.
// The output frequency can be set by programming the fields in the CCM_ANALOG_PLL_VIDEO,
// and CCM_ANALOG_MISC2 register sets according to the following equation.
// PLL output frequency = Fref * (DIV_SELECT + NUM/DENOM)

// nfact:
// This field controls the PLL loop divider.
// Valid range for DIV_SELECT divider value: 27~54.

#define POST_DIV_SELECT 2


//########### Game Engine Settings #######################

#define TILES_MAX_LAYERS  2

// 16x16 pixels tiles or 8x8 if USE_8PIXTILES is set
//#define USE_8PIXTILES 1
#ifdef USE_8PIXTILES
#define TILES_COLS        40
#define TILES_ROWS        30
#define TILES_W           8
#define TILES_H           8
#define TILES_HBITS       3
#define TILES_HMASK       0x7
#else
#define TILES_COLS        20
#define TILES_ROWS        15
#define TILES_W           16
#define TILES_H           16
#define TILES_HBITS       4
#define TILES_HMASK       0xf
#endif

// 32 sprites 16x32 or max 64 16x16 (not larger!!!)
#define SPRITES_MAX       32
#define SPRITES_W         16
#define SPRITES_H         32



#endif //VGA_T4_VGA_SETTINGS_HPP
