#ifndef PIDController_H
#define PIDController_H

class PID {
public:
    PID(float kp, float ki, float kd, float setpoint);
    void setTunings(float kp, float ki, float kd);
    void setSetpoint(float setpoint);
    void setOutputLimits(float min, float max);
    void isYawFn(bool yn);
    float compute(float input);

private:
    float kp, ki, kd;
    float setpoint;
    float outputMin, outputMax;
    float prevError, integral, lastTime;
    float sampleTime;
    bool isYaw;
};

#endif // PIDController_H
