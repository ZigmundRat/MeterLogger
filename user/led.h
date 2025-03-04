#ifndef LED_H_
#define LED_H_

ICACHE_FLASH_ATTR void led_init(void);
ICACHE_FLASH_ATTR void led_on(void);
ICACHE_FLASH_ATTR void led_off(void);
ICACHE_FLASH_ATTR void led_blink(void);
ICACHE_FLASH_ATTR void led_blink_short(void);
ICACHE_FLASH_ATTR void led_pattern_a(void);
ICACHE_FLASH_ATTR void led_pattern_b(void);
ICACHE_FLASH_ATTR void led_pattern_c(void);
ICACHE_FLASH_ATTR void led_stop_pattern(void);
ICACHE_FLASH_ATTR void led_destroy(void);

#endif /* LED_H_ */
