#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#include "robot_config.h"
#include "robot_state.h"
#include "motor_control.h"
#include "destination_manager.h"
#include "path_control.h"
#include "web_control.h"
#include "audio_control.h"
#include "network.h"
#include "clock_display.h"
#include "obstacle_monitor.h"

namespace
{
bool obstaclePaused = false;
uint32_t obstaclePauseStartedAt = 0;
bool wifiWasConnected = false;
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    pinMode(S1, INPUT);
    pinMode(S2, INPUT);
    pinMode(S3, INPUT);
    pinMode(S4, INPUT);
    pinMode(S5, INPUT);

    pinMode(ENA, OUTPUT);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    pinMode(ENB, OUTPUT);

    stopMotors();

    pinMode(AMP_SD, OUTPUT);
    digitalWrite(AMP_SD, LOW);
    pinMode(AMP_GAIN, OUTPUT);
    digitalWrite(AMP_GAIN, LOW);
    digitalWrite(AMP_SD, HIGH);

    setupObstacleMonitor();
    setupClockDisplay();

    lastTime = millis();

    Serial.println();
    Serial.println("========================================");
    Serial.println("       ESP32-S3 LINE FOLLOWER");
    Serial.println("       PID + PATH + AUDIO");
    Serial.println("========================================");

    Serial.println();
    Serial.println("SENSOR : S1..S5 = GPIO 4,5,6,7,15");
    Serial.println("L298N  : ENA=16 IN1=17 IN2=18 IN3=8 IN4=3 ENB=46");
    Serial.println("MAX98357: LRC=9 BCLK=10 DIN=11 GAIN=12 (LOW) SD=13");
    Serial.println("TM1637: CLK=1 DIO=2 (NTP, ICT-7)");
    Serial.println("HY-SRF05: TRIG=45 ECHO=48 OUT=47");

    if (!LittleFS.begin(true))
    {
        Serial.println("[FS] LittleFS mount FAILED");
    }
    else
    {
        Serial.println("[FS] LittleFS OK");
    }

    loadTables();
    setupAudio();

    gCmdQueue = xQueueCreate(8, sizeof(Command));

    connectWiFi();
    startNtpSync();

    if (MDNS.begin(MDNS_NAME))
    {
        Serial.printf("[MDNS] http://%s.local/\n", MDNS_NAME);
    }

    setupWebServer();
}

void loop()
{
    updateObstacleMonitor();
    updateClockDisplay();

    if (gObstacleDetected && !obstaclePaused)
    {
        obstaclePaused = true;
        obstaclePauseStartedAt = millis();

        char notice[96];
        snprintf(notice, sizeof(notice), "OBSTACLE %.1f cm", gObstacleDistanceCm);
        setNotice(notice);
        Serial.printf("[OBSTACLE] detected at %.1f cm; motors paused\n",
                      gObstacleDistanceCm);
    }
    else if (!gObstacleDetected && obstaclePaused)
    {
        const uint32_t pauseDuration = millis() - obstaclePauseStartedAt;
        if (gTurn != 0)
        {
            gTurnStart += pauseDuration;
        }
        gCenteredSince = 0;
        resetPidState();
        obstaclePaused = false;
        setNotice("Obstacle cleared; resuming");
        Serial.println("[OBSTACLE] clear; resuming previous mode");
    }

    // WIFI RECONNECT
    if (WiFi.status() != WL_CONNECTED && millis() - lastWifiCheck > 5000)
    {
        lastWifiCheck = millis();
        Serial.println("[WIFI] Reconnecting...");
        WiFi.reconnect();
    }

    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (wifiConnected && !wifiWasConnected)
    {
        strlcpy(gAudioFile, "welcome.mp3", sizeof(gAudioFile));
        gAudioNew = true;
        Serial.println("[AUDIO] Wi-Fi connected; playing welcome.mp3");
    }
    wifiWasConnected = wifiConnected;

    // COMMANDS
    Command c;

    while (gCmdQueue != nullptr && xQueueReceive(gCmdQueue, &c, 0) == pdTRUE)
    {
        applyCommand(c);
    }

    updateDestinationHold();

    // READ SENSORS
    int s1, s2, s3, s4, s5;

    readSensors(s1, s2, s3, s4, s5);

    bool s1on = (s1 == LINE_ON);
    bool s2on = (s2 == LINE_ON);
    bool s3on = (s3 == LINE_ON);
    bool s4on = (s4 == LINE_ON);
    bool s5on = (s5 == LINE_ON);

    int active = s1on + s2on + s3on + s4on + s5on;

    gSensor[0] = s1on ? 1 : 0;
    gSensor[1] = s2on ? 1 : 0;
    gSensor[2] = s3on ? 1 : 0;
    gSensor[3] = s4on ? 1 : 0;
    gSensor[4] = s5on ? 1 : 0;

    gFullLine = (active == 5);

    // OBSTACLE PAUSE
    if (gObstacleDetected)
    {
        stopMotors();
        gLeftSpeed = 0;
        gRightSpeed = 0;
        telemetryThrottled();
        delay(LINE_LOOP_DELAY_MS);
        return;
    }

    // Detect PATH junctions before entering line-following PID.
    // A turn already in progress must finish before another route symbol is consumed.
    if (robotMode == MODE_PATH && gTurn == 0)
    {
        const bool pattern = s1on && !s3on && s5on;

        // Chỉ "arm" khi robot đang bám line ở giữa (S3 thấy line, S1/S5 không thấy)
        // liên tục đủ CENTER_ARM_MS (200 ms).
        if (!pattern && !s1on && s3on && !s5on)
        {
            if (gCenteredSince == 0)
            {
                gCenteredSince = millis();
            }
            else if (millis() - gCenteredSince >= CENTER_ARM_MS)
            {
                gJunctionArmed = true;
            }
        }
        else
        {
            gCenteredSince = 0;
        }

        // Đã armed + thấy pattern ngã rẽ -> thực hiện lệnh rẽ ngay:
        // handleJunction() dừng động cơ (rồi updateTurn() dừng đủ TURN_PAUSE_MS = 2s,
        // sau đó xoay TURN_DURATION_MS theo hướng L/R trong mã đường đi).
        if (pattern && gJunctionArmed)
        {
            gJunctionArmed = false;

            handleJunction(s1on, s5on);

            telemetryThrottled();
            delay(LINE_LOOP_DELAY_MS);
            return;
        }
    }

    // STOP
    if (robotMode == MODE_STOP)
    {
        stopMotors();
        gLeftSpeed = 0;
        gRightSpeed = 0;

        Serial.printf("S=%d%d%d%d%d  STOP\n",
                      gSensor[0], gSensor[1], gSensor[2],
                      gSensor[3], gSensor[4]);

        telemetryThrottled();
        delay(LINE_LOOP_DELAY_MS);
        return;
    }

    // MANUAL
    if (robotMode == MODE_MANUAL)
    {
        if (millis() - lastDriveCmd > DRIVE_TIMEOUT)
        {
            stopMotors();
            gLeftSpeed = 0;
            gRightSpeed = 0;
            Serial.println("  >>> MANUAL: TIMEOUT -> STOP");
        }
        else
        {
            applyManualDrive();
            Serial.printf("S=%d%d%d%d%d  MANUAL %s L=%d R=%d\n",
                          gSensor[0], gSensor[1], gSensor[2],
                          gSensor[3], gSensor[4],
                          driveDir, gLeftSpeed, gRightSpeed);
        }

        telemetryThrottled();
        delay(LINE_LOOP_DELAY_MS);
        return;
    }

    // PATH: dang re
    if (robotMode == MODE_PATH && gTurn != 0)
    {
        updateTurn();

        const char *turnStatus = gTurn == 0 ? "DONE" : (gTurnPause ? "WAIT" : "ROTATE");
        Serial.printf("S=%d%d%d%d%d  PATH TURN %s %s\n",
                      gSensor[0], gSensor[1], gSensor[2],
                      gSensor[3], gSensor[4],
                      gTurn == 0 ? "-" : (gTurn > 0 ? "R" : "L"),
                      turnStatus);

        telemetryThrottled();
        delay(LINE_LOOP_DELAY_MS);
        return;
    }

    // PID FOLLOW
    error = readLine(s1, s2, s3, s4, s5);

    float correction = calculatePID(error);

    gCorrection = correction;

    int leftSpeed = constrain((int)(baseSpeed - correction), -maxSpeed, maxSpeed);
    int rightSpeed = constrain((int)(baseSpeed + correction), -maxSpeed, maxSpeed);

    driveMotors(leftSpeed, rightSpeed);

    Serial.printf("S=%d%d%d%d%d  Error=%.2f PID=%.2f L=%d R=%d %s\n",
                  gSensor[0], gSensor[1], gSensor[2],
                  gSensor[3], gSensor[4],
                  error, correction, leftSpeed, rightSpeed,
                  modeName());

    telemetryThrottled();

    delay(LINE_LOOP_DELAY_MS);
}
