#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MAX30102 Sensor Object
MAX30105 particleSensor;

// Data Buffers for Signal Processing
#define BUFFER_SIZE 100 
uint32_t redBuffer[BUFFER_SIZE];
uint32_t irBuffer[BUFFER_SIZE];
int bufferIndex = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(); 
  
  // 1. Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();
  delay(1000);

  // 2. Initialize MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sensor Error!");
    display.display();
    while (1);
  }

  // 3. Configure Sensor for High Resolution
  particleSensor.setup(0x1F, 4, 2, 400, 411, 4096);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Ready. Place Finger.");
  display.display();
}

// Function to calculate Hb from the optical buffer
float calculateHemoglobin() {
  uint32_t redMax = 0, redMin = 0xFFFFFFFF;
  uint32_t irMax = 0, irMin = 0xFFFFFFFF;
  uint64_t redSum = 0, irSum = 0;

  // Extract AC (Peak-to-Peak) and DC (Average)
  for(int i = 0; i < BUFFER_SIZE; i++) {
    if(redBuffer[i] > redMax) redMax = redBuffer[i];
    if(redBuffer[i] < redMin) redMin = redBuffer[i];
    redSum += redBuffer[i];

    if(irBuffer[i] > irMax) irMax = irBuffer[i];
    if(irBuffer[i] < irMin) irMin = irBuffer[i];
    irSum += irBuffer[i];
  }

  float dcRed = (float)redSum / BUFFER_SIZE;
  float acRed = (float)(redMax - redMin);
  
  float dcIr = (float)irSum / BUFFER_SIZE;
  float acIr = (float)(irMax - irMin);

  // Basic sanity check to ensure finger is actually on the sensor
  // Return -1.0 to tell the main loop that there is no finger
  if (dcRed < 50000) {
    return -1.0;
  }

  // Prevent division by zero if sensor is empty but somehow passed the above check
  if (dcRed == 0 || dcIr == 0 || acIr == 0) return -1.0;

  // Calculate Ratio of Ratios (R)
  float R = (acRed / dcRed) / (acIr / dcIr);

  // Apply your Python Calibration Equation
  float calculatedHb = (47.50 * R) - 5.31;
  
  return calculatedHb;
}

void loop() {
  particleSensor.check(); 

  while (particleSensor.available()) {
    // Fill the buffers
    redBuffer[bufferIndex] = particleSensor.getRed();
    irBuffer[bufferIndex] = particleSensor.getIR();
    bufferIndex++;
    
    // When buffer is full, calculate and display Hb
    if (bufferIndex >= BUFFER_SIZE) {
      float hb = calculateHemoglobin();
      
      // Update OLED
      display.clearDisplay();
      
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("Non-Invasive Hb");
      display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
      
      display.setTextSize(2);
      display.setCursor(0, 25);
      
      // Check if calculateHemoglobin() returned the "No Finger" error code
      if (hb < 0.0) {
        display.print("No Finger");
      } else {
        display.print(hb, 1); // Print with 1 decimal place
        display.setTextSize(1);
        display.print(" g/dl");
      }
      
      display.display();
      
      // Reset buffer to start collecting the next batch
      bufferIndex = 0; 
    }

    particleSensor.nextSample(); 
  }
}