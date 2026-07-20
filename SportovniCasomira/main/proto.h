/*
 * Sdílené API protokolu CSM - emit řádku na oba kanály (UART + BLE).
 * Implementace v main.c.
 */
#pragma once

void csm_emit(const char *fmt, ...);
