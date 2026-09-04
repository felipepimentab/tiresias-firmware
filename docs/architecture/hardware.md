# Tiresias DK Hardware

![Tiresias DK hardware block diagram](hardware-block-diagram.svg)

## Blocks

- **nRF5340:** application processing and Bluetooth control.
- **ADAU1787:** analog conversion, PCM interface, and DSP.
- **BMI270:** inertial sensor on the shared I2C bus.
- **MX25R1635F:** external QSPI flash.
- **nPM1100 and battery:** system power.
- **Microphone, output transducer, button, LEDs, and antenna:** board I/O.

## Diagram conventions

- Solid arrows: signal or data flow; dashed arrows: power.
- Double-headed arrows: bidirectional interfaces.
- I2C is shared by the BMI270 and ADAU1787; I2S supports playback and capture.

Use the latest schematic and board devicetree as the electrical authority. Verify pinning,
clock direction, power rails, flash wiring, and RF details there.
