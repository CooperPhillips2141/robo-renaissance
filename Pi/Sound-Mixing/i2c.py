import struct
from pydub import AudioSegment
from pydub.playback import play
import sounddevice as sd
import numpy as np
import wave
import subprocess
import smbus2 as smbus
import time
print("Sucessfully imported Libraries!")

"""
This code takes an array of percentages base on emotional frequencies from Arduino and plays mixed audio from a set of 7 audio files associate with 7 emotions
on the feelings wheel. 
A new user input will be accepted once the audio is finished playing. The audio will only play if a new user input is entered
"""

# I2C setup
I2C_ADDRESS = 0x08  # I2C address of the Arduino
bus = smbus.SMBus(1)  # Use I2C bus 1

def recieve_percentages():
    try:
        # Request 35 bytes (8 floats * 4 bytes each)
        data = bus.read_i2c_block_data(I2C_ADDRESS, 0, 28)

        # Convert the byte data to floats
        floats = struct.unpack('7f', bytes(data))
        return list(floats)
    except Exception as e:
        print("Error reading from Arduino:", e)
        return []

def send_I2C(command):
    try:
        bus.write_byte(I2C_ADDRESS, command)
    except Exception as e:
        print("Error sending command to Arduino:", e)

#Set up audio files
audio_files = ["Audio_files/Drums.mp3", "Audio_files/Electric_Bass.mp3", "Audio_files/Electric_Guitar.mp3", "Audio_files/Instrumental.mp3", "Audio_files/Piano.mp3", "Audio_files/Violin.mp3", "Audio_files/Vocal.mp3"]

def sound_mix(audio_files, percentages):
    """
    Mixes audio files based on the given percentages and plays the mixed audio.

    Args:
        audio_files (list of str): List of file paths to the audio files.
        percentages (list of float): List of percentages for each audio file (0.0 to 1.0).

    Returns:
        AudioSegment: The mixed audio.
    """
    # Ensure inputs are valid
    if any(p < 0 or p > 1 for p in percentages):
        raise ValueError("Percentages must be between 0.0 and 1.0.")

    # If all percentages are 0, play nothing
    if sum(percentages) == 0:
        print("All percentages are zero. Nothing to play.")
        return None

    # Initialize a silent audio segment to mix into
    mixed_audio = AudioSegment.silent(duration=10000)

    # Process each audio file
    for i in range(len(audio_files)):
        # Load the audio file
        audio = AudioSegment.from_file(audio_files[i])
        # Adjust volume based on percentage
        
        if percentages[i] != 0:
             volume_adjustment = (1 - percentages[i]) * 12  # Convert to dBFS adjustment
             adjusted_audio = audio - (12 - volume_adjustment)
        else:
             adjusted_audio = audio - 100
        
        # Mix into the combined audio
        mixed_audio = mixed_audio.overlay(adjusted_audio)
    
    print("Playing Audio")
    mixed_audio.export("mixed_output.wav", format="wav")
    print("Successfully mixed and save new audio.")
    try:
        subprocess.run(["aplay", "-D", "hw:3,0", "mixed_output.wav"], check=True)
        print("Playing Mixed Audio with aplay...")
    except subprocess.CalledProcessError as e:
        print(f"Error playing audio: {e}")

new_audio = True
audio_played = False
percentages_past = 0

while True:
    temp = percentages_past
    # Read user input and play audio 
    percentages = recieve_percentages()
    print("Percentages", percentages)
    if percentages_past != percentages:
        sound_mix(audio_files, percentages)
        print("Audio playback complete...")
        percentages_past = percentages
    # Tell Arduino we are ready for another user input after
    send_I2C(1)
    time.sleep(1)
