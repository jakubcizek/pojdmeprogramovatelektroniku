#include "dhcp.h"
#include "w5500.h"
#include "config.h"
#include <string.h>

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_DISCOVER     1
#define DHCP_REQUEST      3
#define DHCP_OFFER        2
#define DHCP_ACK          5

static uint8_t s_xid[4];

// ---- Otevreni socketu 0 v UDP modu, port 68 ---------------------------------
static bool udpOpen() {
  wz_w8(WZ_Sn_MR, WZ_S0_REG_W, WZ_MR_UDP);
  wz_w16(WZ_Sn_PORT, WZ_S0_REG_W, DHCP_CLIENT_PORT);
  wz_cmd(0, WZ_CR_OPEN);
  for (uint8_t i = 0; i < 50; i++) {
    if (wz_status(0) == WZ_SR_UDP) return true;
    delay(1);
  }
  return false;
}

// ---- Sestaveni a odeslani DHCP zpravy ---------------------------------------
// msgType: DISCOVER nebo REQUEST. reqIp/serverId jen pro REQUEST.
static bool dhcpSend(const uint8_t mac[6], uint8_t msgType,
                     const uint8_t reqIp[4], const uint8_t serverId[4]) {
  uint16_t start = wz_txWr(0);
  uint16_t p = start;

  uint8_t hdr[28];
  memset(hdr, 0, sizeof(hdr));
  hdr[0] = 1; hdr[1] = 1; hdr[2] = 6;          // op=REQUEST, htype=ETH, hlen=6
  hdr[4] = s_xid[0]; hdr[5] = s_xid[1];
  hdr[6] = s_xid[2]; hdr[7] = s_xid[3];        // xid
  hdr[10] = 0x80;                              // flags = 0x8000 (broadcast)
  wz_writeBuf(p, WZ_S0_TX_W, hdr, 28, false); p += 28;

  wz_writeBuf(p, WZ_S0_TX_W, mac, 6, false);  p += 6;    // chaddr (MAC)
  wz_writeFill(p, WZ_S0_TX_W, 0, 10);          p += 10;   // chaddr pad
  wz_writeFill(p, WZ_S0_TX_W, 0, 192);         p += 192;  // sname + file

  static const uint8_t cookie[4] = { 0x63, 0x82, 0x53, 0x63 };
  wz_writeBuf(p, WZ_S0_TX_W, cookie, 4, false); p += 4;

  uint8_t o[32];
  uint8_t n = 0;
  o[n++] = 53; o[n++] = 1; o[n++] = msgType;             // 53 message type
  o[n++] = 61; o[n++] = 7; o[n++] = 1;                   // 61 client id
  for (uint8_t i = 0; i < 6; i++) o[n++] = mac[i];
  if (msgType == DHCP_REQUEST) {
    o[n++] = 50; o[n++] = 4;                             // 50 requested IP
    for (uint8_t i = 0; i < 4; i++) o[n++] = reqIp[i];
    o[n++] = 54; o[n++] = 4;                             // 54 server id
    for (uint8_t i = 0; i < 4; i++) o[n++] = serverId[i];
  }
  o[n++] = 55; o[n++] = 2; o[n++] = 1; o[n++] = 3;       // 55 param req
  o[n++] = 255;                                          // end
  wz_writeBuf(p, WZ_S0_TX_W, o, n, false); p += n;

  // Doplnek na min. 300 B (RFC 1542) - padding nulami (PAD).
  uint16_t written = (uint16_t)(p - start);
  if (written < 300) {
    wz_writeFill(p, WZ_S0_TX_W, 0, 300 - written);
    p += 300 - written;
  }

  static const uint8_t bcast[4] = { 255, 255, 255, 255 };
  wz_writeBuf(WZ_Sn_DIPR, WZ_S0_REG_W, bcast, 4, false);
  wz_w16(WZ_Sn_DPORT, WZ_S0_REG_W, DHCP_SERVER_PORT);

  return wz_send(0, p);
}

// ---- Prijem jedne DHCP odpovedi (cte primo z RX bufferu W5500) --------------
// Vraci typ zpravy (OFFER/ACK/...) nebo -1 pri timeoutu.
static int dhcpRecv(uint8_t yi[4], uint8_t sid[4],
                    uint8_t subn[4], uint8_t rtr[4], uint16_t timeoutMs) {
  uint32_t t0 = millis();
  while ((uint16_t)(millis() - t0) < timeoutMs) {
    if (wz_rxAvail(0) >= 8) {
      uint16_t rd   = wz_r16(WZ_Sn_RX_RD, WZ_S0_REG_R);
      uint8_t  uh[8];
      wz_readBuf(rd, WZ_S0_RX_R, uh, 8);            // peerIP[4] port[2] len[2]
      uint16_t plen = ((uint16_t)uh[6] << 8) | uh[7];
      uint16_t base = (uint16_t)(rd + 8);           // zacatek BOOTP zpravy

      int msgType = -1;
      uint8_t chk[8];
      wz_readBuf(base, WZ_S0_RX_R, chk, 8);         // op + .. + xid[4]
      if (chk[0] == 2 &&
          chk[4] == s_xid[0] && chk[5] == s_xid[1] &&
          chk[6] == s_xid[2] && chk[7] == s_xid[3]) {
        wz_readBuf((uint16_t)(base + 16), WZ_S0_RX_R, yi, 4);   // yiaddr
        uint16_t off = 240;
        while ((uint16_t)(off + 1) < plen) {
          uint8_t code;
          wz_readBuf((uint16_t)(base + off), WZ_S0_RX_R, &code, 1);
          if (code == 255) break;                   // END
          if (code == 0)   { off++; continue; }     // PAD
          uint8_t len;
          wz_readBuf((uint16_t)(base + off + 1), WZ_S0_RX_R, &len, 1);
          uint16_t vo = (uint16_t)(base + off + 2);
          if      (code == 53 && len >= 1) { uint8_t v; wz_readBuf(vo, WZ_S0_RX_R, &v, 1); msgType = v; }
          else if (code == 54 && len >= 4) wz_readBuf(vo, WZ_S0_RX_R, sid, 4);
          else if (code == 1  && len >= 4) wz_readBuf(vo, WZ_S0_RX_R, subn, 4);
          else if (code == 3  && len >= 4) wz_readBuf(vo, WZ_S0_RX_R, rtr, 4);
          off = (uint16_t)(off + 2 + len);
        }
      }
      wz_rxConsume(0, (uint16_t)(8 + plen));         // zahod cely datagram
      if (msgType >= 0) return msgType;
    }
    delay(2);
  }
  return -1;
}

bool dhcp_acquire(const uint8_t mac[6],
                  uint8_t ip[4], uint8_t gw[4], uint8_t sn[4]) {
  // Pseudonahodne xid z ADC sumu + casu.
  randomSeed(micros() ^ ((uint32_t)analogRead(A0) << 6) ^ analogRead(A1));
  for (uint8_t i = 0; i < 4; i++) s_xid[i] = (uint8_t)random(256);

  if (!udpOpen()) return false;

  for (uint8_t att = 0; att < 4; att++) {
    DBG(F("DHCP try ")); DBG_LN(att);
    if (!dhcpSend(mac, DHCP_DISCOVER, NULL, NULL)) { DBG_LN(F(" Dsend fail")); continue; }

    uint8_t yi[4]   = { 0, 0, 0, 0 };
    uint8_t sid[4]  = { 0, 0, 0, 0 };
    uint8_t subn[4] = { 255, 255, 255, 0 };
    uint8_t rtr[4]  = { 0, 0, 0, 0 };

    int mt = dhcpRecv(yi, sid, subn, rtr, 4000);
    DBG(F(" offer mt=")); DBG_LN(mt);
    if (mt != DHCP_OFFER) continue;
    if (!dhcpSend(mac, DHCP_REQUEST, yi, sid)) { DBG_LN(F(" Rsend fail")); continue; }
    int mt2 = dhcpRecv(yi, sid, subn, rtr, 4000);
    DBG(F(" ack mt=")); DBG_LN(mt2);
    if (mt2 != DHCP_ACK) continue;

    memcpy(ip, yi, 4);
    memcpy(sn, subn, 4);
    if (rtr[0] | rtr[1] | rtr[2] | rtr[3]) {
      memcpy(gw, rtr, 4);
    } else {                              // brana neuvedena -> odhad x.x.x.1
      gw[0] = yi[0]; gw[1] = yi[1]; gw[2] = yi[2]; gw[3] = 1;
    }
    wz_cmd(0, WZ_CR_CLOSE);
    return true;
  }
  wz_cmd(0, WZ_CR_CLOSE);
  return false;
}
