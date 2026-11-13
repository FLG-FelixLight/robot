#include "robot_config.h"
#include "robot_state.h"
#include "motor_control.h"
#include "path_control.h"
#include "web_control.h"

namespace
{
bool destinationHoldActive = false;
unsigned long destinationHoldStartedAt = 0;
enum class PathActionPhase
{
    NONE,
    JUNCTION_STOP,
    EXECUTE
};
PathActionPhase pathActionPhase = PathActionPhase::NONE;
char pendingPathAction = '\0';
uint32_t pathActionStartedAt = 0;
}

void arrived(const char *why)
{
    stopMotors();
    gLeftSpeed = 0;
    gRightSpeed = 0;
    gTurn = 0;
    pathActionPhase = PathActionPhase::NONE;
    pendingPathAction = '\0';
    robotEnabled = false;
    robotMode = MODE_STOP;

    char b[96];
    snprintf(b, sizeof(b), "ARRIVED dest=%d (%s)", gDestId, why);
    setNotice(b);
    Serial.println(b);

    sendConfig(nullptr);
}

void handleJunction(bool leftOn, bool rightOn)
{
    if (gPathIdx >= gPath.length())
    {
        arrived("no more code");
        return;
    }

    char c = gPath[gPathIdx];

    char event[112];
    snprintf(event, sizeof(event),
             "[JUNCTION] >>> CONFIRMED <<< idx=%u pattern=%d%d%d%d%d code=%c L=%d R=%d",
             (unsigned)gPathIdx,
             gSensor[0], gSensor[1], gSensor[2], gSensor[3], gSensor[4],
             c, leftOn, rightOn);
    Serial.println(event);
    sendEventLog(event);

    if (c == '0')
    {
        arrived("destination");
        destinationHoldActive = true;
        destinationHoldStartedAt = millis();
        strlcpy(gAudioFile, "welcome.mp3", sizeof(gAudioFile));
        gAudioNew = true;
        Serial.println("[PATH] destination stop hold; playing welcome.mp3");
        sendEventLog("[PATH] destination reached; stopped and playing welcome.mp3");
        return;
    }

    gPathIdx++;
    pendingPathAction = c;
    pathActionPhase = PathActionPhase::JUNCTION_STOP;
    pathActionStartedAt = millis();
    gTurn = c == 'L' ? -1 : (c == 'R' ? 1 : 0);
    stopMotors();
    gLeftSpeed = 0;
    gRightSpeed = 0;
    setNotice("Junction confirmed; waiting 2 s");
    snprintf(event, sizeof(event), "[JUNCTION] STOP for %lu ms; next action=%c",
             static_cast<unsigned long>(JUNCTION_STOP_MS), c);
    Serial.println(event);
    sendEventLog(event);
}

void updateDestinationHold()
{
    if (!destinationHoldActive)
    {
        return;
    }

    if (robotMode != MODE_STOP || gPath.length() == 0)
    {
        destinationHoldActive = false;
        return;
    }

    if (millis() - destinationHoldStartedAt >= DESTINATION_HOLD_MS)
    {
        destinationHoldActive = false;
        Serial.println("[PATH] destination hold complete; remaining stopped");
    }
}

bool updatePathAction()
{
    if (pathActionPhase == PathActionPhase::NONE)
    {
        return false;
    }

    if (robotMode != MODE_PATH)
    {
        cancelPathAction();
        return false;
    }

    const uint32_t now = millis();
    if (pathActionPhase == PathActionPhase::JUNCTION_STOP)
    {
        stopMotors();
        gLeftSpeed = 0;
        gRightSpeed = 0;

        if (now - pathActionStartedAt < JUNCTION_STOP_MS)
        {
            return true;
        }

        pathActionPhase = PathActionPhase::EXECUTE;
        pathActionStartedAt = now;
        char event[96];
        snprintf(event, sizeof(event),
                 "[JUNCTION] STOP COMPLETE; ACTION START code=%c duration=%lu ms",
                 pendingPathAction,
                 static_cast<unsigned long>(ROUTE_ACTION_MS));
        Serial.println(event);
        sendEventLog(event);
    }

    if (now - pathActionStartedAt < ROUTE_ACTION_MS)
    {
        const int speed = constrain(baseSpeed, 0, maxSpeed);
        if (pendingPathAction == 'L')
        {
            driveMotors(0, speed);
        }
        else if (pendingPathAction == 'R')
        {
            driveMotors(speed, 0);
        }
        else
        {
            driveMotors(speed, speed);
        }
        return true;
    }

    gTurn = 0;
    pendingPathAction = '\0';
    pathActionPhase = PathActionPhase::NONE;
    gJunctionArmed = false;
    gJunctionSince = 0;
    gCenteredSince = 0;
    resetPidState();
    setNotice("PATH action complete; following line");
    Serial.println("[JUNCTION] ACTION COMPLETE; resume line following");
    sendEventLog("[JUNCTION] ACTION COMPLETE; resume line following");
    return true;
}

void adjustPathActionClock(uint32_t pauseDuration)
{
    if (pathActionPhase != PathActionPhase::NONE)
    {
        pathActionStartedAt += pauseDuration;
    }
}

void cancelPathAction()
{
    pathActionPhase = PathActionPhase::NONE;
    pendingPathAction = '\0';
    gTurn = 0;
}

// ======================================================
// 21. APPLY COMMAND (chay trong loop)
// ======================================================
