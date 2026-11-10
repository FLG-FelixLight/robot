#include "robot_config.h"
#include "robot_state.h"
#include "network.h"

#include <WiFi.h>

const char *wifiStatusName(wl_status_t s)
{
    switch (s)
    {
        case WL_NO_SSID_AVAIL: return "NO_SSID (khong thay ten wifi -> sai ten / song 5GHz / hotspot tat)";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED (sai mat khau / bao mat WPA3 / hotspot day)";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST (rot mang giua chung)";
        case WL_DISCONNECTED: return "DISCONNECTED";
        case WL_IDLE_STATUS: return "IDLE";
        default: return "?";
    }
}

void connectWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.disconnect(true);
    delay(200);

    Serial.println("\n[WIFI] Dang quet song...");

    int n = WiFi.scanNetworks();

    Serial.printf("[WIFI] Thay %d mang:\n", n);

    bool found = false;

    for (int i = 0; i < n; i++)
    {
        String ssid = WiFi.SSID(i);

        Serial.printf(
            "  - \"%s\"  RSSI=%d  ch=%d%s\n",
            ssid.c_str(),
            WiFi.RSSI(i),
            WiFi.channel(i),
            ssid == WIFI_SSID ? "  <== MUC TIEU" : "");

        if (ssid == WIFI_SSID)
        {
            found = true;
        }
    }

    if (!found)
    {
        Serial.printf(
            "[WIFI] KHONG thay \"%s\" -> kiem tra: hotspot da bat? band 2.4GHz? sai ten?\n",
            WIFI_SSID);
        return;
    }

    Serial.printf("\n[WIFI] Connecting to \"%s\" ...\n", WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
    {
        delay(300);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("[WIFI] CONNECTED");
        Serial.print("[WIFI] IP   : ");
        Serial.println(WiFi.localIP());
        Serial.print("[WIFI] RSSI : ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    }
    else
    {
        Serial.printf("[WIFI] FAILED - ly do: %s\n", wifiStatusName(WiFi.status()));
        Serial.println("[WIFI] Se thu lai trong loop");
    }
}

// ======================================================
// 25. SETUP
// ======================================================
