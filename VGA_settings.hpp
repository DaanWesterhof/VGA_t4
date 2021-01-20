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
