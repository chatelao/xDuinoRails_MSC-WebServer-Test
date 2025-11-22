# Host-Side Verification

This document outlines the steps to verify the functions of the Seeed Studio XIAO RP2040 composite device:
1.  **RNDIS Network Interface**
2.  **USB Mass Storage (MSC)**
3.  **Webserver**

## Prerequisites

*   The firmware has been flashed to the device.
*   The device is connected to the host computer via USB.

## 1. Verify USB Mass Storage

When you plug in the device, a small (64KB) USB drive should appear on your system.

*   **Windows:** Check File Explorer for a new drive (e.g., `USB Drive (D:)`).
*   **macOS/Linux:** The drive should mount automatically.

**Verification:**
*   Open the drive.
*   You should see a file named `README.TXT`.
*   Open `README.TXT`. It should contain text instructing you to connect to `http://192.168.7.1`.

*Note: If the drive asks to be formatted, you can try formatting it, but the current firmware initializes it with a FAT12 filesystem in RAM every time it boots. Any data written will be lost on reboot.*

## 2. Verify RNDIS Network Interface

*   **Windows:** Device Manager > Network adapters > "USB Ethernet/RNDIS Gadget" (or similar).
*   **macOS:** System Preferences > Network > "RNDIS/Ethernet Gadget".
*   **Linux:** `ip a` should show a new interface (e.g., `usb0`).

## 3. Verify Webserver

To access the webserver, your computer must be on the same network subnet as the device. The device IP is **192.168.7.1**.

### Step A: Configure Host Network
You must manually assign an IP address to the RNDIS network interface on your computer.

*   **IP Address:** `192.168.7.2` (or any address from .2 to .254)
*   **Subnet Mask:** `255.255.255.0`
*   **Gateway:** (Leave blank or set to 192.168.7.1)

**Windows Example:**
1.  Control Panel > Network and Internet > Network Connections.
2.  Right-click the RNDIS adapter > Properties.
3.  Select "Internet Protocol Version 4 (TCP/IPv4)" > Properties.
4.  Select "Use the following IP address".
5.  Enter IP: `192.168.7.2`, Mask: `255.255.255.0`.
6.  Click OK.

**Linux Example:**
```bash
sudo ip link set dev usb0 up
sudo ip addr add 192.168.7.2/24 dev usb0
```

### Step B: Access the Webserver

**Option 1: Browser**
1.  Open a web browser.
2.  Navigate to `http://192.168.7.1`.
3.  You should see a page saying "Hello from RP2040 RNDIS!".

**Option 2: Automated Script**
A Python script `test_webserver.py` is provided in the root of the repository.
1.  Ensure you have Python installed.
2.  Install requests: `pip install requests`
3.  Run the script: `python test_webserver.py`
