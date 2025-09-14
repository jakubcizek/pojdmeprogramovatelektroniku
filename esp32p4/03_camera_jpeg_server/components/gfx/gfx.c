#include "gfx.h"
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Deklarace privátních funkcí
static const GfxGlyphAA* find_glyph(const GfxFontAA *f, uint32_t cp);
static const char* get_next_utf8_char(const char *s, uint32_t *out_cp);
static uint16_t alpha_blend_565(uint16_t dst, uint16_t tint, uint8_t a);
static void draw_glyph(CameraContext *cc, int32_t pen_x, int32_t line_top_y, const GfxFontAA *font, const GfxGlyphAA *g, uint16_t tint565);
static void draw_glyph_bg(CameraContext *cc, int32_t pen_x, int32_t line_top_y,const GfxFontAA *font, const GfxGlyphAA *g,uint16_t tint565, uint16_t bg565);

//-----------------------------------------------------------------------------
// Funkce pro výpočet šířky textu
// Hodí se pro centrování atp.
//-----------------------------------------------------------------------------
int32_t gfx_get_text_width(const GfxFontAA *font, const char *utf8)
{
    if (!font || !utf8) return 0;
    int32_t w = 0, maxw = 0;
    const char *p = utf8;
    while (p && *p) {
        uint32_t cp; const char *n = get_next_utf8_char(p, &cp);
        if (cp == '\n') { if (w > maxw) maxw = w; w = 0; p = n; continue; }
        const GfxGlyphAA *g = find_glyph(font, cp);
        w += g ? g->xAdvance : 0;
        p = n;
    }
    if (w > maxw) maxw = w;
    return maxw;
}

//-----------------------------------------------------------------------------
// Funkce pro vypsání transparentního textu do framebufferu
//-----------------------------------------------------------------------------
void gfx_print(CameraContext *cc, int32_t x, int32_t y, const GfxFontAA *font, const char *utf8, uint8_t r, uint8_t g, uint8_t b){
    if (!cc || !font || !utf8) return;
    uint16_t tint = rgb888_to_rgb565(r,g,b);
    int32_t pen_x = x;
    int32_t line_top = y; // y = horní okraj řádku; baseline = y + ascent
    const char *p = utf8;
    while (p && *p) {
        uint32_t cp; p = get_next_utf8_char(p, &cp);
        if (cp == '\n') { pen_x = x; line_top += font->line_height; continue; }
        const GfxGlyphAA *gl = find_glyph(font, cp);
        if (gl) { draw_glyph(cc, pen_x, line_top, font, gl, tint); pen_x += gl->xAdvance; }
    }
}

//-----------------------------------------------------------------------------
// Funkce pro vypsání textu s pozadím do framebufferu 
//-----------------------------------------------------------------------------
void gfx_print_bg(CameraContext *cc, int32_t x, int32_t y, const GfxFontAA *font, const char *utf8, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb){
    if (!cc || !font || !utf8) return;
    uint16_t tint = rgb888_to_rgb565(fr,fg,fb);
    uint16_t back = rgb888_to_rgb565(br,bg,bb);
    int32_t pen_x = x;
    int32_t line_top = y;
    const char *p = utf8;
    while (p && *p) {
        uint32_t cp; p = get_next_utf8_char(p, &cp);
        if (cp == '\n') { pen_x = x; line_top += font->line_height; continue; }
        const GfxGlyphAA *gl = find_glyph(font, cp);
        if (gl) { draw_glyph_bg(cc, pen_x, line_top, font, gl, tint, back); pen_x += gl->xAdvance; }
    }
}

//-----------------------------------------------------------------------------
// Funkce pro změnu RGB barvy pixelu na pozici XY
//-----------------------------------------------------------------------------
void gfx_draw_px(CameraContext *cc, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b){
    if (!cc || !cc->frame_buffer_rgb565) return;
    if (x >= cc->width || y >= cc->height) return;
    cc->frame_buffer_rgb565[(size_t)y * cc->width + x] = rgb888_to_rgb565(r, g, b);
}

//-----------------------------------------------------------------------------
// Funkce pro kreslení obdélníku/rámečku
//-----------------------------------------------------------------------------
void gfx_draw_rect(CameraContext *cc, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b, uint32_t border){
    if (!cc || !cc->frame_buffer_rgb565 || !cc->width || !cc->height || !w || !h) return;
    if (x >= cc->width || y >= cc->height) return;
    if (x + w > cc->width) w = cc->width - x;
    if (y + h > cc->height) h = cc->height - y;
    const uint16_t col = rgb888_to_rgb565(r,g,b);
    const uint32_t stride = cc->width;
    if (border == 0 || border >= w/2 || border >= h/2) {
        for (uint32_t j = 0; j < h; ++j) {
            uint16_t *row = cc->frame_buffer_rgb565 + (size_t)(y + j) * stride + x;
            for (uint32_t i = 0; i < w; ++i) row[i] = col;
        }
        return;
    }
    const uint32_t bt = border;
    // Horní
    for (uint32_t j = 0; j < bt; ++j) {
        uint16_t *row = cc->frame_buffer_rgb565 + (size_t)(y + j) * stride + x;
        for (uint32_t i = 0; i < w; ++i) row[i] = col;
    }
    // Spodní
    for (uint32_t j = 0; j < bt; ++j) {
        uint16_t *row = cc->frame_buffer_rgb565 + (size_t)(y + h - 1 - j) * stride + x;
        for (uint32_t i = 0; i < w; ++i) row[i] = col;
    }
    // Vpravo a vlevo
    for (uint32_t j = y + bt; j < y + h - bt; ++j) {
        uint16_t *rowL = cc->frame_buffer_rgb565 + (size_t)j * stride + x;
        for (uint32_t i = 0; i < bt; ++i) rowL[i] = col;

        uint16_t *rowR = cc->frame_buffer_rgb565 + (size_t)j * stride + (x + w - bt);
        for (uint32_t i = 0; i < bt; ++i) rowR[i] = col;
    }
}

//-----------------------------------------------------------------------------
// Funkce pro kreslení barevného textu do ANSI konzole/UART terminálu
// Tímto způsobem bychom mohli při tetsování vykreslit obrázek z framebufferu
// i do příkazové řádky. Ale pozor, hromada dat. Jeden pixel zabere až
// několik desítek bajtů
//-----------------------------------------------------------------------------
void gfx_rgb_printf(uint8_t r, uint8_t g, uint8_t b, const char* txtpx){
    printf("\x1b[38;2;%u;%u;%um%s\x1b[0m", r, g, b, txtpx);
}

//-----------------------------------------------------------------------------
// Funkce pro převod 24bit RGB888 na pracovní 16bit formát framebufferu RGB565
//-----------------------------------------------------------------------------
uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b){
    return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

//-----------------------------------------------------------------------------
// Funkce pro opačný převod RGB565 na RGB888
// Tím pádem získáme 24bit RGB ze surové hodnoty pxielu
// na pozici XY ve framebufferu
//-----------------------------------------------------------------------------
void rgb565_to_rgb888(uint16_t p, uint8_t *r, uint8_t *g, uint8_t *b){
    uint8_t r5 = (p >> 11) & 0x1F;
    uint8_t g6 = (p >> 5)  & 0x3F;
    uint8_t b5 =  p        & 0x1F;
    *r = (r5 << 3) | (r5 >> 2);  // 5 -> 8 bitů
    *g = (g6 << 2) | (g6 >> 4);  // 6 -> 8 bitů
    *b = (b5 << 3) | (b5 >> 2);  // 5 -> 8 bitů
}


//-----------------------------------------------------------------------------
// Funkce pro vykreslení zmenšeniny RGB565 obrázku do ANSI konze/UART terminálu
// O velikosti obrázku rozhoduje term_cols, na který se přepočítají pixely
//-----------------------------------------------------------------------------
void render_rgb565_ansi(const uint16_t *img, int width, int height, int term_cols, const char* txtpx)
{
    if (!img || width <= 0 || height <= 0 || term_cols <= 0) return;
    const double scale = (double)width / term_cols;
    const int term_rows = (int)((double)height / scale / 2.0);

    for (int ry = 0; ry < term_rows; ry++) {
        if ((ry % 2) == 0) {
            // Každé dva řádky uděláme malou pauzu, aby si oddechl CPU
            // Jinak hrozí watchdog timeout
            vTaskDelay(1); 
        }
        int y = (int)(ry * scale * 2.0 + 0.5);
        if (y >= height) y = height - 1;

        for (int cx = 0; cx < term_cols; cx++) {
            int x = (int)(cx * scale + 0.5);
            if (x >= width) x = width - 1;

            const uint16_t p = img[(size_t)y * width + x];
            uint8_t R, G, B;
            rgb565_to_rgb888(p, &R, &G, &B);
            gfx_rgb_printf(R, G, B, txtpx);
        }
        printf("\n");
    }
}

// PRIVÁTNÍ FUNKCE KOMPONENTY

//-----------------------------------------------------------------------------
// Funkce pro vyhledání glyfu podle Unicode kódu
//-----------------------------------------------------------------------------
static const GfxGlyphAA* find_glyph(const GfxFontAA *f, uint32_t cp){
    if (!f || !f->glyphs) return NULL;
    int32_t lo = 0, hi = (int32_t)f->glyph_count - 1;
    while (lo <= hi) {
        int32_t mid = (lo + hi) >> 1;
        uint32_t v = f->glyphs[mid].codepoint;
        if (cp < v) hi = mid - 1;
        else if (cp > v) lo = mid + 1;
        else return &f->glyphs[mid];
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// Funkce pro získání ukazatele na další znak UTF-8 (mají proměnlivou šířku)
//-----------------------------------------------------------------------------
static const char* get_next_utf8_char(const char *s, uint32_t *out_cp){
    if (!s || !*s) { if (out_cp) *out_cp = 0; return s; }
    const unsigned char *p = (const unsigned char*)s;
    uint32_t cp;
    if (p[0] < 0x80) { cp = p[0]; s += 1; }
    else if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); s += 2;
        if (cp < 0x80) cp = 0xFFFD;
    } else if ((p[0] & 0xF0) == 0xE0 &&
               (p[1] & 0xC0) == 0x80 &&
               (p[2] & 0xC0) == 0x80) {
        cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); s += 3;
        if (cp < 0x800) cp = 0xFFFD;
    } else if ((p[0] & 0xF8) == 0xF0 &&
               (p[1] & 0xC0) == 0x80 &&
               (p[2] & 0xC0) == 0x80 &&
               (p[3] & 0xC0) == 0x80) {
        cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); s += 4;
        if (cp < 0x10000 || cp > 0x10FFFF) cp = 0xFFFD;
    } else { cp = 0xFFFD; s += 1; }
    if (out_cp) *out_cp = cp;
    return s;
}

//-----------------------------------------------------------------------------
// Funkce pro alpha-bleding s obarvením
// a = 0..255 (0=transparentní, 255=plný)
//-----------------------------------------------------------------------------
static uint16_t alpha_blend_565(uint16_t dst, uint16_t tint, uint8_t a){
    if (a == 0) return dst;
    if (a == 255) return tint;
    uint32_t dr = (dst >> 11) & 0x1F;
    uint32_t dg = (dst >>  5) & 0x3F;
    uint32_t db =  dst        & 0x1F;
    uint32_t sr = (tint >> 11) & 0x1F;
    uint32_t sg = (tint >>  5) & 0x3F;
    uint32_t sb =  tint        & 0x1F;
    uint32_t ia = 255 - a;
    uint32_t or_ = (dr * ia + sr * a + 127) / 255;
    uint32_t og_ = (dg * ia + sg * a + 127) / 255;
    uint32_t ob_ = (db * ia + sb * a + 127) / 255;
    if (or_ > 31) or_ = 31;
    if (og_ > 63) og_ = 63;
    if (ob_ > 31) ob_ = 31;
    return (uint16_t)((or_ << 11) | (og_ << 5) | (ob_));
}

//-----------------------------------------------------------------------------
// Funkce pro vykreslení glyfu
//-----------------------------------------------------------------------------
static void draw_glyph(CameraContext *cc, int32_t pen_x, int32_t line_top_y, const GfxFontAA *font, const GfxGlyphAA *g, uint16_t tint565){
    if (!cc || !cc->frame_buffer_rgb565 || !font || !g) return;
    const int32_t top_y = line_top_y + font->ascent + g->yOffset;
    const int32_t left_x = pen_x + g->xOffset;
    const uint8_t *src = font->bitmap + g->bitmap_offset;
    const uint32_t W = cc->width, H = cc->height;
    for (int32_t y = 0; y < (int32_t)g->height; ++y) {
        const int32_t dy = top_y + y;
        if ((uint32_t)dy >= H) { src += g->width; continue; }
        uint16_t *dst_row = cc->frame_buffer_rgb565 + (size_t)dy * W;
        for (int32_t x = 0; x < (int32_t)g->width; ++x) {
            const int32_t dx = left_x + x;
            const uint8_t a = *src++;
            if ((uint32_t)dx >= W) continue;
            if (a == 0) continue;

            uint16_t dst = dst_row[dx];
            dst_row[dx] = alpha_blend_565(dst, tint565, a);
        }
    }
}

//-----------------------------------------------------------------------------
// Funkce pro vykreslení glyfu s pzoadím
//-----------------------------------------------------------------------------
static void draw_glyph_bg(CameraContext *cc, int32_t pen_x, int32_t line_top_y, const GfxFontAA *font, const GfxGlyphAA *g, uint16_t tint565, uint16_t bg565){
    if (!cc || !cc->frame_buffer_rgb565 || !font || !g) return;
    const int32_t top_y  = line_top_y + font->ascent + g->yOffset;
    const int32_t left_x = pen_x + g->xOffset;
    const uint8_t *src = font->bitmap + g->bitmap_offset;
    const uint32_t W = cc->width, H = cc->height;
    for (int32_t y = 0; y < (int32_t)g->height; ++y) {
        const int32_t dy = top_y + y;
        if ((uint32_t)dy >= H) { src += g->width; continue; }
        uint16_t *dst_row = cc->frame_buffer_rgb565 + (size_t)dy * W;
        for (int32_t x = 0; x < (int32_t)g->width; ++x) {
            const int32_t dx = left_x + x;
            const uint8_t a = *src++;
            if ((uint32_t)dx >= W) continue;
            dst_row[dx] = bg565;
            if (a) {
                dst_row[dx] = alpha_blend_565(bg565, tint565, a);
            }
        }
    }
}