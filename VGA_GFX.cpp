//
// Created by daang on 1/20/2021.
//

#include "VGA_GFX.hpp"

//--------------------------------------------------------------
// Draw a line between 2 points
// x1,y1   : 1st point
// x2,y2   : 2nd point
// Color   : 16bits color
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawline(int16_t x1, int16_t y1, int16_t x2, int16_t y2, vga_pixel color){
    uint8_t yLonger = 0;
    int incrementVal, endVal;
    int shortLen = y2-y1;
    int longLen = x2-x1;
    int decInc;
    int j = 0, i = 0;

    if(ABS(shortLen) > ABS(longLen)) {
        int swap = shortLen;
        shortLen = longLen;
        longLen = swap;
        yLonger = 1;
    }

    endVal = longLen;

    if(longLen < 0) {
        incrementVal = -1;
        longLen = -longLen;
        endVal--;
    } else {
        incrementVal = 1;
        endVal++;
    }

    if(longLen == 0)
        decInc = 0;
    else
        decInc = (shortLen << 16) / longLen;

    if(yLonger) {
        for(i = 0;i != endVal;i += incrementVal) {
            drawPixel(x1 + (j >> 16),y1 + i,color);
            j += decInc;
        }
    } else {
        for(i = 0;i != endVal;i += incrementVal) {
            drawPixel(x1 + i,y1 + (j >> 16),color);
            j += decInc;
        }
    }
}

//--------------------------------------------------------------
// Draw a horizontal line
// x1,y1   : starting point
// lenght  : lenght in pixels
// color   : 16bits color
//--------------------------------------------------------------
void VGA_T4::GameEngine::draw_h_line(int16_t x, int16_t y, int16_t lenght, vga_pixel color){
    drawline(x , y , x + lenght , y , color);
}

//--------------------------------------------------------------
// Draw a vertical line
// x1,y1   : starting point
// lenght  : lenght in pixels
// color   : 16bits color
//--------------------------------------------------------------
void VGA_T4::GameEngine::draw_v_line(int16_t x, int16_t y, int16_t lenght, vga_pixel color){
    drawline(x , y , x , y + lenght , color);
}

//--------------------------------------------------------------
// Draw a circle.
// x, y - center of circle.
// r - radius.
// color - color of the circle.
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawcircle(int16_t x, int16_t y, int16_t radius, vga_pixel color){
    int16_t a, b, P;

    a = 0;
    b = radius;
    P = 1 - radius;

    do {
        drawPixel(a+x, b+y, color);
        drawPixel(b+x, a+y, color);
        drawPixel(x-a, b+y, color);
        drawPixel(x-b, a+y, color);
        drawPixel(b+x, y-a, color);
        drawPixel(a+x, y-b, color);
        drawPixel(x-a, y-b, color);
        drawPixel(x-b, y-a, color);

        if(P < 0)
            P+= 3 + 2*a++;
        else
            P+= 5 + 2*(a++ - b--);
    } while(a <= b);
}

//--------------------------------------------------------------
// Displays a full circle.
// x          : specifies the X position
// y          : specifies the Y position
// radius     : specifies the Circle Radius
// fillcolor  : specifies the Circle Fill Color
// bordercolor: specifies the Circle Border Color
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawfilledcircle(int16_t x, int16_t y, int16_t radius, vga_pixel fillcolor, vga_pixel bordercolor){
    int32_t  D;    /* Decision Variable */
    uint32_t  CurX;/* Current X Value */
    uint32_t  CurY;/* Current Y Value */

    D = 3 - (radius << 1);

    CurX = 0;
    CurY = radius;

    while (CurX <= CurY)
    {
        if(CurY > 0)
        {
            draw_v_line(x - CurX, y - CurY, 2*CurY, fillcolor);
            draw_v_line(x + CurX, y - CurY, 2*CurY, fillcolor);
        }

        if(CurX > 0)
        {
            draw_v_line(x - CurY, y - CurX, 2*CurX, fillcolor);
            draw_v_line(x + CurY, y - CurX, 2*CurX, fillcolor);
        }
        if (D < 0)
        {
            D += (CurX << 2) + 6;
        }
        else
        {
            D += ((CurX - CurY) << 2) + 10;
            CurY--;
        }
        CurX++;
    }

    drawcircle(x, y, radius,bordercolor);
}

//--------------------------------------------------------------
// Displays an Ellipse.
// cx: specifies the X position
// cy: specifies the Y position
// radius1: minor radius of ellipse.
// radius2: major radius of ellipse.
// color: specifies the Color to use for draw the Border from the Ellipse.
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawellipse(int16_t cx, int16_t cy, int16_t radius1, int16_t radius2, vga_pixel color){
    int x = -radius1, y = 0, err = 2-2*radius1, e2;
    float K = 0, rad1 = 0, rad2 = 0;

    rad1 = radius1;
    rad2 = radius2;

    if (radius1 > radius2)
    {
        do {
            K = (float)(rad1/rad2);
            drawPixel(cx-x,cy+(uint16_t)(y/K),color);
            drawPixel(cx+x,cy+(uint16_t)(y/K),color);
            drawPixel(cx+x,cy-(uint16_t)(y/K),color);
            drawPixel(cx-x,cy-(uint16_t)(y/K),color);

            e2 = err;
            if (e2 <= y) {
                err += ++y*2+1;
                if (-x == y && e2 <= x) e2 = 0;
            }
            if (e2 > x) err += ++x*2+1;
        }
        while (x <= 0);
    }
    else
    {
        y = -radius2;
        x = 0;
        do {
            K = (float)(rad2/rad1);
            drawPixel(cx-(uint16_t)(x/K),cy+y,color);
            drawPixel(cx+(uint16_t)(x/K),cy+y,color);
            drawPixel(cx+(uint16_t)(x/K),cy-y,color);
            drawPixel(cx-(uint16_t)(x/K),cy-y,color);

            e2 = err;
            if (e2 <= x) {
                err += ++x*2+1;
                if (-y == x && e2 <= y) e2 = 0;
            }
            if (e2 > y) err += ++y*2+1;
        }
        while (y <= 0);
    }
}

// Draw a filled ellipse.
// cx: specifies the X position
// cy: specifies the Y position
// radius1: minor radius of ellipse.
// radius2: major radius of ellipse.
// fillcolor  : specifies the Color to use for Fill the Ellipse.
// bordercolor: specifies the Color to use for draw the Border from the Ellipse.
void VGA_T4::GameEngine::drawfilledellipse(int16_t cx, int16_t cy, int16_t radius1, int16_t radius2, vga_pixel fillcolor, vga_pixel bordercolor){
    int x = -radius1, y = 0, err = 2-2*radius1, e2;
    float K = 0, rad1 = 0, rad2 = 0;

    rad1 = radius1;
    rad2 = radius2;

    if (radius1 > radius2)
    {
        do
        {
            K = (float)(rad1/rad2);
            draw_v_line((cx+x), (cy-(uint16_t)(y/K)), (2*(uint16_t)(y/K) + 1) , fillcolor);
            draw_v_line((cx-x), (cy-(uint16_t)(y/K)), (2*(uint16_t)(y/K) + 1) , fillcolor);

            e2 = err;
            if (e2 <= y)
            {
                err += ++y*2+1;
                if (-x == y && e2 <= x) e2 = 0;
            }
            if (e2 > x) err += ++x*2+1;

        }
        while (x <= 0);
    }
    else
    {
        y = -radius2;
        x = 0;
        do
        {
            K = (float)(rad2/rad1);
            draw_h_line((cx-(uint16_t)(x/K)), (cy+y), (2*(uint16_t)(x/K) + 1) , fillcolor);
            draw_h_line((cx-(uint16_t)(x/K)), (cy-y), (2*(uint16_t)(x/K) + 1) , fillcolor);

            e2 = err;
            if (e2 <= x)
            {
                err += ++x*2+1;
                if (-y == x && e2 <= y) e2 = 0;
            }
            if (e2 > y) err += ++y*2+1;
        }
        while (y <= 0);
    }
    drawellipse(cx,cy,radius1,radius2,bordercolor);
}

//--------------------------------------------------------------
// Draw a Triangle.
// ax,ay, bx,by, cx,cy - the triangle points.
// color    - color of the triangle.
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawtriangle(int16_t ax, int16_t ay, int16_t bx, int16_t by, int16_t cx, int16_t cy, vga_pixel color){
    drawline(ax , ay , bx , by , color);
    drawline(bx , by , cx , cy , color);
    drawline(cx , cy , ax , ay , color);
}

//--------------------------------------------------------------
// Draw a Filled Triangle.
// ax,ay, bx,by, cx,cy - the triangle points.
// fillcolor - specifies the Color to use for Fill the triangle.
// bordercolor - specifies the Color to use for draw the Border from the triangle.
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawfilledtriangle(int16_t ax, int16_t ay, int16_t bx, int16_t by, int16_t cx, int16_t cy, vga_pixel fillcolor, vga_pixel bordercolor){
    float ma, mb, mc    ; //'gradient of the lines
    float start, finish ; //'draw a line from start to finish!
    float tempspace     ; //'temporary storage for swapping values...
    double x1,x2,x3      ;
    double y1,y2,y3      ;
    int16_t n           ;

    //' need to sort out ay, by and cy into order.. highest to lowest
    //'
    if(ay < by)
    {
        //'swap x's
        tempspace = ax;
        ax = bx;
        bx = tempspace;

        //'swap y's
        tempspace = ay;
        ay = by;
        by = tempspace;
    }

    if(ay < cy)
    {
        //'swap x's
        tempspace = ax;
        ax = cx;
        cx = tempspace;

        //'swap y's
        tempspace = ay;
        ay = cy;
        cy = tempspace;
    }

    if(by < cy)
    {
        //'swap x's
        tempspace = bx;
        bx = cx;
        cx = tempspace;

        //'swap y's
        tempspace = by;
        by = cy;
        cy = tempspace;
    }

    //' Finally - copy the values in order...

    x1 = ax; x2 = bx; x3 = cx;
    y1 = ay; y2 = by; y3 = cy;

    //'bodge if y coordinates are the same
    if(y1 == y2)  y2 = y2 + 0.01;
    if(y2 == y3)  y3 = y3 + 0.01;
    if(y1 == y3)  y3 = y3 + 0.01;

    ma = (x1 - x2) / (y1 - y2);
    mb = (x3 - x2) / (y2 - y3);
    mc = (x3 - x1) / (y1 - y3);

    //'from y1 to y2
    for(n = 0;n >= (y2 - y1);n--)
    {
        start = n * mc;
        finish = n * ma;
        drawline((int16_t)(x1 - start), (int16_t)(n + y1), (int16_t)(x1 + finish), (int16_t)(n + y1), fillcolor);
    }


    //'and from y2 to y3

    for(n = 0;n >= (y3 - y2);n--)
    {
        start = n * mc;
        finish = n * mb;
        drawline((int16_t)(x1 - start - ((y2 - y1) * mc)), (int16_t)(n + y2), (int16_t)(x2 - finish), (int16_t)(n + y2), fillcolor);
    }

    // draw the border color triangle
    drawtriangle(ax,ay,bx,by,cx,cy,bordercolor);
}


//--------------------------------------------------------------
//  Displays a Rectangle at a given Angle.
//  centerx			: specifies the center of the Rectangle.
//	centery
//  w,h 	: specifies the size of the Rectangle.
//	angle			: specifies the angle for drawing the rectangle
//  color	    	: specifies the Color to use for Fill the Rectangle.
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawquad(int16_t centerx, int16_t centery, int16_t w, int16_t h, int16_t angle, vga_pixel color){
    int16_t	px[4],py[4];
    float	l;
    float	raddeg = 3.14159 / 180;
    float	w2 = w / 2.0;
    float	h2 = h / 2.0;
    float	vec = (w2*w2)+(h2*h2);
    float	w2l;
    float	pangle[4];

    l = sqrtf(vec);
    w2l = w2 / l;
    pangle[0] = acosf(w2l) / raddeg;
    pangle[1] = 180.0 - (acosf(w2l) / raddeg);
    pangle[2] = 180.0 + (acosf(w2l) / raddeg);
    pangle[3] = 360.0 - (acosf(w2l) / raddeg);
    px[0] = (int16_t)(calcco[((int16_t)(pangle[0]) + angle) % 360] * l + centerx);
    py[0] = (int16_t)(calcsi[((int16_t)(pangle[0]) + angle) % 360] * l + centery);
    px[1] = (int16_t)(calcco[((int16_t)(pangle[1]) + angle) % 360] * l + centerx);
    py[1] = (int16_t)(calcsi[((int16_t)(pangle[1]) + angle) % 360] * l + centery);
    px[2] = (int16_t)(calcco[((int16_t)(pangle[2]) + angle) % 360] * l + centerx);
    py[2] = (int16_t)(calcsi[((int16_t)(pangle[2]) + angle) % 360] * l + centery);
    px[3] = (int16_t)(calcco[((int16_t)(pangle[3]) + angle) % 360] * l + centerx);
    py[3] = (int16_t)(calcsi[((int16_t)(pangle[3]) + angle) % 360] * l + centery);
    // here we draw the quad
    drawline(px[0],py[0],px[1],py[1],color);
    drawline(px[1],py[1],px[2],py[2],color);
    drawline(px[2],py[2],px[3],py[3],color);
    drawline(px[3],py[3],px[0],py[0],color);
}

//--------------------------------------------------------------
//  Displays a filled Rectangle at a given Angle.
//  centerx			: specifies the center of the Rectangle.
//	centery
//  w,h 	: specifies the size of the Rectangle.
//	angle			: specifies the angle for drawing the rectangle
//  fillcolor    	: specifies the Color to use for Fill the Rectangle.
//  bordercolor  	: specifies the Color to use for draw the Border from the Rectangle.
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawfilledquad(int16_t centerx, int16_t centery, int16_t w, int16_t h, int16_t angle, vga_pixel fillcolor, vga_pixel bordercolor){
    int16_t	px[4],py[4];
    float	l;
    float	raddeg = 3.14159 / 180;
    float	w2 = w / 2.0;
    float	h2 = h / 2.0;
    float	vec = (w2*w2)+(h2*h2);
    float	w2l;
    float	pangle[4];

    l = sqrtf(vec);
    w2l = w2 / l;
    pangle[0] = acosf(w2l) / raddeg;
    pangle[1] = 180.0 - (acosf(w2l) / raddeg);
    pangle[2] = 180.0 + (acosf(w2l) / raddeg);
    pangle[3] = 360.0 - (acosf(w2l) / raddeg);
    px[0] = (int16_t)(calcco[((int16_t)(pangle[0]) + angle) % 360] * l + centerx);
    py[0] = (int16_t)(calcsi[((int16_t)(pangle[0]) + angle) % 360] * l + centery);
    px[1] = (int16_t)(calcco[((int16_t)(pangle[1]) + angle) % 360] * l + centerx);
    py[1] = (int16_t)(calcsi[((int16_t)(pangle[1]) + angle) % 360] * l + centery);
    px[2] = (int16_t)(calcco[((int16_t)(pangle[2]) + angle) % 360] * l + centerx);
    py[2] = (int16_t)(calcsi[((int16_t)(pangle[2]) + angle) % 360] * l + centery);
    px[3] = (int16_t)(calcco[((int16_t)(pangle[3]) + angle) % 360] * l + centerx);
    py[3] = (int16_t)(calcsi[((int16_t)(pangle[3]) + angle) % 360] * l + centery);
    // We draw 2 filled triangle for made the quad
    // To be uniform we have to use only the Fillcolor
    drawfilledtriangle(px[0],py[0],px[1],py[1],px[2],py[2],fillcolor,fillcolor);
    drawfilledtriangle(px[2],py[2],px[3],py[3],px[0],py[0],fillcolor,fillcolor);
    // here we draw the BorderColor from the quad
    drawline(px[0],py[0],px[1],py[1],bordercolor);
    drawline(px[1],py[1],px[2],py[2],bordercolor);
    drawline(px[2],py[2],px[3],py[3],bordercolor);
    drawline(px[3],py[3],px[0],py[0],bordercolor);
}

//--------------------------------------------------------------
//  Displays a Polygon.
//  centerx			: are specified with PolySet.Center.x and y.
//	centery
//  cx              : Translate the polygon in x direction
//  cy              : Translate the polygon in y direction
//  bordercolor  	: specifies the Color to use for draw the Border from the polygon.
//  polygon points  : are specified with PolySet.Pts[n].x and y
//  After the last polygon point , set PolySet.Pts[n + 1].x to 10000
//  Max number of point for the polygon is set by MaxPolyPoint previously defined.
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawpolygon(int16_t cx, int16_t cy, vga_pixel bordercolor){
    uint8_t n = 1;
    while((PolySet.Pts[n].x < 10000) && (n < MaxPolyPoint)){
        drawline(PolySet.Pts[n].x + cx,
                 PolySet.Pts[n].y + cy,
                 PolySet.Pts[n - 1].x + cx ,
                 PolySet.Pts[n - 1].y + cy,
                 bordercolor);
        n++;
    }
    // close the polygon
    drawline(PolySet.Pts[0].x + cx,
             PolySet.Pts[0].y + cy,
             PolySet.Pts[n - 1].x + cx,
             PolySet.Pts[n - 1].y + cy,
             bordercolor);
}

//--------------------------------------------------------------
//  Displays a filled Polygon.
//  centerx			: are specified with PolySet.Center.x and y.
//	centery
//  cx              : Translate the polygon in x direction
//  cy              : Translate the polygon in y direction
//  fillcolor  	    : specifies the Color to use for filling the polygon.
//  bordercolor  	: specifies the Color to use for draw the Border from the polygon.
//  polygon points  : are specified with PolySet.Pts[n].x and y
//  After the last polygon point , set PolySet.Pts[n + 1].x to 10000
//  Max number of point for the polygon is set by MaxPolyPoint previously defined.
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawfullpolygon(int16_t cx, int16_t cy, vga_pixel fillcolor, vga_pixel bordercolor){
    int n,i,j,k,dy,dx;
    int y,temp;
    int a[MaxPolyPoint][2],xi[MaxPolyPoint];
    float slope[MaxPolyPoint];

    n = 0;

    while((PolySet.Pts[n].x < 10000) && (n < MaxPolyPoint)){
        a[n][0] = PolySet.Pts[n].x;
        a[n][1] = PolySet.Pts[n].y;
        n++;
    }

    a[n][0]=PolySet.Pts[0].x;
    a[n][1]=PolySet.Pts[0].y;

    for(i=0;i<n;i++)
    {
        dy=a[i+1][1]-a[i][1];
        dx=a[i+1][0]-a[i][0];

        if(dy==0) slope[i]=1.0;
        if(dx==0) slope[i]=0.0;

        if((dy!=0)&&(dx!=0)) /*- calculate inverse slope -*/
        {
            slope[i]=(float) dx/dy;
        }
    }

    for(y=0;y< 480;y++)
    {
        k=0;
        for(i=0;i<n;i++)
        {

            if( ((a[i][1]<=y)&&(a[i+1][1]>y))||
                ((a[i][1]>y)&&(a[i+1][1]<=y)))
            {
                xi[k]=(int)(a[i][0]+slope[i]*(y-a[i][1]));
                k++;
            }
        }

        for(j=0;j<k-1;j++) /*- Arrange x-intersections in order -*/
            for(i=0;i<k-1;i++)
            {
                if(xi[i]>xi[i+1])
                {
                    temp=xi[i];
                    xi[i]=xi[i+1];
                    xi[i+1]=temp;
                }
            }

        for(i=0;i<k;i+=2)
        {
            drawline(xi[i] + cx,y + cy,xi[i+1]+1 + cx,y + cy, fillcolor);
        }

    }

    // Draw the polygon outline
    drawpolygon(cx , cy , bordercolor);
}

//--------------------------------------------------------------
//  Displays a rotated Polygon.
//  centerx			: are specified with PolySet.Center.x and y.
//	centery
//  cx              : Translate the polygon in x direction
//  ct              : Translate the polygon in y direction
//  bordercolor  	: specifies the Color to use for draw the Border from the polygon.
//  polygon points  : are specified with PolySet.Pts[n].x and y
//  After the last polygon point , set PolySet.Pts[n + 1].x to 10000
//  Max number of point for the polygon is set by MaxPolyPoint previously defined.
//--------------------------------------------------------------
void VGA_T4::GameEngine::drawrotatepolygon(int16_t cx, int16_t cy, int16_t Angle, vga_pixel fillcolor, vga_pixel bordercolor, uint8_t filled)
{
    Point2D 	SavePts[MaxPolyPoint];
    uint16_t	n = 0;
    int16_t		ctx,cty;
    float		raddeg = 3.14159 / 180;
    float		angletmp;
    float		tosquare;
    float		ptsdist;

    ctx = PolySet.Center.x;
    cty = PolySet.Center.y;

    while((PolySet.Pts[n].x < 10000) && (n < MaxPolyPoint)){
        // Save Original points coordinates
        SavePts[n] = PolySet.Pts[n];
        // Rotate and save all points
        tosquare = ((PolySet.Pts[n].x - ctx) * (PolySet.Pts[n].x - ctx)) + ((PolySet.Pts[n].y - cty) * (PolySet.Pts[n].y - cty));
        ptsdist  = sqrtf(tosquare);
        angletmp = atan2f(PolySet.Pts[n].y - cty,PolySet.Pts[n].x - ctx) / raddeg;
        PolySet.Pts[n].x = (int16_t)((cosf((angletmp + Angle) * raddeg) * ptsdist) + ctx);
        PolySet.Pts[n].y = (int16_t)((sinf((angletmp + Angle) * raddeg) * ptsdist) + cty);
        n++;
    }

    if(filled != 0)
        drawfullpolygon(cx , cy , fillcolor , bordercolor);
    else
        drawpolygon(cx , cy , bordercolor);

    // Get the original points back;
    n=0;
    while((PolySet.Pts[n].x < 10000) && (n < MaxPolyPoint)){
        PolySet.Pts[n] = SavePts[n];
        n++;
    }
}