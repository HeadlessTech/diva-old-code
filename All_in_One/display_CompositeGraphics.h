#pragma once
#include "display_Color.h"

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

    // Diva: one framebuffer, drawn in place, holding only the rows that are
    // ever drawn (rowIsDrawn(y) true). Every other row shows one shared
    // black row; drawing into such a row goes to a scratch row that is never
    // shown, so it can't change the screen. The video interrupt reads each
    // line through frame[y], so rows can share memory.
    // Before: a front and a back buffer of 336x240, allocated as 480 blocks,
    // about 165 KB of heap. Now, for the logo + one image: about 58 KB.
    // Drawing an image takes about 1 ms, so drawing in place doesn't flicker.
    void init(bool (*rowIsDrawn)(int y))
    {
      int drawnRows = 0;
      for (int y = 0; y < yres; y++)
        if (rowIsDrawn(y)) drawnRows++;

      frame = (char**)malloc(yres * sizeof(char*));       // what is shown
      backbuffer = (char**)malloc(yres * sizeof(char*));  // what is drawn into
      // the drawn rows, then the shared black row, then the scratch row;
      // zeroed = black. One block, so it can't fragment the heap.
      char* rows = (char*)calloc((size_t)(drawnRows + 2) * xres, 1);
      if (!frame || !backbuffer || !rows)
      {
        printf("display: out of memory for the framebuffer, restarting\n");
        esp_restart();
      }
      char* black = rows + (size_t)drawnRows * xres;
      char* scratch = black + xres;
      for (int y = 0; y < yres; y++)
      {
        if (rowIsDrawn(y))
        {
          frame[y] = backbuffer[y] = rows;
          rows += xres;
        }
        else
        {
          frame[y] = black;
          backbuffer[y] = scratch;
        }
      }
      zbuffer = nullptr;
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

    // Single buffer (see init): what is drawn is already shown.
    inline void end()
    {
    }
};
