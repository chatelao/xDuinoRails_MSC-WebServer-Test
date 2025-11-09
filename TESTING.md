# Manual Host-Side RNDIS Verification

This document outlines the steps to manually verify that the Seeed Studio XIAO RP2040 is correctly recognized as an RNDIS network device by a host computer.

**Prerequisites:**

*   The firmware from this project has been compiled and flashed to the Seeed Studio XIAO RP2040.
*   The device is connected to the host computer via a USB cable.

**Verification Steps:**

1.  **Connect the Device:**
    *   Connect the XIAO RP2040 to your host computer.

2.  **Check for Device Recognition:**
    *   **Windows:**
        1.  Open the **Device Manager**.
        2.  Look for a new device under the "Network adapters" section. It should appear as "USB Ethernet/RNDIS Gadget" or a similar name.
        3.  If the device appears under "Other devices" with a yellow exclamation mark, you may need to manually install the RNDIS drivers.
    *   **macOS:**
        1.  Open **System Preferences** > **Network**.
        2.  A new network interface, such as "RNDIS/Ethernet Gadget," should appear in the list.
    *   **Linux:**
        1.  Open a terminal.
        2.  Run the command `ip a` or `ifconfig`.
        3.  Look for a new network interface (e.g., `usb0`).

3.  **Expected Outcome:**
    *   If the device is correctly recognized, a new network interface will be visible in your host computer's network settings. This confirms that the TinyUSB RNDIS stack is functioning as expected.
    *   You may not be able to send or receive data without further network configuration (e.g., setting up a DHCP server on the device), but the appearance of the network interface is sufficient to confirm that the RNDIS functionality is initialized correctly.
