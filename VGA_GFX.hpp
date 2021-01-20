//
// Created by daang on 1/20/2021.
//

#ifndef VGA_T4_VGA_GFX_HPP
#define VGA_T4_VGA_GFX_HPP

#include "VGA_t4.h"

namespace VGA_T4 {

    class VGA_HandlerGFX : public VGA_Handler {
    public:

        void drawline(int16_t x1, int16_t y1, int16_t x2, int16_t y2, vga_pixel color);

        void draw_h_line(int16_t x1, int16_t y1, int16_t lenght, vga_pixel color);

        void draw_v_line(int16_t x1, int16_t y1, int16_t lenght, vga_pixel color);

        void drawcircle(int16_t x, int16_t y, int16_t radius, vga_pixel color);

        void drawfilledcircle(int16_t x, int16_t y, int16_t radius, vga_pixel fillcolor, vga_pixel bordercolor);

        void drawellipse(int16_t cx, int16_t cy, int16_t radius1, int16_t radius2, vga_pixel color);

        void drawfilledellipse(int16_t cx, int16_t cy, int16_t radius1, int16_t radius2, vga_pixel fillcolor,
                               vga_pixel bordercolor);

        void drawtriangle(int16_t ax, int16_t ay, int16_t bx, int16_t by, int16_t cx, int16_t cy, vga_pixel color);

        void drawfilledtriangle(int16_t ax, int16_t ay, int16_t bx, int16_t by, int16_t cx, int16_t cy, vga_pixel fillcolor,
                           vga_pixel bordercolor);

        void drawquad(int16_t centerx, int16_t centery, int16_t w, int16_t h, int16_t angle, vga_pixel color);

        void drawfilledquad(int16_t centerx, int16_t centery, int16_t w, int16_t h, int16_t angle, vga_pixel fillcolor,
                            vga_pixel bordercolor);

        void drawpolygon(int16_t cx, int16_t cy, vga_pixel bordercolor);

        void drawfullpolygon(int16_t cx, int16_t cy, vga_pixel fillcolor, vga_pixel bordercolor);

        void drawrotatepolygon(int16_t cx, int16_t cy, int16_t Angle, vga_pixel fillcolor, vga_pixel bordercolor,
                               uint8_t filled);

    };

}

#endif //VGA_T4_VGA_GFX_HPP
