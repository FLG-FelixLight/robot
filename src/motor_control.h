#pragma once

#include <Arduino.h>

void setNotice(const char *s);
void setNotice(const String &s);
void resetPidState();
void driveMotors(int l, int r);
void telemetryThrottled();
void motorLeft(int speed);
void motorRight(int speed);
void stopMotors();
void readSensors(int &s1, int &s2, int &s3, int &s4, int &s5);
float readLine(int s1, int s2, int s3, int s4, int s5);
float calculatePID(float currentError);
void applyManualDrive();
