#pragma once
#include <stdint.h>
using gpio_num_t = int;
int gpio_set_level(gpio_num_t pin, uint32_t value);

// Fake the target SDK output capability; no controller access occurs.
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) ((pin)<49 && (pin)!=46 && ((pin)<22 || (pin)>25))
