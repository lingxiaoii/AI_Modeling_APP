plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.pocketmodeler.app"
    compileSdk = 35
    // 固定 NDK 版本：CI runner 预装该版本，避免自动下载失败。
    ndkVersion = "26.1.10909125"

    defaultConfig {
        applicationId = "com.pocketmodeler.app"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.0.0.1"

        ndk {
            // bgfx/libjpeg 尚未入场（M2c/M5），先只出 arm64 减少构建面。
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17")
                arguments += listOf(
                    "-DPM_BUILD_TESTS=OFF",
                    "-DPM_WITH_MANIFOLD=OFF",
                    "-DPM_WITH_RENDER=OFF",
                    "-DPM_WITH_MCP=OFF"
                )
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
        }
        debug {
            // 固定 debug 签名（仓库内 keystore/debug.keystore，见 README.md）。
            // 覆盖安装不要求先卸载；正式签名仍禁止入库。
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    signingConfigs {
        create("debug") {
            storeFile = file("../keystore/debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }

    buildFeatures {
        viewBinding = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.activity:activity-ktx:1.9.3")
    implementation("androidx.recyclerview:recyclerview:1.3.2")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
}