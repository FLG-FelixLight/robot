#include "robot_config.h"
#include "robot_state.h"
#include "AudioFileSourceFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

float Kp = 35.0;
float Ki = 0.0;
float Kd = 20.0;

int baseSpeed = 150;
int maxSpeed = 255;

float error = 0;
float lastError = 0;
float integral = 0;
unsigned long lastTime = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool robotEnabled = false;
RobotMode robotMode = MODE_STOP;

char driveDir[16] = "stop";
unsigned long lastDriveCmd = 0;

int gSensor[5] = {0, 0, 0, 0, 0};
float gCorrection = 0;
int gLeftSpeed = 0;
int gRightSpeed = 0;
bool gFullLine = false;

char gNotice[96] = "";

unsigned long lastTelemetry = 0;
unsigned long lastWifiCheck = 0;

String gPath = "";
size_t gPathIdx = 0;
int gDestId = 0;

int gTurn = 0;
unsigned long gTurnStart = 0;

bool gJunctionArmed = false;
unsigned long gJunctionSince = 0;
unsigned long gCenteredSince = 0;

std::map<int, String> gTables;

AudioGeneratorMP3 *gMp3 = nullptr;
AudioFileSourceFS *gMp3File = nullptr;
AudioOutputI2S *gAudioOut = nullptr;

char gAudioFile[80] = "";
volatile bool gAudioNew = false;

QueueHandle_t gCmdQueue = nullptr;

bool gObstacleDetected = false;
float gObstacleDistanceCm = 0;
