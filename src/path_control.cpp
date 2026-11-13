#include "robot_config.h"
#include "robot_state.h"
#include "motor_control.h"
#include "path_control.h"
#include "web_control.h"

namespace
{
bool destinationHoldActive = false;
unsigned long destinationHoldStartedAt = 0;
}

void arrived(const char *why)
{
    stopMotors();
    gLeftSpeed = 0;
    gRightSpeed = 0;
    gTurn = 0;
    gTurnPause = false;
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

    Serial.printf("[PATH] junction idx=%u code=%c L=%d R=%d\n",
                  (unsigned)gPathIdx, c, leftOn, rightOn);

    if (c == '0')
    {
        arrived("destination");
        destinationHoldActive = true;
        destinationHoldStartedAt = millis();
        strlcpy(gAudioFile, "welcome.mp3", sizeof(gAudioFile));
        gAudioNew = true;
        Serial.println("[PATH] destination stop hold; playing welcome.mp3");
        return;
    }

    gPathIdx++;

    if (c == 'L')
    {
        stopMotors();
        gLeftSpeed = 0;
        gRightSpeed = 0;
        gTurn = -1;
        gTurnStart = millis();
        gTurnPause = true;
        Serial.println("[PATH] -> LEFT");
    }
    else if (c == 'R')
    {
        stopMotors();
        gLeftSpeed = 0;
        gRightSpeed = 0;
        gTurn = +1;
        gTurnStart = millis();
        gTurnPause = true;
        Serial.println("[PATH] -> RIGHT");
    }
    else
    {
        resetPidState();
        Serial.println("[PATH] -> STRAIGHT");
    }
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

void updateTurn()
{
    const unsigned long now = millis();

    if (gTurnPause)
    {
        stopMotors();
        gLeftSpeed = 0;
        gRightSpeed = 0;

        if (now - gTurnStart >= TURN_PAUSE_MS)
        {
            gTurnPause = false;
            gTurnStart = now;
        }

        return;
    }

    const int speed = TURN_SPEED;
    if (gTurn > 0)
    {
        driveMotors(speed, -speed);
    }
    else
    {
        driveMotors(-speed, speed);
    }

    if (now - gTurnStart >= TURN_DURATION_MS)
    {
        gTurn = 0;
        stopMotors();
        gLeftSpeed = 0;
        gRightSpeed = 0;
        resetPidState();
        gJunctionArmed = false;
        gCenteredSince = 0;
        setNotice("TURN OK");
    }

}

// ======================================================
// 21. APPLY COMMAND (chay trong loop)
// ======================================================
