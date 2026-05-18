#pragma once
#include <Arduino.h>

// =============================================================================
//  HTTP odpoved + HTML stranka v PROGMEM (flash), rozdelena na segmenty.
//  Mezi segmenty main.cpp vklada dynamicka cisla (teplota/vlhkost/statistika).
//
//  - HTTP/1.0 + Connection:close -> telo konci uzavrenim spojeni, nemusime
//    pocitat Content-Length (mensi kod, to je priorita).
//  - HTML jen ASCII; ° a sipky pres HTML entity (zadne UTF-8 bajty).
//  - <meta http-equiv=refresh content=60> -> auto-obnova synchronni s merenim.
// =============================================================================

static const char PG_HEAD[] PROGMEM =
  "HTTP/1.0 200 OK\r\n"
  "Content-Type:text/html\r\n"
  "Connection:close\r\n\r\n"
  "<!DOCTYPE html><html lang=cs><head><meta charset=utf-8>"
  "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
  "<meta http-equiv=refresh content=60><title>SHT31</title><style>"
  "body{margin:0;font-family:system-ui,sans-serif;background:#0f172a;"
  "color:#e2e8f0;display:flex;min-height:100vh;align-items:center;"
  "justify-content:center}"
  ".c{background:#1e293b;padding:clamp(24px,5vw,72px) clamp(28px,7vw,110px);"
  "border-radius:clamp(14px,2vw,28px);box-shadow:0 10px 30px #0006;"
  "text-align:center}"
  "h1{margin:0 0 clamp(14px,2.5vw,32px);font-size:clamp(12px,2vw,26px);"
  "font-weight:600;color:#94a3b8;letter-spacing:1px}"
  ".t{font-size:clamp(54px,15vw,200px);font-weight:700;line-height:1}"
  ".t i{font-size:clamp(22px,4.5vw,64px);font-style:normal;color:#38bdf8}"
  ".h{margin-top:clamp(8px,1.5vw,20px);font-size:clamp(18px,4.5vw,60px);"
  "color:#cbd5e1}"
  ".s{margin-top:clamp(18px,3vw,44px);font-size:clamp(11px,1.6vw,22px);"
  "color:#64748b;line-height:1.7}"
  "</style></head><body><div class=c>"
  "<h1>ATMEGA328P &middot; W5500 &middot; SHT31</h1>"
  "<div class=t>";

// za PG_HEAD: teplota
static const char PG_T2H[]  PROGMEM = "<i>&deg;C</i></div><div class=h>";
// za PG_T2H: vlhkost
static const char PG_H2S[]  PROGMEM = " % rel. vlhkost</div><div class=s>Str&#225;nka: ";
// za PG_H2S: pocet dotazu na stranku (GET /)
static const char PG_HITS[] PROGMEM = " &middot; HTTP: ";
// za PG_HITS: celkovy pocet HTTP GET
static const char PG_TOT[]  PROGMEM = "<br>&#8595; ";
// za PG_TOT: rx bajty
static const char PG_RX[]   PROGMEM = " B &middot; &#8593; ";
// za PG_RX: tx bajty
static const char PG_UPT[]  PROGMEM = " B<br>Server b&#283;&#382;&#237; ";
// za PG_UPT: hruby uptime v hodinach
static const char PG_TAIL[] PROGMEM = " hodin</div></div></body></html>";

// Minimalni odpoved na cesty != "/" (napr. /favicon.ico). Bez tela ->
// nejmensi mozna odpoved; prohlizec si "neexistuje" zapamatuje a prestane
// favicon zadat. Levnejsi nez servirovat celou stranku.
static const char PG_404[] PROGMEM =
  "HTTP/1.0 404 Not Found\r\nConnection:close\r\n\r\n";
