#include "robot_config.h"
#include "robot_state.h"
#include "destination_manager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <limits.h>
#include <stdlib.h>

bool pathValid(const String &p, String &err)
{
    if (p.length() == 0)
    {
        err = "empty code";
        return false;
    }

    if (p.length() > 90)
    {
        err = "code too long";
        return false;
    }

    if (p[p.length() - 1] != '0')
    {
        err = "code must end with 0";
        return false;
    }

    for (size_t i = 0; i + 1 < p.length(); i++)
    {
        char c = p[i];

        if (c == '0')
        {
            err = "0 only allowed at the end";
            return false;
        }

        if (c != 'L' && c != 'R' && c != 'S')
        {
            err = "only L R S 0 allowed";
            return false;
        }
    }

    return true;
}

String tablesJson()
{
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();

    for (std::map<int, String>::iterator it = gTables.begin();
         it != gTables.end(); ++it)
    {
        obj[String(it->first)] = it->second;
    }

    String out;
    serializeJson(doc, out);
    return out;
}

bool loadTables()
{
    if (!LittleFS.exists("/tables.json"))
    {
        Serial.println("[TABLES] /tables.json not found");
        return false;
    }

    File f = LittleFS.open("/tables.json", "r");

    if (!f)
    {
        Serial.println("[TABLES] open failed");
        return false;
    }

    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, f);
    f.close();

    if (e)
    {
        Serial.printf("[TABLES] parse error: %s\n", e.c_str());
        return false;
    }

    std::map<int, String> tmp;

    for (JsonPair kv : doc.as<JsonObject>())
    {
        int id = atoi(kv.key().c_str());

        if (id <= 0)
        {
            continue;
        }

        String p = kv.value().as<String>();
        String err;

        if (!pathValid(p, err))
        {
            Serial.printf("[TABLES] skip %d: %s\n", id, err.c_str());
            continue;
        }

        tmp[id] = p;
    }

    gTables = tmp;

    Serial.printf("[TABLES] loaded %u destination(s)\n",
                  (unsigned)gTables.size());

    return true;
}

bool saveTablesFromJson(const String &body, String &err)
{
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, body);

    if (e)
    {
        err = e.c_str();
        return false;
    }

    if (!doc.is<JsonObject>())
    {
        err = "root must be object";
        return false;
    }

    std::map<int, String> tmp;

    for (JsonPair kv : doc.as<JsonObject>())
    {
        const char *idText = kv.key().c_str();
        char *idEnd = nullptr;
        long parsedId = strtol(idText, &idEnd, 10);

        if (idEnd == idText || *idEnd != '\0' || parsedId <= 0 || parsedId > INT_MAX)
        {
            err = "id must be an integer from 1 to 2147483647";
            return false;
        }

        int id = static_cast<int>(parsedId);

        if (tmp.find(id) != tmp.end())
        {
            err = "duplicate destination id";
            return false;
        }

        if (!kv.value().is<const char *>())
        {
            err = "code must be string";
            return false;
        }

        String p = kv.value().as<String>();

        if (!pathValid(p, err))
        {
            return false;
        }

        tmp[id] = p;
    }

    JsonDocument out;
    JsonObject obj = out.to<JsonObject>();

    for (std::map<int, String>::iterator it = tmp.begin();
         it != tmp.end(); ++it)
    {
        obj[String(it->first)] = it->second;
    }

    File f = LittleFS.open("/tables.json", "w");

    if (!f)
    {
        err = "cannot write tables.json";
        return false;
    }

    serializeJson(out, f);
    f.close();

    gTables = tmp;

    Serial.printf("[TABLES] saved %u destination(s)\n",
                  (unsigned)gTables.size());

    return true;
}

// ======================================================
// 19. WEBSOCKET HELPERS
// ======================================================
