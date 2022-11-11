/*
 * Copyright 2022 Toyota Connected North America
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "../include/comp_surf_filament/comp_surf_filament.h"

#include <bluevk/BlueVK.h>

#include <utils/Log.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <math/scalar.h>

using namespace filament::math;
using namespace bluevk;

struct VulkanDriver {
    VkInstance instance;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    uint32_t graphicsQueueFamilyIndex;
    uint32_t presentQueueFamilyIndex;
    VkPhysicalDevice physicalDevice;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderingFinishedSemaphore;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkSurfaceFormatKHR *surfaceFormats;
    uint32_t surfaceFormatsAllocatedCount;
    uint32_t surfaceFormatsCount;
    uint32_t swapchainDesiredImageCount;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D swapchainSize;
    VkCommandPool commandPool;
    uint32_t swapchainImageCount;
    VkImage *swapchainImages;
    VkCommandBuffer *commandBuffers;
    VkFence *fences;
};

static SDL_Window* gWindow = nullptr;
static VulkanDriver gVulkanDriver = {0};

struct CompSurfContext {
public:
    static uint32_t version();

    CompSurfContext(const char *accessToken,
                    int width,
                    int height,
                    void *nativeWindow,
                    const char *assetsPath,
                    const char *cachePath,
                    const char *miscPath);

    ~CompSurfContext() = default;

    CompSurfContext(const CompSurfContext &) = delete;

    CompSurfContext(CompSurfContext &&) = delete;

    CompSurfContext &operator=(const CompSurfContext &) = delete;

    void de_initialize();

    void run_task();

    void draw_frame(uint32_t time);

    void resize(int width, int height);

private:
    std::string mAccessToken;
    std::string mAssetsPath;
    std::string mCachePath;
    std::string mMiscPath;
    int mWidth;
    int mHeight;

    static void quit(int rc);

    static void createInstance();

    static void createSurface(void *native_window);

    static void findPhysicalDevice();

    static void createDevice();

    static void getQueues();

    static void createSemaphore(VkSemaphore *semaphore);

    static void createSemaphores();

    static void getSurfaceCaps();

    static void getSurfaceFormats();

    static void getSwapchainImages();

    static bool createSwapchain(int w, int h);

    static void destroySwapchain();

    static void destroyCommandBuffers();

    static void destroyCommandPool();

    static void createCommandPool();

    static void createCommandBuffers();

    static void createFences();

    static void destroyFences();

    static void recordPipelineImageBarrier(VkCommandBuffer commandBuffer,
                                           VkAccessFlags sourceAccessMask,
                                           VkAccessFlags destAccessMask,
                                           VkImageLayout sourceLayout,
                                           VkImageLayout destLayout,
                                           VkImage image);

    static void rerecordCommandBuffer(uint32_t frameIndex, const VkClearColorValue *clearColor);

    static void destroySwapchainThings(bool doDestroySwapchain);

    static bool createNewSwapchainThings(int w, int h);

    static void initVulkan(void *native_window, int w, int h);

    bool render();
};
