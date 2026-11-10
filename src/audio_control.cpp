#include "robot_config.h"
#include "robot_state.h"
#include "motor_control.h"
#include "audio_control.h"

#include <LittleFS.h>
#include "AudioFileSourceFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

void setupAudio()
{
    gAudioOut = new AudioOutputI2S();
    gAudioOut->SetPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    gAudioOut->SetGain(0.30);

    xTaskCreatePinnedToCore(audioTask, "audio", 8192, NULL, 1, NULL, 0);
}

void audioTask(void *param)
{
    (void)param;

    for (;;)
    {
        if (gAudioNew)
        {
            gAudioNew = false;

            String req = String(gAudioFile);
            String path = req.startsWith("/") ? req : ("/audio/" + req);

            Serial.printf("[AUDIO] request %s\n", path.c_str());

            if (LittleFS.exists(path))
            {
                if (gMp3)
                {
                    gMp3->stop();
                    delete gMp3;
                    gMp3 = nullptr;
                }

                if (gMp3File)
                {
                    delete gMp3File;
                    gMp3File = nullptr;
                }

                gMp3File = new AudioFileSourceFS(LittleFS, path.c_str());
                gMp3 = new AudioGeneratorMP3();

                if (!gMp3->begin(gMp3File, gAudioOut))
                {
                    setNotice("Audio begin FAILED");
                    Serial.println("[AUDIO] begin failed");
                }
                else
                {
                    setNotice("Playing " + req);
                }
            }
            else
            {
                setNotice("Audio not found: " + path);
                Serial.printf("[AUDIO] not found %s\n", path.c_str());
            }
        }

        if (gMp3 && gMp3->isRunning())
        {
            if (!gMp3->loop())
            {
                gMp3->stop();
                delete gMp3;
                gMp3 = nullptr;

                if (gMp3File)
                {
                    delete gMp3File;
                    gMp3File = nullptr;
                }

                setNotice("Audio done");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// ======================================================
// 24. WIFI CONNECT
// ======================================================
