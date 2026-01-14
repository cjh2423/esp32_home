# 封装层：基于 managed_components 官方/社区组件的驱动封装
# 每个子目录对应一个驱动封装
# 注意：servo 驱动已从 managed_components 移至本地，并修改时钟配置

idf_component_register(
    SRCS "dht/dht_driver.c"
         "bh1750/bh1750_driver.c"
         "servo/servo_driver.c"
         "servo/iot_servo.c"
         "rgb_led/rgb_led.c"
    INCLUDE_DIRS "dht" "bh1750" "servo" "rgb_led"
    REQUIRES driver
             esp-idf-lib__dht
             esp-idf-lib__bh1750
             esp-idf-lib__i2cdev
             led_strip
)
