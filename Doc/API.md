# HTTP API 接口文档

## 1. 获取系统数据

*   **URL**: `/api/data`
*   **Method**: `GET`
*   **Response**: `application/json`

```json
{
    "temperature": 25.6,
    "humidity": 45.2,
    "light": 120.5,
    "smoke": 300,
    "led_state": 1,
    "led_brightness": 128,
    "fan_state": 1,
    "fan_speed": 200,
    "curtain_state": 0
}
```

## 2. LED 控制

*   **开关切换**: `/api/led/toggle`
*   **亮度设置**: `/api/led/brightness?value=<0-255>`

## 3. 风扇控制

*   **开关切换**   `GET /api/fan/toggle`: 切换风扇开关
*   `GET /api/fan/speed?value=<0-255>`: 设置风扇速度
*   `GET /api/curtain/toggle`: 切换窗帘（舵机）开关
