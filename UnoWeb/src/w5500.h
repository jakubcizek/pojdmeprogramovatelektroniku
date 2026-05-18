#pragma once
#include <Arduino.h>

// =============================================================================
//  Minimalni raw ovladac WIZnet W5500 - az 8 socketu, primy SPI registr pristup
// =============================================================================
//
//  Ramec W5500 (VDM): [Addr Hi][Addr Lo][Control] [Data ...]
//  Control = (BSB<<3) | (RWB<<2) | OM(00=VDM)
//
//  Block Select Bits (BSB) pro socket 0:
//    Common register   = 0  -> ctrl read 0x00 / write 0x04
//    Socket0 register   = 1  -> ctrl read 0x08 / write 0x0C
//    Socket0 TX buffer  = 2  -> ctrl write 0x14
//    Socket0 RX buffer  = 3  -> ctrl read  0x18
// =============================================================================

// ---- Control bajty ----------------------------------------------------------
#define WZ_COMMON_R  0x00
#define WZ_COMMON_W  0x04
// Socket 0 (zkratky pouzivane DHCP klientem pri bootu).
#define WZ_S0_REG_R  0x08
#define WZ_S0_REG_W  0x0C
#define WZ_S0_TX_W   0x14
#define WZ_S0_RX_R   0x18
// Control bajt pro libovolny socket s (0..7): BSB = s*4 + {1 reg,2 tx,3 rx}.
#define WZ_SREG_R(s)  ((uint8_t)(((uint8_t)((s) * 4 + 1)) << 3))
#define WZ_SREG_W(s)  ((uint8_t)((((uint8_t)((s) * 4 + 1)) << 3) | 0x04))
#define WZ_STX_W(s)   ((uint8_t)((((uint8_t)((s) * 4 + 2)) << 3) | 0x04))
#define WZ_SRX_R(s)   ((uint8_t)(((uint8_t)((s) * 4 + 3)) << 3))

// ---- Common registry --------------------------------------------------------
#define WZ_MR        0x0000
#define WZ_GAR       0x0001
#define WZ_SUBR      0x0005
#define WZ_SHAR      0x0009
#define WZ_SIPR      0x000F
#define WZ_PHYCFGR   0x002E
#define WZ_VERSIONR  0x0039

// ---- Socket 0 registry ------------------------------------------------------
#define WZ_Sn_MR        0x0000
#define WZ_Sn_CR        0x0001
#define WZ_Sn_IR        0x0002
#define WZ_Sn_SR        0x0003
#define WZ_Sn_PORT      0x0004
#define WZ_Sn_DIPR      0x000C
#define WZ_Sn_DPORT     0x0010
#define WZ_Sn_RXBUF_SZ  0x001E
#define WZ_Sn_TXBUF_SZ  0x001F
#define WZ_Sn_TX_FSR    0x0020
#define WZ_Sn_TX_WR     0x0024
#define WZ_Sn_RX_RSR    0x0026
#define WZ_Sn_RX_RD     0x0028

// ---- Socket prikazy (Sn_CR) -------------------------------------------------
#define WZ_CR_OPEN      0x01
#define WZ_CR_LISTEN    0x02
#define WZ_CR_CONNECT   0x04
#define WZ_CR_DISCON    0x08
#define WZ_CR_CLOSE     0x10
#define WZ_CR_SEND      0x20
#define WZ_CR_RECV      0x40

// ---- Socket stavy (Sn_SR) ---------------------------------------------------
#define WZ_SR_CLOSED        0x00
#define WZ_SR_INIT          0x13
#define WZ_SR_LISTEN        0x14
#define WZ_SR_ESTABLISHED   0x17
#define WZ_SR_CLOSE_WAIT    0x1C
#define WZ_SR_UDP           0x22

// ---- Socket mody (Sn_MR) ----------------------------------------------------
#define WZ_MR_TCP   0x01
#define WZ_MR_UDP   0x02

// ---- Sn_IR bity -------------------------------------------------------------
#define WZ_IR_CON     0x01
#define WZ_IR_DISCON  0x02
#define WZ_IR_RECV    0x04
#define WZ_IR_TIMEOUT 0x08
#define WZ_IR_SEND_OK 0x10

// ---- SPI + cip --------------------------------------------------------------
void    wz_spiInit();
bool    wz_reset();                       // software reset pres MR, vrati ok
uint8_t wz_version();                     // VERSIONR (W5500 = 0x04)
bool    wz_linkUp();                       // PHYCFGR bit0 (1 = link up)
void    wz_setMac(const uint8_t mac[6]);  // SHAR - nutne PRED DHCP
void    wz_setNet(const uint8_t ip[4], const uint8_t gw[4],
                  const uint8_t sn[4], const uint8_t mac[6]);

// ---- Registry / buffery -----------------------------------------------------
void     wz_writeBuf(uint16_t addr, uint8_t ctrl, const uint8_t* p,
                     uint16_t len, bool fromPgm);
void     wz_writeFill(uint16_t addr, uint8_t ctrl, uint8_t val, uint16_t len);
void     wz_readBuf (uint16_t addr, uint8_t ctrl, uint8_t* p, uint16_t len);

void     wz_w8 (uint16_t addr, uint8_t ctrl, uint8_t v);
uint8_t  wz_r8 (uint16_t addr, uint8_t ctrl);
void     wz_w16(uint16_t addr, uint8_t ctrl, uint16_t v);
uint16_t wz_r16(uint16_t addr, uint8_t ctrl);   // big-endian, stabilni cteni

// ---- Socket pomocnici (sn = index socketu 0..7) -----------------------------
void     wz_cmd(uint8_t sn, uint8_t cmd);        // vyda Sn_CR, pocka na dokonceni
uint8_t  wz_status(uint8_t sn);                  // Sn_SR
uint16_t wz_rxAvail(uint8_t sn);                 // Sn_RX_RSR
void     wz_rxConsume(uint8_t sn, uint16_t len); // posun Sn_RX_RD + RECV
uint16_t wz_txWr(uint8_t sn);                    // aktualni Sn_TX_WR
bool     wz_send(uint8_t sn, uint16_t newTxWr);  // Sn_TX_WR=newWr, SEND, SEND_OK
