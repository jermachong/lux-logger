
# Visual Lux Logger

The **Visual Lux Logger** is an embedded system project designed to monitor, log, and visualize ambient light levels over time. It utilizes an integrated light sensor and a pixel-based display to provide a graphical history of light intensity (lux) across adjustable time intervals.

---

## Features

* **Real-time Light Logging:** Continuously captures readings from an onboard light sensor.
* **Visual Grid Display:** Represents light data on a pixel matrix. Each pixel corresponds to a specific time unit, creating a visual "heat map" or trace of light history.
* **Adjustable Time Resolution:** Users can configure the duration of each time unit (e.g., 1 second, 10 seconds, or 1 hour per pixel) directly from the device.
* **Multi-Page Replay:** The logger supports paging, allowing users to scroll through extended data sets. This is ideal for visualizing long-term events like a full sunset or sunrise.
* **Bi-directional UART Communication:**
  * **Data Dump:** Export logged data to a PC for storage or analysis.
  * **Data Population:** Upload existing logs from a PC to the device to resume logging from a specific state.

---

## Functional Requirements

### 1. Visualization Logic

The display maps light intensity to pixel brightness or color. Data is written sequentially from left to right, top to bottom (or as per page orientation).

> **Example:** A sunset will show a gradient from bright to dark pixels across the grid; a sunrise will show the inverse.

### 2. Time Management

The device must handle various sampling rates selectable via the user interface:

| **Time Unit**  | **Description**                            |
| -------------------- | ------------------------------------------------ |
| **1 Second**   | High-resolution logging for rapid light changes. |
| **10 Seconds** | Standard logging for short-term monitoring.      |
| **1 Hour**     | Long-term logging for diurnal cycle tracking.    |

### 3. Data Handling and UART

The system must implement a robust serial protocol over UART to facilitate:

* **Export:** Sending the current buffer of lux values to a terminal or custom PC application.
* **Import:** Receiving a structured data log from a PC and populating the internal memory/display buffer to continue logging seamlessly.

---

## Visual Representation

The logger represents data trends visually. Below is a conceptual representation of how a sunset (left) and sunrise (right) transition might look on an 8x8 pixel display:

**Plaintext**

```
[ Sunset Trace ]          [ Sunrise Trace ]
W W W W W G G G          D D D D D G G G
W W W G G G G G          D D G G G G G G
G G G G G D D D          G G G G G W W W
G D D D D D D D          G G W W W W W W

(W = White/Bright, G = Gray/Dim, D = Dark)
```

---

## License

Project Idea 3 - Academic/Internal Use
