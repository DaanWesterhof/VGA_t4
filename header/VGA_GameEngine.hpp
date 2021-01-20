//
// Created by daang on 1/20/2021.
//

#ifndef VGA_T4_VGA_GAMEENGINE_HPP
#define VGA_T4_VGA_GAMEENGINE_HPP

#include "VGA_t4.h"





namespace VGA_T4 {

    struct Sprite_t {
        int x;
        int y;
        unsigned char index;
    };

    class GameEngine {
    private:
        vga_pixel * tilesbuffer __attribute__((aligned(32))) = NULL;
        vga_pixel * spritesbuffer __attribute__((aligned(32))) = NULL;
        unsigned char * tilesram __attribute__((aligned(32))) = NULL;
        Sprite_t * spritesdata __attribute__((aligned(32))) = NULL;
        int nb_layers = 0;
        int nb_tiles = 0;
        int nb_sprites = 0;
        int hscr[TILES_MAX_LAYERS];
        int vscr[TILES_MAX_LAYERS];
        int hscr_beg[TILES_MAX_LAYERS]={0,0};
        int hscr_end[TILES_MAX_LAYERS]={TILES_ROWS-1, TILES_ROWS-1};
        int hscr_mask=0;

    public:



        void begin_gfxengine(int nblayers, int nbtiles, int nbsprites);

        void run_gfxengine();

        void tile_data(unsigned char index, vga_pixel *data, int len);

        void sprite_data(unsigned char index, vga_pixel *data, int len);

        void sprite(int id, int x, int y, unsigned char index);

        void sprite_hide(int id);

        void tile_draw(int layer, int x, int y, unsigned char index);

        void tile_draw_row(int layer, int x, int y, unsigned char *data, int len);

        void tile_draw_col(int layer, int x, int y, unsigned char *data, int len);

        void set_hscroll(int layer, int rowbeg, int rowend, int mask);

        void set_vscroll(int layer, int colbeg, int colend, int mask);

        void hscroll(int layer, int value);

        void vscroll(int layer, int value);


    private:

        static void QT3_isr(void);

        static void AUDIO_isr(void);

        static void SOFTWARE_isr(void);

        //weird used to be static functions

        void drawSpr(unsigned char index, int x, int y);
        void drawTile(unsigned char tile, int x, int y);
        void drawTileCropL(unsigned char tile, int x, int y);
        void drawTileCropR(unsigned char tile, int x, int y);
        void drawTransTile(unsigned char tile, int x, int y);
        void drawTransTileCropL(unsigned char tile, int x, int y);
        void drawTransTileCropR(unsigned char tile, int x, int y);
        void tileText(unsigned char index, int16_t x, int16_t y, const char * text, vga_pixel fgcolor, vga_pixel bgcolor, vga_pixel *dstbuffer, int dstwidth, int dstheight);
        void tileTextOverlay(int16_t x, int16_t y, const char * text, vga_pixel fgcolor);

    };

}

#endif //VGA_T4_VGA_GAMEENGINE_HPP
