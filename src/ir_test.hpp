#include <Arduino.h>
#include <SPI.h>
#include <TimerInterrupt_Generic.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <step.h>
#include "PIDController.h" 

void setup() {
  Serial.begin(115200);
  pinMode(34, INPUT);
  pinMode(35, INPUT);
}

void loop() {
  Serial.print("GPIO34: ");
  Serial.print(digitalRead(34));
  Serial.print(" | GPIO35: ");
  Serial.println(digitalRead(35));
  delay(500);
}
