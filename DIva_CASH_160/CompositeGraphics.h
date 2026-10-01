#pragma once
#include "Color.h"

#define _BB_PIXEL(X,Y) backbuffer[Y][X]

class CompositeGraphics
{
  public:
    int xres;
    int yres;
    char **frame;
    char **backbuffer;
    char **zbuffer;
    int cursorX, cursorY, cursorBaseX;
    int frontColor, backColor;
    inline void setHue(char hue) {
      Color::setHue(hue);
    }

    CompositeGraphics(int w, int h)
      : xres(w),
        yres(h)
    {
      cursorX = cursorY = cursorBaseX = 0;
      frontColor = 50;
      backColor = -1;
    }

    void init()
    {
      frame = (char**)malloc(yres * sizeof(char*));
      backbuffer = (char**)malloc(yres * sizeof(char*));
      //not enough memory for z-buffer implementation
      //zbuffer = (char**)malloc(yres * sizeof(char*));
      for (int y = 0; y < yres; y++)
      {
        frame[y] = (char*)malloc(xres);
        backbuffer[y] = (char*)malloc(xres);
        //zbuffer[y] = (char*)malloc(xres);
      }
    }
    inline void begin(int clear = -1)
    {
      if (clear > -1)
        for (int y = 0; y < yres; y++)
          for (int x = 0; x < xres; x++)
            _BB_PIXEL(x, y) = Color(clear);
    }

    inline void dot(int x, int y, Color color)
    {
      if ((unsigned int)x < xres && (unsigned int)y < yres)
        _BB_PIXEL(x, y) = color;
    }

    inline void dotAdd(int x, int y, Color color)
    {
      if ((unsigned int)x < xres && (unsigned int)y < yres)
        _BB_PIXEL(x, y) = color + _BB_PIXEL(x, y);
    }

    inline void end()
    {
      char **b = backbuffer;
      backbuffer = frame;
      frame = b;
    }
};
