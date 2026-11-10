#pragma once

#include <Arduino.h>

bool pathValid(const String &p, String &err);
String tablesJson();
bool loadTables();
bool saveTablesFromJson(const String &body, String &err);
