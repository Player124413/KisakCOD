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

        // ARM64 only. The engine is 32-bit x86 today and the whole point of
        // this port is a native ARM64 build, so there is nothing to gain from
        // shipping 32-bit ABIs.
        ndk {
            abiFilters += listOf("arm64-v8a")
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
