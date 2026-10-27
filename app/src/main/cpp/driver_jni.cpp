// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <jni.h>
#include <dlfcn.h>
#include <array>
#include <exception>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <unistd.h>
#include <vulkan/vulkan_raii.hpp>
#include <adrenotools/driver.h>
#include "skyline/common/format.h"
#include "skyline/common/signal.h"
#include "skyline/common/utils.h"
#include "skyline/logger/logger.h"

namespace {
    using SystemDriverInfo = std::array<std::string, 3>;

    std::optional<SystemDriverInfo> QuerySystemDriverInfo() {
        std::unique_ptr<void, decltype(&dlclose)> vulkanLibrary{dlopen("libvulkan.so", RTLD_NOW), &dlclose};
        if (!vulkanLibrary) {
            LOGW("Unable to load libvulkan.so while querying GPU driver metadata");
            return std::nullopt;
        }

        auto getInstanceProcAddr{reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(vulkanLibrary.get(), "vkGetInstanceProcAddr"))};
        if (!getInstanceProcAddr) {
            LOGW("Unable to find vkGetInstanceProcAddr while querying GPU driver metadata");
            return std::nullopt;
        }

        try {
            vk::raii::Context vkContext{getInstanceProcAddr};
            vk::raii::Instance vkInstance{vkContext, vk::InstanceCreateInfo{}};
            vk::raii::PhysicalDevices physicalDevices{vkInstance};
            if (physicalDevices.empty()) {
                LOGW("No Vulkan physical devices found while querying GPU driver metadata");
                return std::nullopt;
            }

            vk::raii::PhysicalDevice physicalDevice{std::move(physicalDevices.front())};
            auto deviceProperties2{physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDriverProperties>()};
            auto properties{deviceProperties2.get<vk::PhysicalDeviceProperties2>().properties};
            auto driverId{vk::to_string(deviceProperties2.get<vk::PhysicalDeviceDriverProperties>().driverID)};
            auto driverVersion{fmt::format("{}.{}.{}", VK_API_VERSION_MAJOR(properties.driverVersion), VK_API_VERSION_MINOR(properties.driverVersion), VK_API_VERSION_PATCH(properties.driverVersion))};

            return SystemDriverInfo{std::move(driverId), std::move(driverVersion), properties.deviceName};
        } catch (const std::exception &error) {
            LOGW("Failed to query system Vulkan driver metadata: {}", error.what());
        } catch (...) {
            LOGW("Failed to query system Vulkan driver metadata due to an unknown error");
        }

        return std::nullopt;
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_org_stratoemu_strato_utils_GpuDriverHelper_00024Companion_getSystemDriverInfo(JNIEnv *env, jobject) {
    auto info{QuerySystemDriverInfo()};
    if (!info)
        return nullptr;

    auto stringClass{env->FindClass("java/lang/String")};
    if (!stringClass)
        return nullptr;

    auto array{env->NewObjectArray(static_cast<jsize>(info->size()), stringClass, nullptr)};
    env->DeleteLocalRef(stringClass);
    if (!array)
        return nullptr;

    for (jsize index{}; index < static_cast<jsize>(info->size()); ++index) {
        auto value{env->NewStringUTF((*info)[index].c_str())};
        if (!value) {
            env->DeleteLocalRef(array);
            return nullptr;
        }

        env->SetObjectArrayElement(array, index, value);
        env->DeleteLocalRef(value);
        if (env->ExceptionCheck()) {
            env->DeleteLocalRef(array);
            return nullptr;
        }
    }

    return array;
}

static bool CheckKgslPresent() {
    constexpr auto KgslPath{"/dev/kgsl-3d0"};
    return access(KgslPath, F_OK) == 0;
}

static bool CheckMaliPresent() {
    constexpr auto MaliPath{"/dev/mali0"};
    return access(MaliPath, F_OK) == 0;
}

extern "C" JNIEXPORT jboolean JNICALL Java_org_stratoemu_strato_utils_GpuDriverHelper_00024Companion_supportsCustomDriverLoading(JNIEnv *, jobject) {
    // AdrenoTools' linker namespace loader is generic enough to load an
    // external Vulkan ICD on both KGSL and Kbase.  Only its optional GPU
    // mapping import path is Adreno/KGSL-specific.
    return CheckKgslPresent() || CheckMaliPresent();
}

extern "C" JNIEXPORT jboolean JNICALL Java_org_stratoemu_strato_utils_GpuDriverHelper_00024Companion_isMaliDevice(JNIEnv *, jobject) {
    return CheckMaliPresent();
}

extern "C" JNIEXPORT jboolean JNICALL Java_org_stratoemu_strato_utils_GpuDriverHelper_00024Companion_supportsForceMaxGpuClocks(JNIEnv *env, jobject instance) {
    // If the KGSL device exists adrenotools can be used to set GPU turbo mode
    return CheckKgslPresent();
}

extern "C" JNIEXPORT void JNICALL Java_org_stratoemu_strato_utils_GpuDriverHelper_00024Companion_forceMaxGpuClocks(JNIEnv *env, jobject instance, jboolean enable) {
    adrenotools_set_turbo(enable);
}
