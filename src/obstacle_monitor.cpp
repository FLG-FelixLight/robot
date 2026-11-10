#include "robot_config.h"
#include "robot_state.h"
#include "obstacle_monitor.h"

#include <driver/gpio.h>
#include <esp_timer.h>

namespace
{
portMUX_TYPE echoMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t echoRiseUs = 0;
volatile uint32_t echoPulseUs = 0;
volatile bool echoHigh = false;
volatile bool echoReady = false;

bool echoPending = false;
bool hasTriggered = false;
uint8_t missedEchoCount = 0;
uint32_t echoStartedAtMs = 0;
uint32_t lastTriggerAtMs = 0;

void IRAM_ATTR onEchoChange()
{
    const uint32_t nowUs = static_cast<uint32_t>(esp_timer_get_time());
    const bool levelHigh = gpio_get_level(static_cast<gpio_num_t>(HY_ECHO)) != 0;

    portENTER_CRITICAL_ISR(&echoMux);
    if (levelHigh)
    {
        echoRiseUs = nowUs;
        echoHigh = true;
    }
    else if (echoHigh)
    {
        echoPulseUs = nowUs - echoRiseUs;
        echoHigh = false;
        echoReady = true;
    }
    portEXIT_CRITICAL_ISR(&echoMux);
}

void recordNoEcho()
{
    gObstacleDistanceCm = 0;
    if (gObstacleDetected && ++missedEchoCount >= OBSTACLE_NO_ECHO_CLEAR_COUNT)
    {
        gObstacleDetected = false;
        missedEchoCount = 0;
    }
}

void recordDistance(uint32_t pulseUs)
{
    const float distanceCm = pulseUs * 0.0343f / 2.0f;
    if (distanceCm <= 0 || distanceCm > 400)
    {
        recordNoEcho();
        return;
    }

    missedEchoCount = 0;
    gObstacleDistanceCm = distanceCm;

    if (!gObstacleDetected && distanceCm < OBSTACLE_STOP_CM)
    {
        gObstacleDetected = true;
    }
    else if (gObstacleDetected && distanceCm >= OBSTACLE_CLEAR_CM)
    {
        gObstacleDetected = false;
    }
}
}

void setupObstacleMonitor()
{
    pinMode(HY_TRIG, OUTPUT);
    digitalWrite(HY_TRIG, LOW);
    pinMode(HY_ECHO, INPUT);
    pinMode(HY_OUT, INPUT);
    attachInterrupt(digitalPinToInterrupt(HY_ECHO), onEchoChange, CHANGE);
}

void updateObstacleMonitor()
{
    uint32_t pulseUs = 0;
    bool hasEcho = false;

    portENTER_CRITICAL(&echoMux);
    if (echoReady)
    {
        pulseUs = echoPulseUs;
        echoReady = false;
        hasEcho = true;
    }
    portEXIT_CRITICAL(&echoMux);

    if (hasEcho)
    {
        echoPending = false;
        recordDistance(pulseUs);
    }
    else if (echoPending &&
             millis() - echoStartedAtMs >= OBSTACLE_ECHO_TIMEOUT_MS)
    {
        echoPending = false;
        recordNoEcho();
    }

    const uint32_t nowMs = millis();
    if (!echoPending &&
        (!hasTriggered || nowMs - lastTriggerAtMs >= OBSTACLE_PING_INTERVAL_MS))
    {
        portENTER_CRITICAL(&echoMux);
        echoReady = false;
        echoHigh = false;
        portEXIT_CRITICAL(&echoMux);

        digitalWrite(HY_TRIG, LOW);
        delayMicroseconds(2);
        digitalWrite(HY_TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(HY_TRIG, LOW);

        echoPending = true;
        hasTriggered = true;
        echoStartedAtMs = nowMs;
        lastTriggerAtMs = nowMs;
    }
}
