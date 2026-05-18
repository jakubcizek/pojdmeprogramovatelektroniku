#pragma once
#include <Arduino.h>

// Minimalni DHCP klient (jen DISCOVER/REQUEST/ACK, bez obnovy lease).
// Pouziva socket 0 v UDP modu. Pri uspechu naplni ip/gw/sn a vrati true.
bool dhcp_acquire(const uint8_t mac[6],
                  uint8_t ip[4], uint8_t gw[4], uint8_t sn[4]);
