# Desk Companion

A small Arduino control panel that sits next to my monitor and controls my Windows PC:
media playback, system volume and microphone mute, with the current song shown on an OLED.

The project has two parts that talk to each other over USB serial:

- **Firmware** (`src/`) – runs on an Arduino Uno, reads the inputs and drives the displays.
- **PC helper** (`pc-helper/`) – a Python program on Windows that turns the Arduino's messages
  into actions and sends song info back.

## Features

| Input / output | What it does |
|---|---|
| Touch sensor, tap | Play / pause |
| Touch sensor, hold | Mute / unmute the microphone |
| Two push buttons | Previous / next track |
| Potentiometer | System volume in 10 steps |
| 1.3" OLED | Song title (scrolls when too long), artist, play status, clock, volume bar |
| 7-segment display | Current volume level (0–9) |
| Red LED | Lights up while the microphone is muted |

Media control works with anything that shows up in Windows' media controls,
e.g. Spotify or YouTube in the browser. Mute state is read back from Windows,
so the LED stays correct even when you mute with the mouse.

## Hardware

- Arduino Uno (DFRduino UNO R3)
- Waveshare 1.3" OLED Module (C) – SH1107, 64×128, SPI
- DFRobot Gravity Touch Sensor V2
- F5101BH single-digit 7-segment display (common anode) + 7 resistors (220 Ω – 1 kΩ)
- 10 kΩ potentiometer
- 2 push buttons
- Red LED + 1 resistor
- Breadboard and jumper wires

Full pin assignment and breadboard positions: [docs/wiring.md](docs/wiring.md)

## Setup

### 1. Firmware

Requires [VS Code](https://code.visualstudio.com/) with the **PlatformIO IDE** extension.

1. Open the repo's root folder in VS Code (*File → Open Folder*). PlatformIO detects the project from `platformio.ini` and builds the code in `src/`.
2. Click **Upload** (→) in the blue status bar. PlatformIO installs the U8g2 library automatically.
3. The OLED shows *Waiting for PC...* until the helper is running.

### 2. PC helper (Windows)

Requires Python 3.9 or newer.

```
cd pc-helper
py -m pip install -r requirements.txt
py desk_helper.py
```

The helper finds the Arduino's COM port automatically. If it picks the wrong one,
set `PORT = "COM3"` (with your port) at the top of `desk_helper.py`.

Close the PlatformIO Serial Monitor before starting the helper, only one program can use the port.

## Serial protocol

One message per line, 9600 baud.

| Direction | Message | Meaning |
|---|---|---|
| Arduino → PC | `HELLO` | Arduino started |
| Arduino → PC | `PLAY_PAUSE`, `PREV`, `NEXT` | Media control |
| Arduino → PC | `MUTE` | Toggle microphone |
| Arduino → PC | `VOL:<0-9>` | Knob position |
| PC → Arduino | `SONG:<title>\|<artist>` | Current song |
| PC → Arduino | `PLAY:<0\|1>` | Playing or paused |
| PC → Arduino | `MUTE:<0\|1>` | Actual microphone state |
| PC → Arduino | `TIME:<HH:MM>` | Clock |

The PC sends its status every second. If the Arduino hears nothing for 5 seconds,
it falls back to *Waiting for PC...*.

## Project structure

```
desk-companion/
├── platformio.ini
├── src/
│   └── main.cpp
├── pc-helper/
│   ├── desk_helper.py
│   └── requirements.txt
└── docs/
    └── wiring.md
```

## Ideas for later

- IR remote support (an IR receiver is planned as an additional input)
- Start the helper automatically with Windows
- A case for the finished build
