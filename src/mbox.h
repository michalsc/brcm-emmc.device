#ifndef _MBOX_H
#define _MBOX_H

#include "emmc.h"
#include <stdint.h>

uint32_t get_clock_rate(uint32_t clock_id, struct EMMCBase * EMMCBase);
uint32_t set_clock_rate(uint32_t clock_id, uint32_t speed, struct EMMCBase * EMMCBase);
uint32_t get_clock_state(uint32_t id, struct EMMCBase * EMMCBase);
uint32_t set_clock_state(uint32_t id, uint32_t state, struct EMMCBase * EMMCBase);
uint32_t get_power_state(uint32_t id, struct EMMCBase * EMMCBase);
uint32_t set_power_state(uint32_t id, uint32_t state, struct EMMCBase * EMMCBase);
uint32_t get_extgpio_state(uint32_t id, struct EMMCBase * EMMCBase);
uint32_t set_extgpio_state(uint32_t id, uint32_t state, struct EMMCBase * EMMCBase);

#if 0
uint32_t set_led_state(uint32_t id, uint32_t state, struct EMMCBase * EMMCBase);

uint32_t set_sdhost_clock(uint32_t speed, struct EMMCBase * EMMCBase);
#endif

#endif /* _MBOX_H */
