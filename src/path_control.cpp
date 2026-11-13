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
enum class TurnSensorPhase
{
    WAIT_CENTER_LINE,
    WAIT_CENTER_CLEAR,
    WAIT_CENTER_REACQUIRE
};
PathActionPhase pathActionPhase = PathActionPhase::NONE;
char pendingPathAction = '\0';
uint32_t pathActionStartedAt = 0;
uint32_t turnSensorCandidateSince = 0;
TurnSensorPhase turnSensorPhase = TurnSensorPhase::WAIT_CENTER_LINE;

void completePathAction()
{
    gTurn = 0;
    pendingPathAction = '\0';
    pathActionPhase = PathActionPhase::NONE;
    turnSensorCandidateSince = 0;
    turnSensorPhase = TurnSensorPhase::WAIT_CENTER_LINE;
    gJunctionArmed = false;
    gJunctionSince = 0;
    gCenteredSince = 0;
    resetPidState();
    setNotice("PATH action complete; following line");
    Serial.println("[JUNCTION] ACTION COMPLETE; resume line following");
    sendEventLog("[JUNCTION] ACTION COMPLETE; resume line following");
}

void stopOnTurnTimeout()
{
    stopMotors();
    gLeftSpeed = 0;
    gRightSpeed = 0;
    robotEnabled = false;
    robotMode = MODE_STOP;
    gTurn = 0;
    pendingPathAction = '\0';
    pathActionPhase = PathActionPhase::NONE;
    turnSensorCandidateSince = 0;
    turnSensorPhase = TurnSensorPhase::WAIT_CENTER_LINE;
    gJunctionArmed = false;
    gJunctionSince = 0;
    gCenteredSince = 0;

    setNotice("PATH turn timeout; center sensor did not reacquire line");
    Serial.println("[JUNCTION] TURN TIMEOUT; S3 did not reacquire line, stopped safely");
    sendEventLog("[JUNCTION] TURN TIMEOUT; S3 did not reacquire line, stopped safely");
    sendConfig(nullptr);
}
}

void arrived(const char *why)
{
    stopMotors();
    gLeftSpeed = 0;
    gRightSpeed = 0;
    gTurn = 0;
    pathActionPhase = PathActionPhase::NONE;
    pendingPathAction = '\0';
    turnSensorCandidateSince = 0;
    turnSensorPhase = TurnSensorPhase::WAIT_CENTER_LINE;
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
    turnSensorCandidateSince = 0;
    turnSensorPhase = TurnSensorPhase::WAIT_CENTER_LINE;
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
        turnSensorCandidateSince = 0;
        turnSensorPhase = TurnSensorPhase::WAIT_CENTER_LINE;
        char event[96];
        if (pendingPathAction == 'L' || pendingPathAction == 'R')
        {
            snprintf(event, sizeof(event),
                     "[JUNCTION] STOP COMPLETE; ACTION START code=%c until S3 reacquires (timeout=%lu ms)",
                     pendingPathAction,
                     static_cast<unsigned long>(ROUTE_TURN_TIMEOUT_MS));
        }
        else
        {
            snprintf(event, sizeof(event),
                     "[JUNCTION] STOP COMPLETE; ACTION START code=%c duration=%lu ms",
                     pendingPathAction,
                     static_cast<unsigned long>(ROUTE_ACTION_MS));
        }
        Serial.println(event);
        sendEventLog(event);
    }

    const int speed = constrain(baseSpeed, 0, maxSpeed);
    if (pendingPathAction == 'L' || pendingPathAction == 'R')
    {
        const bool centerOnLine = gSensor[2] == 1;
        const bool expectedSensorState =
            turnSensorPhase == TurnSensorPhase::WAIT_CENTER_LINE
                ? centerOnLine
                : (turnSensorPhase == TurnSensorPhase::WAIT_CENTER_CLEAR
                       ? !centerOnLine
                       : centerOnLine);

        if (expectedSensorState)
        {
            if (turnSensorCandidateSince == 0)
            {
                turnSensorCandidateSince = now;
            }
            else if (now - turnSensorCandidateSince >= ROUTE_TURN_SENSOR_STABLE_MS)
            {
                turnSensorCandidateSince = 0;
                if (turnSensorPhase == TurnSensorPhase::WAIT_CENTER_LINE)
                {
                    turnSensorPhase = TurnSensorPhase::WAIT_CENTER_CLEAR;
                    Serial.println("[JUNCTION] S3 on line; waiting for stable departure");
                    sendEventLog("[JUNCTION] S3 on line; waiting for stable departure");
                }
                else if (turnSensorPhase == TurnSensorPhase::WAIT_CENTER_CLEAR)
                {
                    turnSensorPhase = TurnSensorPhase::WAIT_CENTER_REACQUIRE;
                    Serial.println("[JUNCTION] S3 left line; waiting to reacquire");
                    sendEventLog("[JUNCTION] S3 left line; waiting to reacquire");
                }
                else
                {
                    char completeEvent[96];
                    snprintf(completeEvent, sizeof(completeEvent),
                             "[JUNCTION] S3 reacquired line; turn complete after %lu ms",
                             static_cast<unsigned long>(now - pathActionStartedAt));
                    Serial.println(completeEvent);
                    sendEventLog(completeEvent);
                    completePathAction();
                    return true;
                }
            }
        }
        else
        {
            turnSensorCandidateSince = 0;
        }

        if (now - pathActionStartedAt >= ROUTE_TURN_TIMEOUT_MS)
        {
            stopOnTurnTimeout();
            return true;
        }

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

    if (now - pathActionStartedAt < ROUTE_ACTION_MS)
    {
        driveMotors(speed, speed);
        return true;
    }

    completePathAction();
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
    turnSensorCandidateSince = 0;
    turnSensorPhase = TurnSensorPhase::WAIT_CENTER_LINE;
    gTurn = 0;
}

// ======================================================
// 21. APPLY COMMAND (chay trong loop)
// ======================================================
