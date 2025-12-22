# SkyView AMOLED – Hardware Guide

**Battery • Power Management • Audio Module**

This document provides complete hardware notes for the **SkyView AMOLED flight instrument**, based on the **LilyGO T-Display S3 AMOLED 1.75”** board.

---

## Contents

- 🔋 Battery & power management
- ⚡ Power-off behaviour (SY6970 BATFET)
- 🔊 Audio output (I2S amplifier + speaker)
- 🪛 MAX98357A wiring diagrams
- 🧩 3D enclosure options

---

## 🔋 1. Battery Guide

### ✔ Recommended Battery

The reference build uses a **523450 Li-Po battery (1000 mAh)**.
**Buy the recommended battery:**  
[523450 Li-Po 1000 mAh on AliExpress](https://www.aliexpress.com/item/1005006859221606.html?spm=a2g0o.order_detail.order_detail_item.3.7d59f19cIAvFBz)
<p align="left">
  <img src="/Documentation/Hardware/build_images/Battery_523450.png" width="430" alt="MAX98357A to LilyGO AMOLED wiring">
</p>

| Property | Value |
|--------|------|
| Dimensions | 5.2 mm × 34 mm × 50 mm |
| Capacity | 1000 mAh |
| Fit | Tight fit in round enclosure (~50 mm internal Ø) |
| Modification | Light rounding of two corners |
| Runtime | ~10 hours |

**Measured power consumption**
- Idle system draw: ~100 mA  
- Expected runtime: ~10 hours  
- Confirmed via internal battery logger


---

## ⚡ 2. Power-Off Behaviour & BATFET Cut-Off (SY6970)

Earlier firmware versions left the PMU partially active, causing **~5 mA background drain** during storage.

➡️ **Resolved:** Full BATFET shutdown implemented.

---

### BATFET OFF (Shipping / Storage Mode)

When BATFET is disabled:

- Battery is electronically disconnected
- Idle drain drops to **0 mA**
- Ideal for long-term storage and shipping

---

### Wake Procedure After BATFET OFF

When the battery is disconnected, the physical power button cannot wake the board.

**To wake the device:**
1. Plug in USB power briefly  
2. Battery reconnects  
3. Device boots normally

---

### Firmware Power Logic

| Action | Result |
|-----|------|
| Long press | Deep sleep |
| Long press again | Wake |
| Double-press + long press | Full power-off (BATFET OFF) |
| Wake after BATFET OFF | Requires USB |

✔ Suitable for long-term storage  
✔ Prevents battery depletion

---

## 🔊 3. Audio Output (I2S Amplifier + Speaker)

SkyView AMOLED supports **full voice traffic alerts**, including:

- Traffic distance callouts
- Danger warnings
- `$PFLAA` alarm voice prompts
- Configurable voice alerts can be disabled in the menu

Here is the demo of the voice alerts - Youtube video:  
[![SkyView AMOLED Voice Alerts Demo](https://img.youtube.com/vi/VIDEO_ID/0.jpg)](https://www.youtube.com/watch?v=VIDEO_ID)
---

### 🚫 What's Not Yet Implemented

The following audio features are planned:

- Thermal vario tones
- Altitude announcements
- Airspace warnings

(These are on the roadmap.)

---

## 🔉 4. MAX98357A I2S Audio Amplifier

You can add a **MAX98357A Class-D I2S amplifier** to the SkyView LiliyGo 1.75 AMOLED and a **8 Ω cavity speaker** internally to achive loud, clear audio output.


---

### ✔ Recommended Parts

| Part | Notes |
|----|------|
| MAX98357A I2S amplifier | [Link AliExpress](https://www.aliexpress.com/item/1005007068815767.html?spm=a2g0o.order_list.order_list_main.10.6f4c1802BtdR80) |
|20x14mm Cavity speaker 8ohm 0.5-2W | [Link AliExpress](https://www.aliexpress.com/item/1005004431389186.html?spm=a2g0o.order_list.order_list_main.244.6f4c1802BtdR80) |

<p align="left">
  <img src="/Documentation/Hardware/build_images/MAX98357a_product.png" width="430" alt="MAX98357A to LilyGO AMOLED wiring">
    <img src="/Documentation/Hardware/build_images/Cavity_Speaker_8_Ohm.png" width="430" alt="MAX98357A to LilyGO AMOLED wiring">
</p>


## 🪛 5. Wiring: MAX98357A → LilyGO AMOLED

<p align="left">
  <img src="/Documentation/Hardware/build_images/AMOLED_MAX98357a connection.png" width="430" alt="MAX98357A to LilyGO AMOLED wiring">
</p>

### Audio Amplifier Wiring Table

| LilyGO Pin | MAX98357A Pin | Description |
|:-----------|:--------------|:------------|
| BAT        | **VIN**       | Power |
| GND        | **GND**       | Ground |
| GPIO 02   | **BCLK**      | Bit clock |
| GPIO 01   | **LRCLK (WS)**| Word select |
| GPIO 03   | **DIN (DOUT)**| Audio data |
| GPIO 47   | **SD**        | Shutdown / Enable |

---

### ✔ Recommended Installation Method

For a clean, compact assembly:

1. Remove the pin header from the LilyGO board  
2. Trim the MAX98357A PCB edges if needed  
3. Align matching pins  
4. Solder **board-to-board** directly  where needed

As per digram.

---

## 🧩 3D Case Models

### ✔ Watch Style Enclosure (Current)

Designed for:
- 523450 (1000 mAh) battery
- MAX98357A amplifier
- Internal micro speaker
- SD Card access
- Large power button

Other features:
- Internal speaker cavity
- Optional wrist strap attachement

---


