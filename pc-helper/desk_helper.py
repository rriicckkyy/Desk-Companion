"""Desk Companion - PC helper for Windows.

Listens to the Arduino over USB serial and controls Windows:
  PLAY_PAUSE / NEXT / PREV -> media controls (Spotify, browser, ...)
  VOL:<0-9>                -> system volume
  MUTE                     -> toggles the microphone

Every second it sends the current song, play state, mic mute state and time back.

Run with:  py desk_helper.py
Close the PlatformIO Serial Monitor first, only one program can use the COM port.
"""

import asyncio
import time
import unicodedata
from ctypes import POINTER, cast

import serial
import serial.tools.list_ports
from comtypes import CLSCTX_ALL
from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
from winrt.windows.media.control import (
    GlobalSystemMediaTransportControlsSessionManager as MediaManager,
    GlobalSystemMediaTransportControlsSessionPlaybackStatus as PlaybackStatus,
)

PORT = None  # e.g. "COM3". None = detect automatically
BAUD = 9600


# ---------- Serial ----------

def find_port():
    for p in serial.tools.list_ports.comports():
        info = f"{p.description} {p.manufacturer or ''}".lower()
        if any(key in info for key in ("arduino", "ch340", "usb serial", "usb-serial")):
            return p.device
    return None


def send(ser, message):
    ser.write((message + "\n").encode("ascii", errors="ignore"))


def clean(text, limit):
    """The OLED font only has plain ASCII: turn 'Beyoncé' into 'Beyonce'."""
    text = unicodedata.normalize("NFKD", text or "")
    text = text.encode("ascii", "ignore").decode()
    return text.replace("|", "/").replace("\n", " ")[:limit]


# ---------- Audio (volume + microphone) ----------

def endpoint_volume(device):
    # newer pycaw versions wrap the device, older ones need Activate()
    if hasattr(device, "EndpointVolume"):
        return device.EndpointVolume
    interface = device.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
    return cast(interface, POINTER(IAudioEndpointVolume))


def set_volume(level):
    speakers = endpoint_volume(AudioUtilities.GetSpeakers())
    speakers.SetMasterVolumeLevelScalar(level / 9, None)


def get_mic():
    mic = AudioUtilities.GetMicrophone()
    return endpoint_volume(mic) if mic else None


def mic_muted():
    mic = get_mic()
    return bool(mic.GetMute()) if mic else False


def toggle_mic():
    mic = get_mic()
    if mic:
        mic.SetMute(not mic.GetMute(), None)


# ---------- Handling messages from the Arduino ----------

async def handle_command(command, manager, ser):
    session = manager.get_current_session()

    if command == "PLAY_PAUSE" and session:
        await session.try_toggle_play_pause_async()
    elif command == "NEXT" and session:
        await session.try_skip_next_async()
    elif command == "PREV" and session:
        await session.try_skip_previous_async()
    elif command == "MUTE":
        toggle_mic()
        send(ser, f"MUTE:{int(mic_muted())}")  # update the LED right away
    elif command.startswith("VOL:"):
        set_volume(int(command[4:]))
    else:
        if command:
            print(f"   (Arduino says: {command})")
        return

    print(f"<- {command}")


async def send_status(manager, ser):
    title, artist, playing = "", "", False
    session = manager.get_current_session()
    if session:
        try:
            props = await session.try_get_media_properties_async()
            title, artist = props.title, props.artist
            playing = session.get_playback_info().playback_status == PlaybackStatus.PLAYING
        except OSError:
            pass  # the media app closed while we were asking

    send(ser, f"SONG:{clean(title, 40)}|{clean(artist, 30)}")
    send(ser, f"PLAY:{int(playing)}")
    send(ser, f"MUTE:{int(mic_muted())}")
    send(ser, "TIME:" + time.strftime("%H:%M"))


# ---------- Main loop ----------

async def run(port):
    print(f"Connecting to {port} ...")
    with serial.Serial(port, BAUD, timeout=0) as ser:
        await asyncio.sleep(2)  # the Arduino restarts when the port opens
        print("Connected. Press Ctrl+C to stop.")

        manager = await MediaManager.request_async()
        buffer = ""
        last_status = 0.0

        while True:
            buffer += ser.read(ser.in_waiting or 1).decode("ascii", errors="ignore")
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                await handle_command(line.strip(), manager, ser)

            if time.monotonic() - last_status >= 1:
                last_status = time.monotonic()
                await send_status(manager, ser)

            await asyncio.sleep(0.02)


async def main():
    while True:
        port = PORT or find_port() or "COM3"
        try:
            await run(port)
        except serial.SerialException as error:
            print(f"Serial problem: {error}")
            print("Is the Arduino plugged in and the Serial Monitor closed? Retrying in 3 s ...")
            await asyncio.sleep(3)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nStopped.")