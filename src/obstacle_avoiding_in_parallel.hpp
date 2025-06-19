
//// try precise turning when maze walking
//// move all constants outside of loop()

//// Battery usage

//// use position loop when balancing
//// yaw correction: +_0.1
//// try turn a bit slower
// fast during straight for line tracking
// pid for line tracking
// faster (max=50) w and s (but maybe smaller p)
// avoid obstacle simultaneously
// use f and h for t and test
// change speed = 10

#include <Arduino.h>
#include <SPI.h>
#include <TimerInterrupt_Generic.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <step.h>
#include "PIDController.h"
#include <ArduinoJson.h>

// The Stepper pins
#define STEPPER1_DIR_PIN 16
#define STEPPER1_STEP_PIN 17
#define STEPPER2_DIR_PIN 4
#define STEPPER2_STEP_PIN 14
#define STEPPER_EN_PIN 15
#define TOGGLE_PIN 32

// ADC pins
const int ADC_CS_PIN = 5;
const int ADC_SCK_PIN = 18;
const int ADC_MISO_PIN = 19;
const int ADC_MOSI_PIN = 23;

// CONSTANTS
const int PRINT_INTERVAL = 1000;
const int LOOP_INTERVAL = 10;
const int STEPPER_INTERVAL_US = 20;
// const float kx = 20.0;
const float VREF = 4.096;

const float alpha = 0.98;
const float alphaEMA = 0.1;

float filteredAngle = 0.0, previousFilteredAngle = 0.0;
float emaSpeed1 = 0.0, emaSpeed2 = 0.0;
float speedCmPerSecond = 0.0;
float speedCmPerSecond1 = 0.0, speedCmPerSecond2 = 0.0;
float rotationalSpeedRadPerSecond = 0.0;
const float wheelDiameter = 6.7;
const float wheelCircumference = PI * wheelDiameter;
const int stepsPerRevolution = 200 * 16;
float distancePerStep = wheelCircumference / stepsPerRevolution;
const float trackWidth = 12.4;

// precise turning
bool preciseTurning = false;
float targetYawAngle = 0.0;
float yawAngle = 0.0;  // in radians
float gyroXBias = 0.0; // adjust!!!

// precise moving
float cumulativeDistance = 0.0; // cm
float targetDistance = 0.0;     // cm
bool movingToTarget = false;
float vDesired = 0.0; // adjust to close to speedCmPerSecond
// const float dbcompensation = 70;  // Minimum speed in rad/s to prevent stalling

// const float deadBand = 1;
bool motorsEnabled = true;

float turnVal = 0.0;

float yawCorrection = 0;

// line tracking
bool line_tracking_enable = 0;

const int lineSensorLeftPin = 35;
const int lineSensorRightPin = 34;

// obstacle avoiding
bool obstacle_avoiding_enable = 0;

const int TRIG_PIN = 33;
const int ECHO_PIN = 25;
const float obstacle_distance_threshold = 20; // cm
bool obstacleDetected = false;

unsigned long turnStartTime = 0;
const unsigned long turn_duration = 400;

volatile unsigned long echoStart = 0;
volatile unsigned long echoEnd = 0;
volatile bool echoReceived = false;
static unsigned long lastTriggerTime = 0;
static float latestDistance = 0;

// battery monitoring
const float START_CAPACITY_MAH = 2000.0 * 2.0;
float remaining_capacity_mAh = START_CAPACITY_MAH;
// const int SAMPLE_INTERVAL_MS = 10;
const int AVERAGE_WINDOW_MS = 1000;
// float current_samples[AVERAGE_WINDOW_MS];
int sample_index = 0;

unsigned long last_sample_time = 0;
unsigned long last_update_time = 0;

const float logicSensingResistance = 0.01;
const float motorSensingResistance = 0.1;
// static unsigned long startTime = millis(); // for testing

float logic_delta_charge_mAh = 0.0;
float motor_delta_charge_mAh = 0.0;
float logic_avg_current_mA = 0.0;
float motor_avg_current_mA = 0.0;
float battery_percentage = 0.0;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 500;
// Global objects
ESP32Timer ITimer(3);
Adafruit_MPU6050 mpu; // Default pins for I2C are SCL: IO22, SDA: IO21

step step1(STEPPER_INTERVAL_US, STEPPER1_STEP_PIN, STEPPER1_DIR_PIN);
step step2(STEPPER_INTERVAL_US, STEPPER2_STEP_PIN, STEPPER2_DIR_PIN);

// speedpid could be larger!!!!
PID balancePid(1500.0, 0.0, 130.0, 0.0); // 1500,0,130 //900,0,110 // p enough or larger //500,0,130 turns maybe better //1200,0,130 //1000,0,130 //900,0,110 // a=30 //(remember the +-120 limits in PIDController.h) p should be large for fast reaction, i is replaced by bias, and a not too large d reduces oscillation //(900,0,50,0) //(9000, 5, 30, 0) //(9000, 17, 80, 0) //adjust
PID speedPid(4.5, 0.0, 0.5, 0.0);        //4.5,0,0.5 // 4.5 //3.5 falls once //(not a) 90  //2.2-2.6 // smaller p i d values //(3.6, 0, 0.9, 0) // (1.0, 0.38, 0.23, 0)
PID positionPid(3.5, 0.0, 0.2, 0.0);   //2.5,0.035,0 // 1,0.5,0.7 //6 //10,0,3 is wrong  // must have lowest bandwidth
PID yawPid(4.0, 0.0, 0.0, 0.0);          // 4,0,0 // a=0.03
// To improve turning exactly 90 degrees, you can add a dedicated turning PID controller that drives the robot's turnVal based on the error between the current yaw angle and a target yaw angle (e.g., ±π/2 radians = ±90°).
PID turnPid(52, 0.0, 37.0, 0.0); //52,0,32 //12.5,0,12//left values are without ending condition //1,0,2 // 1,0.13,4 //1,0,4// for mpu //1,0,2 works(h more accurate than f) //a=3 // (12.0, 0.0, 1.0, 0.0) // (14,0,0.2,0) //20,0,0.2 //30,0,20

// === ISR ===
bool IRAM_ATTR TimerHandler(void *timerNo)
{
    static bool toggle = false;
    step1.runStepper();
    step2.runStepper();
    digitalWrite(TOGGLE_PIN, toggle);
    toggle = !toggle;
    return true;
}

void IRAM_ATTR echoISR()
{
    if (digitalRead(ECHO_PIN) == HIGH)
    {
        echoStart = micros(); // Rising edge
    }
    else
    {
        echoEnd = micros();  // Falling edge
        echoReceived = true; // Tell loop to process
    }
}

uint16_t readADC(uint8_t channel)
{
    uint8_t tx0 = 0x06 | (channel >> 2); // Command Byte 0 = Start bit + single-ended mode + MSB of channel
    uint8_t tx1 = (channel & 0x03) << 6; // Command Byte 1 = Remaining 2 bits of channel

    digitalWrite(ADC_CS_PIN, LOW);

    SPI.transfer(tx0);                // Send Command Byte 0
    uint8_t rx0 = SPI.transfer(tx1);  // Send Command Byte 1 and receive high byte of result
    uint8_t rx1 = SPI.transfer(0x00); // Send dummy byte and receive low byte of result

    digitalWrite(ADC_CS_PIN, HIGH);

    uint16_t result = ((rx0 & 0x0F) << 8) | rx1; // Combine high and low byte into 12-bit result
    return result;
}

float read_logic_current_sensor()
{
    int raw = readADC(0);

    float voltage = raw * (4.096 / 4095.0);
    float current_mA = (voltage / logicSensingResistance / 100) * 1000.0;
    return current_mA;
}

float read_motor_current_sensor()
{
    int raw = readADC(1);

    float voltage = raw * (4.096 / 4095.0);
    float current_mA = (voltage / motorSensingResistance * 2 / 50) * 1000.0; // two 10kΩ resistors as potential divider to decraese the output voltage to within 4.096V

    return current_mA;
}

// BAD FOR TIMING!!!
// float getUltrasonicDistanceCM()
// {
//   digitalWrite(TRIG_PIN, LOW);
//   delayMicroseconds(2);
//   digitalWrite(TRIG_PIN, HIGH);
//   delayMicroseconds(10);
//   digitalWrite(TRIG_PIN, LOW);
//   long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout: 30ms = 5m
//   float distance = duration * 0.034 / 2;          // cm
//   return distance;
// }

void setup()
{

    Serial.begin(115200);
    pinMode(TOGGLE_PIN, OUTPUT);

    if (!mpu.begin())
    {
        // Serial.println("Failed to find MPU6050 chip");
        while (1)
        {
            delay(10);
        };
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

    if (!ITimer.attachInterruptInterval(STEPPER_INTERVAL_US, TimerHandler))
    {
        // Serial.println("Failed to start stepper interrupt");
        while (1)
            delay(10);
    }
    // Serial.println("Initialised Interrupt for Stepper");

    step1.setAccelerationRad(10.0);
    step2.setAccelerationRad(10.0);

    pinMode(STEPPER_EN_PIN, OUTPUT);
    digitalWrite(STEPPER_EN_PIN, false); // Enable motors

    // Set up ADC and SPI
    pinMode(ADC_CS_PIN, OUTPUT);
    digitalWrite(ADC_CS_PIN, HIGH);
    SPI.begin(ADC_SCK_PIN, ADC_MISO_PIN, ADC_MOSI_PIN, ADC_CS_PIN);

    yawPid.isYawFn(true);

    // line tracking
    pinMode(lineSensorLeftPin, INPUT);
    pinMode(lineSensorRightPin, INPUT);

    // ultrasound
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoISR, CHANGE);

    // gyro.x bias
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    for (int i = 0; i < 500; i++)
    {

        gyroXBias += g.gyro.x;
        delay(2);
    }
    gyroXBias /= 500.0;

    // getPosition() bias

    // for (int i = 0; i < 500; i++)
    // {

    //     cumulativeDistance += (step1.getPosition() - step2.getPosition()) * distancePerStep;
    //     delay(2);
    // }
    // cumulativeDistance /= 500.0;
}

// === LOOP ===
void loop()
{
    static unsigned long printTimer = 0;
    static unsigned long loopTimer = 0;

    unsigned long present = millis();
    if (present - lastSendTime >= sendInterval)
    {
        lastSendTime = present;

        StaticJsonDocument<200> doc;
        JsonObject tele = doc.createNestedObject("tele");
        tele["bat_v"] = battery_percentage;
        tele["speed"] = 10;

        // 序列化并写入串口
        serializeJson(doc, Serial);
        Serial.println(); // 发送换行符作为数据分隔（树莓派方便识别）
    }

    if (millis() > loopTimer)
    {

        // launch
        // step1.setTargetSpeedRad (-30);
        // step2.setTargetSpeedRad(30);

        loopTimer += LOOP_INTERVAL;

        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        float pitch = atan2(a.acceleration.z, a.acceleration.x)-0.04;// + 0.005; // bias
        // Serial.print(pitch);
        // Serial.println();
        // delay(300);
        float gyroPitchRate = g.gyro.y;
        // Serial.print("g.gyro.y: ");
        // Serial.println(g.gyro.y);

        float dt = LOOP_INTERVAL / 1000.0; // change from ms to s

        filteredAngle = (1 - alpha) * pitch + alpha * (gyroPitchRate * dt + previousFilteredAngle);
        previousFilteredAngle = filteredAngle;
        // Serial.print(filteredAngle);
        // Serial.println();

        float rawSpeed1 = step1.getSpeed() / 2000.0;
        // Serial.print("raw speed: ");
        // Serial.println(rawSpeed1);
        float rawSpeed2 = step2.getSpeed() / 2000.0;
        // Serial.println(rawSpeed1);

        // Serial.print((rawSpeed1-rawSpeed2)/2); //steps per sec
        // Serial.print(" ");

        emaSpeed1 = alphaEMA * rawSpeed1 + (1 - alphaEMA) * emaSpeed1;
        emaSpeed2 = alphaEMA * rawSpeed2 + (1 - alphaEMA) * emaSpeed2;

        float avgSpeed = (emaSpeed1 - emaSpeed2) / 2.0;
        // Serial.print(avgSpeed);
        // Serial.print(" ");

        // float distancePerStep = wheelCircumference / stepsPerRevolution;
        speedCmPerSecond = avgSpeed * distancePerStep;
        // Serial.print(speedCmPerSecond);
        // Serial.print("    ");

        // Serial.print((step1.getSpeedRad()) * wheelCircumference / 2); //Do not call from ISR!!!
        // Serial.print("    ");

        cumulativeDistance += speedCmPerSecond * dt;
        // Serial.print(cumulativeDistance);
        // Serial.println();

        // getPosition() related
        //  cumulativeDistance = (step1.getPosition() - step2.getPosition()) * distancePerStep; // cm = (cm/s * s)
        //  Serial.print((step1.getPosition() - step2.getPosition()) * distancePerStep);
        //  Serial.print(" ");

        // check chatgpt
        // needs test
        speedCmPerSecond1 = emaSpeed1 * distancePerStep;
        speedCmPerSecond2 = emaSpeed2 * distancePerStep;
        rotationalSpeedRadPerSecond = (speedCmPerSecond1 + speedCmPerSecond2) / trackWidth;

        // === Outer loop: velocity PID ===

        // if (millis() - startTime > 1000) {
        // speedPid.setSetpoint(-0.02);  // Stop after 1 second
        //}

        // if (millis() - startTime > 2000) {  // 1000 ms = 1 second
        // balancePid.setSetpoint(-0.02);  // Stop movement(adjust angle)
        //} // for testing

        //===cmd control===
        if (Serial.available() > 0)
        {
            char cmd = Serial.read();
            if (cmd == 'p')
            {
                vDesired = 0; // stop speed command
                turnVal = 0;  // stop any turning
                yawAngle = 0.0;
                targetYawAngle = 0;
                preciseTurning = false;
                movingToTarget = false;
                // Serial.println("STOP command received");
            }
            if (cmd == 'w') // 30cm/s with line
            {
                vDesired = 25; //~33.5cm/s // 30*wheelDiameter/2
                turnVal = 0;
                yawAngle = 0.0;
                targetYawAngle = 0;
                preciseTurning = false;
                movingToTarget = false;
                // Serial.println("FORWARD command received");
            }
            if (cmd == 's') // 20cm/s with line
            {
                vDesired = -25; //-30*wheelDiameter/2
                turnVal = 0;
                yawAngle = 0.0;
                targetYawAngle = 0;
                preciseTurning = false;
                movingToTarget = false;
                // Serial.println("BACKWARD command received");
            }
            if (cmd == 'a')
            {
                vDesired = 0; // 30*wheelDiameter/2
                turnVal = 3.0;
                yawAngle = 0.0;
                targetYawAngle = 0;
                preciseTurning = false;
                movingToTarget = false;
                // Serial.println("LEFT command received");
            }
            if (cmd == 'd')
            {
                vDesired = 0; //-30*wheelDiameter/2
                turnVal = -3.0;
                yawAngle = 0.0;
                targetYawAngle = 0;
                preciseTurning = false;
                movingToTarget = false;
                // Serial.println("RIGHT command received");
            }
            if (cmd == 'f')
            {
                vDesired = 0;
                turnVal = 0.7; // adjust!!!
                yawAngle = 0.0;
                targetYawAngle = PI / 2; // 90° left
                preciseTurning = true;

                movingToTarget = false;
                // cumulativeDistance = 0.0;
                // targetDistance = 10.0;
                // Serial.println("LEFT 90° TURN initiated"); // Serial.println("LEFT command received");
            }
            if (cmd == 'h')
            {
                vDesired = 0;
                turnVal = -0.7; // adjust!!!
                yawAngle = 0.0;
                targetYawAngle = -PI / 2; // 90° right
                preciseTurning = true;

                movingToTarget = false; // same as above

                // Serial.println("RIGHT 90° TURN initiated"); // Serial.println("RIGHT command received");
            }
            if (cmd == 't')
            { // Move forward 15 cm
                cumulativeDistance = 0.0;
                targetDistance = 30.0;
                movingToTarget = true;
                // Serial.println("MOVE TO 15CM command received");
            }
            if (cmd == 'l')
            {
                line_tracking_enable = 1;
                movingToTarget = false;
                // Serial.println("LINE TRACKING command received");
            }
            if (cmd == 'k')
            {
                line_tracking_enable = 0;
                movingToTarget = false;
                // Serial.println("LINE TRACKING command received");
            }

            if (cmd == 'o')
            {
                obstacle_avoiding_enable = 1;
                movingToTarget = false;
                // Serial.println("OBSTACLE AVOIDING command received");
            }
            if (cmd == 'i')
            {
                obstacle_avoiding_enable = 0;
                movingToTarget = false; // add function
                // Serial.println("LINE TRACKING command received");
            }
            // if (cmd == 'b')
            // {

            //     // Serial.print("Battery: ");
            //     // Serial.print(battery_percentage);
            //     // Serial.println("%");
            // }
        }

        // no help
        // static unsigned long prevMicros = micros();
        // unsigned long nowMicros = micros();
        // dt = (nowMicros - prevMicros) / 1e6; // convert to seconds
        // prevMicros = nowMicros;

        float gyroYawRate = g.gyro.x - gyroXBias; // rad/s //ADJUST???
        // static float filteredYawRate = 0;
        // filteredYawRate = 0.9 * filteredYawRate + 0.1 * gyroYawRate;
        // yawAngle += filteredYawRate * dt;

        // const float alphaYaw = 0.1; // EMA smoothing factor (adjust as needed)
        // static float filteredYawRate = 0;
        // filteredYawRate = alphaYaw * gyroYawRate + (1 - alphaYaw) * filteredYawRate;
        // yawAngle += filteredYawRate * dt;

        yawAngle += gyroYawRate * dt; // integrate over time
        // yawAngle = alphaEMA * y + (1 - alphaEMA) * yawAngle;

        // Serial.print("GyroX Bias: ");
        // Serial.println(gyroXBias);
        // Serial.print("gyro.x: ");
        // Serial.println(gyroYawRate);
        // Serial.print("yaw angle: ");
        // Serial.println(yawAngle);

        if (preciseTurning)
        {
            float yawError = targetYawAngle - yawAngle;
            turnPid.setSetpoint(targetYawAngle);
            float pidTurnOutput = turnPid.compute(yawAngle) * 0.4;

            // Limit the turn rate to safe bounds
            pidTurnOutput = constrain(pidTurnOutput, -3.0, 3.0);
            turnVal = pidTurnOutput;
            // Serial.print("turnVal: ");
            // Serial.println(turnVal);

            // Stop when close enough
            if (abs(yawError) < 0.017) // adjust!!!!!!!
            {                         // within ~1 degree
                preciseTurning = false;
                turnVal = 0;
                vDesired = 0;
                Serial.println("Finished precise turning.");
            }
        }

        // === line tracking control ==

        // try precise turning
        if (line_tracking_enable)
        {
            bool leftLineValue = digitalRead(lineSensorLeftPin);
            bool rightLineValue = digitalRead(lineSensorRightPin);
            // Serial.print(leftLineValue);
            // Serial.print(" ");
            // Serial.println(rightLineValue);

            // black arena = 1
            if (leftLineValue == 1 && rightLineValue == 0)
            {
                // right turn
                vDesired = 0;
                turnVal = -5;
            }
            else if (leftLineValue == 0 && rightLineValue == 1)
            {
                // left turn
                vDesired = 0;
                turnVal = 5;
            }
            else if (leftLineValue == 0 && rightLineValue == 0)
            {
                // when both sensors are above the line, move backward slowly
                vDesired = -6;
                turnVal = 0;
            }
            else if (leftLineValue == 1 && rightLineValue == 1)
            {
                // advance
                vDesired = 12; //10 // 12 to 22 are acceptable
                turnVal = 0;
            }
        }

        // ==obstacle avoiding==
        if (obstacle_avoiding_enable)
        {

            if (millis() - lastTriggerTime > 100)
            { // Trigger every 100ms
                digitalWrite(TRIG_PIN, LOW);
                delayMicroseconds(2);
                digitalWrite(TRIG_PIN, HIGH);
                delayMicroseconds(10);
                digitalWrite(TRIG_PIN, LOW);
                lastTriggerTime = millis();
            }

            if (echoReceived)
            {
                noInterrupts(); // prevent data races
                unsigned long duration = echoEnd - echoStart;
                echoReceived = false;
                interrupts();

                latestDistance = duration * 0.034 / 2.0; // cm
                // Serial.print("Distance: ");
                // Serial.println(latestDistance);
            }

            // float obstacleDistance = getUltrasonicDistanceCM();

            if (!obstacleDetected && latestDistance < obstacle_distance_threshold) // Obstacle ahead while moving forward
            {
                obstacleDetected = true;
                vDesired = 0;
                turnVal = -3.0; // adjustable
                turnStartTime = millis();
            }
            else if (!obstacleDetected && latestDistance >= obstacle_distance_threshold) // moves forward
            {
                obstacleDetected = false;
                vDesired = 15; // adjustable
                turnVal = 0;
            }

            if (obstacleDetected && millis() - turnStartTime >= turn_duration) // after turning for 1s
            {

                obstacleDetected = false;
                vDesired = 15;
                turnVal = 0;
                // Serial.println("Turn finished. Moving forward.");
            }
        }

        if (movingToTarget)
        {
            float positionError = targetDistance - cumulativeDistance;

            // Stop within ±1 cm
            if (abs(positionError) < 0.05) // adjust!!!!!!!
            {
                vDesired = 0; // no turning control
                movingToTarget = false;
                Serial.println("Target reached.");
            }
            else
            {
                positionPid.setSetpoint(targetDistance);
                vDesired = positionPid.compute(cumulativeDistance) * 0.5; // note bandwidth issue
                // Serial.println();
                // Serial.println(vDesired);
                // Clamp max speed
                vDesired = constrain(vDesired, -70 * 0.5, 70 * 0.5); // cm/s //needless
            }
        }

        // Serial.print(vDesired); //position pid output
        // Serial.println();

        speedPid.setSetpoint(vDesired);
        float speedOutput = speedPid.compute(speedCmPerSecond);
        // Serial.print(speedOutput);
        // Serial.println();

        /*float targetPitch = 0;
        if(vDesired < 0){ // vDesired = +2 or -2
           targetPitch = (speedOutput - 100) * 0.0006; //adjust
        }
        else{
           targetPitch = (speedOutput + 100) * 0.0006;
        } // adjust*/

        float targetPitch = speedOutput * 0.00045; // adjust!!!
        // targetPitch = -0.05; //skip pid
        balancePid.setSetpoint(targetPitch); // targetPitch is in rad. make targetPitch + 0.2rad or -0.2rad (let vDesired=1)
        // balancePid.setSetpoint(0);

        targetPitch = constrain(targetPitch, -0.3, 0.3); // ±18 degrees

        // Serial.print("targetPitch: ");
        // Serial.println(targetPitch);

        // Serial.print("filteredAngle: ");
        // Serial.println(filteredAngle);

        float balanceOutput = balancePid.compute(filteredAngle);
        // Serial.print("balance: ");
        // Serial.println(balanceOutput);

        if (!turnVal)
        {
            yawCorrection = yawPid.compute(rotationalSpeedRadPerSecond);
            // Serial.print("yaw correction: ");
            // Serial.println(yawCorrection);
        }
        else
        {
            yawCorrection = 0;
        }

        // if (abs(balanceOutput) < deadBand) balanceOutput = 0;
        //  Apply deadband compensation to prevent motor stall

        // if (abs(balanceOutput) > 79 ) {
        // balanceOutput = copysign(dbcompensation, balanceOutput);
        //}

        step1.setAccelerationRad(-balanceOutput - turnVal + yawCorrection);
        step2.setAccelerationRad(balanceOutput - turnVal + yawCorrection); // adjust //determines bandwidth
        // Serial.print(balanceOutput);
        // Serial.print(" ");
        // Serial.print(turnVal);
        // Serial.print(" ");
        // Serial.println(yawCorrection);

        // Serial.print("speedCmPerSecond: ");
        // Serial.println(speedCmPerSecond);

        if (!vDesired && !turnVal)
        { // initiate,p
            if (balanceOutput > 0)
            {
                step1.setTargetSpeedRad(-50); // proportional to vDesired(set as a constant for initialisation) and a set constant for vDesired=0
                step2.setTargetSpeedRad(50);
            }
            else
            {
                step1.setTargetSpeedRad(50);
                step2.setTargetSpeedRad(-50);
            }
        }
        else if (turnVal)
        { // a,d
            if (balanceOutput > 0)
            {
                step1.setTargetSpeedRad(-7); // 5-10 // proportional to vDesired(set as a constant for initialisation) and a set constant for vDesired=0
                step2.setTargetSpeedRad(7);
                // turnVal = 7.0;
            }
            else
            {
                step1.setTargetSpeedRad(7);
                step2.setTargetSpeedRad(-7);
                // turnVal = 7.0;
            }
        }
        else if (!turnVal)
        { // w,s
            if (balanceOutput > 0)
            {
                step1.setTargetSpeedRad(-50); // proportional to vDesired(set as a constant for initialisation) and a set constant for vDesired=0
                step2.setTargetSpeedRad(50);
            }
            else
            {
                step1.setTargetSpeedRad(50);
                step2.setTargetSpeedRad(-50);
            }
        }

        /*if (balanceOutput > 0) {
        step1.setTargetSpeedRad (-30); //10-30 //adjust!!! //proportional to vDesired(set as a constant for initialisation) and a set constant for vDesired=0
        step2.setTargetSpeedRad(30);
      } else {
        step1.setTargetSpeedRad(30);
        step2.setTargetSpeedRad(-30);
      }*/

        // initial place for code after avoiding obstacle
        // if (obstacle_avoiding_enable && obstacleDetected && millis() - turnStartTime >= turn_duration)
        // {

        //     obstacleDetected = false;
        //     vDesired = 15;
        //     turnVal = 0;
        //     // Serial.println("Turn finished. Moving forward.");
        // }
    }

    // Step 1
    // if (now - last_sample_time >= SAMPLE_INTERVAL_MS) {
    //     float current_mA = read_current_sensor();  // Replace with your actual current reading code
    //     Serial.print("current reading: (mA): ");
    //     Serial.println(current_mA);
    //     current_samples[sample_index] = current_mA;
    //     sample_index = (sample_index + 1) % AVERAGE_WINDOW_MS;
    //     last_sample_time = now;
    // }

    logic_avg_current_mA = 0.0;
    motor_avg_current_mA = 0.0;
    unsigned long now = millis();
    if (now - last_update_time >= AVERAGE_WINDOW_MS)
    {
        //for (int i = 0; i < 1; i++)
        //{
            logic_avg_current_mA += read_logic_current_sensor();
        //}
        //logic_avg_current_mA /= 1;
        // Serial.print("logic current: ");
        // Serial.println(logic_avg_current_mA);
        logic_delta_charge_mAh = logic_avg_current_mA * (AVERAGE_WINDOW_MS / 3600000.0); // ms → h

        //for (int i = 0; i < 1; i++)
        //{
            motor_avg_current_mA += read_motor_current_sensor();
        //}
        motor_avg_current_mA /= 1;
        // Serial.print("motor current: ");
        // Serial.println(motor_avg_current_mA);
        motor_delta_charge_mAh = motor_avg_current_mA * (AVERAGE_WINDOW_MS / 3600000.0); // ms → h

        remaining_capacity_mAh = remaining_capacity_mAh - logic_delta_charge_mAh - motor_delta_charge_mAh;
        if (remaining_capacity_mAh < 0)
        {
            remaining_capacity_mAh = 0.0;
        }
        battery_percentage = (remaining_capacity_mAh / START_CAPACITY_MAH) * 100.0;

        // Serial.print("Battery: ");
        // Serial.print(battery_percentage);
        // Serial.println("%");

        last_update_time = now;
    }

    // if (millis() > printTimer)
    // {

    //     printTimer += PRINT_INTERVAL;
    //     // Serial.print("Angle: ");
    //     // Serial.print(filteredAngle * 180 / PI);
    //     // Serial.print(" | Speed: ");
    //     // Serial.print(speedCmPerSecond);
    //     // Serial.print(" | Rotation: ");
    //     // Serial.println(rotationalSpeedRadPerSecond);

    //     // Serial.print(readADC(0));
    //     // Serial.println();
    //     // Serial.print("logic current reading: (mA): ");
    //     // Serial.println(logic_avg_current_mA);

    //     // Serial.print(readADC(1));
    //     // Serial.println();
    //     // Serial.print("motor current reading: (mA): ");
    //     // Serial.println(motor_avg_current_mA);
    // }
}