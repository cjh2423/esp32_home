idf_component_register(SRCS "app_control.c"
                    INCLUDE_DIRS "."
                    REQUIRES common config driver led fan motor buzzer mq2 sr)
