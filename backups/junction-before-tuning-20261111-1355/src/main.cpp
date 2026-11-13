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
bool junctionPatternActive = false;
bool manualTimeoutLogged = false;

bool hasLineGapLinePattern(const int sensors[5])
{
    bool foundLine = false;
    bool foundGapAfterLine = false;

    for (int i = 0; i < 5; i++)
    {
        if (sensors[i] == 1)
        {
            if (foundGapAfterLine)
            {
                return true;
            }

            foundLine = true;
        }
        else if (foundLine)
        {
            foundGapAfterLine = true;
        }
    }

    return false;
}
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
        adjustPathActionClock(pauseDuration);
        gJunctionSince = 0;
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

    if (updatePathAction())
    {
        telemetryThrottled();
        delay(LINE_LOOP_DELAY_MS);
        return;
    }

    // Detect PATH junctions before entering line-following PID.
    // Timed route actions must finish before another route symbol is consumed.
    if (robotMode == MODE_PATH && gTurn == 0)
    {
        const bool pattern = hasLineGapLinePattern(gSensor);

        if (pattern != junctionPatternActive)
        {
            junctionPatternActive = pattern;
            char event[96];
            snprintf(event, sizeof(event),
                     "[JUNCTION] %s S=%d%d%d%d%d armed=%s",
                     pattern ? "PATTERN DETECTED" : "pattern cleared",
                     gSensor[0], gSensor[1], gSensor[2],
                     gSensor[3], gSensor[4],
                     gJunctionArmed ? "yes" : "no");
            Serial.println(event);
            sendEventLog(event);
        }

        if (pattern)
        {
            if (gJunctionSince == 0)
            {
                gJunctionSince = millis();
            }
        }
        else
        {
            gJunctionSince = 0;
        }

        if (!pattern && s3on)
        {
            if (gCenteredSince == 0)
            {
                gCenteredSince = millis();
            }
            else if (!gJunctionArmed &&
                     millis() - gCenteredSince >= CENTER_ARM_MS)
            {
                gJunctionArmed = true;
                Serial.println("[JUNCTION] detector ARMED");
                sendEventLog("[JUNCTION] detector ARMED");
            }
        }
        else
        {
            gCenteredSince = 0;
        }

        if (pattern && gJunctionArmed)
        {
            if (gJunctionSince != 0 &&
                millis() - gJunctionSince >= JUNCTION_STABLE_MS)
            {
                gJunctionArmed = false;
                gJunctionSince = 0;

                handleJunction(s1on, s5on);

                telemetryThrottled();
                delay(LINE_LOOP_DELAY_MS);
                return;
            }

            telemetryThrottled();
            delay(LINE_LOOP_DELAY_MS);
            return;
        }
    }
    else if (junctionPatternActive)
    {
        junctionPatternActive = false;
        Serial.println("[JUNCTION] pattern cleared (PATH detector inactive)");
    }

    // STOP
    if (robotMode == MODE_STOP)
    {
        stopMotors();
        gLeftSpeed = 0;
        gRightSpeed = 0;

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
            if (!manualTimeoutLogged)
            {
                Serial.println("[MANUAL] command timeout; motors stopped");
                manualTimeoutLogged = true;
            }
        }
        else
        {
            applyManualDrive();
            manualTimeoutLogged = false;
        }

        telemetryThrottled();
        delay(LINE_LOOP_DELAY_MS);
        return;
    }

    // Keep the previous motor command for outermost-sensor-only readings.
    const bool outermostOnly =
        (gSensor[0] == 1 && gSensor[1] == 0 && gSensor[2] == 0 &&
         gSensor[3] == 0 && gSensor[4] == 0) ||
        (gSensor[0] == 0 && gSensor[1] == 0 && gSensor[2] == 0 &&
         gSensor[3] == 0 && gSensor[4] == 1);

    if (outermostOnly && (gLeftSpeed != 0 || gRightSpeed != 0))
    {
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

    telemetryThrottled();

    delay(LINE_LOOP_DELAY_MS);
}
