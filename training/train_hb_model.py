import pandas as pd
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import MinMaxScaler
import os
import re

# 1. Map the filenames to the Ground Truth Hemoglobin values
ground_truth_map = {
    "onyebuchi_victor.csv": 12.0,
    "abarikwu_kelechi.csv": 13.3, 
    "amuzie_chibuike.csv": 13.3,
    "orizu_victor.csv": 15.0,
    "ama_eric.csv": 16.3
}

# 2. Parse the Raw Arduino Serial Logs
path = os.getcwd()
parsed_data = []

print("--- Data Extraction ---")
for filename, target_hb in ground_truth_map.items():
    filepath = os.path.join(path, filename)
    if not os.path.exists(filepath):
        print(f"Warning: Could not find {filename}")
        continue
        
    print(f"Parsing {filename} (Target Hb: {target_hb} g/dL)...")
    with open(filepath, 'r') as file:
        for line in file:
            # Look for the pattern: "-> [number], [number]"
            match = re.search(r'->\s*(\d+)\s*,\s*(\d+)', line)
            if match:
                red_val = int(match.group(1))
                ir_val = int(match.group(2))
                parsed_data.append([red_val, ir_val, target_hb])

# Convert to a clean Pandas DataFrame
df = pd.DataFrame(parsed_data, columns=['Red', 'Infra Red', 'Hemoglobin'])

# Save the clean dataset for your records
clean_filename = "Clean_Hemoglobin_Dataset.csv"
df.to_csv(clean_filename, index=False)
print(f"\nSuccessfully extracted {len(df)} data points.")
print(f"Saved clean dataset to '{clean_filename}'\n")

# 3. Preprocess the Data 
X = df[['Red', 'Infra Red']].values
y = df['Hemoglobin'].values

scaler = MinMaxScaler()
X_scaled = scaler.fit_transform(X)

print("--- C++ CALIBRATION CONSTANTS ---")
print(f"float RED_MIN = {scaler.data_min_[0]};")
print(f"float RED_MAX = {scaler.data_max_[0]};")
print(f"float IR_MIN = {scaler.data_min_[1]};")
print(f"float IR_MAX = {scaler.data_max_[1]};")
print("---------------------------------\n")

X_train, X_test, y_train, y_test = train_test_split(X_scaled, y, test_size=0.2, random_state=42)

# 4. Build and Train the Neural Network
model = tf.keras.Sequential([
    tf.keras.layers.Dense(16, activation='relu', input_shape=(2,)),
    tf.keras.layers.Dense(8, activation='relu'),
    tf.keras.layers.Dense(1, activation='linear') 
])

model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=0.005), 
              loss='mse', 
              metrics=['mae'])

print("Training the Neural Network...")
model.fit(X_train, y_train, epochs=100, batch_size=16, validation_split=0.2, verbose=1)

test_loss, test_mae = model.evaluate(X_test, y_test, verbose=0)
print(f"\nFinal Test Mean Absolute Error: {test_mae:.2f} g/dL")

# 5. Convert to TensorFlow Lite and C++ Array
print("\nConverting model to C++ header...")
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

c_str = "#ifndef HB_MODEL_H\n#define HB_MODEL_H\n\n"
c_str += f"const unsigned int hb_model_len = {len(tflite_model)};\n"
c_str += "const unsigned char hb_model[] = {\n"
hex_array = [f"0x{b:02x}" for b in tflite_model]
for i in range(0, len(hex_array), 12):
    c_str += "    " + ", ".join(hex_array[i:i+12]) + ",\n"
c_str += "};\n\n#endif // HB_MODEL_H\n"

with open("model.h", "w") as f:
    f.write(c_str)

print("Success! 'model.h' and 'Clean_Hemoglobin_Dataset.csv' have been generated.")
