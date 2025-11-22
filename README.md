# xDuinoRails_DccCvMscWeb

An Arduino library for the Seeed Studio XIAO RP2040 (and other RP2040 boards) that provides RNDIS network interface, Mass Storage Class (RAM Disk), and a Web Server.

## Features

*   **RNDIS**: USB Network device (IP: 192.168.7.1).
*   **MSC**: USB Mass Storage device (64KB RAM Disk).
*   **Web Server**: LwIP-based HTTP server serving content from the internal file system.

## Project Structure

This repository is structured as a standard Arduino Library.

*   **`src/`**: Library source code.
*   **`examples/`**: Example sketches.
*   **`platformio/`**: PlatformIO configuration for building the example.

## Usage

### Arduino IDE

1.  Download this repository as a ZIP file.
2.  In Arduino IDE, go to **Sketch** > **Include Library** > **Add .ZIP Library...** and select the downloaded file.
3.  Go to **File** > **Examples** > **xDuinoRails_DccCvMscWeb** > **RndisMscWeb**.
4.  Select your board (e.g., Seeed Studio XIAO RP2040).
5.  **Important**: Select **USB Stack: Adafruit TinyUSB** in the Tools menu.
6.  Upload the sketch.

### PlatformIO

1.  Open the `platformio` directory in VS Code / PlatformIO.
2.  Run `pio run` or `pio run -t upload`.
3.  The configuration in `platformio.ini` automatically builds the included example using the library code.

## Notes

This library relies on the `arduino-pico` core and its LwIP/TinyUSB integration. It uses specific build flags (like `-DLWIP_HTTPD=1`) which are set in `platformio.ini` but might require manual core modification or specific board menu settings in Arduino IDE if not using the provided platformio config or if the core defaults don't enable these LwIP features.

For Arduino IDE, the CI workflow automatically patches some TinyUSB headers. Users might need to ensure `rndis_protocol.h` and `ndis.h` are available if they encounter compilation errors (this library includes them in `src/`).
