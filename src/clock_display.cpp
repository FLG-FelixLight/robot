#include "robot_config.h"
#include "clock_display.h"

#include <Arduino.h>
#include <time.h>

namespace
{
const uint8_t digitSegments[] = {
    0x3f, 0x06, 0x5b, 0x4f, 0x66,
    0x6d, 0x7d, 0x07, 0x7f, 0x6f};
const uint8_t displayBrightness = 4;
uint32_t lastDisplayUpdateMs = 0;

void writeByte(uint8_t value)
{
    pinMode(TM1637_DIO, OUTPUT);
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
        digitalWrite(TM1637_CLK, LOW);
        digitalWrite(TM1637_DIO, (value & (1U << bit)) ? HIGH : LOW);
        digitalWrite(TM1637_CLK, HIGH);
    }

    digitalWrite(TM1637_CLK, LOW);
    pinMode(TM1637_DIO, INPUT);
    digitalWrite(TM1637_CLK, HIGH);
    digitalWrite(TM1637_CLK, LOW);
    pinMode(TM1637_DIO, OUTPUT);
}

void startTransmission()
{
    pinMode(TM1637_DIO, OUTPUT);
    digitalWrite(TM1637_CLK, HIGH);
    digitalWrite(TM1637_DIO, HIGH);
    digitalWrite(TM1637_DIO, LOW);
    digitalWrite(TM1637_CLK, LOW);
}

void stopTransmission()
{
    digitalWrite(TM1637_CLK, LOW);
    digitalWrite(TM1637_DIO, LOW);
    digitalWrite(TM1637_CLK, HIGH);
    digitalWrite(TM1637_DIO, HIGH);
}

void showSegments(const uint8_t segments[4])
{
    startTransmission();
    writeByte(0x40);
    stopTransmission();

    startTransmission();
    writeByte(0xc0);
    for (uint8_t i = 0; i < 4; ++i)
    {
        writeByte(segments[i]);
    }
    stopTransmission();

    startTransmission();
    writeByte(static_cast<uint8_t>(0x88 | displayBrightness));
    stopTransmission();
}

void showUnsynchronized()
{
    const uint8_t dashes[4] = {0x40, 0x40, 0x40, 0x40};
    showSegments(dashes);
}
}

void setupClockDisplay()
{
    pinMode(TM1637_CLK, OUTPUT);
    pinMode(TM1637_DIO, OUTPUT);
    digitalWrite(TM1637_CLK, HIGH);
    digitalWrite(TM1637_DIO, HIGH);

    showUnsynchronized();
}

void startNtpSync()
{
    configTzTime(NTP_TIMEZONE, "pool.ntp.org", "time.google.com");
}

void updateClockDisplay()
{
    const uint32_t nowMs = millis();
    if (nowMs - lastDisplayUpdateMs < 1000)
    {
        return;
    }
    lastDisplayUpdateMs = nowMs;

    const time_t now = time(nullptr);
    if (now < 1700000000)
    {
        showUnsynchronized();
        return;
    }

    struct tm localTime;
    if (localtime_r(&now, &localTime) == nullptr)
    {
        showUnsynchronized();
        return;
    }

    uint8_t segments[4] = {
        digitSegments[localTime.tm_hour / 10],
        digitSegments[localTime.tm_hour % 10],
        digitSegments[localTime.tm_min / 10],
        digitSegments[localTime.tm_min % 10]};
    if ((localTime.tm_sec % 2) == 0)
    {
        segments[1] |= 0x80;
    }
    showSegments(segments);
}
