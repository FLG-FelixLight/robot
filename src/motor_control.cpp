#include "robot_config.h"
#include "robot_state.h"
#include "motor_control.h"
#include "web_control.h"

#include <string.h>

void setNotice(const char *s)
{
    strlcpy(gNotice, s, sizeof(gNotice));
}

void setNotice(const String &s)
{
    setNotice(s.c_str());
}

void resetPidState()
{
    integral = 0;
    lastError = 0;
    error = 0;
    lastTime = millis();
}

void driveMotors(int l, int r)
{
    motorLeft(l);
    motorRight(r);
    gLeftSpeed = l;
    gRightSpeed = r;
}

void telemetryThrottled()
{
    if (millis() - lastTelemetry >= TELEMETRY_INTERVAL)
    {
        lastTelemetry = millis();
        ws.cleanupClients();
        sendTelemetry();
    }
}

// ======================================================
// 12. MOTOR LEFT
// ======================================================

void motorLeft(int speed)
{
    speed = constrain(speed, -255, 255);

    if (speed > 0)
    {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        analogWrite(ENA, speed);
    }
    else if (speed < 0)
    {
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        analogWrite(ENA, -speed);
    }
    else
    {
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        analogWrite(ENA, 0);
    }
}

// ======================================================
// 13. MOTOR RIGHT
// ======================================================

void motorRight(int speed)
{
    speed = constrain(speed, -255, 255);

    if (speed > 0)
    {
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        analogWrite(ENB, speed);
    }
    else if (speed < 0)
    {
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        analogWrite(ENB, -speed);
    }
    else
    {
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        analogWrite(ENB, 0);
    }
}

// ======================================================
// 14. STOP
// ======================================================

void stopMotors()
{
    analogWrite(ENA, 0);
    analogWrite(ENB, 0);

    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);

    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
}

// ======================================================
// 15. READ SENSOR
// ======================================================

void readSensors(
    int &s1,
    int &s2,
    int &s3,
    int &s4,
    int &s5)
{
    s1 = digitalRead(S1);
    s2 = digitalRead(S2);
    s3 = digitalRead(S3);
    s4 = digitalRead(S4);
    s5 = digitalRead(S5);
}

// ======================================================
// 16. READ LINE POSITION
// ======================================================

float readLine(
    int s1,
    int s2,
    int s3,
    int s4,
    int s5)
{
    int sensor[5] = {
        s1,
        s2,
        s3,
        s4,
        s5};

    int weight[5] = {
        -2,
        -1,
        0,
        1,
        2};

    int total = 0;
    int count = 0;

    for (int i = 0; i < 5; i++)
    {
        if (sensor[i] == LINE_ON)
        {
            total += weight[i];
            count++;
        }
    }

    if (count == 0)
    {
        return lastError;
    }

    return (float)total / count;
}

// ======================================================
// 17. PID
// ======================================================

float calculatePID(float currentError)
{
    unsigned long currentTime = millis();

    float dt = (currentTime - lastTime) / 1000.0;

    if (dt <= 0)
    {
        dt = 0.001;
    }

    lastTime = currentTime;

    float P = currentError;

    integral += currentError * dt;
    integral = constrain(integral, -10.0, 10.0);

    float derivative = (currentError - lastError) / dt;

    lastError = currentError;

    return Kp * P + Ki * integral + Kd * derivative;
}

// ======================================================
// 18. TABLES (data/tables.json)
// ======================================================

void applyManualDrive()
{
    int fast = constrain(baseSpeed, 0, maxSpeed);
    int slow = fast / 2;

    int L = 0;
    int R = 0;

    if (strcmp(driveDir, "forward") == 0)
    {
        L = fast;
        R = fast;
    }
    else if (strcmp(driveDir, "back") == 0)
    {
        L = -fast;
        R = -fast;
    }
    else if (strcmp(driveDir, "left") == 0)
    {
        L = 0;
        R = fast;
    }
    else if (strcmp(driveDir, "right") == 0)
    {
        L = fast;
        R = 0;
    }
    else if (strcmp(driveDir, "forward_left") == 0)
    {
        L = slow;
        R = fast;
    }
    else if (strcmp(driveDir, "forward_right") == 0)
    {
        L = fast;
        R = slow;
    }
    else if (strcmp(driveDir, "back_left") == 0)
    {
        L = -slow;
        R = -fast;
    }
    else if (strcmp(driveDir, "back_right") == 0)
    {
        L = -fast;
        R = -slow;
    }

    driveMotors(L, R);
}
