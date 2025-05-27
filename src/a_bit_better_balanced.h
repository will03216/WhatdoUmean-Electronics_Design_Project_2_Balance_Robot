#include <Arduino.h>
#include <SPI.h>
#include <TimerInterrupt_Generic.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <step.h>
#include "PIDController.h"
// === PIN DEFINITIONS ===
#define STEPPER1_DIR_PIN  16
#define STEPPER1_STEP_PIN 17
#define STEPPER2_DIR_PIN  4
#define STEPPER2_STEP_PIN 14
#define STEPPER_EN_PIN    15
#define TOGGLE_PIN        32
// === CONSTANTS ===
const int PRINT_INTERVAL = 500;
const int LOOP_INTERVAL = 10;
const int STEPPER_INTERVAL_US = 20;
const float kx = 20.0;
const float VREF = 4.096;
const float alpha = 0.98;
const float alphaEMA = 0.1;
const float wheelDiameter = 6.7;
const float wheelCircumference = PI * wheelDiameter;
const int stepsPerRevolution = 200 * 16;
const float trackWidth = 12.4;
// === OBJECTS ===
ESP32Timer ITimer(3);
Adafruit_MPU6050 mpu;
step step1(STEPPER_INTERVAL_US, STEPPER1_STEP_PIN, STEPPER1_DIR_PIN);
step step2(STEPPER_INTERVAL_US, STEPPER2_STEP_PIN, STEPPER2_DIR_PIN);
PID balancePid(9000,0,50,0); //(9000, 17, 80, 0);
PID speedPid(1.0, 0.38, 0.23, 0.0);
PID yawPid(3.0, 0.0, 0.0, 0.0);
// === GLOBAL STATE ===
float filteredAngle = 0.0, previousFilteredAngle = 0.0;
float emaSpeed1 = 0.0, emaSpeed2 = 0.0;
float speedCmPerSecond = 0.0;
float speedCmPerSecond1 = 0.0, speedCmPerSecond2 = 0.0;
float rotationalSpeedRadPerSecond = 0.0;
float turnSetpoint = 0.0;
const float deadBand = 7.0;
bool motorsEnabled = true;
// === ISR ===
bool IRAM_ATTR TimerHandler(void *timerNo) {
  static bool toggle = false;
  step1.runStepper();
  step2.runStepper();
  digitalWrite(TOGGLE_PIN, toggle);
  toggle = !toggle;
  return true;
}
// === SETUP ===
void setup() {
  Serial.begin(115200);
  pinMode(TOGGLE_PIN, OUTPUT);
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
  step1.setAccelerationRad(10.0); //adjust
  step2.setAccelerationRad(10.0);
  pinMode(STEPPER_EN_PIN, OUTPUT);
  digitalWrite(STEPPER_EN_PIN, LOW);  // Enable motors
  if (!ITimer.attachInterruptInterval(STEPPER_INTERVAL_US, TimerHandler)) {
    Serial.println("Failed to start timer!");
    while (1);
  }
  Serial.println("Stepper interrupt initialized");
  yawPid.isYawFn(true);
}
// === LOOP ===
void loop() {
  static unsigned long printTimer = 0;
  static unsigned long loopTimer = 0;
  if (millis() > loopTimer) {
    loopTimer += LOOP_INTERVAL;
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float pitch = atan2(a.acceleration.z, a.acceleration.x); //could add bias
    float gyroPitchRate = g.gyro.y;
    float dt = LOOP_INTERVAL / 1000.0;
    filteredAngle = (1 - alpha) * pitch + alpha * (gyroPitchRate * dt + previousFilteredAngle);
    previousFilteredAngle = filteredAngle;
    float rawSpeed1 = step1.getSpeed() / 2000.0;
    float rawSpeed2 = step2.getSpeed() / 2000.0;
    emaSpeed1 = alphaEMA * rawSpeed1 + (1 - alphaEMA) * emaSpeed1;
    emaSpeed2 = alphaEMA * rawSpeed2 + (1 - alphaEMA) * emaSpeed2;
    float avgSpeed = (emaSpeed1 - emaSpeed2) / 2.0;
    float distancePerStep = wheelCircumference / stepsPerRevolution;
    speedCmPerSecond = avgSpeed * distancePerStep;
    speedCmPerSecond1 = emaSpeed1 * distancePerStep;
    speedCmPerSecond2 = emaSpeed2 * distancePerStep;
    rotationalSpeedRadPerSecond = (speedCmPerSecond1 + speedCmPerSecond2) / trackWidth;
    double speedOutput = speedPid.compute(speedCmPerSecond);
    double targetPitch = speedOutput * 0.001;
    balancePid.setSetpoint(0.01); //could add bias when balancing
    double balanceOutput = balancePid.compute(filteredAngle);
    double yawCorrection = yawPid.compute(rotationalSpeedRadPerSecond);
    if (abs(balanceOutput) < deadBand) balanceOutput = 0;
    step1.setAccelerationRad(-balanceOutput - turnSetpoint + yawCorrection);
    step2.setAccelerationRad(balanceOutput - turnSetpoint + yawCorrection);
    if (balanceOutput > 0) {
      step1.setTargetSpeedRad(-20);
      step2.setTargetSpeedRad(20);
    } else {
      step1.setTargetSpeedRad(20);
      step2.setTargetSpeedRad(-20);
    }
  }
  if (millis() > printTimer) {
    printTimer += PRINT_INTERVAL;
    Serial.print("Angle: ");
    Serial.print(filteredAngle * 180 / PI);
    Serial.print(" | Speed: ");
    Serial.print(speedCmPerSecond);
    Serial.print(" | Rotation: ");
    Serial.println(rotationalSpeedRadPerSecond);
  }
}
