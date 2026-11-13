#pragma once

#include <Arduino.h>

void arrived(const char *why);
void handleJunction(bool leftOn, bool rightOn);
bool updatePathAction();
void adjustPathActionClock(uint32_t pauseDuration);
void cancelPathAction();
void updateDestinationHold();
