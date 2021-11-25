extern MQTT_Client mqtt_client;
extern uint8_t mesh_ssid[AP_SSID_LENGTH + 1];

ICACHE_FLASH_ATTR void meter_is_ready(void);
#ifndef IMPULSE
ICACHE_FLASH_ATTR void meter_sent_data(void);
#endif
ICACHE_FLASH_ATTR void wifi_changed_cb(uint8_t status);
ICACHE_FLASH_ATTR void mqtt_connected_cb(uint32_t *args);
ICACHE_FLASH_ATTR void mqtt_disconnected_cb(uint32_t *args);
ICACHE_FLASH_ATTR void mqtt_published_cb(uint32_t *args);
ICACHE_FLASH_ATTR void mqtt_ping_response_cb(uint32_t *args);
ICACHE_FLASH_ATTR void mqtt_data_cb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len);
ICACHE_FLASH_ATTR void mqtt_timeout_cb(uint32_t *args);
ICACHE_FLASH_ATTR void mqtt_send_wifi_scan_results_cb(const struct bss_info *info);
ICACHE_FLASH_ATTR void user_gpio_init();

#ifdef IMPULSE
ICACHE_FLASH_ATTR void gpio_int_init();
void gpio_int_handler();
ICACHE_FLASH_ATTR void impulse_meter_init(void);
#endif

ICACHE_FLASH_ATTR void user_init(void);
ICACHE_FLASH_ATTR void system_init_done(void);

ICACHE_FLASH_ATTR void mqtt_flash_error(uint16_t calculated_crc, uint16_t saved_crc);
