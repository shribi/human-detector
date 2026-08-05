import numpy as np
import matplotlib.pyplot as plt

# 1. Load your raw 16-bit data (Change 'filename.pcm' to your file)
# If you have an in-memory array, pass it directly to np.array(your_list, dtype=np.int16)
pcm_data = np.fromfile("./recordings/rawFile.raw", dtype=np.int16)

# 2. Handle channels (Optional: Extract channel 1 if data is stereo interleaved)
# pcm_data = pcm_data[0::2] 

# 3. Create time domain axis based on your sampling rate (e.g., 16000 Hz)
sample_rate = 16000
time_axis = np.arange(len(pcm_data)) / sample_rate

# 4. Generate the plot
plt.figure(figsize=(10, 4))
plt.plot(time_axis, pcm_data, color='blue', linewidth=0.5)

# 5. Graph cleanups
plt.title("16-bit PCM Audio Waveform")
plt.xlabel("Time (seconds)")
plt.ylabel("Amplitude (-32768 to 32767)")
plt.ylim(-32768, 32767)
plt.grid(True)
plt.savefig("waveform_output.png", dpi=300, bbox_inches='tight')
