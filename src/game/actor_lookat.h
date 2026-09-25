#pragma once

#ifndef KISAK_SP 
#error This file is for SinglePlayer only 
#endif

#include "actor.h"

void __cdecl Actor_InitLookAt(actor_s *self);
void __cdecl Actor_SetLookAtAnimNodes(
    actor_s *self,
    uint16_t animStraight,
    uint16_t animLeft,
    uint16_t animRight);
void __cdecl Actor_SetLookAt(actor_s *self, float *vPosition, double fTurnAccel);
float __cdecl Actor_CurrentLookAtAnimYawMax(actor_s *self);
float __cdecl Actor_CurrentLookAtYawMax(actor_s *self);
void __cdecl Actor_SetLookAtYawLimits(actor_s *self, double fAnimYawLimit, double fYawLimit, double fBlendTime);
void __cdecl Actor_StopLookAt(actor_s *self, double fTurnAccel);
void __cdecl Actor_UpdateLookAt(actor_s *self);
