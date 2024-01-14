#ifndef _MBOX_H
#define _MBOX_H

#include "wifipi.h"
#include <stdint.h>

uint32_t get_clock_rate(uint32_t clock_id, struct WiFiBase * WiFiBase);
uint32_t set_clock_rate(uint32_t clock_id, uint32_t speed, struct WiFiBase * WiFiBase);
uint32_t get_clock_state(uint32_t id, struct WiFiBase * WiFiBase);
uint32_t set_clock_state(uint32_t id, uint32_t state, struct WiFiBase * WiFiBase);
uint32_t get_power_state(uint32_t id, struct WiFiBase * WiFiBase);
uint32_t set_power_state(uint32_t id, uint32_t state, struct WiFiBase * WiFiBase);

#endif /* _MBOX_H */
