idf_component_register(
    SRCS "voice_recognition.c" "inmp441_driver.c" "afe_processor.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer config
    PRIV_REQUIRES esp-sr
)
