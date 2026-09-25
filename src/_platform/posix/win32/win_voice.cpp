// win_voice.cpp -- voice chat, stubbed for singleplayer.
//
// The Win32 build records from a DirectSound capture buffer, encodes with
// speex, and ships the packets over the net channel. None of that applies to a
// singleplayer build: there is nobody to talk to, and the client's voice UI is
// never reachable.
//
// The whole Voice_* surface is kept because cl_voice.cpp calls it
// unconditionally, and every entry point returns the "nothing to do" answer.
// Voice_IsClientTalking returns false for every client, which is what makes the
// HUD's talking indicators stay dark.

#include <universal/q_shared.h>
#include "win_local.h"
#include "win_storage.h"

#include <string.h>

#define VOICE_SAMPLE_RATE 8000
#define VOICE_FRAME_SAMPLES 320

static bool s_voiceInitialized = false;
static bool s_recording = false;
static double s_voiceLevel = 0.0;
static uint8_t s_localVoiceData[VOICE_FRAME_SAMPLES];

bool __cdecl Voice_Init()
{
    // There is no capture device to open and no peer to send to. Reporting
    // failure keeps the client on its non-voice path, which is the correct
    // singleplayer behaviour.
    Com_Printf(CON_CHANNEL_SYSTEM, "Voice chat unavailable (singleplayer build)\n");
    s_voiceInitialized = false;
    return false;
}

void __cdecl Voice_Shutdown()
{
    s_voiceInitialized = false;
    s_recording = false;
    s_voiceLevel = 0.0;
}

char __cdecl Voice_StartRecording()
{
    s_recording = true;
    return 1;
}

char __cdecl Voice_StopRecording()
{
    s_recording = false;
    s_voiceLevel = 0.0;
    return 1;
}

int __cdecl Voice_GetLocalVoiceData()
{
    if (!s_recording)
        return 0;
    memset(s_localVoiceData, 0, sizeof(s_localVoiceData));
    return VOICE_FRAME_SAMPLES;
}

bool __cdecl Voice_SendVoiceData()
{
    return false;
}

void __cdecl Voice_Playback() {}

double __cdecl Voice_GetVoiceLevel()
{
    return s_voiceLevel;
}

void __cdecl Voice_IncomingVoiceData(uint8_t talker, uint8_t *data, int packetDataSize)
{
    (void)talker; (void)data; (void)packetDataSize;
}

bool __cdecl Voice_IsClientTalking(uint32_t clientNum)
{
    (void)clientNum;
    return false;
}
