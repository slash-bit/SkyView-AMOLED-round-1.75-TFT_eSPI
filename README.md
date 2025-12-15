# SkyView Traffic Awareness Device

SkyView is a traffic awareness device designed to display air traffic received through EC radio devices, such as **SoftRF SenseCap Card**, and visualize **FLARM NMEA** or **Garmin GDL90** data on its screen.

### Key Features:
- This code is a fork to support [round Waveshare 1.75 AMOLED Display](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75)

  <img width="300" height="300" alt="image" src="https://github.com/user-attachments/assets/ab13a9ab-3450-4c14-bfc4-0d2d30d6ca7d" />

- Best suited as a companion device for **SoftRF hardware**, providing user-friendly visualization of nearby traffic.
- Proven compatibility with **SkyEcho**, **Stratux**, **PilotAware** and **PowerMouse+** devices, as reported by builders.

### Waveshare board shenanigans
- the Waveshare board offers two buttons. One labeled BOOT, the other PWR.
- The PWR button can be used to power off the device with a long press > 3 sec, and **very long** press (~7 sec.) to power back on.
- The SLEEP button on the **Settings** page of the touch screen can also be used to power the device off.
- The board will also power on when:
  - A battery is inserted
  - A powered USB cable is inserted
- The BOOT button acts as a MODE switch to step through the SkyView screens in a circular method:
  
   **Radar Screen** ➡️ **Traffic Info Screen** ➡️ **Compass Screen** (if enabled)
- sometimes the board is not recognized by Windows when the USB-C cable is connected. In that case press both PWR and BOOT simulataneously and then release BOOT button. This should result in a COM port again (check Windows Device Manager).

### Supported Data Input Methods:
- **Classic Serial Interface**
- **Wi-Fi UDP Connection**
- **Bluetooth LE**
- ❗**Note: Legacy Bluetooth SPP is $\color{red}{\textrm{NOT}}$ possible** ❗

---

### Enhanced Functionality:
This is a forked repository of SkyView:
- **Supported in PlatformIO**: A versatile tool for building and compiling firmware for various devices and screens.
- **Supports AMOLED Colour Displays**: Optimized for vibrant and high-resolution visualizations.
- **Swipe Page Navigation**: Switching between pages is done effortlessly by swiping **left-right** or **up-down**.
- **Voice Output**: traffic advisory, plus danger alert, requires voice data on SD card.

---

### Demo with Voice Alerts ###
[![Demo with Voice Alerts](https://img.youtube.com/vi/tQvVy7dao8s/hqdefault.jpg)](https://www.youtube.com/watch?v=tQvVy7dao8s)

### Pages and Visualizations:

#### **Radar Page**  
Displays a dynamic visualization of nearby traffic.

![SkyView_Radar_Page300x300](https://github.com/user-attachments/assets/a582ca45-425f-4d3f-b65a-485dfb4d9953)


![SkyView_Radar_Page_300x300_2](https://github.com/user-attachments/assets/6b980b27-f55a-47da-8d94-9f8d6f9b4d1f)

---

#### **Traffic Info Page**  
Provides detailed information on nearby traffic, including individual aircraft views.

<img src="https://github.com/user-attachments/assets/dbf50cfd-8fea-4adb-9e6a-514a463d0512" alt="Traffic Info Overview" width="500px">

![Buddy-Aircraft_View300x300](https://github.com/user-attachments/assets/70256d1b-6bc8-48ec-bd24-8c667b0fe275)


---


---

#### **Compass Page**  
A specialised page for orientation and navigation. Can be switched off in settings via the Web Server.

<img src="https://github.com/user-attachments/assets/9613bb03-ed0b-48e4-b963-74c99b9cf339" alt="Compass Page" width="300px">

---

#### **Settings Page**
Key added feature is the **Settings Page** which allows to change settings "on the fly". Such as Radar View Orientation- North Up or Track Up. Traffic Filtering.
Powering Off the device (Sleep mode).

<img width="405" height="410" alt="image" src="https://github.com/user-attachments/assets/d78a2057-068a-4a9d-97e6-22c7c6ca96b9" />


---
#### ** 3D Case**
The case for the 1.75" Display is not ready yet.


---
