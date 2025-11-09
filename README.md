# RNDIS USB Device for Seeed Studio XIAO RP2040

This project turns a Seeed Studio XIAO RP2040 into a USB RNDIS network device.

## Project Structure

This repository contains two separate projects:

*   **`platformio/`**: A [PlatformIO](https://platformio.org/) project.
*   **`arduino/`**: An [Arduino IDE](https://www.arduino.cc/en/software) sketch.

Both projects provide the same functionality.

## Compiling the Firmware

### PlatformIO

1.  Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO IDE extension](https://platformio.org/platformio-ide).
2.  Open the `platformio` directory in Visual Studio Code.
3.  Click the "Build" button in the PlatformIO toolbar.

### Arduino IDE

1.  Install the [Arduino IDE](https://www.arduino.cc/en/software).
2.  Install the Seeed Studio XIAO RP2040 board support package. You can find instructions on how to do this [here](https://wiki.seeedstudio.com/XIAO-RP2040-with-Arduino/).
3.  Install the "Adafruit TinyUSB Library" from the Arduino Library Manager.
4.  Open the `arduino/RNDIS_USB/RNDIS_USB.ino` file in the Arduino IDE.
5.  Select the "Seeed Studio XIAO RP2040" board from the "Tools" menu.
6.  Click the "Verify" button to compile the sketch.
