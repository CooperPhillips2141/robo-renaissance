from pydub import AudioSegment
from pydub.playback import play
import numpy as np
import pygame
import wave
import subprocess
import smbus2
import time
import struct
import RPi.GPIO as GPIO
import random

while True:
    try:
        pygame.init()
        pygame.mixer.init()
        break
    except pygame.error as e:
        print(f"Failed to initialize pygame.mixer: {e}")
        time.sleep(1)

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
reading_interval = 5*60 #read PIRs every 5 minute
reading_duration = 5 #read PIRs for 5 seconds
PIR_trip = False

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
            send_I2C(2)
            print("PIR ACTIVE /n")
            pir_values = read_pir_sensors()  # Read and print sensor values
            time.sleep(1)  # Small delay to avoid excessive processing
            if 1 in pir_values:
                send_I2C(1)
                #return 1
            elif 0 in pir_values:
                send_I2C(0)
                #return 0
        else:
            print("PIR reading complete. Waiting for the next cycle...")
            reading_active = False  # Stop reading and wait for the next cycle
            print("PIR NOT ACTIVE /n")
            send_I2C(3)
"""
This code takes an array of percentages base on emotional frequencies from Arduino and plays mixed audio from a set of 7 audio files associate with 7 emotions
on the feelings wheel. 
A new user input will be accepted once the audio is finished playing. The audio will only play if a new user input is entered
"""

def sound_mix(emotions, percentages, target_loudness = -30.0):
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
    mixed_audio = AudioSegment.silent(duration=0)
    
    # Establish randomized list for playing audio in unique order
    index = [0,1,2,3,4,5,6]
    random.shuffle(index)
    
    choice = random.randint(0,1)
    
    # Process each audio file
    for i in range(len(emotions)):
        if percentages[index[i]] != 0:
             # Load the audio file
             if choice == 0:
                audio = AudioSegment.from_file("Balanced_Audio/" + emotions[index[i]])
             else:
                 audio = AudioSegment.from_file("Balanced_Audio/" + emotionsAlt[index[i]])
             # Adjust volume based on percentage
             volume_adjustment = (1 - percentages[index[i]]) * 12  # Convert to dBFS adjustment
             adjusted_audio = audio - (12 - volume_adjustment)
             # Mix into the combined audio
             mixed_audio = mixed_audio + adjusted_audio
        
    # Normalize the overall volume to the target loudness
    loudness_difference = target_loudness - mixed_audio.dBFS
    mixed_audio = mixed_audio.apply_gain(loudness_difference)
    
    print("Playing Audio")
    mixed_audio.export("mixed_output.wav", format="wav")
    music = pygame.mixer.Sound("mixed_output.wav")
    print("Successfully mixed and save new audio.")
    try:
        pygame.mixer.Sound.play(music)
        print("Playing Mixed Audio...")
    except subprocess.CalledProcessError as e:
        print(f"Error playing audio: {e}")


# Example audio file name/ directory, make sure there are 7 total (one for each percentage)
#audio_files = ["Audio_files/Angry.mp3", "Audio_files/Bad.mp3", "Audio_files/Disgusted.mp3", "Audio_files/Fearful.mp3", "Audio_files/Happy.mp3", "Audio_files/Sad.mp3",  "Audio_files/Surprised.mp3"]
emotions = np.array(["Sad Balanced.mp3", "Disgusted Balanced.mp3", "Anger Balanced.mp3", "Fear Balanced.mp3", "Bad Balanced.mp3", "Surprised Balanced.wav", "Happy Balanced.wav"])
emotionsAlt = np.array(["Sad.mp3", "Disgusted.mp3", "Angry.mp3", "Fearful.mp3", "Bad.mp3", "Surprised.mp3", "Happy.mp3"])
percentages_past = 0

while True:
    temp = percentages_past
    # Read user input and play audio 
    percentages = receive_percentages()
    print("Percentages", percentages)
    proximity_test()
    if percentages_past != percentages:
        sound_mix(emotions, percentages)
        print("Audio playback complete...")
        percentages_past = percentages  # send userDetected flag over I2C to Arduino 

    time.sleep(1)
