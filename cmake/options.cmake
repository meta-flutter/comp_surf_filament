#
# Copyright 2020 Toyota Connected North America
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Filament Options
#
set(FILAMENT_SUPPORTS_XCB OFF)
set(FILAMENT_SUPPORTS_XLIB OFF)
set(FILAMENT_SUPPORTS_EGL_ON_LINUX OFF)
set(FILAMENT_SUPPORTS_WAYLAND ON)

set(FILAMENT_LINUX_IS_MOBILE OFF)
set(FILAMENT_SKIP_SDL2 OFF)

set(FILAMENT_SUPPORTS_OPENGL OFF)
set(FILAMENT_SUPPORTS_VULKAN ON)
set(FILAMENT_SUPPORTS_METAL OFF)

set(FILAMENT_USE_SWIFTSHADER OFF)
set(FILAMENT_ENABLE_MATDBG OFF)


# Target system.
if (IS_MOBILE_TARGET)
    set(MATC_TARGET mobile)
else()
    set(MATC_TARGET desktop)
endif()

set(MATC_API_FLAGS )

if (FILAMENT_SUPPORTS_OPENGL)
    set(MATC_API_FLAGS ${MATC_API_FLAGS} -a opengl)
endif()
if (FILAMENT_SUPPORTS_VULKAN)
    set(MATC_API_FLAGS ${MATC_API_FLAGS} -a vulkan)
endif()
if (FILAMENT_SUPPORTS_METAL)
    set(MATC_API_FLAGS ${MATC_API_FLAGS} -a metal)
endif()

# Disable optimizations and enable debug info (preserves names in SPIR-V)
if (FILAMENT_DISABLE_MATOPT)
    set(MATC_OPT_FLAGS -gd)
endif()

set(MATC_BASE_FLAGS ${MATC_API_FLAGS} -p ${MATC_TARGET} ${MATC_OPT_FLAGS})
