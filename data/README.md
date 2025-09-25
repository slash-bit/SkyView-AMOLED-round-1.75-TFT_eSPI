# Data
The SD.ZIP archive can be downloaded (raw) and extracted in the root of a suitable FAT32 formatted SD card for the Waveshare board.

This should result in the following directory structure:
<img width="123" height="187" alt="image" src="https://github.com/user-attachments/assets/1dcdcf53-46f2-4ba3-be70-4ad6380921f1" />

### OGN database
- download most recent .CDB file that contains OGN Aircrafts Data from [this location](http://soaringweather.no-ip.info/ADB/data/ogn.cdb).
- (re)place **ogn.cdb** file in the **/Aircrafts** folder of the SD card
- Verify on the **Status** page of SkyView Web Server at [http://192.168.1.1](http://192.168.1.1) if the database was loaded correctly (shows number of OGN records).

### BuddyList.txt
- This text file contains a simple text file with per line:
  
    ID, FirstName LastName

- The IDs in this file will result in identified traffic on the radar screen.
- Use the Web Server to upload a new BuddyList.txt file



