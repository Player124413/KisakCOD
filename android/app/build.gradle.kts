plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.kisakcod.android"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.kisakcod.android"
        minSdk = 24          // Android 7.0 — covers ~99% of devices in use
        targetSdk = 34
        versionCode = 4
        versionName = "0.1.0-M1"

        ndk {
            // ARMv7 for old phones/tablets, arm64 for everything modern.
            // x86_64 is kept for emulators during development.
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17", "-Wall", "-Wno-unused-parameter")
                arguments += listOf("-DANDROID_STL=c++_shared")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/jni/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // Small app (no third-party deps), minify adds risk for zero gain here.
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"))
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true // keeps .so uncompressed for direct use by the engine
        }
    }
}

// No external dependencies on purpose: builds offline with just the SDK.
dependencies {}