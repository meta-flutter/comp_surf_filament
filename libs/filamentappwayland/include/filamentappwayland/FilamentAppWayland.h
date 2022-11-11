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

#ifndef TNT_FILAMENT_SAMPLE_FILAMENTAPP_WAYLAND_H
#define TNT_FILAMENT_SAMPLE_FILAMENTAPP_WAYLAND_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <filament/Engine.h>
#include <filament/Viewport.h>

#include <camutils/Manipulator.h>

#include <utils/Path.h>
#include <utils/Entity.h>

#include "Config.h"
#include "IBL.h"
#include "Cube.h"
#include "Timer.hpp"

namespace filament {
    class Renderer;

    class Scene;

    class View;
} // namespace filament

namespace filagui {
    class ImGuiHelper;
} // namespace filagui

class IBL;

class MeshAssimp;

class FilamentAppWayland {
public:
    using SetupCallback = std::function<void(void *data, filament::Engine *, filament::View *, filament::Scene *)>;
    using CleanupCallback =
    std::function<void(void *data, filament::Engine *, filament::View *, filament::Scene *)>;
    using PreRenderCallback = std::function<void(filament::Engine *, filament::View *,
                                                 filament::Scene *, filament::Renderer *)>;
    using PostRenderCallback = std::function<void(filament::Engine *, filament::View *,
                                                  filament::Scene *, filament::Renderer *)>;
    using ImGuiCallback = std::function<void(filament::Engine *, filament::View *)>;
    using AnimCallback = std::function<void(void *data, filament::Engine *, filament::View *, double now)>;
    using ResizeCallback = std::function<void(filament::Engine *, filament::View *)>;
    using DropCallback = std::function<void(std::string)>;

    static FilamentAppWayland &get();

    ~FilamentAppWayland();

    void animate(AnimCallback animation) { mAnimation = animation; }

    void resize(ResizeCallback resize) { mResize = resize; }

    void setDropHandler(DropCallback handler) { mDropHandler = handler; }

    void run(void *data, const Config &config, size_t width, size_t height, SetupCallback setup, CleanupCallback cleanup,
             ImGuiCallback imgui = ImGuiCallback(), PreRenderCallback preRender = PreRenderCallback(),
             PostRenderCallback postRender = PostRenderCallback());

    void run_task();

    void draw_frame(uint32_t time);

    void stop();

    filament::Material const *getDefaultMaterial() const noexcept { return mDefaultMaterial; }

    filament::Material const *getTransparentMaterial() const noexcept { return mTransparentMaterial; }

    IBL *getIBL() const noexcept { return mIBL.get(); }

    filament::Texture *getDirtTexture() const noexcept { return mDirt; }

    filament::View *getGuiView() const noexcept;

    void close() { mClosed = true; }

//TODO    void setSidebarWidth(int width) { mSidebarWidth = width; }

//TODO    void setWindowTitle(const char *title) { mWindowTitle = title; }

    float &getCameraFocalLength() { return mCameraFocalLength; }

    void addOffscreenView(filament::View *view) { mOffscreenViews.push_back(view); }

    [[nodiscard]] size_t getSkippedFrameCount() const { return mSkippedFrames; }

    FilamentAppWayland(const FilamentAppWayland &rhs) = delete;

    FilamentAppWayland(FilamentAppWayland &&rhs) = delete;

    FilamentAppWayland &operator=(const FilamentAppWayland &rhs) = delete;

    FilamentAppWayland &operator=(FilamentAppWayland &&rhs) = delete;

private:
    FilamentAppWayland();

    using CameraManipulator = filament::camutils::Manipulator<float>;

    static bool manipulatorKeyFromKeycode(uint32_t scancode, CameraManipulator::Key &key);

    class CView {
    public:
        CView(filament::Renderer &renderer, std::string name);

        virtual ~CView();

        void setCameraManipulator(CameraManipulator *cm);

        void setViewport(filament::Viewport const &viewport);

        void setCamera(filament::Camera *camera);

        bool intersects(ssize_t x, ssize_t y);

        virtual void mouseDown(int button, ssize_t x, ssize_t y);

        virtual void mouseUp(ssize_t x, ssize_t y);

        virtual void mouseMoved(ssize_t x, ssize_t y);

        virtual void mouseWheel(ssize_t x);

        virtual void keyDown(uint32_t scancode);

        virtual void keyUp(uint32_t scancode);

        [[nodiscard]] filament::View const *getView() const { return view; }

        filament::View *getView() { return view; }

        CameraManipulator *getCameraManipulator() { return mCameraManipulator; }

    private:
        enum class Mode : uint8_t {
            NONE, ROTATE, TRACK
        };

        filament::Engine &engine;
        filament::Viewport mViewport;
        filament::View *view = nullptr;
        CameraManipulator *mCameraManipulator = nullptr;
        std::string mName;
    };

    class GodView : public CView {
    public:
        using CView::CView;

        void setGodCamera(filament::Camera *camera);
    };

    class Window {
        friend class FilamentAppWayland;

    public:
        Window(FilamentAppWayland *filamentApp, const Config &config,
               std::string title, size_t w, size_t h);

        virtual ~Window();

        void mouseDown(int button, ssize_t x, ssize_t y);

        void mouseUp(ssize_t x, ssize_t y);

        void mouseMoved(ssize_t x, ssize_t y);

        void mouseWheel(ssize_t x);

        void keyDown(uint32_t scancode);

        void keyUp(uint32_t scancode);

        void resize();

        filament::Renderer *getRenderer() { return mRenderer; }

        filament::SwapChain *getSwapChain() { return mSwapChain; }

    private:
        void configureCamerasForWindow();

        void fixupMouseCoordinatesForHdpi(ssize_t &x, ssize_t &y) const;

        FilamentAppWayland *const mFilamentApp = nullptr;
        const bool mIsHeadless;

        filament::Renderer *mRenderer = nullptr;
        filament::Engine::Backend mBackend;

        CameraManipulator *mMainCameraMan;
        CameraManipulator *mDebugCameraMan;
        filament::SwapChain *mSwapChain = nullptr;

        utils::Entity mCameraEntities[3];
        filament::Camera *mCameras[3] = {nullptr};
        filament::Camera *mMainCamera;
        filament::Camera *mDebugCamera;
        filament::Camera *mOrthoCamera;

        std::vector<std::unique_ptr<CView>> mViews;
        CView *mMainView;
        CView *mUiView;
        CView *mDepthView;
        GodView *mGodView;
        CView *mOrthoView;

        size_t mWidth = 0;
        size_t mHeight = 0;
        ssize_t mLastX = 0;
        ssize_t mLastY = 0;

        CView *mMouseEventTarget = nullptr;

        // Keep track of which view should receive a key's keyUp event.
        std::unordered_map<uint32_t, CView *> mKeyEventTarget;
    };

    friend class Window;

    void loadIBL(const Config &config);

    void loadDirt(const Config &config);

    filament::Engine *mEngine = nullptr;
    filament::Scene *mScene = nullptr;
    std::unique_ptr<IBL> mIBL;
    filament::Texture *mDirt = nullptr;
    bool mClosed = false;
    double mTime = 0;
    Timer_t mTimer;


    filament::Material const *mDefaultMaterial = nullptr;
    filament::Material const *mTransparentMaterial = nullptr;
    filament::Material const *mDepthMaterial = nullptr;
    filament::MaterialInstance *mDepthMI = nullptr;
    std::unique_ptr<filagui::ImGuiHelper> mImGuiHelper;
    AnimCallback mAnimation;
    ResizeCallback mResize;
    DropCallback mDropHandler;
//TODO    int mSidebarWidth = 0;
    size_t mSkippedFrames = 0;
    std::string mWindowTitle;
    std::vector<filament::View *> mOffscreenViews;
    float mCameraFocalLength = 28.0f;

    std::unique_ptr<FilamentAppWayland::Window> mAppWindow;
    std::unique_ptr<Cube> mAppCameraCube;
    std::unique_ptr<Cube> mAppLightmapCube;
//TODO    int mAppSidebarWidth;
    float mAppCameraFocalLength;
    PreRenderCallback mAppPreRenderCallback;
    PostRenderCallback mAppPostRenderCallback;
    CleanupCallback mAppCleanupCallback;
    ImGuiCallback mAppImgGuiCallback;
};

#endif // TNT_FILAMENT_SAMPLE_FILAMENTAPP_WAYLAND_H
