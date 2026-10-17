import os
import re
import numpy as np
import pandas as pd
from scipy.signal import butter, filtfilt, find_peaks
from scipy.stats import linregress
import matplotlib.pyplot as plt

# 1. Clinical Data Mapping (Hb g/dl)
# Maps the clinical values from your physical notebook to the file names.
clinical_hb = {
    'onyebuchi_victor': 12.0,
    'abarikwu_kelechi': 13.3,
    'goodness_akunye': 13.3,
    'amuzie_chibuike': 13.3,
    'orizu_victor': 15.0,
    'ama_eric': 16.3 
}

# 2. Signal Processing Function
def extract_ac_dc(signal, fs=40):
    """
    Extracts the AC (pulsatile) and DC (baseline) components of a raw optical signal.
    fs is the approximate sampling frequency in Hz.
    """
    # Calculate DC component (mean of the signal)
    dc = np.mean(signal)
    
    # Calculate AC component by isolating the heartbeat (detrending & filtering)
    detrended = signal - dc
    
    # Bandpass filter (0.5 Hz to 3 Hz to capture normal human heart rates: 30-180 bpm)
    nyq = 0.5 * fs
    low = 0.5 / nyq
    high = 3.0 / nyq
    
    # Apply filter if the signal is long enough
    if len(detrended) > 15:
        b, a = butter(2, [low, high], btype='band')
        ac_signal = filtfilt(b, a, detrended)
    else:
        ac_signal = detrended
    
    # Find peaks and troughs to calculate Peak-to-Peak for the AC component
    peaks, _ = find_peaks(ac_signal, distance=fs//2)
    troughs, _ = find_peaks(-ac_signal, distance=fs//2)
    
    if len(peaks) > 0 and len(troughs) > 0:
        ac = np.mean(ac_signal[peaks]) - np.mean(ac_signal[troughs])
    else:
        # Fallback to RMS scaled to peak-to-peak equivalent if peaks aren't clear
        ac = np.std(ac_signal) * 2 * np.sqrt(2)
        
    return ac, dc

# 3. Read and Process CSV Files
results = []

# Regex to extract the numbers from the Serial Monitor format: "18:48:45.639 -> 56159,104504"
line_regex = re.compile(r'.*->\s*(\d+),(\d+)')

# Look for all CSV files in the current working directory
files = [f for f in os.listdir('.') if f.endswith('.csv')]

for file in files:
    # Normalize filename to match dictionary keys
    key = file.lower().replace('.csv', '')
    if key not in clinical_hb:
        print(f"Skipping {file}, no matching clinical data found.")
        continue
        
    red_vals = []
    ir_vals = []
    
    # Parse the data
    with open(file, 'r') as f:
        for line in f:
            match = line_regex.match(line.strip())
            if match:
                red_vals.append(float(match.group(1)))
                ir_vals.append(float(match.group(2)))
                
    if not red_vals:
        continue
        
    # Convert lists to numpy arrays for mathematical processing
    red = np.array(red_vals)
    ir = np.array(ir_vals)
    
    # Process signals (assuming approx 40Hz sampling rate from ESP32)
    ac_red, dc_red = extract_ac_dc(red, fs=40)
    ac_ir, dc_ir = extract_ac_dc(ir, fs=40)
    
    # Prevent division by zero
    if dc_red == 0 or dc_ir == 0 or ac_ir == 0:
        continue
        
    # Calculate Ratio of Ratios (R)
    ratio_of_ratios = (ac_red / dc_red) / (ac_ir / dc_ir)
    
    results.append({
        'Patient': key,
        'Hb_Level': clinical_hb[key],
        'R_value': ratio_of_ratios
    })

# 4. Generate Calibration Model
df = pd.DataFrame(results)

if df.empty:
    print("No valid data processed. Please check file formatting.")
else:
    # Average duplicates (e.g., if there are multiple CSV files for the same person)
    df = df.groupby('Patient').mean(numeric_only=True).reset_index()

    # Drop the known motion artifact outlier to clean the regression line
    df_clean = df[df['Patient'] != 'goodness_akunye']

    print("\n--- Calculated Patient Data ---")
    print(df_clean[['Patient', 'Hb_Level', 'R_value']])

    # Perform Linear Regression to find the formula (Hb = m * R + c)
    if len(df_clean) > 1:
        slope, intercept, r_val, p_val, std_err = linregress(df_clean['R_value'], df_clean['Hb_Level'])
        
        print(f"\n--- Final Embedded Calibration Equation ---")
        print(f"Hb = {slope:.4f} * R + ({intercept:.4f})")
        print(f"R-squared: {r_val**2:.4f}")

        # 5. Plot and Save the Curve
        plt.figure(figsize=(8, 5))
        plt.scatter(df_clean['R_value'], df_clean['Hb_Level'], color='blue', label='Cleaned Data Points')
        
        # Plot regression line
        r_range = np.linspace(df_clean['R_value'].min()*0.9, df_clean['R_value'].max()*1.1, 100)
        plt.plot(r_range, intercept + slope * r_range, color='red', label=f'Fit: Hb = {slope:.2f}*R {intercept:+.2f}')
        
        # Label the points
        for i, row in df_clean.iterrows():
            plt.annotate(row['Patient'], (row['R_value'], row['Hb_Level']), textcoords="offset points", xytext=(0,10), ha='center')
            
        plt.xlabel('Calculated Ratio of Ratios (R)')
        plt.ylabel('Clinical Hb Level (g/dl)')
        plt.title('Hemoglobin Calibration Curve')
        plt.legend()
        plt.grid(True)
        plt.savefig('calibration_curve.png')
        print("\n[*] Plot successfully generated and saved as 'calibration_curve.png'")
