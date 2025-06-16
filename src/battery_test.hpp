#include <Arduino.h>
#include <SPI.h>

const float START_CAPACITY_MAH = 2000.0;
float remaining_capacity_mAh = START_CAPACITY_MAH;

const int SAMPLE_INTERVAL_MS = 1;
const int AVERAGE_WINDOW_MS = 20;
//float current_samples[AVERAGE_WINDOW_MS];
int sample_index = 0;

unsigned long last_sample_time = 0;
unsigned long last_update_time = 0;

const float resistance = 0.1;

//ADC pins
const int ADC_CS_PIN        = 5;
const int ADC_SCK_PIN       = 18;
const int ADC_MISO_PIN      = 19;
const int ADC_MOSI_PIN      = 23;

void setup() {
    Serial.begin(115200);
    //analogReadResolution(12);  // Optional: defaults to 12 bits on ESP32

    pinMode(ADC_CS_PIN, OUTPUT);
    digitalWrite(ADC_CS_PIN, HIGH);
    SPI.begin(ADC_SCK_PIN, ADC_MISO_PIN, ADC_MOSI_PIN, ADC_CS_PIN);
}

uint16_t readADC(uint8_t channel) {
  uint8_t tx0 = 0x06 | (channel >> 2);  // Command Byte 0 = Start bit + single-ended mode + MSB of channel
  uint8_t tx1 = (channel & 0x03) << 6;  // Command Byte 1 = Remaining 2 bits of channel

  digitalWrite(ADC_CS_PIN, LOW); 

  SPI.transfer(tx0);                    // Send Command Byte 0
  uint8_t rx0 = SPI.transfer(tx1);      // Send Command Byte 1 and receive high byte of result
  uint8_t rx1 = SPI.transfer(0x00);     // Send dummy byte and receive low byte of result

  digitalWrite(ADC_CS_PIN, HIGH); 

  uint16_t result = ((rx0 & 0x0F) << 8) | rx1; // Combine high and low byte into 12-bit result
  return result;
}

float read_current_sensor() {
    int raw = readADC(0); 
    // Serial.print(readADC(0));
    // Serial.println();
    float voltage = raw * (4.096 / 4095.0);  // Assuming 4.096V ADC, 12-bit (ESP32)
    
    float current_mA = (voltage / resistance) * 1000.0;  // R = 0.1Ω → convert to mA
    return current_mA;
}







void loop() {
    unsigned long now = millis();

    // Step 1
    // if (now - last_sample_time >= SAMPLE_INTERVAL_MS) {
    //     float current_mA = read_current_sensor();  // Replace with your actual current reading code
    //     Serial.print("current reading: (mA): ");
    //     Serial.println(current_mA);
    //     current_samples[sample_index] = current_mA;
    //     sample_index = (sample_index + 1) % AVERAGE_WINDOW_MS;
    //     last_sample_time = now;
    // }

  

    // Step 2
    if (now - last_update_time >= AVERAGE_WINDOW_MS) {
        float avg_current_mA = 0;
        for (int i = 0; i < AVERAGE_WINDOW_MS; i++) {
            avg_current_mA += read_current_sensor();
        }
        Serial.print("current reading: (mA): ");
    Serial.println(read_current_sensor());
        avg_current_mA /= AVERAGE_WINDOW_MS;

        // Step 3
        float delta_charge_mAh = avg_current_mA * (AVERAGE_WINDOW_MS / 3600000.0);  // ms → h

        // Step 4
        remaining_capacity_mAh -= delta_charge_mAh;
        if (remaining_capacity_mAh < 0) remaining_capacity_mAh = 0;

        // Step 5
        float battery_percentage = (remaining_capacity_mAh / START_CAPACITY_MAH) * 100;
        Serial.print("Battery: ");
        Serial.print(battery_percentage);
        Serial.println("%");

        last_update_time = now;
    }
}