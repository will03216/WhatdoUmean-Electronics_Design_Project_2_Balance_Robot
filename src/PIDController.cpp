#include "PIDController.h"
#include <algorithm>
#include <chrono> //for ioplatform 
#include <cmath>

using namespace std::chrono;

PID::PID(float kp, float ki, float kd, float setpoint)
    : kp(kp), ki(ki), kd(kd), setpoint(setpoint),
      outputMin(-120.0), outputMax(120.0), //adjust
      prevError(0.0), integral(0.0), isYaw(false)
{
    lastTime = duration<float>(system_clock::now().time_since_epoch()).count();
}

void PID::setTunings(float kp, float ki, float kd) {
    this->kp = kp; this->ki = ki; this->kd = kd;
}

void PID::setSetpoint(float setpoint) {
    this->setpoint = setpoint;
}

void PID::setOutputLimits(float min, float max) {
    if (min >= max) return;
    outputMin = min; outputMax = max;
}

void PID::isYawFn(bool yn) {
    isYaw = yn;
}

float PID::compute(float input) {
    float currentTime = duration<float>(system_clock::now().time_since_epoch()).count();
    float dt = currentTime - lastTime;

    float error = setpoint - input;
    if (isYaw) {
        if (error > M_PI) error -= 2 * M_PI;
        else if (error < -M_PI) error += 2 * M_PI;
    }

    integral += error * dt;
    float derivative = (error - prevError) / dt;
    float output = kp * error + ki * integral + kd * derivative;

    output = std::max(outputMin, std::min(output, outputMax));
    prevError = error;
    lastTime = currentTime;

    return output;
}
