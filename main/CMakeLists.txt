idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_event config dht11 bh1750 mq2 led fan wifi motor buzzer common app_control web_ui sr)
