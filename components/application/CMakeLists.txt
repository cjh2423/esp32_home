idf_component_register(SRCS "application.c"
                    INCLUDE_DIRS "."
                    REQUIRES freertos nvs_flash esp_event
                             config common app_control
                             wifi web_ui sr
                             dht11 bh1750 mq2 led fan motor buzzer rgb_led)
