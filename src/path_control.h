#pragma once

void arrived(const char *why);
void handleJunction(bool leftOn, bool rightOn);
bool updateTurn(bool centerOn, int active);
void updateDestinationHold();
