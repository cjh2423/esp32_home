idf_component_register(SRCS "wifi.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_netif esp_event config
                             nvs_flash esp_http_server esp_timer json
                             idf::78__esp-wifi-connect)
