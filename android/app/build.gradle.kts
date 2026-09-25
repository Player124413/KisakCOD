plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.kisak.cod"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.kisak.cod"
        minSdk = 26          // Vulkan device support + scoped storage behaviour
        targetSdk = 34
        versionCode = 1
        versionName = "0.1.0-android-wip"

        // 32-bit ARM only -- and this is a hard constraint, not a preference.
        //
        // The engine is a 32-bit codebase. Its fastfile loader streams
        // hardcoded 32-bit struct sizes with 4-byte pointer slots
        // (Load_Stream(atStreamStart, (uint8_t *)varGfxImage, 36) for GfxImage,
        // then DB_PushStreamPos(4) to skip the 4-byte name pointer), and
        // DB_ConvertOffsetToPointer(uint32_t *) converts 32-bit offsets back
        // into pointers. 315 static_asserts pin 32-bit layouts; a 64-bit build
        // fails 3429 of them across 60 translation units, every one of them
        // pointer-width drift.
        //
        // So: armeabi-v7a, not arm64-v8a. AAPCS is what makes this work -- it
        // aligns 64-bit members to 8, unlike i386 SysV which aligns them to 4,
        // so the ARM32 layouts match what the static_asserts expect. See
        // docs/ANDROID_PORT.md.
        ndk {
            abiFilters += listOf("armeabi-v7a")
        }

        externalNativeBuild {
            cmake {
                // Flip to OFF once the engine port is done.
                arguments += "-DKISAK_ANDROID_STUB=ON"
                cppFlags += "-std=c++20"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
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
            // Required so the loader finds libkisakcod.so uncompressed.
            useLegacyPackaging = true
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
}
