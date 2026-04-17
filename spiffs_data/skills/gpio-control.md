# GPIO Control

Control and monitor GPIO pins on the ESP32-S3 for digital I/O.

## When to use
When the user asks to:
- Turn on/off LEDs, relays, or other outputs
- Turn on/off a water pump relay
- Capture and send a live camera photo to Telegram
- Check switch states, button presses, or sensor readings
- Read analog sensor values such as soil moisture
- Explain the plant soil moisture level from the GPIO7 sensor
- Confirm digital I/O status (switch confirmation)
- Get an overview of all GPIO pin states

## How to use
1. To **read a switch/sensor**: use gpio_read with the pin number
   - Returns HIGH (1) or LOW (0)
   - HIGH typically means switch is ON / circuit closed
   - LOW typically means switch is OFF / circuit open
2. To **set an output**: use gpio_write with pin and state (1=HIGH, 0=LOW)
3. To **control the water pump on GPIO44**: use water_pump_control with state
   - Defaults to active-low relay logic
   - ON writes GPIO LOW
   - OFF writes GPIO HIGH
4. To **send a live camera photo to Telegram**: use camera_send_photo with chat_id
   - Captures JPEG from the XIAO ESP32S3 Sense camera
   - Streams the photo directly to Telegram
   - Does not save the image to ESP32 flash
   - If the photo is green-tinted, use wb_mode: 3 for office/fluorescent light or wb_mode: 4 for warmer home light
   - wb_mode values: 0 auto, 1 sunny, 2 cloudy, 3 office/fluorescent, 4 home/incandescent
5. To **scan all pins**: use gpio_read_all for a full status overview
6. To **read analog soil moisture**: use adc_read with the ADC GPIO pin
   - Returns raw ADC value
   - If dry_raw and wet_raw are provided, also returns moisture percentage
   - Prefer GPIO1-GPIO10 on ESP32-S3 when WiFi is active
7. To **explain the plant moisture sensor on GPIO7**: use soil_moisture_report
   - Returns a human-friendly moisture report
   - If chat_id is provided, sends the report to Telegram
   - dry_raw and wet_raw can override calibration
8. For **switch confirmation**: read the pin, report state, optionally toggle and re-read to verify

## Pin safety
- Only pins within the allowed range can be accessed
- ESP32 flash pins (6-11) are always blocked
- If a pin is rejected, suggest an alternative within the allowed range

## Example
User: "Check if the switch on pin 4 is on"
→ gpio_read {"pin": 4}
→ "Pin 4 = HIGH"
→ "The switch on pin 4 is currently ON (HIGH)."

User: "Turn on the relay on pin 5"
→ gpio_write {"pin": 5, "state": 1}
→ "Pin 5 set to HIGH"
→ gpio_read {"pin": 5}
→ "Pin 5 = HIGH"
→ "Relay on pin 5 is now ON. Confirmed HIGH."

User: "Turn on the water pump"
→ water_pump_control {"state": "on"}
→ "Water pump on GPIO44 is ON (GPIO level=LOW, active_low=true)"

User: "Turn off the water pump"
→ water_pump_control {"state": "off"}
→ "Water pump on GPIO44 is OFF (GPIO level=HIGH, active_low=true)"

User: "Take a photo and send it to Telegram"
→ camera_send_photo {"chat_id": "123456789", "caption": "Live plant photo"}
→ "Camera photo sent to Telegram chat 123456789"

User: "Take a photo but fix the green tint"
→ camera_send_photo {"chat_id": "123456789", "caption": "Live plant photo", "wb_mode": 3, "warmup_frames": 2}
→ "Camera photo sent to Telegram chat 123456789"

User: "Read soil moisture on GPIO4"
→ adc_read {"pin": 4}
→ "GPIO4 ADC raw=2380"

User: "Read calibrated soil moisture on GPIO4"
→ adc_read {"pin": 4, "dry_raw": 3200, "wet_raw": 1200}
→ "GPIO4 ADC raw=2380, moisture=41% (dry_raw=3200, wet_raw=1200)"

User: "Tell me the plant moisture"
→ soil_moisture_report {}
→ "盆栽而家泥土濕度大約係 41%。有少少乾，可以留意或者少量補水。"

User: "Send the plant moisture to Telegram"
→ soil_moisture_report {"chat_id": "123456789"}
→ "Telegram soil moisture report sent to 123456789"
