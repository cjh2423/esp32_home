# HTTP API 接口文档

## 1. 获取系统数据

- **URL**: `/api/data`
- **Method**: `GET`
- **Response**: `application/json`

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
  "curtain_state": 0,
  "control_mode": 0
}
```

## 2. LED 控制

- `POST /api/led/toggle`
- `POST /api/led/brightness?value=<0-255>`

## 3. 风扇控制

- `POST /api/fan/toggle`
- `POST /api/fan/speed?value=<0-255>`

## 4. 窗帘控制

- `POST /api/curtain/toggle`

## 5. 模式控制

- `POST /api/mode/toggle`

## 6. RGB 控制

- `POST /api/rgb/preset?c=<red|green|blue|yellow|cyan|magenta|white|orange|purple>`
- `POST /api/rgb/color?r=<0-255>&g=<0-255>&b=<0-255>`
