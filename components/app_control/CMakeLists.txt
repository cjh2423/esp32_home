idf_component_register(SRCS "app_control.c"
                    INCLUDE_DIRS "."
                    REQUIRES common config driver led fan buzzer mq2 sr managed_wrappers)
