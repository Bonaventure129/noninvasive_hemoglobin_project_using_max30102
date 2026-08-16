#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h" // Works for MAX30102

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C // Standard I2C address for 0.96" OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MAX30102 Sensor Object
MAX30105 particleSensor;

void setup() {
  Serial.begin(115200);
  Wire.begin(); // Uses default SDA and SCL pins on ESP32-C3
  
  // 1. Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
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
    Serial.println("MAX30102 was not found. Please check wiring/power.");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sensor Error!");
    display.display();
    while (1);
  }

  // 3. Configure Sensor for High Resolution
  byte ledBrightness = 0x1F; // Options: 0=Off to 255=50mA
  byte sampleAverage = 4;    // Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2;          // Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 400;      // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411;      // Options: 69, 118, 215, 411
  int adcRange = 4096;       // Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Setup Complete.");
  display.display();
  delay(1000);
}

void loop() {
  // Check if new data is available
  particleSensor.check(); 

  while (particleSensor.available()) {
    // Read raw Red and IR values
    uint32_t irValue = particleSensor.getIR();
    uint32_t redValue = particleSensor.getRed();
    
    // Print to Serial for data logging (CSV format: Red, IR)
    Serial.print(redValue);
    Serial.print(",");
    Serial.println(irValue);

    // Update OLED Display
    display.clearDisplay();
    
    // Title
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Raw Optical Data");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    // Red Value
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.print("RED:");
    display.setTextSize(2);
    display.setCursor(30, 15);
    display.print(redValue);
    
    // IR Value
    display.setTextSize(1);
    display.setCursor(0, 45);
    display.print("IR :");
    display.setTextSize(2);
    display.setCursor(30, 40);
    display.print(irValue);
    
    display.display();

    // Advance to next sample in the FIFO buffer
    particleSensor.nextSample(); 
  }
}
