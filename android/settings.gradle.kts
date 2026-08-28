// KisakCOD Android Port — project settings.
// The port is intentionally dependency-free on the Kotlin/Java side:
// everything uses plain android.* platform APIs, so the app builds with
// just the Android SDK (no network dependency for Maven artifacts).
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = "KisakCOD-Android"
include(":app")