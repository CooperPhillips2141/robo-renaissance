from pydub import AudioSegment
from pydub.playback import play
import sounddevice as sd
import numpy as np
import wave
import subprocess
import smbus2
import time
import RPi.GPIO as GPIO

'''
User detection via PIR sensing returns true if at least one PIR sensor detects movement every 5 minutes
'''

# Define PIR sensor pins
PIR_PINS = [17, 27, 4]  # Example GPIO pins

# Setup GPIO
GPIO.setmode(GPIO.BCM)
for pin in PIR_PINS:
    GPIO.setup(pin, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)  # Enable pull-down resistors

read_time = time.time()
reading_start_time = 0
reading_active = False
reading_interval = 5*60 #read PIRs every 1 minute
reading_duration = 5 #read PIRs for 5 seconds

def read_pir_sensors():
    sensor_values = [GPIO.input(pin) for pin in PIR_PINS]
    print(f"PIR Values: {sensor_values}")
    return sensor_values

def proximity_test():
    global reading_active, read_time, reading_start_time, pir_values
    current_time = time.time()
    if not reading_active and (current_time - reading_start_time >= reading_interval):
        print("Starting PIR sensor reading for 5 seconds...")
        reading_active = True
        reading_start_time = current_time
        read_time = current_time  # Reset last read time for the next cycle

    # Read PIR values for 30 seconds
    if reading_active:
        if current_time - reading_start_time < reading_duration:
            pir_values = read_pir_sensors()  # Read and print sensor values
            time.sleep(1)  # Small delay to avoid excessive processing
        else:
            print("PIR reading complete. Waiting for the next cycle...")
            reading_active = False  # Stop reading and wait for the next cycle
    if 1 in pir_values:
        return True
    else:
        return False
"""
This code takes an array of percentages base on emotional frequencies from Arduino and plays mixed audio from a set of 7 audio files associate with 7 emotions
on the feelings wheel. 
A new user input will be accepted once the audio is finished playing. The audio will only play if a new user input is entered
"""

# I2C setup
I2C_ADDRESS = 0x08  # I2C address of the Arduino
bus = smbus2.SMBus(1)  # Use I2C bus 1

def receive_percentages():
    """
    Requests 7 floats from the Arduino.
    Returns a list of floats.
    """
    try:
        bus.write_byte(I2C_ADDRESS, ord('F'))  # Send 'F' to request floats
        time.sleep(0.1)  # Allow Arduino to prepare data

        # Read 28 bytes (7 floats x 4 bytes each)
        data = bus.read_i2c_block_data(I2C_ADDRESS, 0, 28)
        float_list = [struct.unpack('f', bytes(data[i:i + 4]))[0] for i in range(0, 28, 4)]
        return float_list
    except Exception as e:
        print("Error recieving list from Arduino: ", e)

def send_I2C(command):
    try:
        bus.write_byte(I2C_ADDRESS, command)
    except Exception as e:
        print("Error sending command to Arduino:", e)

def sound_mix(audio_files, percentages, target_loudness = -50.0):
    """
    Mixes audio files based on the given percentages and plays the mixed audio.

    Args:
        audio_files (list of str): List of file paths to the audio files.
        percentages (list of float): List of percentages for each audio file (0.0 to 1.0).

    Returns:
        AudioSegment: The mixed audio.
    """
    if percentages == None:
        return None
    # Ensure inputs are valid
    if any(p < 0 or p > 1 for p in percentages):
        raise ValueError("Percentages must be between 0.0 and 1.0.")

    # If all percentages are 0, play nothing
    if sum(percentages) == 0:
        print("All percentages are zero. Nothing to play.")
        return None

    # Initialize a silent audio segment to mix into
    # CHANGE "duration=___" to change audio length (units = milliseconds)
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

    # Normalize the overall volume to the target loudness
    loudness_difference = target_loudness - mixed_audio.dBFS
    mixed_audio = mixed_audio.apply_gain(loudness_difference)
    
    print("Playing Audio")
    mixed_audio.export("mixed_output.wav", format="wav")
    print("Successfully mixed and save new audio.")
    try:
        subprocess.run(["aplay", "-D", "hw:3,0", "mixed_output.wav"], check=True)
        print("Playing Mixed Audio with aplay...")
    except subprocess.CalledProcessError as e:
        print(f"Error playing audio: {e}")


# Example audio file name/ directory, make sure there are 7 total (one for each percentage)
#audio_files = ["Audio_files/Angry.mp3", "Audio_files/Bad.mp3", "Audio_files/Disgusted.mp3", "Audio_files/Fearful.mp3", "Audio_files/Happy.mp3", "Audio_files/Sad.mp3",  "Audio_files/Surprised.mp3"]
audio_files = ["Audio_files/Drums.mp3", "Audio_files/Electric_Bass.mp3", "Audio_files/Electric_Guitar.mp3", "Audio_files/Instrumental.mp3", "Audio_files/Piano.mp3",  "Audio_files/Violin.mp3", "Audio_files/Vocal.mp3"] 
# Variable to store past percentages list
percentages_past = 0

while True:
    temp = percentages_past
    # Read user input and play audio 
    percentages = receive_percentages()
    print("Percentages", percentages)
    if percentages_past != percentages:
        #percentages = [0.1, 0.1, 0.1, 0.2, 0.2, 0.2, 0.1]
        sound_mix(audio_files, percentages)
        print("Audio playback complete...")
        percentages_past = percentages
    send_I2C(proximity_test)  # send userDetected flag over I2C to Arduino 

    time.sleep(1)
