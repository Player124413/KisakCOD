// win_steam.h -- Steamworks interface.
//
// Singleplayer does not need Steam: there is no auth ticket to validate, no
// dedicated server to check clients against, and no matchmaking. The whole
// interface compiles to no-ops so the client code that calls it keeps linking.
// win_steam.cpp returns "not initialized" from every query, which is what the
// engine already handles for a non-Steam build.

#pragma once

#include "win_local.h"

#include <cstdint>

void Steam_Init();
void Steam_Shutdown();

// Called by Client to get a Ticket to send to the Dedicated Server
bool Steam_UpdateClientAuthTicket(netadr_t serverIpv4);

bool Steam_GetRawClientTicket(unsigned char **pBuffer, uint32 *pSize);

void Steam_CancelClientTicket();

uint64_t Steam_GetClientSteamID64();

// Called by DEDICATED to do an Initial Check on Client's ticket
bool Steam_CheckClientTicket(const void *pAuthTicket, uint32 authTicketLen, uint64_t steamID64);

// Called by DEDICATED periodically to ensure Steam hasn't sent us any new Clients to kick
void Steam_CheckClients();

void Steam_OnClientDropped(uint64_t steamID64);

void Steam_SV_AddTestCommands();

extern bool g_steamInitialized;
