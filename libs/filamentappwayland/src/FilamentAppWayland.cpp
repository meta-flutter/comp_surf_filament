#include <memory>

/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <filamentappwayland/FilamentAppWayland.h>


#include <iostream>
#include <utility>

#include <imgui.h>

#include <utils/EntityManager.h>
#include <utils/Path.h>

#include <filament/Camera.h>
#include <filament/Material.h>
#include <filament/Renderer.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/View.h>

#ifndef NDEBUG

#include <filament/DebugRegistry.h>

#endif

#include <filagui/ImGuiHelper.h>

#include <filamentappwayland/Cube.h>

#include <stb_image.h>

#include "generated/resources/filamentappwl.h"

using namespace filament;
using namespace filagui;
using namespace filament::math;
using namespace utils;

FilamentAppWayland &FilamentAppWayland::get() {
    static FilamentAppWayland filamentApp;
    return filamentApp;
}

FilamentAppWayland::FilamentAppWayland() {

}

FilamentAppWayland::~FilamentAppWayland() {

}

View *FilamentAppWayland::getGuiView() const noexcept {
    return mImGuiHelper->getView();
}

void FilamentAppWayland::run(void *data, const Config &config, size_t width, size_t height,
                             SetupCallback setupCallback, CleanupCallback cleanupCallback,
                             ImGuiCallback imguiCallback, PreRenderCallback preRender,
                             PostRenderCallback postRender) {
    mAppPreRenderCallback = std::move(preRender);
    mAppPostRenderCallback = std::move(postRender);
    mAppCleanupCallback = std::move(cleanupCallback);
    mAppImgGuiCallback = std::move(imguiCallback);

    mWindowTitle = config.title;
    std::unique_ptr<FilamentAppWayland::Window> window(
            new FilamentAppWayland::Window(this, config, config.title, width, height));
    mAppWindow = std::move(window);

    mDepthMaterial = Material::Builder()
            .package(FILAMENTAPPWL_DEPTHVISUALIZER_DATA, FILAMENTAPPWL_DEPTHVISUALIZER_SIZE)
            .build(*mEngine);

    mDepthMI = mDepthMaterial->createInstance();

    mDefaultMaterial = Material::Builder()
            .package(FILAMENTAPPWL_AIDEFAULTMAT_DATA, FILAMENTAPPWL_AIDEFAULTMAT_SIZE)
            .build(*mEngine);

    mTransparentMaterial = Material::Builder()
            .package(FILAMENTAPPWL_TRANSPARENTCOLOR_DATA, FILAMENTAPPWL_TRANSPARENTCOLOR_SIZE)
            .build(*mEngine);

    std::unique_ptr<Cube> cameraCube(new Cube(*mEngine, mTransparentMaterial, {1, 0, 0}));
    mAppCameraCube = std::move(cameraCube);
    // we can't cull the light-frustum because it's not applied a rigid transform
    // and currently, filament assumes that for culling
    std::unique_ptr<Cube> lightmapCube(new Cube(*mEngine, mTransparentMaterial, {0, 1, 0}, false));
    mAppLightmapCube = std::move(lightmapCube);
    mScene = mEngine->createScene();

    mAppWindow->mMainView->getView()->setVisibleLayers(0x4, 0x4);

    if (config.splitView) {
        auto &rcm = mEngine->getRenderableManager();

        rcm.setLayerMask(rcm.getInstance(mAppCameraCube->getSolidRenderable()), 0x3, 0x2);
        rcm.setLayerMask(rcm.getInstance(mAppCameraCube->getWireFrameRenderable()), 0x3, 0x2);

        rcm.setLayerMask(rcm.getInstance(mAppLightmapCube->getSolidRenderable()), 0x3, 0x2);
        rcm.setLayerMask(rcm.getInstance(mAppLightmapCube->getWireFrameRenderable()), 0x3, 0x2);

        // Create the camera mesh
        mScene->addEntity(mAppCameraCube->getWireFrameRenderable());
        mScene->addEntity(mAppCameraCube->getSolidRenderable());

        mScene->addEntity(mAppLightmapCube->getWireFrameRenderable());
        mScene->addEntity(mAppLightmapCube->getSolidRenderable());

        mAppWindow->mDepthView->getView()->setVisibleLayers(0x4, 0x4);
        mAppWindow->mGodView->getView()->setVisibleLayers(0x6, 0x6);
        mAppWindow->mOrthoView->getView()->setVisibleLayers(0x6, 0x6);

        // only preserve the color buffer for additional views; depth and stencil can be discarded.
        mAppWindow->mDepthView->getView()->setShadowingEnabled(false);
        mAppWindow->mGodView->getView()->setShadowingEnabled(false);
        mAppWindow->mOrthoView->getView()->setShadowingEnabled(false);
    }

    loadDirt(config);
    loadIBL(config);
    if (mIBL != nullptr) {
        mIBL->getSkybox()->setLayerMask(0x7, 0x4);
        mScene->setSkybox(mIBL->getSkybox());
        mScene->setIndirectLight(mIBL->getIndirectLight());
    }

    for (auto &view: mAppWindow->mViews) {
        view->getView()->setScene(mScene);
    }

    setupCallback(data, mEngine, mAppWindow->mMainView->getView(), mScene);


    mAppCameraFocalLength = mCameraFocalLength;

    std::cout << "Timer Latency: " << mTimer.TestLatency().count() << std::endl;

    mTimer.Start();
}

void FilamentAppWayland::run_task() {
}

void FilamentAppWayland::draw_frame(uint32_t time) {
    (void)time;

    auto& window = mAppWindow;
    auto& preRender = mAppPreRenderCallback;
    auto& postRender = mAppPostRenderCallback;
    auto& lightmapCube = mAppLightmapCube;
    auto& cameraCube = mAppCameraCube;
    auto& imguiCallback = mAppImgGuiCallback;

    if (mCameraFocalLength != mAppCameraFocalLength) {
      window->configureCamerasForWindow();
        mAppCameraFocalLength = mCameraFocalLength;
    }

    if (!UTILS_HAS_THREADING) {
        mEngine->execute();
    }

    // Allow the app to animate the scene if desired.
    if (mAnimation) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double, std::milli>> now1 = now;
        auto now2 = now1.time_since_epoch().count() / 1000.0f;
        mAnimation(this, mEngine, window->mMainView->getView(), now2);
    }

    // Calculate the time step.
    mTimer.End();
    mTimer.CalcDuration();
    auto duration = mTimer.GetDuration() / 1000000.0f;
    const float timeStep = duration.count();
    mTimer.Start();

    // Update the camera manipulators for each view.
    for (auto const& view : window->mViews) {
        auto* cm = view->getCameraManipulator();
        if (cm) {
            cm->update(timeStep);
        }
    }

    // Update the position and orientation of the two cameras.
    filament::math::float3 eye, center, up;
    window->mMainCameraMan->getLookAt(&eye, &center, &up);
    window->mMainCamera->lookAt(eye, center, up);
    window->mDebugCameraMan->getLookAt(&eye, &center, &up);
    window->mDebugCamera->lookAt(eye, center, up);

    // Update the cube distortion matrix used for frustum visualization.
    const Camera* lightmapCamera = window->mMainView->getView()->getDirectionalLightCamera();
    lightmapCube->mapFrustum(*mEngine, lightmapCamera);
    cameraCube->mapFrustum(*mEngine, window->mMainCamera);


    Renderer* renderer = window->getRenderer();

    if (preRender) {
        preRender(mEngine, window->mViews[0]->getView(), mScene, renderer);
    }

    if (renderer->beginFrame(window->getSwapChain())) {
        for (filament::View* offscreenView : mOffscreenViews) {
            renderer->render(offscreenView);
        }
        for (auto const& view : window->mViews) {
            renderer->render(view->getView());
        }
        if (postRender) {
            postRender(mEngine, window->mViews[0]->getView(), mScene, renderer);
        }
        renderer->endFrame();

    } else {
        ++mSkippedFrames;
    }
}

void FilamentAppWayland::stop() {
    if (mImGuiHelper) {
        mImGuiHelper.reset();
    }

    mAppCleanupCallback(this, mEngine, mAppWindow->mMainView->getView(), mScene);

    mAppCameraCube.reset();
    mAppLightmapCube.reset();
    mAppWindow.reset();

    mIBL.reset();
    mEngine->destroy(mDepthMI);
    mEngine->destroy(mDepthMaterial);
    mEngine->destroy(mDefaultMaterial);
    mEngine->destroy(mTransparentMaterial);
    mEngine->destroy(mScene);
    Engine::destroy(&mEngine);
    mEngine = nullptr;
}

void FilamentAppWayland::loadIBL(const Config &config) {
    if (!config.iblDirectory.empty()) {
        Path iblPath(config.iblDirectory);

        if (!iblPath.exists()) {
            std::cerr << "The specified IBL path does not exist: " << iblPath << std::endl;
            return;
        }

        mIBL = std::make_unique<IBL>(*mEngine);

        if (!iblPath.isDirectory()) {
            if (!mIBL->loadFromEquirect(iblPath)) {
                std::cerr << "Could not load the specified IBL: " << iblPath << std::endl;
                mIBL.reset(nullptr);
                return;
            }
        } else {
            if (!mIBL->loadFromDirectory(iblPath)) {
                std::cerr << "Could not load the specified IBL: " << iblPath << std::endl;
                mIBL.reset(nullptr);
                return;
            }
        }
    }
}

void FilamentAppWayland::loadDirt(const Config &config) {
    if (!config.dirt.empty()) {
        Path dirtPath(config.dirt);

        if (!dirtPath.exists()) {
            std::cerr << "The specified dirt file does not exist: " << dirtPath << std::endl;
            return;
        }

        if (!dirtPath.isFile()) {
            std::cerr << "The specified dirt path is not a file: " << dirtPath << std::endl;
            return;
        }

        int w, h, n;

        unsigned char *data = stbi_load(dirtPath.getAbsolutePath().c_str(), &w, &h, &n, 3);
        assert(n == 3);

        mDirt = Texture::Builder()
                .width(w)
                .height(h)
                .format(Texture::InternalFormat::RGB8)
                .build(*mEngine);

        mDirt->setImage(*mEngine, 0, {data, size_t(w * h * 3),
                                      Texture::Format::RGB, Texture::Type::UBYTE,
                                      (Texture::PixelBufferDescriptor::Callback) &stbi_image_free});
    }
}

// ------------------------------------------------------------------------------------------------

FilamentAppWayland::Window::Window(FilamentAppWayland *filamentApp,
                                   const Config &config, std::string title, size_t w, size_t h)
        : mFilamentApp(filamentApp), mIsHeadless(config.headless), mWidth(w), mHeight(h) {

    if (config.headless) {
        mFilamentApp->mEngine = Engine::create(config.backend);
        mSwapChain = mFilamentApp->mEngine->createSwapChain((uint32_t) w, (uint32_t) h);
        mWidth = w;
        mHeight = h;
    } else {

        // Create the Engine after the window in case this happens to be a single-threaded platform.
        // For single-threaded platforms, we need to ensure that Filament's OpenGL context is
        // current, rather than the one created by SDL.
        mFilamentApp->mEngine = Engine::create(config.backend);

        // get the resolved backend
        mBackend = config.backend = mFilamentApp->mEngine->getBackend();

#if defined(__APPLE__)
        ::prepareNativeWindow(mWindow);

        void* metalLayer = nullptr;
        if (config.backend == filament::Engine::Backend::METAL) {
            metalLayer = setUpMetalLayer(nativeWindow);
            // The swap chain on Metal is a CAMetalLayer.
            nativeSwapChain = metalLayer;
        }

#if defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
        if (config.backend == filament::Engine::Backend::VULKAN) {
            // We request a Metal layer for rendering via MoltenVK.
            setUpMetalLayer(nativeWindow);
        }
#endif
#endif

        // Select the feature level to use
        config.featureLevel = std::min(config.featureLevel,
                                       mFilamentApp->mEngine->getSupportedFeatureLevel());
        mFilamentApp->mEngine->setActiveFeatureLevel(config.featureLevel);

        mSwapChain = mFilamentApp->mEngine->createSwapChain(config.native_window);
    }
    mRenderer = mFilamentApp->mEngine->createRenderer();

    // create cameras
    utils::EntityManager &em = utils::EntityManager::get();
    em.create(3, mCameraEntities);
    mCameras[0] = mMainCamera = mFilamentApp->mEngine->createCamera(mCameraEntities[0]);
    mCameras[1] = mDebugCamera = mFilamentApp->mEngine->createCamera(mCameraEntities[1]);
    mCameras[2] = mOrthoCamera = mFilamentApp->mEngine->createCamera(mCameraEntities[2]);

    // set exposure
    for (auto camera: mCameras) {
        camera->setExposure(16.0f, 1 / 125.0f, 100.0f);
    }

    // create views
    mViews.emplace_back(mMainView = new CView(*mRenderer, "Main View"));
    if (config.splitView) {
        mViews.emplace_back(mDepthView = new CView(*mRenderer, "Depth View"));
        mViews.emplace_back(mGodView = new GodView(*mRenderer, "God View"));
        mViews.emplace_back(mOrthoView = new CView(*mRenderer, "Ortho View"));
    }
//    mViews.emplace_back(mUiView = new CView(*mRenderer, "UI View"));

    // set-up the camera manipulators
    mMainCameraMan = CameraManipulator::Builder()
            .targetPosition(0, 0, -4)
            .flightMoveDamping(15.0)
            .build(config.cameraMode);
    mDebugCameraMan = CameraManipulator::Builder()
            .targetPosition(0, 0, -4)
            .build(camutils::Mode::ORBIT);

    mMainView->setCamera(mMainCamera);
    mMainView->setCameraManipulator(mMainCameraMan);
    if (config.splitView) {
        // Depth view always uses the main camera
        mDepthView->setCamera(mMainCamera);

        // The god view uses the main camera for culling, but the debug camera for viewing
        mGodView->setCamera(mMainCamera);
        mGodView->setGodCamera(mDebugCamera);

        // Ortho view obviously uses an ortho camera
        mOrthoView->setCamera((Camera *) mMainView->getView()->getDirectionalLightCamera());

        mDepthView->setCameraManipulator(mMainCameraMan);
        mGodView->setCameraManipulator(mDebugCameraMan);
    }

    // configure the cameras
    configureCamerasForWindow();

    mMainCamera->lookAt({4, 0, -4}, {0, 0, -4}, {0, 1, 0});
}

FilamentAppWayland::Window::~Window() {
    mViews.clear();
    utils::EntityManager &em = utils::EntityManager::get();
    for (auto e: mCameraEntities) {
        mFilamentApp->mEngine->destroyCameraComponent(e);
        em.destroy(e);
    }
    mFilamentApp->mEngine->destroy(mRenderer);
    mFilamentApp->mEngine->destroy(mSwapChain);

    delete mMainCameraMan;
    delete mDebugCameraMan;
}

void FilamentAppWayland::Window::mouseDown(int button, ssize_t x, ssize_t y) {
    fixupMouseCoordinatesForHdpi(x, y);
    y = mHeight - y;
    for (auto const &view: mViews) {
        if (view->intersects(x, y)) {
            mMouseEventTarget = view.get();
            view->mouseDown(button, x, y);
            break;
        }
    }
}

void FilamentAppWayland::Window::mouseWheel(ssize_t x) {
    if (mMouseEventTarget) {
        mMouseEventTarget->mouseWheel(x);
    } else {
        for (auto const &view: mViews) {
            if (view->intersects(mLastX, mLastY)) {
                view->mouseWheel(x);
                break;
            }
        }
    }
}

void FilamentAppWayland::Window::mouseUp(ssize_t x, ssize_t y) {
    fixupMouseCoordinatesForHdpi(x, y);
    if (mMouseEventTarget) {
        y = mHeight - y;
        mMouseEventTarget->mouseUp(x, y);
        mMouseEventTarget = nullptr;
    }
}

void FilamentAppWayland::Window::mouseMoved(ssize_t x, ssize_t y) {
    fixupMouseCoordinatesForHdpi(x, y);
    y = mHeight - y;
    if (mMouseEventTarget) {
        mMouseEventTarget->mouseMoved(x, y);
    }
    mLastX = x;
    mLastY = y;
}

void FilamentAppWayland::Window::keyDown(uint32_t key) {
    auto &eventTarget = mKeyEventTarget[key];

    // keyDown events can be sent multiple times per key (for key repeat)
    // If this key is already down, do nothing.
    if (eventTarget) {
        return;
    }

    // Decide which view will get this key's corresponding keyUp event.
    // If we're currently in a mouse grap session, it should be the mouse grab's target view.
    // Otherwise, it should be whichever view we're currently hovering over.
    CView *targetView = nullptr;
    if (mMouseEventTarget) {
        targetView = mMouseEventTarget;
    } else {
        for (auto const &view: mViews) {
            if (view->intersects(mLastX, mLastY)) {
                targetView = view.get();
                break;
            }
        }
    }

    if (targetView) {
        targetView->keyDown(key);
        eventTarget = targetView;
    }
}

void FilamentAppWayland::Window::keyUp(uint32_t key) {
    auto &eventTarget = mKeyEventTarget[key];
    if (!eventTarget) {
        return;
    }
    eventTarget->keyUp(key);
    eventTarget = nullptr;
}

void FilamentAppWayland::Window::fixupMouseCoordinatesForHdpi(ssize_t &x, ssize_t &y) const {
    int dw, dh, ww, wh;
#if 0 //TODO
    SDL_GL_GetDrawableSize(mWindow, &dw, &dh);
    SDL_GetWindowSize(mWindow, &ww, &wh);
#endif //TODO
    x = x * dw / ww;
    y = y * dh / wh;
}

void FilamentAppWayland::Window::resize() {

#if defined(__APPLE__)
    void *nativeWindow = nullptr; // TODO ::getNativeWindow(mWindow);

    if (mBackend == filament::Engine::Backend::METAL) {
        resizeMetalLayer(nativeWindow);
    }

#if defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
    if (mBackend == filament::Engine::Backend::VULKAN) {
        resizeMetalLayer(nativeWindow);
    }
#endif

#endif

    configureCamerasForWindow();

    // Call the resize callback, if this FilamentApp has one. This must be done after
    // configureCamerasForWindow, so the viewports are correct.
    if (mFilamentApp->mResize) {
        mFilamentApp->mResize(mFilamentApp->mEngine, mMainView->getView());
    }
}

void FilamentAppWayland::Window::configureCamerasForWindow() {
    float dpiScaleX = 1.0f;

    const uint32_t width = mWidth;
    const uint32_t height = mHeight;

    const float3 at(0, 0, -4);
    const double ratio = double(height) / double(width);
//TODO    const int sidebar = mFilamentApp->mSidebarWidth * dpiScaleX;

    const bool splitview = mViews.size() > 2;

    // To trigger a floating-point exception, users could shrink the window to be smaller than
    // the sidebar. To prevent this we simply clamp the width of the main viewport.
    const uint32_t mainWidth = splitview ? width : std::max(1, (int) width); //TODO - sidebar

    double near = 0.1;
    double far = 100;
    mMainCamera->setLensProjection(mFilamentApp->mCameraFocalLength, double(mainWidth) / height, near, far);
    mDebugCamera->setProjection(45.0, double(width) / height, 0.0625, 4096, Camera::Fov::VERTICAL);
    mOrthoCamera->setProjection(Camera::Projection::ORTHO, -3, 3, -3 * ratio, 3 * ratio, near, far);
    mOrthoCamera->lookAt({0, 0, 0}, {0, 0, -4});

    // We're in split view when there are more views than just the Main and UI views.
    if (splitview) {
        uint32_t vpw = width / 2;
        uint32_t vph = height / 2;
        mMainView->setViewport({0, 0, vpw, vph});
        mDepthView->setViewport({int32_t(vpw), 0, width - vpw, vph});
        mGodView->setViewport({int32_t(vpw), int32_t(vph), width - vpw, height - vph});
        mOrthoView->setViewport({0, int32_t(vph), vpw, height - vph});
    } else {
        mMainView->setViewport({0, 0, mainWidth, height}); //sidebar
    }
#if 0 //TODO
    mUiView->setViewport({0, 0, width, height});
#endif
}

// ------------------------------------------------------------------------------------------------

FilamentAppWayland::CView::CView(Renderer &renderer, std::string name)
        : engine(*renderer.getEngine()), mName(name) {
    view = engine.createView();
    view->setName(name.c_str());
}

FilamentAppWayland::CView::~CView() {
    engine.destroy(view);
}

void FilamentAppWayland::CView::setViewport(Viewport const &viewport) {
    mViewport = viewport;
    view->setViewport(viewport);
    if (mCameraManipulator) {
        mCameraManipulator->setViewport(viewport.width, viewport.height);
    }
}

void FilamentAppWayland::CView::mouseDown(int button, ssize_t x, ssize_t y) {
    if (mCameraManipulator) {
        mCameraManipulator->grabBegin(x, y, button == 3);
    }
}

void FilamentAppWayland::CView::mouseUp(ssize_t x, ssize_t y) {
    if (mCameraManipulator) {
        mCameraManipulator->grabEnd();
    }
}

void FilamentAppWayland::CView::mouseMoved(ssize_t x, ssize_t y) {
    if (mCameraManipulator) {
        mCameraManipulator->grabUpdate(x, y);
    }
}

void FilamentAppWayland::CView::mouseWheel(ssize_t x) {
    if (mCameraManipulator) {
        mCameraManipulator->scroll(0, 0, x);
    }
}

bool FilamentAppWayland::manipulatorKeyFromKeycode(uint32_t scancode, CameraManipulator::Key &key) {
    switch (scancode) {
#if 0 //TODO
        case SDL_SCANCODE_W:
            key = CameraManipulator::Key::FORWARD;
            return true;
        case SDL_SCANCODE_A:
            key = CameraManipulator::Key::LEFT;
            return true;
        case SDL_SCANCODE_S:
            key = CameraManipulator::Key::BACKWARD;
            return true;
        case SDL_SCANCODE_D:
            key = CameraManipulator::Key::RIGHT;
            return true;
        case SDL_SCANCODE_E:
            key = CameraManipulator::Key::UP;
            return true;
        case SDL_SCANCODE_Q:
            key = CameraManipulator::Key::DOWN;
            return true;
#endif //TODO
        default:
            return false;
    }
}

void FilamentAppWayland::CView::keyUp(uint32_t scancode) {
    if (mCameraManipulator) {
        CameraManipulator::Key key;
        if (manipulatorKeyFromKeycode(scancode, key)) {
            mCameraManipulator->keyUp(key);
        }
    }
}

void FilamentAppWayland::CView::keyDown(uint32_t scancode) {
    if (mCameraManipulator) {
        CameraManipulator::Key key;
        if (manipulatorKeyFromKeycode(scancode, key)) {
            mCameraManipulator->keyDown(key);
        }
    }
}

bool FilamentAppWayland::CView::intersects(ssize_t x, ssize_t y) {
    if (x >= mViewport.left && x < mViewport.left + mViewport.width)
        if (y >= mViewport.bottom && y < mViewport.bottom + mViewport.height)
            return true;

    return false;
}

void FilamentAppWayland::CView::setCameraManipulator(CameraManipulator *cm) {
    mCameraManipulator = cm;
}

void FilamentAppWayland::CView::setCamera(Camera *camera) {
    view->setCamera(camera);
}

void FilamentAppWayland::GodView::setGodCamera(Camera *camera) {
    getView()->setDebugCamera(camera);
}
