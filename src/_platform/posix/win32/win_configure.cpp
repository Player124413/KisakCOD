// win_configure.cpp -- CPU/GPU detection for the POSIX/Android build.
//
// The Win32 build walks the registry for the video card's description string
// and uses CPUID for the vendor and model. CPUID still exists on x86 (and is
// what __cpuid in msvc_compat.h wraps), but there is no registry and, on ARM,
// no CPUID at all -- so the CPU strings come from /proc/cpuinfo and the GPU
// string is left for the host to fill in once DXVK has a Vulkan device.
//
// This matters more than it looks: Sys_FindInfo runs before the renderer is
// created, and sys_info.gpuDescription is what the engine prints and what the
// auto-configure path reads to pick a default quality level.

#include <universal/q_shared.h>
#include "win_configure.h"
#include "win_local.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// CPU
// ---------------------------------------------------------------------------

static bool Kisak_ReadCpuinfoField(const char *key, char *out, int outSize)
{
    out[0] = 0;
    FILE *f = fopen("/proc/cpuinfo", "rb");
    if (!f) return false;

    char line[1024];
    size_t keyLen = strlen(key);
    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, key, keyLen) != 0) continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        ++colon;
        while (*colon == ' ' || *colon == '\t') ++colon;
        char *nl = strpbrk(colon, "\r\n");
        if (nl) *nl = 0;
        I_strncpyz(out, colon, outSize);
        fclose(f);
        return true;
    }
    fclose(f);
    return false;
}

void Sys_DetectCpuVendorAndName(char *vendor, char *name)
{
    if (!vendor || !name) return;

    vendor[0] = 0;
    name[0] = 0;

#if defined(__i386__) || defined(__x86_64__)
    // Real CPUID on x86. The engine's SysInfo.cpuVendor is 13 bytes and
    // cpuName is 49, which is exactly what CPUID gives (12 chars + NUL, and
    // the 48-char brand string + NUL).
    int regs[4];
    __cpuid(regs, 0);
    memcpy(vendor + 0, &regs[1], 4);
    memcpy(vendor + 4, &regs[3], 4);
    memcpy(vendor + 8, &regs[2], 4);
    vendor[12] = 0;

    __cpuid(regs, 0x80000000);
    if ((unsigned)regs[0] >= 0x80000004u)
    {
        char brand[49];
        __cpuid(regs, 0x80000002); memcpy(brand + 0,  regs, 16);
        __cpuid(regs, 0x80000003); memcpy(brand + 16, regs, 16);
        __cpuid(regs, 0x80000004); memcpy(brand + 32, regs, 16);
        brand[48] = 0;
        // CPUID pads the brand string with leading spaces on some parts.
        const char *b = brand;
        while (*b == ' ') ++b;
        I_strncpyz(name, b, 49);
    }
    else
    {
        Kisak_ReadCpuinfoField("model name", name, 49);
    }
#else
    // No CPUID on ARM. /proc/cpuinfo has both fields.
    Kisak_ReadCpuinfoField("vendor_id", vendor, 13);
    Kisak_ReadCpuinfoField("Hardware", name, 49);
    if (!name[0])
        Kisak_ReadCpuinfoField("model name", name, 49);
    if (!name[0])
        Kisak_ReadCpuinfoField("Processor", name, 49);
#endif

    if (!vendor[0]) I_strncpyz(vendor, "Unknown", 13);
    if (!name[0])   I_strncpyz(name, "Unknown", 49);
}

uint32_t __cdecl Sys_GetPhysicalCpuCount()
{
    // Count distinct physical ids in /proc/cpuinfo. On big.LITTLE that is what
    // distinguishes the two clusters, which is the number the engine actually
    // wants for its worker-thread heuristics.
    FILE *f = fopen("/proc/cpuinfo", "rb");
    if (!f)
        return 1;

    char line[512];
    int physicalId = -1;
    int coreId = -1;
    int ids[64];
    int nids = 0;

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "physical id", 11) == 0)
        {
            char *colon = strchr(line, ':');
            if (colon) physicalId = atoi(colon + 1);
        }
        else if (strncmp(line, "core id", 7) == 0)
        {
            char *colon = strchr(line, ':');
            if (colon) coreId = atoi(colon + 1);
        }
        else if (line[0] == '\n')
        {
            if (physicalId >= 0 && coreId >= 0)
            {
                int key = physicalId * 1000 + coreId;
                bool seen = false;
                for (int i = 0; i < nids; ++i)
                    if (ids[i] == key) { seen = true; break; }
                if (!seen && nids < 64) ids[nids++] = key;
            }
            physicalId = -1;
            coreId = -1;
        }
    }
    fclose(f);

    return nids > 0 ? (uint32_t)nids : 1;
}

int __cdecl Sys_AddApicIdIfUnique(int apicId, int *ids, int *count)
{
    for (int i = 0; i < *count; ++i)
        if (ids[i] == apicId) return 0;
    ids[(*count)++] = apicId;
    return 1;
}

// ---------------------------------------------------------------------------
// GPU
//
// The Win32 build reads the driver's description string out of the registry.
// There is no registry, and at the time this runs the Vulkan device does not
// exist yet -- the engine calls Sys_FindInfo before R_CreateDeviceInternal.
// So the host supplies the string once it has one (KISAK_GPU_DESCRIPTION, set
// by the Android bridge from VkPhysicalDeviceProperties.deviceName), and
// until then the engine is told so rather than being handed a guess.
// ---------------------------------------------------------------------------

void __cdecl Sys_DetectVideoCard(int bufSize, char *out)
{
    if (!out || bufSize <= 0) return;
    const char *gpu = getenv("KISAK_GPU_DESCRIPTION");
    if (gpu && gpu[0])
        I_strncpyz(out, gpu, bufSize);
    else
        I_strncpyz(out, "Detecting...", bufSize);
}

// ---------------------------------------------------------------------------
// Speed estimation
//
// The Win32 build measures the CPU by timing a fixed instruction mix against
// the RDTSC counter. __rdtsc is available in msvc_compat.h on x86; on ARM the
// generic implementation falls back to clock_gettime, which is coarse but
// monotonic. The result only feeds the engine's "estimated GHz" printout and
// the auto-configure heuristic, so a rough number is acceptable -- but a wrong
// one is not, because it would pick the wrong default quality.
// ---------------------------------------------------------------------------

static double Kisak_BenchmarkGHz()
{
    const int iterations = 20000000;
    uint64_t start = __rdtsc();

    volatile double acc = 0.0;
    for (int i = 1; i <= iterations; ++i)
        acc = acc + (double)i * 0.5;

    uint64_t end = __rdtsc();
    (void)acc;

    // How long did that actually take? Time it with the monotonic clock, then
    // divide the tick count by the elapsed seconds.
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    start = __rdtsc();
    for (int i = 1; i <= iterations; ++i)
        acc = acc + (double)i * 0.5;
    end = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    (void)acc;

    double elapsed = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    if (elapsed <= 0.0) return 1.0;

    return (double)(end - start) / elapsed / 1e9;
}

double Sys_BenchmarkGHz(void)
{
    static double cached = -1.0;
    if (cached > 0.0) return cached;

    double best = 0.0;
    for (int i = 0; i < 3; ++i)
    {
        double ghz = Kisak_BenchmarkGHz();
        if (ghz > best) best = ghz;
    }
    // A sane floor: reporting 0 GHz would make the engine's auto-configure
    // pick the lowest quality permanently.
    if (best < 0.1) best = 0.1;
    cached = best;
    return best;
}

void __cdecl Sys_SetAutoConfigureGHz(SysInfo *info)
{
    if (!info) return;
    info->configureGHz = Sys_BenchmarkGHz();
}
