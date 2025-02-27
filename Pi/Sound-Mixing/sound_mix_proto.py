import struct
from pydub import AudioSegment
from pydub.playback import play
import sounddevice as sd
import numpy as np
import wave
import subprocess
print("All libraries imported")

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
             # Mix into the combined audio
             mixed_audio = mixed_audio + adjusted_audio
        
    
    print("Playing Audio")
    mixed_audio.export("mixed_output.wav", format="wav")
    print("Successfully mixed and save new audio.")
    try:
        subprocess.run(["aplay", "-D", "hw:3,0", "mixed_output.wav"], check=True)
        print("Playing Mixed Audio with aplay...")
    except subprocess.CalledProcessError as e:
        print(f"Error playing audio: {e}")

# Example percentage array but make sure it sums to 1
percentages = np.array([0.1, 0.2, 0.0, 0.0, 0.1, 0.6, 0.0])
# Example audio file name/ directory, make sure there are 7 total
audio_files = np.array(["Audio_files/Drums.mp3", "Audio_files/Electric_Bass.mp3", "Audio_files/Electric_Guitar.mp3", "Audio_files/Instrumental.mp3", "Audio_files/Piano.mp3", "Audio_files/Violin.mp3", "Audio_files/Vocal.mp3"])

p = np.random.permutation(len(percentages))  # Generate a shuffledx array
percentages, audio_files = percentages[p], audio_files[p]  # Apply the shuffled index to both lists (Note. This randomization may be done in a number of different ways depending on the final main code.)

# run function 
sound_mix(audio_files, percentages)
