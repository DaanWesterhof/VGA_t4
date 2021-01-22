//
// Created by daang on 1/20/2021.
//

#include "../include/VGA_GameEngine.hpp"

/*******************************************************************
 Experimental GAME engine supporting:
 - Multiple tiles layers with independent scrolling
 - Sprites (MAX_SPRITES)
 - up to 256 redefinable tiles
 - up to 256 redefinable sprites
*******************************************************************/





void VGA_T4::GameEngine::drawSpr(unsigned char index, int x, int y) {
    if ((x + SPRITES_W) <= 0) return;
    if (x >= (fb_width-hscr_mask)) return;
    if ((y + SPRITES_H) <= 0) return;
    if (y >= fb_height) return;

    vga_pixel * src=&spritesbuffer[index*SPRITES_W*SPRITES_H];
    int i,j;
    vga_pixel pix;
    for (j=0; j<SPRITES_H; j++)
    {
        vga_pixel * dst=&framebuffer[((j+y)*fb_stride)+x];
        for (i=0; i<SPRITES_W; i++)
        {
            pix=*src++;
            if ( (!pix) || ((x+i) < 0) || ((x+i) > (fb_width-hscr_mask)) || ((y+j) < 0) || ((y+j) >= fb_height) ) dst++;
            else *dst++ = pix;
        }
    }
}


void VGA_T4::GameEngine::drawTile(unsigned char tile, int x, int y) {
    vga_pixel * src=&tilesbuffer[tile*TILES_W*TILES_H];
    int i,j;
    for (j=0; j<TILES_H; j++)
    {
        vga_pixel * dst=&framebuffer[((j+y)*fb_stride)+x];
        for (i=0; i<TILES_W; i++)
        {
            *dst++ = *src++;
        }
    }
}

void VGA_T4::GameEngine::drawTileCropL(unsigned char tile, int x, int y) {
    vga_pixel * src=&tilesbuffer[tile*TILES_W*TILES_H];
    int i,j;
    vga_pixel pix;
    for (j=0; j<TILES_H; j++)
    {
        vga_pixel * dst=&framebuffer[((j+y)*fb_stride)+x];
        for (i=0; i<TILES_W; i++)
        {
            pix=*src++;
            if ((x+i) < 0) dst++;
            else
                *dst++ = pix;
        }
    }
}

void VGA_T4::GameEngine::drawTileCropR(unsigned char tile, int x, int y) {
    vga_pixel * src=&tilesbuffer[tile*TILES_W*TILES_H];
    int i,j;
    vga_pixel pix;
    for (j=0; j<TILES_H; j++)
    {
        vga_pixel * dst=&framebuffer[((j+y)*fb_stride)+x];
        for (i=0; i<TILES_W; i++)
        {
            pix = *src++;
            if ((x+i) > (fb_width-hscr_mask)) *dst++=0;
            else
                *dst++ = pix;
        }
    }
}

void VGA_T4::GameEngine::drawTransTile(unsigned char tile, int x, int y) {
    vga_pixel * src=&tilesbuffer[tile*TILES_W*TILES_H];
    vga_pixel pix;
    int i,j;
    for (j=0; j<TILES_H; j++)
    {
        vga_pixel * dst=&framebuffer[((j+y)*fb_stride)+x];
        for (i=0; i<TILES_W; i++)
        {
            if ((pix=*src++)) *dst++ = pix;
            else dst++;
        }
    }
}

void VGA_T4::GameEngine::drawTransTileCropL(unsigned char tile, int x, int y) {
    vga_pixel * src=&tilesbuffer[tile*TILES_W*TILES_H];
    vga_pixel pix;
    int i,j;
    for (j=0; j<TILES_H; j++)
    {
        vga_pixel * dst=&framebuffer[((j+y)*fb_stride)+x];
        for (i=0; i<TILES_W; i++)
        {
            pix=*src++;
            if ((x+i) < 0) dst++;
            else
            if (pix) *dst++ = pix;
            else dst++;
        }
    }
}

void VGA_T4::GameEngine::drawTransTileCropR(unsigned char tile, int x, int y) {
    vga_pixel * src=&tilesbuffer[tile*TILES_W*TILES_H];
    vga_pixel pix;
    int i,j;
    for (j=0; j<TILES_H; j++)
    {
        vga_pixel * dst=&framebuffer[((j+y)*fb_stride)+x];
        for (i=0; i<TILES_W; i++)
        {
            if ((x+i) > (fb_width-hscr_mask)) src++;
            else
            if ((pix=*src++)) *dst++ = pix;
            else *dst++;
        }
    }
}



void VGA_T4::GameEngine::tileText(unsigned char index, int16_t x, int16_t y, const char * text, vga_pixel fgcolor, vga_pixel bgcolor, vga_pixel *dstbuffer, int dstwidth, int dstheight) {
    vga_pixel c;
    vga_pixel * dst;

    while ((c = *text++)) {
        const unsigned char * charpt=&font8x8[c][0];
        int l=y;
        for (int i=0;i<8;i++)
        {
            unsigned char bits;
            dst=&dstbuffer[(index*dstheight+l)*dstwidth+x];
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

void VGA_T4::GameEngine::tileTextOverlay(int16_t x, int16_t y, const char * text, vga_pixel fgcolor) {
    vga_pixel c;
    vga_pixel * dst;

    while ((c = *text++)) {
        const unsigned char * charpt=&font8x8[c][0];
        int l=y;
        for (int i=0;i<8;i++)
        {
            unsigned char bits;
            dst=&framebuffer[+l*fb_stride+x];
            bits = *charpt++;
            if (bits&0x01) *dst++=fgcolor;
            else dst++;
            bits = bits >> 1;
            if (bits&0x01) *dst++=fgcolor;
            else dst++;
            bits = bits >> 1;
            if (bits&0x01) *dst++=fgcolor;
            else dst++;
            bits = bits >> 1;
            if (bits&0x01) *dst++=fgcolor;
            else dst++;
            bits = bits >> 1;
            if (bits&0x01) *dst++=fgcolor;
            else dst++;
            bits = bits >> 1;
            if (bits&0x01) *dst++=fgcolor;
            else dst++;
            bits = bits >> 1;
            if (bits&0x01) *dst++=fgcolor;
            else dst++;
            bits = bits >> 1;
            if (bits&0x01) *dst++=fgcolor;
            else dst++;
            l++;
        }
        x +=8;
    }
}



//############################################################################################################# start propper functions
static const char * hex = "0123456789ABCDEF";

void VGA_T4::GameEngine::begin_gfxengine(int nblayers, int nbtiles, int nbsprites)
{
    nb_layers = nblayers;
    nb_tiles = nbtiles;
    nb_sprites = nbsprites;

    if (spritesbuffer == NULL) spritesbuffer = (vga_pixel*)malloc(SPRITES_W*SPRITES_H*sizeof(vga_pixel)*nb_sprites);
    if (tilesbuffer == NULL) tilesbuffer = (vga_pixel*)malloc(TILES_W*TILES_H*sizeof(vga_pixel)*nb_tiles);
    if (tilesram == NULL) tilesram = (unsigned char *)malloc(TILES_COLS*TILES_ROWS*nb_layers);
    if (spritesdata == NULL) spritesdata = (VGA_T4::Sprite_t *)malloc(SPRITES_MAX*sizeof(Sprite_t));

    memset((void*)spritesbuffer,0, SPRITES_W*SPRITES_H*sizeof(vga_pixel)*nb_sprites);
    memset((void*)tilesbuffer,0, TILES_W*TILES_H*sizeof(vga_pixel)*nb_tiles);
    memset((void*)tilesram,0,TILES_COLS*TILES_ROWS*nb_layers);

    /* Random test tiles */
    char numhex[3];
    for (int i=0; i<nb_tiles; i++)
    {
        int r = random(0x40,0xff);
        int g = random(0x40,0xff);
        int b = random(0x40,0xff);
        if (i==0) {
            memset((void*)&tilesbuffer[TILES_W*TILES_H*sizeof(vga_pixel)*i],0, TILES_W*TILES_H*sizeof(vga_pixel));
        }
        else {
            memset((void*)&tilesbuffer[TILES_W*TILES_H*sizeof(vga_pixel)*i],VGA_RGB(r,g,b), TILES_W*TILES_H*sizeof(vga_pixel));
            numhex[0] = hex[(i>>4) & 0xf];
            numhex[1] = hex[i & 0xf];
            numhex[2] = 0;
            if (TILES_W == 16 )tileText(i, 0, 0, numhex, VGA_RGB(0xff,0xff,0xff), VGA_RGB(0x40,0x40,0x40), tilesbuffer,TILES_W,TILES_H);
        }
    }
    /* Random test sprites */
    for (int i=0; i<nb_sprites; i++)
    {
        int r = random(0x40,0xff);
        int g = random(0x40,0xff);
        int b = random(0x40,0xff);
        if (i==0) {
            memset((void*)&spritesbuffer[SPRITES_W*SPRITES_H*sizeof(vga_pixel)*i],0, SPRITES_W*SPRITES_H*sizeof(vga_pixel));
        }
        else {
            memset((void*)&spritesbuffer[SPRITES_W*SPRITES_H*sizeof(vga_pixel)*i],VGA_RGB(r,g,b), SPRITES_W*SPRITES_H*sizeof(vga_pixel));
            numhex[0] = hex[(i>>4) & 0xf];
            numhex[1] = hex[i & 0xf];
            numhex[2] = 0;
            tileText(i, 0, 0, numhex, VGA_RGB(0xff,0xff,0x00), VGA_RGB(0x00,0x00,0x00),spritesbuffer,SPRITES_W,SPRITES_H);
        }
    }
}


void VGA_T4::GameEngine::run_gfxengine()
{
    waitLine(480+40);

    unsigned char * tilept;

    // Layer 0
    for (int j=0; j<TILES_ROWS; j++)
    {
        tilept = &tilesram[j*TILES_COLS];
        if ( (j>=hscr_beg[0]) && (j<=hscr_end[0]) ) {
            int modcol = (hscr[0] >> TILES_HBITS) % TILES_COLS;
            for (int i=0; i<TILES_COLS; i++)
            {
                (i == 0) ? drawTileCropL(tilept[modcol], (i<<TILES_HBITS) - (hscr[0] & TILES_HMASK), j*TILES_H) :
                (i == (TILES_COLS-1))?drawTileCropR(tilept[modcol], (i<<TILES_HBITS) - (hscr[0] & TILES_HMASK), j*TILES_H) :
                drawTile(tilept[modcol], (i<<TILES_HBITS) - (hscr[0] & TILES_HMASK), j*TILES_H);
                modcol++;
                modcol = modcol % TILES_COLS;
            }
        }
        else {
            for (int i=0; i<TILES_COLS; i++)
            {
                (i == (TILES_COLS-1)) ? drawTileCropR(tilept[i], (i<<TILES_HBITS), j*TILES_H) :
                drawTile(tilept[i], (i<<TILES_HBITS), j*TILES_H);
            }
        }
    }

    // Other layers
    if (nb_layers > 1) {
        int lcount = 1;
        while (lcount < nb_layers) {
            for (int j=0; j<TILES_ROWS; j++)
            {
                tilept = &tilesram[(j+lcount*TILES_ROWS)*TILES_COLS];
                if ( (j>=hscr_beg[lcount]) && (j<=hscr_end[lcount]) ) {
                    int modcol = (hscr[lcount] >> TILES_HBITS) % TILES_COLS;
                    for (int i=0; i<TILES_COLS; i++)
                    {
                        (i == 0) ? drawTransTileCropL(tilept[modcol], (i<<TILES_HBITS) - (hscr[lcount] & TILES_HMASK), j*TILES_H) :
                        (i == (TILES_COLS-1))?drawTransTileCropR(tilept[modcol], (i<<TILES_HBITS) - (hscr[lcount] & TILES_HMASK), j*TILES_H) :
                        drawTransTile(tilept[modcol], (i<<TILES_HBITS) - (hscr[lcount] & TILES_HMASK), j*TILES_H);
                        modcol++;
                        modcol = modcol % TILES_COLS;
                    }
                }
                else {
                    for (int i=0; i<TILES_COLS; i++)
                    {
                        drawTransTile(tilept[i], (i<<TILES_HBITS), j*TILES_H);
                    }
                }
            }
            lcount++;
        }
    }

/*
 static char * ro="01234567890123456789012345678901234567";
 for (int i=0; i<TILES_ROWS*2; i++)
 {
  tileTextOverlay(0, i*8, ro, VGA_RGB(0x00,0xff,0x00));
 }
*/

    for (int i=0; i<SPRITES_MAX; i++)
    {
        drawSpr(spritesdata[i].index, spritesdata[i].x, spritesdata[i].y);
    }
}

void VGA_T4::GameEngine::tile_data(unsigned char index, vga_pixel * data, int len)
{
    memcpy((void*)&tilesbuffer[index*TILES_W*TILES_H],(void*)data,len);
}

void VGA_T4::GameEngine::sprite_data(unsigned char index, vga_pixel * data, int len)
{
    memcpy((void*)&spritesbuffer[index*SPRITES_W*SPRITES_H],(void*)data,len);
}

void VGA_T4::GameEngine::sprite(int id , int x, int y, unsigned char index)
{
    if (id < SPRITES_MAX) {
        spritesdata[id].x = x;
        spritesdata[id].y = y;
        spritesdata[id].index = index;
    }
}


void VGA_T4::GameEngine::sprite_hide(int id)
{
    if (id < SPRITES_MAX) {
        spritesdata[id].x = -16;
        spritesdata[id].y = -16;
        spritesdata[id].index = 0;
    }
}

void VGA_T4::GameEngine::tile_draw(int layer, int x, int y, unsigned char index)
{
    tilesram[(y+layer*TILES_ROWS)*TILES_COLS+x] = index;
}

void VGA_T4::GameEngine::tile_draw_row(int layer, int x, int y, unsigned char * data, int len)
{
    while (len--)
    {
        tilesram[(y+layer*TILES_ROWS)*TILES_COLS+x++] = *data++;
    }
}

void VGA_T4::GameEngine::tile_draw_col(int layer, int x, int y, unsigned char * data, int len)
{
    while (len--)
    {
        tilesram[(y++ +layer*TILES_ROWS)*TILES_COLS+x] = *data++;
    }
}

void VGA_T4::GameEngine::hscroll(int layer, int value)
{
    hscr[layer] = value;
}

void VGA_T4::GameEngine::vscroll(int layer, int value)
{
    vscr[layer] = value;
}

void VGA_T4::GameEngine::set_hscroll(int layer, int rowbeg, int rowend, int mask)
{
    hscr_beg[layer] = rowbeg;
    hscr_end[layer] = rowend;
    hscr_mask = mask+1;
}

void VGA_T4::GameEngine::set_vscroll(int layer, int colbeg, int colend, int mask)
{
    hscr_beg[layer] = colbeg;
    hscr_end[layer] = colend;
    hscr_mask = mask+1;
}

