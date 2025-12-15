## SkyView BLE Connection

This photo shows SkyView connected to a BLE device, waiting for GPS Fix:

<img width="500" height="500" alt="image" src="https://github.com/user-attachments/assets/2badc07a-e58c-4824-89de-238effe75664" />

The BLE battery level is visible if the BLE device (in this case a SoftRF T-Beam) supports reporting battery level.

During the NO DATA and NO FIX phase, the SkyView display will fade in/fade out to reduce battery usage and to indicate it's not ready for normal operation.
As soon as valid GPS data comes in, the device will switch to **Radar View**.
