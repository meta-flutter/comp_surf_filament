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

#include "context.h"

#include "generated/resources/resources.h"
#include "generated/resources/monkey.h"


using namespace filament;
using namespace filament::math;
using utils::Entity;
using utils::EntityManager;


uint32_t CompSurfContext::version() {
    return 0x00010000;
}

CompSurfContext::CompSurfContext(const char *accessToken,
                                 int width,
                                 int height,
                                 void *nativeWindow,
                                 const char *assetsPath,
                                 const char *cachePath,
                                 const char *miscPath)
        : mAccessToken(accessToken),
          mAssetsPath(assetsPath),
          mCachePath(cachePath),
          mMiscPath(miscPath),
          mWidth(width),
          mHeight(height) {

    mConfig.title = "hellopbr";
    mConfig.assetPath = assetsPath;
    mConfig.width = mWidth;
    mConfig.height = mHeight;
    mConfig.native_window = nativeWindow;
    mConfig.iblDirectory = mAssetsPath + "/ibl/lightroom_14b";

    auto setup = [](void *data, Engine *engine, View *view, Scene *scene) {
        auto app = reinterpret_cast<CompSurfContext *>(data)->mApp;
        auto& tcm = engine->getTransformManager();
        auto& rcm = engine->getRenderableManager();
        auto& em = utils::EntityManager::get();

        // Instantiate material.
        app.material = Material::Builder()
                .package(RESOURCES_AIDEFAULTMAT_DATA, RESOURCES_AIDEFAULTMAT_SIZE).build(*engine);
        auto mi = app.materialInstance = app.material->createInstance();
        mi->setParameter("baseColor", RgbType::LINEAR, float3{0.8});
        mi->setParameter("metallic", 1.0f);
        mi->setParameter("roughness", 0.4f);
        mi->setParameter("reflectance", 0.5f);

        // Add geometry into the scene.
        app.mesh = MeshReader::loadMeshFromBuffer(engine, MONKEY_SUZANNE_DATA, nullptr, nullptr, mi);
        auto ti = tcm.getInstance(app.mesh.renderable);
        app.transform = mat4f{ mat3f(1), float3(0, 0, -4) } * tcm.getWorldTransform(ti);
        rcm.setCastShadows(rcm.getInstance(app.mesh.renderable), false);
        scene->addEntity(app.mesh.renderable);

        // Add light sources into the scene.
        app.light = em.create();
        LightManager::Builder(LightManager::Type::SUN)
                .color(Color::toLinear<ACCURATE>(sRGBColor(0.98f, 0.92f, 0.89f)))
                .intensity(110000)
                .direction({ 0.7, -1, -0.8 })
                .sunAngularRadius(1.9f)
                .castShadows(false)
                .build(*engine, app.light);
        scene->addEntity(app.light);
    };

    auto cleanup = [](void *data, Engine *engine, View *, Scene *) {
        auto app = reinterpret_cast<CompSurfContext *>(data)->mApp;
        engine->destroy(app.light);
        engine->destroy(app.materialInstance);
        engine->destroy(app.mesh.renderable);
        engine->destroy(app.material);
    };

    FilamentAppWayland::get().animate([](void *data, Engine *engine, View *view, double now) {
        auto app = reinterpret_cast<CompSurfContext *>(data)->mApp;
        auto& tcm = engine->getTransformManager();
        auto ti = tcm.getInstance(app.mesh.renderable);
        tcm.setTransform(ti, app.transform * mat4f::rotation(now, float3{ 0, 1, 0 }));
    });

    FilamentAppWayland::get().run(this, mConfig, width, height, setup, cleanup, nullptr, nullptr, nullptr);
}

void CompSurfContext::de_initialize() {
    FilamentAppWayland::get().stop();
}

void CompSurfContext::run_task() {
}

void CompSurfContext::resize(int width, int height) {
    mWidth = width;
    mHeight = height;
}

void CompSurfContext::draw_frame(uint32_t time) {
    FilamentAppWayland::get().draw_frame(time);
}
