package com.kisak.cod.util

import android.app.ActivityManager
import android.content.Context
import android.content.pm.PackageManager
import android.opengl.GLES10
import androidx.core.content.getSystemService

/**
 * Device capability probe.
 *
 * Why this exists: DXVK's Vulkan requirements are the single biggest
 * compatibility risk in this port. Upstream DXVK 2.6+ needs Vulkan 1.3, which
 * many Android GPU drivers — especially the stock Qualcomm proprietary Adreno
 * drivers — do not advertise. If DXVK is handed a device it cannot use, the
 * engine dies at device creation with no useful message.
 *
 * The launcher therefore probes before launching and:
 *   - refuses to start with an actionable message when Vulkan is unusable,
 *   - otherwise picks a conservative resolution scale and frame limit so the
 *     first run is playable instead of a 4fps slideshow.
 *
 * NOTE: this Kotlin-side probe is a *pre-filter*. The authoritative check is
 * the native vkEnumeratePhysicalDevices probe (see DeviceTier.nativeProbe in
 * the JNI layer), which also reports the optional extensions DXVK needs.
 */
object DeviceTier {

    enum class GpuVendor { ADRENO, MALI, POWERVR, INTEL, NVIDIA, UNKNOWN }

    enum class Level(val label: String) {
        BLOCKED("Vulkan unavailable"),
        MINIMAL("Minimal"),
        LOW("Low"),
        MEDIUM("Medium"),
        HIGH("High"),
    }

    data class Report(
        val gpuRenderer: String,
        val vendor: GpuVendor,
        val vulkanVersion: Int,      // packed VK_MAKE_VERSION
        val vulkanVersionString: String,
        val deviceMemoryMb: Int,
        val cpuCores: Int,
        val level: Level,
        val recommendedResScale: Float,
        val recommendedFrameLimit: Int,
        val warnings: List<String>,
        val blockingReason: String?,
    ) {
        val isUsable: Boolean get() = blockingReason == null
    }

    // Vulkan version packing: (major shl 22) or (minor shl 12) or patch
    private const val VK_1_0 = 0x400000
    private const val VK_1_1 = 0x401000
    private const val VK_1_2 = 0x402000
    private const val VK_1_3 = 0x403000

    fun probe(context: Context): Report {
        val renderer = runCatching { GLES10.glGetString(GLES10.GL_RENDERER).orEmpty() }
            .getOrDefault("")
        val vendor = detectVendor(renderer)

        val vulkanVersion = highestVulkanVersion(context.packageManager)
        val memMb = memoryMb(context)
        val cores = Runtime.getRuntime().availableProcessors()

        val warnings = mutableListOf<String>()
        var blocking: String? = null

        if (vulkanVersion <= 0) {
            blocking = "This device does not expose Vulkan. DXVK renders through Vulkan, " +
                "so the game cannot start here."
        } else if (vulkanVersion < VK_1_1) {
            blocking = "Vulkan 1.0 only. DXVK needs at least Vulkan 1.1. " +
                "Update your GPU driver or use a device with newer drivers."
        } else {
            if (vulkanVersion < VK_1_2) {
                warnings += "Vulkan $vulkanVersionString detected. This port must be built " +
                    "against a DXVK revision that only requires Vulkan 1.1 " +
                    "(DXVK 2.6+ requires 1.3 and will fail to create a device)."
            }
            if (vendor == GpuVendor.MALI || vendor == GpuVendor.POWERVR) {
                warnings += "Non-Adreno GPU detected ($renderer). DXVK is exercised far less " +
                    "on these drivers; expect to lower the resolution and disable effects."
            }
            if (memMb <= 3000) {
                warnings += "Only ${memMb}MB of app memory available. COD4 was tuned for " +
                    "desktop memory budgets; the hunk allocator may need reducing."
            }
        }

        val level = when {
            blocking != null -> Level.BLOCKED
            vulkanVersion >= VK_1_3 && memMb >= 6000 && cores >= 8 -> Level.HIGH
            vulkanVersion >= VK_1_1 && memMb >= 5000 && cores >= 6 -> Level.MEDIUM
            vulkanVersion >= VK_1_1 && memMb >= 3500 -> Level.LOW
            else -> Level.MINIMAL
        }

        val (resScale, frameLimit) = when (level) {
            Level.BLOCKED -> 0.5f to 30
            Level.MINIMAL -> 0.50f to 30
            Level.LOW -> 0.60f to 30
            Level.MEDIUM -> 0.80f to 60
            Level.HIGH -> 1.0f to 60
        }

        return Report(
            gpuRenderer = renderer,
            vendor = vendor,
            vulkanVersion = vulkanVersion,
            vulkanVersionString = vulkanVersionToString(vulkanVersion),
            deviceMemoryMb = memMb,
            cpuCores = cores,
            level = level,
            recommendedResScale = resScale,
            recommendedFrameLimit = frameLimit,
            warnings = warnings,
            blockingReason = blocking,
        )
    }

    private fun highestVulkanVersion(pm: PackageManager): Int {
        if (!pm.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_LEVEL)) return 0

        // hasSystemFeature(name, version) reports whether the device advertises
        // that version OR NEWER, so walking downwards and returning the first
        // match yields the highest version the device actually has.
        for (v in intArrayOf(VK_1_3, VK_1_2, VK_1_1, VK_1_0)) {
            if (pm.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_VERSION, v)) return v
        }
        return VK_1_0
    }

    private fun detectVendor(renderer: String): GpuVendor {
        val r = renderer.lowercase()
        return when {
            r.contains("adreno") -> GpuVendor.ADRENO
            r.contains("mali") -> GpuVendor.MALI
            r.contains("powervr") || r.contains("videocore") -> GpuVendor.POWERVR
            r.contains("intel") -> GpuVendor.INTEL
            r.contains("nvidia") || r.contains("tegra") -> GpuVendor.NVIDIA
            else -> GpuVendor.UNKNOWN
        }
    }

    private fun memoryMb(context: Context): Int {
        val am = context.getSystemService<ActivityManager>() ?: return 0
        return try {
            val info = ActivityManager.MemoryInfo()
            am.getMemoryInfo(info)
            (info.totalMem / (1024 * 1024)).toInt()
        } catch (t: Throwable) {
            am.memoryClass
        }
    }

    private fun vulkanVersionToString(v: Int): String =
        if (v <= 0) "none" else "${(v shr 22) and 0x3FF}.${(v shr 12) and 0x3FF}.${v and 0xFFF}"
}
