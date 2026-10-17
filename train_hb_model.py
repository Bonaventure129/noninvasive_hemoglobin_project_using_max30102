import pandas as pd
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import MinMaxScaler
import os

# 1. Load the Dataset
csv_filename = "Final Dataset Hb PPG final year project.csv"
print(f"Loading data from {csv_filename}...")
df = pd.read_csv(csv_filename)

# Ensure columns match exactly (Adjust these string names if your CSV header is slightly different)
X = df[['Red', 'Infra Red']].values
y = df['Hemoglobin'].values

# 2. Preprocess the Data (Crucial for Optical Sensors)
# Neural networks fail if you feed them massive raw numbers (like 100,000). 
# We must scale the Red/IR inputs to a range between 0 and 1.
scaler = MinMaxScaler()
X_scaled = scaler.fit_transform(X)

# Print the scaling constants (You will need to hardcode these into your C++ firmware later!)
print("\n--- C++ CALIBRATION CONSTANTS ---")
print(f"float RED_MIN = {scaler.data_min_[0]};")
print(f"float RED_MAX = {scaler.data_max_[0]};")
print(f"float IR_MIN = {scaler.data_min_[1]};")
print(f"float IR_MAX = {scaler.data_max_[1]};")
print("---------------------------------\n")

# Split into Training (80%) and Testing (20%) sets
X_train, X_test, y_train, y_test = train_test_split(X_scaled, y, test_size=0.2, random_state=42)

# 3. Build the Artificial Neural Network (ANN)
model = tf.keras.Sequential([
    tf.keras.layers.Dense(16, activation='relu', input_shape=(2,)),
    tf.keras.layers.Dense(8, activation='relu'),
    tf.keras.layers.Dense(1, activation='linear') # Linear output for regression (g/dL)
])

# Compile with Mean Squared Error (Standard for regression)
model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=0.005), 
              loss='mse', 
              metrics=['mae'])

# 4. Train the Model
print("Training the Neural Network...")
history = model.fit(X_train, y_train, epochs=100, batch_size=16, validation_split=0.2, verbose=1)

# Evaluate accuracy
test_loss, test_mae = model.evaluate(X_test, y_test, verbose=0)
print(f"\nFinal Test Mean Absolute Error: {test_mae:.2f} g/dL")

# 5. Convert to TensorFlow Lite
print("\nConverting model to TensorFlow Lite format...")
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

with open("hb_model.tflite", "wb") as f:
    f.write(tflite_model)

# 6. Convert TFLite to C-Byte Array for the ESP32-C3
print("Generating C++ header file (model.h)...")
def convert_to_c_array(tflite_model_bytes, output_filename):
    c_str = ""
    c_str += "#ifndef HB_MODEL_H\n#define HB_MODEL_H\n\n"
    c_str += f"const unsigned int hb_model_len = {len(tflite_model_bytes)};\n"
    c_str += "const unsigned char hb_model[] = {\n"
    
    # Format bytes as hex
    hex_array = [f"0x{b:02x}" for b in tflite_model_bytes]
    
    # Add line breaks for readability in C++
    for i in range(0, len(hex_array), 12):
        c_str += "    " + ", ".join(hex_array[i:i+12]) + ",\n"
        
    c_str += "};\n\n#endif // HB_MODEL_H\n"
    
    with open(output_filename, "w") as f:
        f.write(c_str)

convert_to_c_array(tflite_model, "model.h")
print("\nSuccess! 'model.h' has been generated. Move this file to your Arduino project folder.")
