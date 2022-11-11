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

#include <filament/Engine.h>
#include <filament/LightManager.h>
#include <filament/Material.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/TransformManager.h>
#include <filament/View.h>

#include <utils/EntityManager.h>

#include <filameshio/MeshReader.h>

#include <filamentappwayland/Config.h>
#include <filamentappwayland/FilamentAppWayland.h>

#include <memory>
#include <string>

using namespace filament;
using namespace filamesh;
using namespace filament::math;

using Backend = Engine::Backend;

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

    struct App {
        utils::Entity light;
        Material* material;
        MaterialInstance* materialInstance;
        MeshReader::Mesh mesh;
        mat4f transform;
    };

    Config mConfig;
    App mApp;
};
