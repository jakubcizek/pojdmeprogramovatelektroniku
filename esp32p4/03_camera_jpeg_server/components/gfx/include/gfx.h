#ifndef KLOBOUK_GFX_H
#define KLOBOUK_GFX_H

#include <stdint.h>
#include "camera.h"

// Struktury pro glyfy/obrázky a font
typedef struct GfxGlyphAA {
    uint32_t codepoint;      // Unicode (U+XXXX)
    uint16_t width;   
    uint16_t height;       
    int16_t  xAdvance;  
    int16_t  xOffset;   
    int16_t  yOffset;      
    uint32_t bitmap_offset; 
} GfxGlyphAA;

typedef struct GfxFontAA {
    const uint8_t  *bitmap;    
    const GfxGlyphAA *glyphs;   
    uint32_t glyph_count;
    int16_t ascent;         
    int16_t descent;    
    int16_t line_height;   
} GfxFontAA;

// Deklarace veřejných funkcí komponenty
void gfx_draw_rect(CameraContext *cc,uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b, uint32_t border);
void gfx_draw_px(CameraContext *cc, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b);
void gfx_print(CameraContext *cc, int32_t x, int32_t y,const GfxFontAA *font, const char *utf8,uint8_t r, uint8_t g, uint8_t b);
void gfx_print_bg(CameraContext *cc, int32_t x, int32_t y,  const GfxFontAA *font, const char *utf8, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb);
int32_t gfx_get_text_width(const GfxFontAA *font, const char *utf8);
void rgb565_to_rgb888(uint16_t p, uint8_t *r, uint8_t *g, uint8_t *b);
uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b);


#endif