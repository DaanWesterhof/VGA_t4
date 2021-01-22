/*
	This file is part of VGA_t4 library.

	VGA_t4 library is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Copyright (C) 2020 J-M Harvengt

	Inspired from the original Teensy3 uVGA library of Eric PREVOTEAU.
	QTIMER/FlexIO code based on Teensy4 examples of KurtE, Manitou and easone 
	from the Teensy4 forum (https://forum.pjrc.com)
*/

#include <Arduino.h>
#include "VGA_t4.h"
#include "VGA_font8x8.h"
#include "VGA_settings.hpp"

// Objective:
// generates VGA signal fully in hardware with as little as possible CPU help

// Principle:
// QTimer3 (timer3) used to generate H-PUSE and line interrupt (and V-PULSE)
// 2 FlexIO shift registers (1 and 2) and 2 DMA channels used to generate
// RGB out, combined to create 8bits(/12bits) output.

// Note:
// - supported resolutions: 320x240,320x480,640x240 and 640x480 pixels
// - experimental resolution: 352x240,352x480
// - experimental resolution: 512x240,512x480 (not stable)
// - video memory is allocated using malloc in T4 heap
// - as the 2 DMA transfers are not started exactly at same time, there is a bit of color smearing 
//   but tried to be compensated by pixel shifting 
// - Default is 8bits RRRGGGBB (332) 
//   But 12bits GBB0RRRRGGGBB (444) feasible BUT NOT TESTED !!!!
// - Only ok at 600MHz else some disturbances visible





// Full buffer including back/front porch 
static vga_pixel * gfxbuffer __attribute__((aligned(32))) = NULL;
static uint32_t dstbuffer __attribute__((aligned(32)));

// Visible vuffer
static vga_pixel * framebuffer;
static int  fb_width;
static int  fb_height;
static int  fb_stride;
static int  maxpixperline;
static int  left_border;
static int  right_border;
static int  line_double;
static int  pix_shift;
static int  ref_div_select;
static int  ref_freq_num;
static int  ref_freq_denom;
static int  ref_pix_shift;
static int  combine_shiftreg;

#ifdef DEBUG
static uint32_t   ISRTicks_prev = 0;
volatile uint32_t ISRTicks = 0;
#endif 

uint8_t    VGA_T4::VGA_Handler::_vsync_pin = -1;
DMAChannel VGA_T4::VGA_Handler::flexio1DMA;
DMAChannel VGA_T4::VGA_Handler::flexio2DMA;
static volatile uint32_t VSYNC = 0;
static volatile uint32_t currentLine=0;
#define NOP asm volatile("nop\n\t");



PolyDef	PolySet;  // will contain a polygon data

//absoluteley nessicairy for callback functions of ISR

FASTRUN void QT3_isr() {
  TMR3_SCTRL3 &= ~(TMR_SCTRL_TCF);
  TMR3_CSCTRL3 &= ~(TMR_CSCTRL_TCF1|TMR_CSCTRL_TCF2);

  cli();
  
  // V-PULSE
  if (currentLine > 0) {
    digitalWrite(VGA_T4::VGA_Handler::_vsync_pin, 1);
    VSYNC = 0;
  } else {
    digitalWrite(VGA_T4::VGA_Handler::_vsync_pin, 0);
    VSYNC = 1;
  }
  
  currentLine++;
  currentLine = currentLine % 525;


  uint32_t y = (currentLine - TOP_BORDER) >> line_double;
  // Visible area  
  if (y >= 0 && y < fb_height) {  
    // Disable DMAs
    //DMA_CERQ = flexio2DMA.channel;
    //DMA_CERQ = flexio1DMA.channel; 

    // Setup src adress
    // Aligned 32 bits copy
    unsigned long * p=(uint32_t *)&gfxbuffer[fb_stride*y];
    VGA_T4::VGA_Handler::flexio2DMA.TCD->SADDR = p;
    if (pix_shift & DMA_HACK) 
    {
      // Unaligned copy
      uint8_t * p2=(uint8_t *)&gfxbuffer[fb_stride*y+(pix_shift&0xf)];
      VGA_T4::VGA_Handler::flexio1DMA.TCD->SADDR = p2;
    }
    else  {
      p=(uint32_t *)&gfxbuffer[fb_stride*y+(pix_shift&0xc)]; // multiple of 4
      VGA_T4::VGA_Handler::flexio1DMA.TCD->SADDR = p;
    }

    // Enable DMAs
    DMA_SERQ = VGA_T4::VGA_Handler::flexio2DMA.channel;
    DMA_SERQ = VGA_T4::VGA_Handler::flexio1DMA.channel;
    arm_dcache_flush_delete((void*)((uint32_t *)&gfxbuffer[fb_stride*y]), fb_stride);
  }
  sei();  

#ifdef DEBUG
  ISRTicks++; 
#endif  
  asm volatile("dsb");
}


VGA_T4::VGA_Handler::VGA_Handler(int vsync_pin) {
    _vsync_pin = vsync_pin;

}



static void set_videoClock(int nfact, int32_t nmult, uint32_t ndiv, bool force) // sets PLL5
{
//if (!force && (CCM_ANALOG_PLL_VIDEO & CCM_ANALOG_PLL_VIDEO_ENABLE)) return;

    CCM_ANALOG_PLL_VIDEO = CCM_ANALOG_PLL_VIDEO_BYPASS | CCM_ANALOG_PLL_VIDEO_ENABLE
 			             | CCM_ANALOG_PLL_VIDEO_POST_DIV_SELECT(1) // 2: 1/1; 1: 1/2; 0: 1/4
 			             | CCM_ANALOG_PLL_VIDEO_DIV_SELECT(nfact);  
 	CCM_ANALOG_PLL_VIDEO_NUM   = nmult /*& CCM_ANALOG_PLL_VIDEO_NUM_MASK*/;
 	CCM_ANALOG_PLL_VIDEO_DENOM = ndiv /*& CCM_ANALOG_PLL_VIDEO_DENOM_MASK*/;  	
 	CCM_ANALOG_PLL_VIDEO &= ~CCM_ANALOG_PLL_VIDEO_POWERDOWN;//Switch on PLL
 	while (!(CCM_ANALOG_PLL_VIDEO & CCM_ANALOG_PLL_VIDEO_LOCK)) {}; //Wait for pll-lock  	
   	
   	const int div_post_pll = 1; // other values: 2,4
  
  	if(div_post_pll>3) CCM_ANALOG_MISC2 |= CCM_ANALOG_MISC2_DIV_MSB;  	
  	CCM_ANALOG_PLL_VIDEO &= ~CCM_ANALOG_PLL_VIDEO_BYPASS;//Disable Bypass
}

void VGA_T4::VGA_Handler::tweak_video(int shiftdelta, int numdelta, int denomdelta)
{
  if ( (numdelta != 0) || (denomdelta != 0) )   {
    set_videoClock(ref_div_select,ref_freq_num+numdelta,ref_freq_denom+denomdelta,true);  
  }  
  if (shiftdelta != 0) {
    pix_shift = ref_pix_shift + shiftdelta;
  }
}

// display VGA image
vga_error_t VGA_T4::VGA_Handler::begin(vga_mode_t mode)
{
  uint32_t flexio_clock_div;
  combine_shiftreg = 0;
//  int div_select = 49;
//  int num = 135;  
//  int denom = 100;  
  int div_select = 20;
  int num = 9800;
  int denom = 10000;  
  int flexio_clk_sel = FLEXIO_CLK_SEL_PLL5;   
  int flexio_freq = ( 24000*div_select + (num*24000)/denom )/POST_DIV_SELECT;
  set_videoClock(div_select,num,denom,true);
  switch(mode) {
      case vga_mode_t::VGA_MODE_320x240:
          left_border = backporch_pix/2;
          right_border = frontporch_pix/2;
          fb_width = 320;
          fb_height = 240 ;
          fb_stride = left_border+fb_width+right_border;
          maxpixperline = fb_stride;
          flexio_clock_div = flexio_freq/(pix_freq/2);
          line_double = 1;
          pix_shift = 2+DMA_HACK;
          break;
      case vga_mode_t::VGA_MODE_320x480:
          left_border = backporch_pix/2;
          right_border = frontporch_pix/2;
          fb_width = 320;
          fb_height = 480 ;
          fb_stride = left_border+fb_width+right_border;
          maxpixperline = fb_stride;
          flexio_clock_div = flexio_freq/(pix_freq/2);
          line_double = 0;
          pix_shift = 2+DMA_HACK;
          break;
      case vga_mode_t::VGA_MODE_640x240:
          left_border = backporch_pix;
          right_border = frontporch_pix;
          fb_width = 640;
          fb_height = 240 ;
          fb_stride = left_border+fb_width+right_border;
          maxpixperline = fb_stride;
          flexio_clock_div = flexio_freq/pix_freq;
          line_double = 1;
          pix_shift = 4;
          combine_shiftreg = 1;
          break;
      case vga_mode_t::VGA_MODE_640x480:
          left_border = backporch_pix;
          right_border = frontporch_pix;
          fb_width = 640;
          fb_height = 480 ;
          fb_stride = left_border+fb_width+right_border;
          maxpixperline = fb_stride;
          flexio_clock_div = (flexio_freq/pix_freq);
          line_double = 0;
          pix_shift = 4;
          combine_shiftreg = 1;
          break;
      case vga_mode_t::VGA_MODE_512x240:
          left_border = backporch_pix/1.3;
          right_border = frontporch_pix/1.3;
          fb_width = 512;
          fb_height = 240 ;
          fb_stride = left_border+fb_width+right_border;
          maxpixperline = fb_stride;
          flexio_clock_div = flexio_freq/(pix_freq/1.3)+2;
          line_double = 1;
          pix_shift = 0;
          break;
      case vga_mode_t::VGA_MODE_512x480:
          left_border = backporch_pix/1.3;
          right_border = frontporch_pix/1.3;
          fb_width = 512;
          fb_height = 480 ;
          fb_stride = left_border+fb_width+right_border;
          maxpixperline = fb_stride;
          flexio_clock_div = flexio_freq/(pix_freq/1.3)+2;
          line_double = 0;
          pix_shift = 0;
          break;
      case vga_mode_t::VGA_MODE_352x240:
          left_border = backporch_pix/1.75;
          right_border = frontporch_pix/1.75;
          fb_width = 352;
          fb_height = 240 ;
          fb_stride = left_border+fb_width+right_border;
          maxpixperline = fb_stride;
          flexio_clock_div = flexio_freq/(pix_freq/1.75)+2;
          line_double = 1;
          pix_shift = 2+DMA_HACK;
          break;
      case vga_mode_t::VGA_MODE_352x480:
          left_border = backporch_pix/1.75;
          right_border = frontporch_pix/1.75;
          fb_width = 352;
          fb_height = 480 ;
          fb_stride = left_border+fb_width+right_border;
          maxpixperline = fb_stride;
          flexio_clock_div = flexio_freq/(pix_freq/1.75)+2;
          line_double = 0;
          pix_shift = 2+DMA_HACK;
          break;
  }	

  // Save param for tweek adjustment
  ref_div_select = div_select;
  ref_freq_num = num;
  ref_freq_denom = denom;
  ref_pix_shift = pix_shift;

  Serial.println("frequency");
  Serial.println(flexio_freq);
  Serial.println("div");
  Serial.println(flexio_freq/pix_freq);

  pinMode(_vsync_pin, OUTPUT);
  pinMode(PIN_HBLANK, OUTPUT);

  /* Basic pin setup FlexIO1 */
  pinMode(PIN_G_B2, OUTPUT);  // FlexIO1:4 = 0x10
  pinMode(PIN_R_B0, OUTPUT);  // FlexIO1:5 = 0x20
  pinMode(PIN_R_B1, OUTPUT);  // FlexIO1:6 = 0x40
  pinMode(PIN_R_B2, OUTPUT);  // FlexIO1:7 = 0x80
#ifdef BITS12
  pinMode(PIN_R_B3, OUTPUT);  // FlexIO1:8 = 0x100
#endif
  /* Basic pin setup FlexIO2 */
  pinMode(PIN_B_B0, OUTPUT);  // FlexIO2:0 = 0x00001
  pinMode(PIN_B_B1, OUTPUT);  // FlexIO2:1 = 0x00002
  pinMode(PIN_G_B0, OUTPUT);  // FlexIO2:2 = 0x00004
  pinMode(PIN_G_B1, OUTPUT);  // FlexIO2:3 = 0x00008
#ifdef BITS12
  pinMode(PIN_B_B2, OUTPUT);  // FlexIO2:10 = 0x00400
  pinMode(PIN_B_B3, OUTPUT);  // FlexIO2:11 = 0x00800
  pinMode(PIN_G_B3, OUTPUT);  // FlexIO2:12 = 0x01000
#endif

  /* High speed and drive strength configuration */
  *(portControlRegister(PIN_G_B2)) = 0xFF; 
  *(portControlRegister(PIN_R_B0)) = 0xFF;
  *(portControlRegister(PIN_R_B1)) = 0xFF;
  *(portControlRegister(PIN_R_B2)) = 0xFF;
#ifdef BITS12
  *(portControlRegister(PIN_R_B3)) = 0xFF;
#endif
  *(portControlRegister(PIN_B_B0)) = 0xFF; 
  *(portControlRegister(PIN_B_B1)) = 0xFF;
  *(portControlRegister(PIN_G_B0)) = 0xFF;
  *(portControlRegister(PIN_G_B1)) = 0xFF;
#ifdef BITS12  
  *(portControlRegister(PIN_B_B2))  = 0xFF;
  *(portControlRegister(PIN_B_B3))  = 0xFF;
  *(portControlRegister(PIN_G_B3)) = 0xFF;
#endif


  /* Set clock for FlexIO1 and FlexIO2 */
  CCM_CCGR5 &= ~CCM_CCGR5_FLEXIO1(CCM_CCGR_ON);
  CCM_CDCDR = (CCM_CDCDR & ~(CCM_CDCDR_FLEXIO1_CLK_SEL(3) | CCM_CDCDR_FLEXIO1_CLK_PRED(7) | CCM_CDCDR_FLEXIO1_CLK_PODF(7))) 
    | CCM_CDCDR_FLEXIO1_CLK_SEL(flexio_clk_sel) | CCM_CDCDR_FLEXIO1_CLK_PRED(0) | CCM_CDCDR_FLEXIO1_CLK_PODF(0);
  CCM_CCGR3 &= ~CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
  CCM_CSCMR2 = (CCM_CSCMR2 & ~(CCM_CSCMR2_FLEXIO2_CLK_SEL(3))) | CCM_CSCMR2_FLEXIO2_CLK_SEL(flexio_clk_sel);
  CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_FLEXIO2_CLK_PRED(7)|CCM_CS1CDR_FLEXIO2_CLK_PODF(7)) )
    | CCM_CS1CDR_FLEXIO2_CLK_PRED(0) | CCM_CS1CDR_FLEXIO2_CLK_PODF(0);


 /* Set up pin mux FlexIO1 */
  *(portConfigRegister(PIN_G_B2)) = 0x14;
  *(portConfigRegister(PIN_R_B0)) = 0x14;
  *(portConfigRegister(PIN_R_B1)) = 0x14;
  *(portConfigRegister(PIN_R_B2)) = 0x14;
#ifdef BITS12
  *(portConfigRegister(PIN_R_B3)) = 0x14;
#endif
  /* Set up pin mux FlexIO2 */
  *(portConfigRegister(PIN_B_B0)) = 0x14;
  *(portConfigRegister(PIN_B_B1)) = 0x14;
  *(portConfigRegister(PIN_G_B0)) = 0x14;
  *(portConfigRegister(PIN_G_B1)) = 0x14;
#ifdef BITS12
  *(portConfigRegister(PIN_B_B2)) = 0x14;
  *(portConfigRegister(PIN_B_B3)) = 0x14;
  *(portConfigRegister(PIN_G_B3)) = 0x14;
#endif

  /* Enable the clock */
  CCM_CCGR5 |= CCM_CCGR5_FLEXIO1(CCM_CCGR_ON);
  CCM_CCGR3 |= CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
  /* Enable the FlexIO with fast access */
  FLEXIO1_CTRL = FLEXIO_CTRL_FLEXEN | FLEXIO_CTRL_FASTACC;
  FLEXIO2_CTRL = FLEXIO_CTRL_FLEXEN | FLEXIO_CTRL_FASTACC;

  uint32_t timerSelect, timerPolarity, pinConfig, pinSelect, pinPolarity, shifterMode, parallelWidth, inputSource, stopBit, startBit;
  uint32_t triggerSelect, triggerPolarity, triggerSource, timerMode, timerOutput, timerDecrement, timerReset, timerDisable, timerEnable;

  /* Shifter 0 registers for FlexIO2 */ 
#ifdef BITS12
  parallelWidth = FLEXIO_SHIFTCFG_PWIDTH(16); // 16-bit parallel shift width
  pinSelect = FLEXIO_SHIFTCTL_PINSEL(0);      // Select pins FXIO_D0 through FXIO_D12
#else
  parallelWidth = FLEXIO_SHIFTCFG_PWIDTH(4);  // 8-bit parallel shift width
  pinSelect = FLEXIO_SHIFTCTL_PINSEL(0);      // Select pins FXIO_D0 through FXIO_D3
#endif
  inputSource = FLEXIO_SHIFTCFG_INSRC*(1);    // Input src from next shifter
  stopBit = FLEXIO_SHIFTCFG_SSTOP(0);         // Stop bit disabled
  startBit = FLEXIO_SHIFTCFG_SSTART(0);       // Start bit disabled, transmitter loads data on enable 
  timerSelect = FLEXIO_SHIFTCTL_TIMSEL(0);    // Use timer 0
  timerPolarity = FLEXIO_SHIFTCTL_TIMPOL*(1); // Shift on negedge of clock 
  pinConfig = FLEXIO_SHIFTCTL_PINCFG(3);      // Shifter pin output
  pinPolarity = FLEXIO_SHIFTCTL_PINPOL*(0);   // Shifter pin active high polarity
  shifterMode = FLEXIO_SHIFTCTL_SMOD(2);      // Shifter transmit mode
  /* Shifter 0 registers for FlexIO1 */
  FLEXIO2_SHIFTCFG0 = parallelWidth | inputSource | stopBit | startBit;
  FLEXIO2_SHIFTCTL0 = timerSelect | timerPolarity | pinConfig | pinSelect | pinPolarity | shifterMode;

  /* Shifter 0 registers for FlexIO1 */ 
#ifdef BITS12
  parallelWidth = FLEXIO_SHIFTCFG_PWIDTH(5);  // 5-bit parallel shift width
  pinSelect = FLEXIO_SHIFTCTL_PINSEL(4);      // Select pins FXIO_D4 through FXIO_D8
#else
  parallelWidth = FLEXIO_SHIFTCFG_PWIDTH(4);  // 8-bit parallel shift width
  pinSelect = FLEXIO_SHIFTCTL_PINSEL(4);      // Select pins FXIO_D4 through FXIO_D7
#endif  
  FLEXIO1_SHIFTCFG0 = parallelWidth | inputSource | stopBit | startBit;
  FLEXIO1_SHIFTCTL0 = timerSelect | timerPolarity | pinConfig | pinSelect | pinPolarity | shifterMode;
  
  if (combine_shiftreg) {
    pinConfig = FLEXIO_SHIFTCTL_PINCFG(0);    // Shifter pin output disabled
    FLEXIO2_SHIFTCFG1 = parallelWidth | inputSource | stopBit | startBit;
    FLEXIO2_SHIFTCTL1 = timerSelect | timerPolarity | pinConfig | shifterMode;
    FLEXIO1_SHIFTCFG1 = parallelWidth | inputSource | stopBit | startBit;
    FLEXIO1_SHIFTCTL1 = timerSelect | timerPolarity | pinConfig | shifterMode;
  }
  /* Timer 0 registers for FlexIO2 */ 
  timerOutput = FLEXIO_TIMCFG_TIMOUT(1);      // Timer output is logic zero when enabled and is not affected by the Timer reset
  timerDecrement = FLEXIO_TIMCFG_TIMDEC(0);   // Timer decrements on FlexIO clock, shift clock equals timer output
  timerReset = FLEXIO_TIMCFG_TIMRST(0);       // Timer never reset
  timerDisable = FLEXIO_TIMCFG_TIMDIS(2);     // Timer disabled on Timer compare
  timerEnable = FLEXIO_TIMCFG_TIMENA(2);      // Timer enabled on Trigger assert
  stopBit = FLEXIO_TIMCFG_TSTOP(0);           // Stop bit disabled
  startBit = FLEXIO_TIMCFG_TSTART*(0);        // Start bit disabled
  if (combine_shiftreg) {
    triggerSelect = FLEXIO_TIMCTL_TRGSEL(1+4*(1)); // Trigger select Shifter 1 status flag
  }
  else {
    triggerSelect = FLEXIO_TIMCTL_TRGSEL(1+4*(0)); // Trigger select Shifter 0 status flag
  }    
  triggerPolarity = FLEXIO_TIMCTL_TRGPOL*(1); // Trigger active low
  triggerSource = FLEXIO_TIMCTL_TRGSRC*(1);   // Internal trigger selected
  pinConfig = FLEXIO_TIMCTL_PINCFG(0);        // Timer pin output disabled
  //pinSelect = FLEXIO_TIMCTL_PINSEL(0);        // Select pin FXIO_D0
  //pinPolarity = FLEXIO_TIMCTL_PINPOL*(0);     // Timer pin polarity active high
  timerMode = FLEXIO_TIMCTL_TIMOD(1);         // Dual 8-bit counters baud mode
  // flexio_clock_div : Output clock frequency is N times slower than FlexIO clock (41.7 ns period) (23.980MHz?)

  int shifts_per_transfer;
#ifdef BITS12
  shifts_per_transfer = 8;
#else
  if (combine_shiftreg) {
    shifts_per_transfer = 8; // Shift out 8 times with every transfer = 64-bit word = contents of Shifter 0+1
  }
  else {
    shifts_per_transfer = 4; // Shift out 4 times with every transfer = 32-bit word = contents of Shifter 0 
  }
#endif
  FLEXIO2_TIMCFG0 = timerOutput | timerDecrement | timerReset | timerDisable | timerEnable | stopBit | startBit;
  FLEXIO2_TIMCTL0 = triggerSelect | triggerPolarity | triggerSource | pinConfig /*| pinSelect | pinPolarity*/ | timerMode;
  FLEXIO2_TIMCMP0 = ((shifts_per_transfer*2-1)<<8) | ((flexio_clock_div/2-1)<<0);
  /* Timer 0 registers for FlexIO1 */
  FLEXIO1_TIMCFG0 = timerOutput | timerDecrement | timerReset | timerDisable | timerEnable | stopBit | startBit;
  FLEXIO1_TIMCTL0 = triggerSelect | triggerPolarity | triggerSource | pinConfig /*| pinSelect | pinPolarity*/ | timerMode;
  FLEXIO1_TIMCMP0 = ((shifts_per_transfer*2-1)<<8) | ((flexio_clock_div/2-1)<<0);
#ifdef DEBUG
  Serial.println("FlexIO setup complete");
#endif

  /* Enable DMA trigger on Shifter0, DMA request is generated when data is transferred from buffer0 to shifter0 */ 
  if (combine_shiftreg) {
    FLEXIO2_SHIFTSDEN |= (1<<1); 
    FLEXIO1_SHIFTSDEN |= (1<<1);
  }
  else {
    FLEXIO2_SHIFTSDEN |= (1<<0); 
    FLEXIO1_SHIFTSDEN |= (1<<0);
  }
  /* Disable DMA channel so it doesn't start transferring yet */
  flexio1DMA.disable();
  flexio2DMA.disable();
  /* Set up DMA channel to use Shifter 0 trigger */
  flexio1DMA.triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO1_REQUEST0);
  flexio2DMA.triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO2_REQUEST0);


  if (combine_shiftreg) {
    flexio2DMA.TCD->NBYTES = 8;
    flexio2DMA.TCD->SOFF = 4;
    flexio2DMA.TCD->SLAST = -maxpixperline;
    flexio2DMA.TCD->BITER = maxpixperline / 8;
    flexio2DMA.TCD->CITER = maxpixperline / 8;
    flexio2DMA.TCD->DADDR = &FLEXIO2_SHIFTBUF0;
    flexio2DMA.TCD->DOFF = 0;
    flexio2DMA.TCD->DLASTSGA = 0;
    flexio2DMA.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(3); // 32bits => 64bits
    flexio2DMA.TCD->CSR |= DMA_TCD_CSR_DREQ;
  
    flexio1DMA.TCD->NBYTES = 8;
    flexio1DMA.TCD->SOFF = 4;
    flexio1DMA.TCD->SLAST = -maxpixperline;
    flexio1DMA.TCD->BITER = maxpixperline / 8;
    flexio1DMA.TCD->CITER = maxpixperline / 8;
    flexio1DMA.TCD->DADDR = &FLEXIO1_SHIFTBUFNBS0;
    flexio1DMA.TCD->DOFF = 0;
    flexio1DMA.TCD->DLASTSGA = 0;
    flexio1DMA.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(3); // 32bits => 64bits
    flexio1DMA.TCD->CSR |= DMA_TCD_CSR_DREQ;     
  }
  else {
    // Setup DMA2 Flexio2 copy
    flexio2DMA.TCD->NBYTES = 4;
    flexio2DMA.TCD->SOFF = 4;
    flexio2DMA.TCD->SLAST = -maxpixperline;
    flexio2DMA.TCD->BITER = maxpixperline / 4;
    flexio2DMA.TCD->CITER = maxpixperline / 4;
    flexio2DMA.TCD->DADDR = &FLEXIO2_SHIFTBUF0;
    flexio2DMA.TCD->DOFF = 0;
    flexio2DMA.TCD->DLASTSGA = 0;
    flexio2DMA.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2); // 32bits
    flexio2DMA.TCD->CSR |= DMA_TCD_CSR_DREQ;

    // Setup DMA1 Flexio1 copy
    // Use pixel shift to avoid color smearing?
    if (pix_shift & DMA_HACK)
    {
      if (pix_shift & 0x3 == 0) {
        // Aligned 32 bits copy (32bits to 32bits)
        flexio1DMA.TCD->NBYTES = 4;
        flexio1DMA.TCD->SOFF = 4;
        flexio1DMA.TCD->SLAST = -maxpixperline;
        flexio1DMA.TCD->BITER = maxpixperline / 4;
        flexio1DMA.TCD->CITER = maxpixperline / 4;
        flexio1DMA.TCD->DADDR = &FLEXIO1_SHIFTBUFNBS0;
        flexio1DMA.TCD->DOFF = 0;
        flexio1DMA.TCD->DLASTSGA = 0;
        flexio1DMA.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2); // 32bits to 32bits
        flexio1DMA.TCD->CSR |= DMA_TCD_CSR_DREQ;
      }
      else {
        // Unaligned (src) 32 bits copy (8bits to 32bits)
        flexio1DMA.TCD->NBYTES = 4;
        flexio1DMA.TCD->SOFF = 1;
        flexio1DMA.TCD->SLAST = -maxpixperline;
        flexio1DMA.TCD->BITER = maxpixperline / 4;
        flexio1DMA.TCD->CITER = maxpixperline / 4;
        flexio1DMA.TCD->DADDR = &FLEXIO1_SHIFTBUFNBS0;
        flexio1DMA.TCD->DOFF = 0;
        flexio1DMA.TCD->DLASTSGA = 0;
        flexio1DMA.TCD->ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(2); // 8bits to 32bits
        flexio1DMA.TCD->CSR |= DMA_TCD_CSR_DREQ; // disable on completion
      }
    }
    else 
    {
        // Aligned 32 bits copy
        flexio1DMA.TCD->NBYTES = 4;
        flexio1DMA.TCD->SOFF = 4;
        flexio1DMA.TCD->SLAST = -maxpixperline;
        flexio1DMA.TCD->BITER = maxpixperline / 4;
        flexio1DMA.TCD->CITER = maxpixperline / 4;
        flexio1DMA.TCD->DADDR = &FLEXIO1_SHIFTBUFNBS0;
        flexio1DMA.TCD->DOFF = 0;
        flexio1DMA.TCD->DLASTSGA = 0;
        flexio1DMA.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2); // 32bits
        flexio1DMA.TCD->CSR |= DMA_TCD_CSR_DREQ;
    }    
  }

#ifdef DEBUG
  Serial.println("DMA setup complete");
#endif

  // enable clocks for QTIMER3: generates the 15KHz for hsync
  // Pulse:
  // low  : 3.8133 us => 569x6.7ns
  // total: 31.777 us => 4743x6.7ns (high = 4174x6.7ns)
  // (OLD TEST)
  // (4us low, 28us high => 32us)
  // (597x6.7ns for 4us)
  // (4179x6.7ns for 28us) 
  CCM_CCGR6 |= 0xC0000000;              //enable clocks to CG15 of CGR6 for QT3
  //configure QTIMER3 Timer3 for test of alternating Compare1 and Compare2
  
  #define MARGIN_N 1005 // 1206 at 720MHz //1005 at 600MHz
  #define MARGIN_D 1000

  TMR3_CTRL3 = 0b0000000000100000;      //stop all functions of timer 
  // Invert output pin as we want the interupt on rising edge
  TMR3_SCTRL3 = 0b0000000000000011;     //0(TimerCompareFlag),0(TimerCompareIntEnable),00(TimerOverflow)0000(NoCapture),0000(Capture Disabled),00, 1(INV output),1(OFLAG to Ext Pin)
  TMR3_CNTR3 = 0;
  TMR3_LOAD3 = 0;
  /* Inverted timings */
  TMR3_COMP13 = ((4174*MARGIN_N)/MARGIN_D)-1;
  TMR3_CMPLD13 = ((4174*MARGIN_N)/MARGIN_D)-1;
  TMR3_COMP23 = ((569*MARGIN_N)/MARGIN_D)-1;
  TMR3_CMPLD23 = ((569*MARGIN_N)/MARGIN_D)-1;

  TMR3_CSCTRL3 = 0b0000000010000101;    //Compare1 only enabled - Compare Load1 control and Compare Load2 control both on
  TMR3_CTRL3 = 0b0011000000100100;      // 001(Count rising edges Primary Source),1000(IP Bus Clock),00 (Secondary Source), 
                                        // 0(Count Once),1(Count up to Compare),0(Count Up),0(Co Channel Init),100(Toggle OFLAG on alternating Compare1/Compare2)
  //configure Teensy pin Compare output
  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_03 = 1;      // QT3 Timer3 is now on pin 15
  attachInterruptVector(IRQ_QTIMER3, QT3_isr);  //declare which routine performs the ISR function
  NVIC_ENABLE_IRQ(IRQ_QTIMER3);  
#ifdef DEBUG
  Serial.println("QTIMER3 setup complete");
  Serial.print("V-PIN is ");
  Serial.println(_vsync_pin);
#endif

  /* initialize gfx buffer */
  if (gfxbuffer == NULL) gfxbuffer = (vga_pixel*)malloc(fb_stride*fb_height*sizeof(vga_pixel)+4); // 4bytes for pixel shift
  if (gfxbuffer == NULL) return(vga_error_t::VGA_ERROR);

  memset((void*)&gfxbuffer[0],0, fb_stride*fb_height*sizeof(vga_pixel)+4);
  framebuffer = (vga_pixel*)&gfxbuffer[left_border];

  return(vga_error_t::VGA_OK);
}

void VGA_T4::VGA_Handler::end()
{
  cli(); 
  /* Disable DMA channel so it doesn't start transferring yet */
  flexio1DMA.disable();
  flexio2DMA.disable(); 
  FLEXIO2_SHIFTSDEN &= ~(1<<0);
  FLEXIO1_SHIFTSDEN &= ~(1<<0);
  /* disable clocks for flexio and qtimer */
  CCM_CCGR5 &= ~CCM_CCGR5_FLEXIO1(CCM_CCGR_ON);
  CCM_CCGR3 &= ~CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
  CCM_CCGR6 &= ~0xC0000000;
  sei(); 
  delay(50);
  if (gfxbuffer != NULL) free(gfxbuffer); 
}

void debug()
{
#ifdef DEBUG
  delay(1000);
  uint32_t t=ISRTicks;
  if (ISRTicks_prev != 0) Serial.println(t-ISRTicks_prev);
  ISRTicks_prev = t;
#endif  
}

// retrieve size of the frame buffer
void VGA_T4::VGA_Handler::get_frame_buffer_size(int *width, int *height)
{
  *width = fb_width;
  *height = fb_height;
}

void VGA_T4::VGA_Handler::waitSync()
{
  while (VSYNC == 0) {};
}

void VGA_T4::VGA_Handler::waitLine(int line)
{
  while (currentLine != line) {};
}

void VGA_T4::VGA_Handler::clear(vga_pixel color) {
  int i,j;
  for (j=0; j<fb_height; j++)
  {
    vga_pixel * dst=&framebuffer[j*fb_stride];
    for (i=0; i<fb_width; i++)
    {
      *dst++ = color;
    }
  }
}


void VGA_T4::VGA_Handler::drawPixel(int x, int y, vga_pixel color){
	if((x>=0) && (x<=fb_width) && (y>=0) && (y<=fb_height))
		framebuffer[y*fb_stride+x] = color;
}

vga_pixel VGA_T4::VGA_Handler::getPixel(int x, int y){
  return(framebuffer[y*fb_stride+x]);
}

vga_pixel * VGA_T4::VGA_Handler::getLineBuffer(int j) {
  return (&framebuffer[j*fb_stride]);
}

void VGA_T4::VGA_Handler::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, vga_pixel color) {
  int i,j,l=y;
  for (j=0; j<h; j++)
  {
    vga_pixel * dst=&framebuffer[l*fb_stride+x];
    for (i=0; i<w; i++)
    {
      *dst++ = color;
    }
    l++;
  }
}

void VGA_T4::VGA_Handler::drawText(int16_t x, int16_t y, const char * text, vga_pixel fgcolor, vga_pixel bgcolor, bool doublesize) {
  vga_pixel c;
  vga_pixel * dst;
  
  while ((c = *text++)) {
    const unsigned char * charpt=&font8x8[c][0];

    int l=y;
    for (int i=0;i<8;i++)
    {     
      unsigned char bits;
      if (doublesize) {
        dst=&framebuffer[l*fb_stride+x];
        bits = *charpt;     
        if (bits&0x01) *dst++=fgcolor;
        else *dst++=bgcolor;
        bits = bits >> 1;     
        if (bits&0x01) *dst++=fgcolor;
        else *dst++=bgcolor;
        bits = bits >> 1;     
        if (bits&0x01) *dst++=fgcolor;
        else *dst++=bgcolor;
        bits = bits >> 1;     
        if (bits&0x01) *dst++=fgcolor;
        else *dst++=bgcolor;
        bits = bits >> 1;     
        if (bits&0x01) *dst++=fgcolor;
        else *dst++=bgcolor;
        bits = bits >> 1;     
        if (bits&0x01) *dst++=fgcolor;
        else *dst++=bgcolor;
        bits = bits >> 1;     
        if (bits&0x01) *dst++=fgcolor;
        else *dst++=bgcolor;
        bits = bits >> 1;     
        if (bits&0x01) *dst++=fgcolor;
        else *dst++=bgcolor;
        l++;
      }
      dst=&framebuffer[l*fb_stride+x]; 
      bits = *charpt++;     
      if (bits&0x01) *dst++=fgcolor;
      else *dst++=bgcolor;
      bits = bits >> 1;     
      if (bits&0x01) *dst++=fgcolor;
      else *dst++=bgcolor;
      bits = bits >> 1;     
      if (bits&0x01) *dst++=fgcolor;
      else *dst++=bgcolor;
      bits = bits >> 1;     
      if (bits&0x01) *dst++=fgcolor;
      else *dst++=bgcolor;
      bits = bits >> 1;     
      if (bits&0x01) *dst++=fgcolor;
      else *dst++=bgcolor;
      bits = bits >> 1;     
      if (bits&0x01) *dst++=fgcolor;
      else *dst++=bgcolor;
      bits = bits >> 1;     
      if (bits&0x01) *dst++=fgcolor;
      else *dst++=bgcolor;
      bits = bits >> 1;     
      if (bits&0x01) *dst++=fgcolor;
      else *dst++=bgcolor;
      l++;
    }
    x +=8;
  } 
}

void VGA_T4::VGA_Handler::drawSprite(int16_t x, int16_t y, const int16_t *bitmap) {
    drawSprite(x,y,bitmap, 0,0,0,0);
}

void VGA_T4::VGA_Handler::drawSprite(int16_t x, int16_t y, const int16_t *bitmap, uint16_t arx, uint16_t ary, uint16_t arw, uint16_t arh)
{
  int bmp_offx = 0;
  int bmp_offy = 0;
  int16_t *bmp_ptr;
    
  int w =*bitmap++;
  int h = *bitmap++;


  if ( (arw == 0) || (arh == 0) ) {
    // no crop window
    arx = x;
    ary = y;
    arw = w;
    arh = h;
  }
  else {
    if ( (x>(arx+arw)) || ((x+w)<arx) || (y>(ary+arh)) || ((y+h)<ary)   ) {
      return;
    }
    
    // crop area
    if ( (x > arx) && (x<(arx+arw)) ) { 
      arw = arw - (x-arx);
      arx = arx + (x-arx);
    } else {
      bmp_offx = arx;
    }
    if ( ((x+w) > arx) && ((x+w)<(arx+arw)) ) {
      arw -= (arx+arw-x-w);
    }  
    if ( (y > ary) && (y<(ary+arh)) ) {
      arh = arh - (y-ary);
      ary = ary + (y-ary);
    } else {
      bmp_offy = ary;
    }
    if ( ((y+h) > ary) && ((y+h)<(ary+arh)) ) {
      arh -= (ary+arh-y-h);
    }     
  }

   
  int l=ary;
  bitmap = bitmap + bmp_offy*w + bmp_offx;
  for (int row=0;row<arh; row++)
  {
    vga_pixel * dst=&framebuffer[l*fb_stride+arx];  
    bmp_ptr = (int16_t *)bitmap;
    for (int col=0;col<arw; col++)
    {
        uint16_t pix= *bmp_ptr++;
        *dst++ = VGA_RGB(R16(pix),G16(pix),B16(pix));
    } 
    bitmap +=  w;
    l++;
  } 
}

void VGA_T4::VGA_Handler::writeLine(int width, int height, int y, uint8_t *buf, vga_pixel *palette) {
  if ( (height<fb_height) && (height > 2) ) y += (fb_height-height)/2;
  vga_pixel * dst=&framebuffer[y*fb_stride];
  if (width > fb_width) {
#ifdef TFT_LINEARINT    
    int delta = (width/(width-fb_width))-1;
    int pos = delta;
    for (int i=0; i<fb_width; i++)
    {
      uint16_t val = palette[*buf++];
      pos--;
      if (pos == 0) {
#ifdef LINEARINT_HACK
        val  = ((uint32_t)palette[*buf++] + val)/2;
#else
        uint16_t val2 = *buf++;
        val = RGBVAL16((R16(val)+R16(val2))/2,(G16(val)+G16(val2))/2,(B16(val)+B16(val2))/2);
#endif        
        pos = delta;
      }
      *dst++=val;
    }
#else
    int step = ((width << 8)/fb_width);
    int pos = 0;
    for (int i=0; i<fb_width; i++)
    {
      *dst++=palette[buf[pos >> 8]];
      pos +=step;
    }  
#endif
  }
  else if ((width*2) == fb_width) {
    for (int i=0; i<width; i++)
    {
      *dst++=palette[*buf];
      *dst++=palette[*buf++];
    } 
  }
  else {
    if (width <= fb_width) {
      dst += (fb_width-width)/2;
    }
    for (int i=0; i<width; i++)
    {
      *dst++=palette[*buf++];
    } 
  }
}

void VGA_T4::VGA_Handler::writeLine(int width, int height, int y, vga_pixel *buf) {
  if ( (height<fb_height) && (height > 2) ) y += (fb_height-height)/2;
  uint8_t * dst=&framebuffer[y*fb_stride];    
  if (width > fb_width) {
    int step = ((width << 8)/fb_width);
    int pos = 0;
    for (int i=0; i<fb_width; i++)
    {
      *dst++ = buf[pos >> 8];
      pos +=step;
    }        
  }
  else if ((width*2) == fb_width) {
    if ( ( !(pix_shift & DMA_HACK) ) && (pix_shift & 0x3) ) {
      vga_pixel *buf2 = buf + (pix_shift & 0x3);
      for (int i=0; i<width; i++)
      {
        *dst++ = (*buf & 0xf0) | (*buf2 & 0x0f);
        *dst++ = (*buf++ & 0xf0) | (*buf2++ & 0x0f);
      }
    }  
    else {
      for (int i=0; i<width; i++)
      {
        *dst++=*buf;
        *dst++=*buf++;
      }
    }      
  }
  else {
    if (width <= fb_width) {
      dst += (fb_width-width)/2;
    }
    if ( ( !(pix_shift & DMA_HACK) ) && (pix_shift & 0x3) ) {
      vga_pixel *buf2 = buf + (pix_shift & 0x3);
      for (int i=0; i<width; i++)
      {
        *dst++ = (*buf++ & 0xf0) | (*buf2++ & 0x0f);
      }
    }  
    else {
      for (int i=0; i<width; i++)
      {
        *dst++=*buf++;
      }
    }
  }
}

void VGA_T4::VGA_Handler::writeLine16(int width, int height, int y, uint16_t *buf) {
  if ( (height<fb_height) && (height > 2) ) y += (fb_height-height)/2;
  uint8_t * dst=&framebuffer[y*fb_stride];    
  if (width > fb_width) {
    int step = ((width << 8)/fb_width);
    int pos = 0;
    for (int i=0; i<fb_width; i++)
    {
      uint16_t pix = buf[pos >> 8];
      *dst++ = VGA_RGB(R16(pix),G16(pix),B16(pix)); 
      pos +=step;
    }        
  }
  else if ((width*2) == fb_width) {
    for (int i=0; i<width; i++)
    {
      uint16_t pix = *buf++;
      uint8_t col = VGA_RGB(R16(pix),G16(pix),B16(pix));
      *dst++= col;
      *dst++= col;
    }       
  }
  else {
    if (width <= fb_width) {
      dst += (fb_width-width)/2;
    }
    for (int i=0; i<width; i++)
    {
      uint16_t pix = *buf++;
      *dst++=VGA_RGB(R16(pix),G16(pix),B16(pix));
    }      
  }
}

void VGA_T4::VGA_Handler::writeScreen(int width, int height, int stride, uint8_t *buf, vga_pixel *palette) {
  uint8_t *buffer=buf;
  uint8_t *src; 

  int i,j,y=0;
  if (width*2 <= fb_width) {
    for (j=0; j<height; j++)
    {
      vga_pixel * dst=&framebuffer[y*fb_stride];
      src=buffer;
      for (i=0; i<width; i++)
      {
        vga_pixel val = palette[*src++];
        *dst++ = val;
        *dst++ = val;
      }
      y++;
      if (height*2 <= fb_height) {
        dst=&framebuffer[y*fb_stride];    
        src=buffer;
        for (i=0; i<width; i++)
        {
          vga_pixel val = palette[*src++];
          *dst++ = val;
          *dst++ = val;
        }
        y++;
      } 
      buffer += stride;
    }
  }
  else if (width <= fb_width) {
    //dst += (fb_width-width)/2;
    for (j=0; j<height; j++)
    {
      vga_pixel * dst=&framebuffer[y*fb_stride+(fb_width-width)/2];  
      src=buffer;
      for (i=0; i<width; i++)
      {
        vga_pixel val = palette[*src++];
        *dst++ = val;
      }
      y++;
      if (height*2 <= fb_height) {
        dst=&framebuffer[y*fb_stride+(fb_width-width)/2];   
        src=buffer;
        for (i=0; i<width; i++)
        {
          vga_pixel val = palette[*src++];
          *dst++ = val;
        }
        y++;
      }
      buffer += stride;  
    }
  }   
}

void VGA_T4::VGA_Handler::copyLine(int width, int height, int ysrc, int ydst) {
  if ( (height<fb_height) && (height > 2) ) {
    ysrc += (fb_height-height)/2;
    ydst += (fb_height-height)/2;
  }    
  uint8_t * src=&framebuffer[ysrc*fb_stride];    
  uint8_t * dst=&framebuffer[ydst*fb_stride]; 
  memcpy(dst,src,width);   
} 







/*******************************************************************
 Experimental I2S interrupt based sound driver for PCM51xx !!!
*******************************************************************/

FLASHMEM static void set_audioClock(int nfact, int32_t nmult, uint32_t ndiv, bool force) // sets PLL4
{
  if (!force && (CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_ENABLE)) return;

  CCM_ANALOG_PLL_AUDIO = CCM_ANALOG_PLL_AUDIO_BYPASS | CCM_ANALOG_PLL_AUDIO_ENABLE
           | CCM_ANALOG_PLL_AUDIO_POST_DIV_SELECT(2) // 2: 1/4; 1: 1/2; 0: 1/1
           | CCM_ANALOG_PLL_AUDIO_DIV_SELECT(nfact);

  CCM_ANALOG_PLL_AUDIO_NUM   = nmult & CCM_ANALOG_PLL_AUDIO_NUM_MASK;
  CCM_ANALOG_PLL_AUDIO_DENOM = ndiv & CCM_ANALOG_PLL_AUDIO_DENOM_MASK;
  
  CCM_ANALOG_PLL_AUDIO &= ~CCM_ANALOG_PLL_AUDIO_POWERDOWN;//Switch on PLL
  while (!(CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_LOCK)) {}; //Wait for pll-lock
  
  const int div_post_pll = 1; // other values: 2,4
  CCM_ANALOG_MISC2 &= ~(CCM_ANALOG_MISC2_DIV_MSB | CCM_ANALOG_MISC2_DIV_LSB);
  if(div_post_pll>1) CCM_ANALOG_MISC2 |= CCM_ANALOG_MISC2_DIV_LSB;
  if(div_post_pll>3) CCM_ANALOG_MISC2 |= CCM_ANALOG_MISC2_DIV_MSB;
  
  CCM_ANALOG_PLL_AUDIO &= ~CCM_ANALOG_PLL_AUDIO_BYPASS;//Disable Bypass
}

#define AUDIO_SAMPLE_RATE_EXACT  11025.0 //44117.64706 //11025.0 //22050.0 //44117.64706 //31778.0

FLASHMEM static void config_sai1()
{
  CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);
  double fs = AUDIO_SAMPLE_RATE_EXACT;
  // PLL between 27*24 = 648MHz und 54*24=1296MHz
  int n1 = 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
  int n2 = 1 + (24000000 * 27) / (fs * 256 * n1);
  double C = (fs * 256 * n1 * n2) / 24000000;
  int c0 = C;
  int c2 = 10000;
  int c1 = C * c2 - (c0 * c2);

  set_audioClock(c0, c1, c2, true);
  // clear SAI1_CLK register locations
  CCM_CSCMR1 = (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI1_CLK_SEL_MASK))
               | CCM_CSCMR1_SAI1_CLK_SEL(2); // &0x03 // (0,1,2): PLL3PFD0, PLL5, PLL4

  n1 = n1 / 2; //Double Speed for TDM

  CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
               | CCM_CS1CDR_SAI1_CLK_PRED(n1 - 1) // &0x07
               | CCM_CS1CDR_SAI1_CLK_PODF(n2 - 1); // &0x3f

  IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK))
                    | (IOMUXC_GPR_GPR1_SAI1_MCLK_DIR | IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(0));  //Select MCLK


  // configure transmitter
  int rsync = 0;
  int tsync = 1;

  I2S1_TMR = 0;
  I2S1_TCR1 = I2S_TCR1_RFW(1);
  I2S1_TCR2 = I2S_TCR2_SYNC(tsync) | I2S_TCR2_BCP // sync=0; tx is async;
        | (I2S_TCR2_BCD | I2S_TCR2_DIV((1)) | I2S_TCR2_MSEL(1));
  I2S1_TCR3 = I2S_TCR3_TCE;
  I2S1_TCR4 = I2S_TCR4_FRSZ((2-1)) | I2S_TCR4_SYWD((32-1)) | I2S_TCR4_MF
        | I2S_TCR4_FSD | I2S_TCR4_FSE | I2S_TCR4_FSP;
  I2S1_TCR5 = I2S_TCR5_WNW((32-1)) | I2S_TCR5_W0W((32-1)) | I2S_TCR5_FBT((32-1));


  I2S1_RMR = 0;
  I2S1_RCR1 = I2S_RCR1_RFW(1);
  I2S1_RCR2 = I2S_RCR2_SYNC(rsync) | I2S_RCR2_BCP  // sync=0; rx is async;
        | (I2S_RCR2_BCD | I2S_RCR2_DIV((1)) | I2S_RCR2_MSEL(1));
  I2S1_RCR3 = I2S_RCR3_RCE;
  I2S1_RCR4 = I2S_RCR4_FRSZ((2-1)) | I2S_RCR4_SYWD((32-1)) | I2S_RCR4_MF
        | I2S_RCR4_FSE | I2S_RCR4_FSP | I2S_RCR4_FSD;
  I2S1_RCR5 = I2S_RCR5_WNW((32-1)) | I2S_RCR5_W0W((32-1)) | I2S_RCR5_FBT((32-1));

  //CORE_PIN23_CONFIG = 3;  // MCLK
  CORE_PIN21_CONFIG = 3;  // RX_BCLK
  CORE_PIN20_CONFIG = 3;  // RX_SYNC
  CORE_PIN7_CONFIG  = 3;  // TX_DATA0
  I2S1_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE;
  I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE  | I2S_TCSR_FRDE ;//<-- not using DMA */;
}



//DMAMEM __attribute__((aligned(32))) static uint32_t i2s_tx[1024];

static bool fillfirsthalf = true;
static uint16_t cnt = 0;
static uint16_t sampleBufferSize = 0;

static void (*fillsamples)(short * stream, int len) = nullptr;

static uint32_t * i2s_tx_buffer __attribute__((aligned(32)));
static uint16_t * i2s_tx_buffer16;
static uint16_t * txreg = (uint16_t *)((uint32_t)&I2S1_TDR0 + 2);


FASTRUN void AUDIO_isr() {
  
  *txreg = i2s_tx_buffer16[cnt]; 
  cnt = cnt + 1;
  cnt = cnt & (sampleBufferSize*2-1);

  if (cnt == 0) {
    fillfirsthalf = false;
    NVIC_SET_PENDING(IRQ_SOFTWARE);
  } 
  else if (cnt == sampleBufferSize) {
    fillfirsthalf = true;
    NVIC_SET_PENDING(IRQ_SOFTWARE);
  }
/*
  I2S1_TDR0 = i2s_tx_buffer[cnt]; 
  cnt = cnt + 1;
  cnt = cnt & (sampleBufferSize-1);
  if (cnt == 0) {
    fillfirsthalf = false;
    NVIC_SET_PENDING(IRQ_SOFTWARE);
  } 
  else if (cnt == sampleBufferSize/2) {
    fillfirsthalf = true;
    NVIC_SET_PENDING(IRQ_SOFTWARE);
  }
*/
}

FASTRUN void SOFTWARE_isr() {
  //Serial.println("x");
  if (fillfirsthalf) {
    fillsamples((short *)i2s_tx_buffer, sampleBufferSize);
    arm_dcache_flush_delete((void*)i2s_tx_buffer, (sampleBufferSize/2)*sizeof(uint32_t));
  }  
  else { 
    fillsamples((short *)&i2s_tx_buffer[sampleBufferSize/2], sampleBufferSize);
    arm_dcache_flush_delete((void*)&i2s_tx_buffer[sampleBufferSize/2], (sampleBufferSize/2)*sizeof(uint32_t));
  }
}

// display VGA image
FLASHMEM void VGA_T4::VGA_Handler::begin_audio(int samplesize, void (*callback)(short * stream, int len))
{
  fillsamples = callback;
  i2s_tx_buffer =  (uint32_t*)malloc(samplesize*sizeof(uint32_t)); //&i2s_tx[0];

  if (i2s_tx_buffer == NULL) {
    Serial.println("could not allocate audio samples");
    return;  
  }
  memset((void*)i2s_tx_buffer,0, samplesize*sizeof(uint32_t));
  arm_dcache_flush_delete((void*)i2s_tx_buffer, samplesize*sizeof(uint32_t));
  i2s_tx_buffer16 = (uint16_t*)i2s_tx_buffer;

  sampleBufferSize = samplesize;

  config_sai1();
  attachInterruptVector(IRQ_SAI1, AUDIO_isr);
  NVIC_ENABLE_IRQ(IRQ_SAI1);
  NVIC_SET_PRIORITY(IRQ_QTIMER3, 0);  // 0 highest priority, 255 = lowest priority 
  NVIC_SET_PRIORITY(IRQ_SAI1, 127);
  attachInterruptVector(IRQ_SOFTWARE, SOFTWARE_isr);
  NVIC_SET_PRIORITY(IRQ_SOFTWARE, 208);
  NVIC_ENABLE_IRQ(IRQ_SOFTWARE);

  I2S1_TCSR |= 1<<8;  // start generating TX FIFO interrupts

  Serial.print("Audio sample buffer = ");
  Serial.println(samplesize);
}
 
FLASHMEM void VGA_T4::VGA_Handler::end_audio() {
  if (i2s_tx_buffer != NULL) {
  	free(i2s_tx_buffer);
  }
}

