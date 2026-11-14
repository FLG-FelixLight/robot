#pragma once

void arrived(const char *why);
void beginJunctionPause(bool leftOn, bool rightOn);
void handleJunction(bool leftOn, bool rightOn);
void updateTurn();
void updateDestinationHold();
