# Peripheral
## I2C
* SDA: IO32
* SCL: IO25

### TSL 25911 Light Sensor
* I2C Address: 0x29
* Quantity: Light
* Range: 0 - 88000 Lux
* Sensitivity: 188 uLux

### ENS 160 && AHT 20
* I2C Address: 0x53

#### ENS 160 Gas Sensor
* Quantity: TVOC, eCO2, AQI
* Range: 0 - 65535 ppb

#### AHT 20 Temperature and Humidity Sensor
* Quantity: Temperature, Humidity
* Range: -40 to 85 °C, 0 to 100% RH
* Sensitivity: ±0.3 °C, ±2% RH
* Resolution: 0.01 °C, 0.024% RH

## IO
### Buttom 1
* OUT: IO35
* GND: IO39 (temporary)

### Buttom 2
Not connected yet