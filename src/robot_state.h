#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <map>

class AudioGeneratorMP3;
class AudioFileSourceFS;
class AudioOutputI2S;

enum RobotMode
{
    MODE_STOP,
    MODE_AUTO,
    MODE_MANUAL,
    MODE_PATH
};

enum CommandType
{
    CMD_NONE = 0,
    CMD_START,
    CMD_STOP,
    CMD_ESTOP,
    CMD_DRIVE,
    CMD_GOTO,
    CMD_PLAY,
    CMD_CONFIG
};

struct Command
{
    uint8_t type;
    char text[96];
    int value;
    float kp;
    float ki;
    float kd;
    int base;
};

extern float Kp;
extern float Ki;
extern float Kd;
extern int baseSpeed;
extern int maxSpeed;

extern float error;
extern float lastError;
extern float integral;
extern unsigned long lastTime;

extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern bool robotEnabled;
extern RobotMode robotMode;
extern char driveDir[16];
extern unsigned long lastDriveCmd;
extern int gSensor[5];
extern float gCorrection;
extern int gLeftSpeed;
extern int gRightSpeed;
extern bool gFullLine;
extern char gNotice[96];
extern unsigned long lastTelemetry;
extern unsigned long lastWifiCheck;

extern String gPath;
extern size_t gPathIdx;
extern int gDestId;
extern int gTurn;
extern unsigned long gTurnStart;
extern bool gTurnPause;
extern bool gJunctionArmed;
extern unsigned long gCenteredSince;
extern std::map<int, String> gTables;

extern AudioGeneratorMP3 *gMp3;
extern AudioFileSourceFS *gMp3File;
extern AudioOutputI2S *gAudioOut;
extern char gAudioFile[80];
extern volatile bool gAudioNew;

extern QueueHandle_t gCmdQueue;

extern bool gObstacleDetected;
extern float gObstacleDistanceCm;
