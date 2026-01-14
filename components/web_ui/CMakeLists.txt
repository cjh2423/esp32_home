idf_component_register(SRCS "http_server.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_server json common config
                    PRIV_REQUIRES app_control
                    EMBED_FILES "html/index.html")
