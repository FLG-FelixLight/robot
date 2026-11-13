#include "robot_config.h"
#include "robot_state.h"
#include "destination_manager.h"
#include "motor_control.h"
#include "path_control.h"
#include "audio_control.h"
#include "web_control.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <string.h>

const char *modeName()
{
    switch (robotMode)
    {
        case MODE_AUTO:   return "auto";
        case MODE_MANUAL: return "manual";
        case MODE_PATH:   return "path";
        default:          return "stop";
    }
}

void sendConfig(AsyncWebSocketClient *client)
{
    String msg = "{\"type\":\"config\",";
    msg += "\"kp\":" + String(Kp, 3) + ",";
    msg += "\"ki\":" + String(Ki, 3) + ",";
    msg += "\"kd\":" + String(Kd, 3) + ",";
    msg += "\"base\":" + String(baseSpeed) + ",";
    msg += "\"max\":" + String(maxSpeed) + ",";
    msg += "\"on\":" + String(robotEnabled ? "true" : "false") + ",";
    msg += "\"mode\":\"" + String(modeName()) + "\",";
    msg += "\"dest\":" + String(gDestId) + ",";
    msg += "\"dir\":\"" + String(driveDir) + "\"}";

    if (client != nullptr)
    {
        client->text(msg);
    }
    else
    {
        ws.textAll(msg);
    }
}

void sendTelemetry()
{
    if (ws.count() == 0)
    {
        return;
    }

    String msg = "{\"type\":\"telemetry\",";
    msg += "\"s\":[" + String(gSensor[0]) + "," + String(gSensor[1]) + "," +
           String(gSensor[2]) + "," + String(gSensor[3]) + "," +
           String(gSensor[4]) + "],";
    msg += "\"err\":" + String(error, 2) + ",";
    msg += "\"pid\":" + String(gCorrection, 2) + ",";
    msg += "\"l\":" + String(gLeftSpeed) + ",";
    msg += "\"r\":" + String(gRightSpeed) + ",";
    msg += "\"full\":" + String(gFullLine ? "true" : "false") + ",";
    msg += "\"on\":" + String(robotEnabled ? "true" : "false") + ",";
    msg += "\"mode\":\"" + String(modeName()) + "\",";
    msg += "\"dir\":\"" + String(driveDir) + "\",";
    msg += "\"dest\":" + String(gDestId) + ",";
    msg += "\"step\":" + String((int)gPathIdx) + ",";
    msg += "\"notice\":\"" + String(gNotice) + "\",";
    msg += "\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
    msg += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("0.0.0.0")) + "\"}";

    ws.textAll(msg);
}

// ======================================================
// 20. PATH / JUNCTION
// ======================================================

void applyCommand(const Command &c)
{
    switch (c.type)
    {
        case CMD_START:
            resetPidState();
            robotEnabled = true;
            robotMode = MODE_AUTO;
            strlcpy(driveDir, "stop", sizeof(driveDir));

            gPath = "";
            gPathIdx = 0;
            gDestId = 0;
            gTurn = 0;
            gTurnPause = false;
            gJunctionArmed = false;
            gCenteredSince = 0;

            setNotice("AUTO start");
            Serial.println("[WEB] START (AUTO)");
            break;

        case CMD_GOTO:
            resetPidState();
            robotEnabled = true;
            robotMode = MODE_PATH;
            strlcpy(driveDir, "stop", sizeof(driveDir));

            gPath = c.text;
            gPathIdx = 0;
            gDestId = c.value;
            gTurn = 0;
            gTurnPause = false;
            gJunctionArmed = false;
            gCenteredSince = 0;

            {
                char b[96];
                snprintf(b, sizeof(b), "PATH dest=%d code=%s",
                         gDestId, gPath.c_str());
                setNotice(b);
                Serial.printf("[WEB] GOTO %s\n", b);
            }
            break;

        case CMD_STOP:
        case CMD_ESTOP:
            robotEnabled = false;
            robotMode = MODE_STOP;
            strlcpy(driveDir, "stop", sizeof(driveDir));
            gTurn = 0;
            gTurnPause = false;
            gPath = "";
            gPathIdx = 0;
            stopMotors();
            gLeftSpeed = 0;
            gRightSpeed = 0;
            setNotice(c.type == CMD_ESTOP ? "E-STOP" : "STOP");
            Serial.println(c.type == CMD_ESTOP ? "[WEB] E-STOP" : "[WEB] STOP");
            break;

        case CMD_DRIVE:
            strlcpy(driveDir, c.text, sizeof(driveDir));

            if (strcmp(driveDir, "stop") == 0)
            {
                robotEnabled = false;
                robotMode = MODE_STOP;
                stopMotors();
                gLeftSpeed = 0;
                gRightSpeed = 0;
            }
            else
            {
                robotEnabled = true;
                robotMode = MODE_MANUAL;
                lastDriveCmd = millis();
            }

            Serial.printf("[WEB] DRIVE %s\n", driveDir);
            break;

        case CMD_CONFIG:
            Kp = c.kp;
            Ki = c.ki;
            Kd = c.kd;
            baseSpeed = c.base;

            Serial.printf("[WEB] PID Kp=%.3f Ki=%.3f Kd=%.3f base=%d\n",
                          Kp, Ki, Kd, baseSpeed);
            break;

        case CMD_PLAY:
            strlcpy(gAudioFile, c.text, sizeof(gAudioFile));
            gAudioNew = true;
            Serial.printf("[WEB] PLAY %s\n", gAudioFile);
            break;

        default:
            break;
    }

    sendConfig(nullptr);
}

// ======================================================
// 22. HANDLE COMMAND FROM WEB
// ======================================================

void handleCommand(const String &msg)
{
    JsonDocument doc;

    if (deserializeJson(doc, msg))
    {
        return;
    }

    const char *cmd = doc["cmd"] | "";
    Command c;
    memset(&c, 0, sizeof(c));

    if (strcmp(cmd, "start") == 0)
    {
        c.type = CMD_START;
    }
    else if (strcmp(cmd, "stop") == 0)
    {
        c.type = CMD_STOP;
    }
    else if (strcmp(cmd, "estop") == 0)
    {
        c.type = CMD_ESTOP;
    }
    else if (strcmp(cmd, "drive") == 0)
    {
        c.type = CMD_DRIVE;
        strlcpy(c.text, doc["dir"] | "stop", sizeof(c.text));
    }
    else if (strcmp(cmd, "goto") == 0)
    {
        int id = doc["id"] | 0;

        std::map<int, String>::iterator it = gTables.find(id);

        if (it == gTables.end())
        {
            char b[80];
            snprintf(b, sizeof(b), "Khong co ma diem den %d", id);
            setNotice(b);
            sendConfig(nullptr);
            return;
        }

        c.type = CMD_GOTO;
        c.value = id;
        strlcpy(c.text, it->second.c_str(), sizeof(c.text));
    }
    else if (strcmp(cmd, "play") == 0)
    {
        c.type = CMD_PLAY;
        strlcpy(c.text, doc["file"] | "welcome.mp3", sizeof(c.text));
    }
    else if (strcmp(cmd, "config") == 0)
    {
        c.type = CMD_CONFIG;
        c.kp = doc["kp"] | Kp;
        c.ki = doc["ki"] | Ki;
        c.kd = doc["kd"] | Kd;
        c.base = doc["base"] | baseSpeed;
    }
    else if (strcmp(cmd, "tables_reload") == 0)
    {
        loadTables();
        setNotice("Tables reloaded");
        sendConfig(nullptr);
        return;
    }

    if (c.type != CMD_NONE && gCmdQueue != nullptr)
    {
        xQueueSend(gCmdQueue, &c, 0);
    }
}

void onWsEvent(
    AsyncWebSocket *server,
    AsyncWebSocketClient *client,
    AwsEventType type,
    void *arg,
    uint8_t *data,
    size_t len)
{
    (void)server;

    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("[WS] Client #%u connected\n", client->id());
        sendConfig(client);
        sendTelemetry();
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        if (info->final &&
            info->index == 0 &&
            info->len == len &&
            info->opcode == WS_TEXT)
        {
            data[len] = 0;
            handleCommand(String((char *)data));
        }
    }
}

// ======================================================
// 23. AUDIO TASK
// ======================================================

void setupWebServer()
{
// WEBSOCKET
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // REST API (dang ky truoc serveStatic)
    server.on("/api/tables", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", tablesJson()); });

    server.on(
        "/api/tables", HTTP_PUT,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            static String body;

            if (index == 0)
            {
                body = "";
            }

            if (body.length() + len < 8192)
            {
                body.concat((const char *)data, len);
            }

            if (index + len >= total)
            {
                String err;

                if (saveTablesFromJson(body, err))
                {
                    request->send(200, "application/json", "{\"ok\":true}");
                }
                else
                {
                    String out = "{\"ok\":false,\"error\":\"" + err + "\"}";
                    request->send(400, "application/json", out);
                }
            }
        });

    // STATIC FILES
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(404, "text/plain", "Not found"); });

    server.begin();

    Serial.println("[HTTP] Server started on port 80");

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("[HTTP] Mo trinh duyet: http://");
        Serial.println(WiFi.localIP());
    }

    Serial.println();
}
