// win_steam.cpp -- Steamworks, stubbed for singleplayer.
//
// Every Steam entry point the engine calls is here, returning the "not
// available" answer. That is not laziness: singleplayer has no auth ticket to
// validate, no dedicated server to check clients against, and no matchmaking.
// The engine already has a non-Steam code path (g_steamInitialized stays false)
// and these stubs are what let it take that path without a link error.
//
// If multiplayer is ever wanted, this is the file that grows a real
// steamclient.so binding.

#include <universal/q_shared.h>
#include "win_local.h"
#include "win_steam.h"

bool g_steamInitialized = false;

void Steam_Init()
{
    // Steam_Init runs unconditionally at startup. Logging the reason keeps the
    // console honest instead of leaving a silent gap where the ticket flow
    // would be.
    Com_Printf(CON_CHANNEL_SYSTEM, "Steam is not available (singleplayer build)\n");
    g_steamInitialized = false;
}

void Steam_Shutdown()
{
    g_steamInitialized = false;
}

bool Steam_UpdateClientAuthTicket(netadr_t serverIpv4)
{
    (void)serverIpv4;
    return false;
}

bool Steam_GetRawClientTicket(unsigned char **pBuffer, uint32 *pSize)
{
    if (pBuffer) *pBuffer = nullptr;
    if (pSize) *pSize = 0;
    return false;
}

void Steam_CancelClientTicket() {}

uint64_t Steam_GetClientSteamID64()
{
    return 0;
}

bool Steam_CheckClientTicket(const void *pAuthTicket, uint32 authTicketLen, uint64_t steamID64)
{
    (void)pAuthTicket; (void)authTicketLen; (void)steamID64;
    return false;
}

void Steam_CheckClients() {}

void Steam_OnClientDropped(uint64_t steamID64)
{
    (void)steamID64;
}

void Steam_SV_AddTestCommands() {}
