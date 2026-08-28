#pragma once


struct gentity_s;

struct EntHandleInfo // sizeof=0x8 // (SP/MP same)
{                                       // ...
    void *handle;
    uint16_t next;              // ...
    uint16_t prev;              // ...
};
#if !defined(__LP64__) && !defined(_WIN64)
static_assert(sizeof(EntHandleInfo) == 0x8);
#endif

struct EntHandleList // sizeof=0x2 // (SP/MP same)
{                                       // ...
    uint16_t infoIndex;
};
#if !defined(__LP64__) && !defined(_WIN64)
static_assert(sizeof(EntHandleList) == 0x2);
#endif

struct EntHandle // sizeof=0x4 // (SP/MP same)
{                                       // ...
    uint16_t number;
    uint16_t infoIndex;

    void setEnt(gentity_s *ent);
    gentity_s *ent() const;
    int32_t entnum();
    bool isDefined() const;

    static void Init();
    static void Shutdown();
};
#if !defined(__LP64__) && !defined(_WIN64)
static_assert(sizeof(EntHandle) == 0x4);
#endif

void __cdecl EntHandleDissociate(gentity_s *ent);
void __cdecl EntHandleDissociateInternal(EntHandleList *entHandleList);
void __cdecl RemoveEntHandleInfo(EntHandleList *entHandleList, uint32_t oldInfoIndex);
uint32_t __cdecl AddEntHandleInfo(EntHandleList *entHandleList, void *handle);


#ifdef KISAK_SP
struct sentient_s;

#define MAX_SENTIENTS 33

struct SentientHandle
{
    uint16_t number;
    uint16_t infoIndex;

    sentient_s *sentient() const;
    void setSentient(sentient_s *sentient);

    bool isDefined() const;

    static void Init();
};

void SentientHandleDissociate(sentient_s *sentient);
#endif