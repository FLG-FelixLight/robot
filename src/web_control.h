#pragma once

#include <ESPAsyncWebServer.h>
#include "robot_state.h"

void setupWebServer();
const char *modeName();
void sendConfig(AsyncWebSocketClient *client);
void sendTelemetry();
void sendEventLog(const char *message);
void applyCommand(const Command &c);
