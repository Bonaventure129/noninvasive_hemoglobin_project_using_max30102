#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"

// --- TENSORFLOW LITE MICRO INCLUDES ---
#include <tflm_esp32.h> 
#include "model.h" // Your generated AI brain

// --- HARDWARE PINS ---
#define I2C_SDA 8   
#define I2C_SCL 9   
#define SCREEN_ADDRESS 0x3C
#define MAX_ADDRESS   0x57

Adafruit_SSD1306 display(128, 64, &Wire, -1);
MAX30105 particleSensor;

// --- TENSORFLOW VARIABLES ---
const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

// Allocate memory for the AI (4KB is plenty for our ANN)
constexpr int tensorArenaSize = 4 * 1024;
alignas(16) uint8_t tensorArena[tensorArenaSize];

// --- CALIBRATION CONSTANTS (From Python MinMaxScaler) ---
const float RED_MIN = 55428.0;
const float RED_MAX = 107439.0;
const float IR_MIN = 101850.0;
const float IR_MAX = 119420.0;

// --- STATE MACHINE ---
enum SystemState { NO_FINGER, MEASURING, RESULT_READY };
SystemState currentState = NO_FINGER;
unsigned long fingerDetectTime = 0;

uint64_t redSum = 0;
uint64_t irSum = 0;
uint32_t sampleCount = 0;
float finalHbResult = 0.0;

void setup() {
  Serial.begin(115200);
  
  // 1. Give the hardware 1 second to let the power supply fully stabilize
  delay(1000); 
  
  Wire.begin(I2C_SDA, I2C_SCL); 
  Wire.setClock(400000); 

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("OLED failed"));
    while(1);
  }

  // 2. Aggressive Sensor Initialization Loop (Fixes I2C Hang)
  int retryCount = 0;
  while (!particleSensor.begin(Wire,100000, MAX_ADDRESS)) {
    Serial.println("MAX30102 stuck. Resetting I2C Bus...");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Sensor Frozen!");
    display.println("Rebooting bus...");
    display.display();
    
    // Forcefully restart the I2C communication
    Wire.end();
    delay(500);
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(500);
    
    retryCount++;
    if (retryCount > 10) {
      display.clearDisplay();
      display.setCursor(0, 20);
      display.println("CHECK WIRING!");
      display.display();
      while(1); // Halt if completely disconnected
    }
  }

  // 3. Force a Software Reset to wipe the sensor's memory from the last session
  particleSensor.softReset();
  delay(100);

  // 4. Apply your specific dataset parameters
  particleSensor.setup(0x28, 4, 2, 200, 411, 4096);

  particleSensor.setPulseAmplitudeIR(0x24);
  
  // --- TENSORFLOW INITIALIZATION ---
  tflModel = tflite::GetModel(hb_model);
  
  // Explicitly load ONLY the mathematical operations your Neural Network uses
  static tflite::MicroMutableOpResolver<2> tflOpsResolver;
  tflOpsResolver.AddFullyConnected();
  tflOpsResolver.AddRelu();
  
  // Initialize the interpreter
  static tflite::MicroInterpreter static_interpreter(
    tflModel, tflOpsResolver, tensorArena, tensorArenaSize
  );
  tflInterpreter = &static_interpreter;

  // Allocate memory for the tensors
  tflInterpreter->AllocateTensors();

  // Get pointers for input and output
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);
  
  renderDisplay();
}

void loop() {
  uint32_t currentIr = particleSensor.getIR();
  uint32_t currentRed = particleSensor.getRed();

  if (currentIr < 50000) {
    // Finger removed
    if (currentState != NO_FINGER) {
      currentState = NO_FINGER;
      renderDisplay();
    }
  } else {
    // Finger is present
    if (currentState == NO_FINGER) {
      currentState = MEASURING;
      fingerDetectTime = millis();
      redSum = 0; irSum = 0; sampleCount = 0;
    }

    if (currentState == MEASURING) {
      redSum += currentRed;
      irSum += currentIr;
      sampleCount++;

      // Wait exactly 8 seconds to build a stable average
      if (millis() - fingerDetectTime >= 8000) {
        float avgRed = (float)redSum / sampleCount;
        float avgIr = (float)irSum / sampleCount;

        // Apply MinMaxScaler Mathematics to match the Python training data
        float scaledRed = (avgRed - RED_MIN) / (RED_MAX - RED_MIN);
        float scaledIr = (avgIr - IR_MIN) / (IR_MAX - IR_MIN);

        // Constrain between 0 and 1 to prevent AI hallucination
        if (scaledRed < 0) scaledRed = 0; if (scaledRed > 1) scaledRed = 1;
        if (scaledIr < 0) scaledIr = 0; if (scaledIr > 1) scaledIr = 1;

        // --- NEW DEBUG PRINTS ---
        Serial.println("--- AI INPUT DIAGNOSTIC ---");
        Serial.print("Raw Avg Red: "); Serial.println(avgRed);
        Serial.print("Raw Avg IR:  "); Serial.println(avgIr);
        Serial.print("Scaled Red:  "); Serial.println(scaledRed, 4);
        Serial.print("Scaled IR:   "); Serial.println(scaledIr, 4);
        Serial.println("---------------------------");

        // --- RUN NATIVE TENSORFLOW INFERENCE ---
        // 1. Feed scaled values into the AI input tensor
        tflInputTensor->data.f[0] = scaledRed;
        tflInputTensor->data.f[1] = scaledIr;
        
        // 2. Run the Neural Network
        tflInterpreter->Invoke();
        
        // 3. Read the output prediction
        finalHbResult = tflOutputTensor->data.f[0];
        
        currentState = RESULT_READY;
        renderDisplay(); 
      } else {
        // Update countdown every 500ms
        static unsigned long lastScreenUpdate = 0;
        if (millis() - lastScreenUpdate > 500) {
          lastScreenUpdate = millis();
          renderDisplay();
        }
      }
    }
  }
}

// --- UI RENDERING ---
void renderDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  if (currentState == NO_FINGER) {
    display.setTextSize(1);
    display.setCursor(5, 20);
    display.println("Waiting for Patient");
    display.setCursor(0, 40);
    display.println("Place finger firmly.");
    
  } else if (currentState == MEASURING) {
    int secondsLeft = 8 - ((millis() - fingerDetectTime) / 1000);
    
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ANALYZING BLOOD...");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    
    display.setTextSize(2);
    display.setCursor(35, 25);
    display.print(secondsLeft);
    display.print(" sec");
    
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.println("Keep finger still");
    
  } else if (currentState == RESULT_READY) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ANN PREDICTION:");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    
    display.setCursor(10, 25);
    display.setTextSize(3);
    display.print(finalHbResult, 1);
    
    display.setTextSize(1);
    display.setCursor(85, 40);
    display.print("g/dL");
  }
  
  display.display();
}