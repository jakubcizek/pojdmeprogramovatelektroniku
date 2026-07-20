idf_component_register(SRCS "main.c" "ble_uart.c" "ssd1306.c" "ota_wifi.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_uart esp_driver_i2c
                                     esp_timer nvs_flash bt
                                     esp_wifi esp_netif esp_event esp_http_server app_update
                       INCLUDE_DIRS "")
