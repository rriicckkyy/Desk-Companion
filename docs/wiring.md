# Wiring

Board: Arduino Uno (DFRduino UNO R3). All parts share the breadboard power rails.

## Power

| From | To | Wire |
|---|---|---|
| Arduino 5V | Right + rail | Red |
| Arduino GND | Right − rail | Black |
| Right + rail | Left + rail (bridge at the bottom) | Red |
| Right − rail | Left − rail (bridge at the bottom) | Black |

## OLED – Waveshare 1.3" OLED (C), SH1107, 4-wire SPI

The colors are fixed by the module's cable.

| OLED pin | Cable | Arduino |
|---|---|---|
| VCC | Red | + rail |
| GND | Black | − rail |
| DIN | Blue | 11 |
| CLK | Yellow | 13 |
| CS | Orange | 10 |
| DC | Green | 7 |
| RST | White | 8 |

Pin 12 is reserved by hardware SPI and stays unused.

## Touch sensor – DFRobot Gravity Touch Sensor V2

| Sensor wire | Arduino |
|---|---|
| Green (signal) | 2 |
| Red (VCC) | + rail |
| Black (GND) | − rail |

## 7-segment display – F5101BH (common anode)

Placed across the center gap in rows 10–14. Each segment goes through its own
resistor (220 Ω – 1 kΩ) to the Arduino. One COM pin goes to the + rail, the
decimal point and the second COM stay unconnected.

| Segment | Display pin | Arduino | Wire |
|---|---|---|---|
| a | 7 | 3 | Orange |
| b | 6 | 4 | Yellow |
| c | 4 | 5 | Green |
| d | 2 | 6 | Blue |
| e | 1 | 9 | Purple |
| f | 9 | A0 | White |
| g | 10 | A1 | Gray |
| COM | 3 or 8 | + rail | Red |

A segment lights up when its pin is LOW.

## Potentiometer – 10 kΩ (volume knob)

| Leg | Hole | Connects to | Wire |
|---|---|---|---|
| Outer | c28 | Left + rail | Red |
| Middle | c29 | A2 | Blue |
| Outer | c30 | Left − rail | Black |

Swap red and black if the knob turns the wrong way.

## Push buttons (across the center gap)

| Button | Legs | Signal wire | Ground wire |
|---|---|---|---|
| Previous | e19, e21, f19, f21 | Green: a19 → A3 | Black: j21 → right − rail |
| Next | e23, e25, f23, f25 | White: a23 → A4 | Black: j25 → right − rail |

No resistors needed, the code uses the Arduino's internal pull-ups.

## Mute LED

| Part | Holes | Connects to | Wire |
|---|---|---|---|
| LED long leg (+) | row 2, right half | A5 | Orange (from f2) |
| LED short leg (−) | row 3, right half | resistor → right − rail | – |
