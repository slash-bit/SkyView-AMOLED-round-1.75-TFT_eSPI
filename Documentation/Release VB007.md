### Release VB007

- Enhanced power management with new Power Options menu
- Visual thermal indicators on Radar screen
- Toggle between average and instantaneous climb rate in Traffic Info view
- Improved button controls with new long-press menu
- Better watchdog handling during BLE scans
- Power optimization for Waveshare AMOLED 1.75

---

## What's New in VB007

1. **Power Options Menu**

    - New long-press button feature brings up a dedicated Power Options menu
    - Two power-saving modes available:
        - **Sleep Mode**: Quick deep sleep with fast wake up by long hold the button capability
        - **Full Shutdown**: Complete power-off via BATFET battery disconnect for maximum power saving on LilyGo boards
    - Easy access to power management without navigating through settings

    <div align="left">

    ![Power Options Menu](images/power_options.jpg)

    </div>

---

2. **Button Controls**

    - **Single click**: Change view mode (cycles through Radar → Traffic Info → Compass)
    - **Double click**: Go to Settings page
    - **Long press**: Show Power Options menu
    - **Power Options Menu interactions**:
        - Single click: Enter Sleep mode
        - Double click: Full shutdown

    The new button control system is managed by a dedicated FreeRTOS task (`vTask`) for reliable operation.

---

3. **Show Thermals on Radar**

    - New option in Settings to display thermal activity
    - Aircraft with positive climb rates are marked with colored dots on the Radar screen:
        - **Yellow**: Climb rate 1.5 - 2.0 m/s (gentle lift)
        - **Orange**: Climb rate 2.0 - 3.5 m/s (moderate thermal)
        - **Red**: Climb rate ≥ 3.5 m/s (strong thermal)
    - Toggle this feature ON/OFF in Settings page
    - Helps identify thermaling aircraft and active lift areas
    - Available in all radar view modes

---

4. **Average vs Instantaneous Climb Rate**

    - Tap on the "Climbrate" area in Traffic Info screen to toggle between display modes
    - **Instantaneous mode**: Shows real-time climb rate from current NMEA data
    - **Average mode**: Shows 10-sample rolling average for smoother indication
        &#x2610; "AV" indicator appears when average mode is active
    - Useful for filtering out climb rate fluctuations and getting a better sense of sustained climbs
    - Buffer automatically resets when switching to a different aircraft

---

5. **Power Optimization**

    - Disabled analog power path on Waveshare AMOLED 1.75 to reduce power consumption
    - Improved watchdog handling during BLE scans prevents unwanted device resets
    - Better power management for extended battery life

---

6. **BLE and SPIFFS Improvements**

    - Watchdog timer properly disabled/enabled around BLE scans
    - Prevents device resets during long scans
    - Enhanced SPIFFS mount safety checks before filesystem operations
    - More reliable BLE reconnection timing

---

**Easy Firmware Update**

Watch the video for instructions on how to **update SkyView firmware** via the built-in **Web Interface**:
https://youtu.be/NbYAM7KYvmo

Link to the new firmware:
<span style="font-size: smaller;">

[SkyView_firmware_VB007.bin](https://github.com/slash-bit/SkyView-AMOLED-round-1.75-TFT_eSPI/releases)
</span>

---

**Compatibility**

This firmware is compatible with:
:heavy_check_mark: LilyGo AMOLED 1.75" T-Display (full features including BATFET shutdown)
&cross; Not yet compatible with Waveshare AMOLED 1.75" H0175Y003AM display

---
