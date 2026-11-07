cmake_minimum_required(VERSION 3.20)
project(boku C CXX)
enable_language(ASM)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(BOKU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/boku")
set(BOKU_BUILD_SEED "1" CACHE STRING "Per-build seed for the encrypted target table")
set(BOKU_BUILD_COMMIT "dev" CACHE STRING "Build commit shown in the log")
string(LENGTH "${BOKU_BUILD_COMMIT}" BOKU_BUILD_COMMIT_LEN)
if(BOKU_BUILD_COMMIT_LEN GREATER 7)
  string(SUBSTRING "${BOKU_BUILD_COMMIT}" 0 7 BOKU_BUILD_COMMIT_SHORT)
else()
  set(BOKU_BUILD_COMMIT_SHORT "${BOKU_BUILD_COMMIT}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT DEFINED ANDROID_ABI OR NOT ANDROID_ABI MATCHES "^arm64-v8a$")
  message(FATAL_ERROR
    "Unsupported or missing ANDROID_ABI='${ANDROID_ABI}'. "
    "Use the NDK toolchain file with arm64-v8a.")
endif()

# ShadowHook is vendored as source (v2.0.1) and built into libboku.so as a
# static archive. Upstream only publishes a shared library through Prefab, but
# this mod is injected as one .so; the static build keeps that contract and
# avoids a second libshadowhook.so in the game process.
set(SHADOWHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ShadowHook")
if(NOT EXISTS "${SHADOWHOOK_DIR}/shadowhook.c")
  message(FATAL_ERROR "ShadowHook sources not found in ${SHADOWHOOK_DIR}")
endif()
file(GLOB SHADOWHOOK_SOURCES CONFIGURE_DEPENDS
  "${SHADOWHOOK_DIR}/*.c"
  "${SHADOWHOOK_DIR}/arch/arm64/*.c"
  "${SHADOWHOOK_DIR}/arch/arm64/*.S"
  "${SHADOWHOOK_DIR}/common/*.c"
  "${SHADOWHOOK_DIR}/third_party/*/*.c")
# sh_jni.c defines a second JNI_OnLoad and the Java API; the menu owns the only
# JNI_OnLoad exported by libboku.so.
list(FILTER SHADOWHOOK_SOURCES EXCLUDE REGEX "/sh_jni\\.c$")
add_library(shadowhook STATIC ${SHADOWHOOK_SOURCES})
set_target_properties(shadowhook PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden)
target_compile_features(shadowhook PRIVATE c_std_11)
# The same size-oriented flags as upstream, minus -Weverything -Werror. Stack
# protection is off because the crash probe hooks __stack_chk_fail and the hook
# engine must never call into that proxy from its own frames.
target_compile_options(shadowhook PRIVATE
  -Os
  -funwind-tables
  -fno-stack-protector
  -fno-ident
  -ffunction-sections
  -fdata-sections)
target_include_directories(shadowhook PRIVATE
  "${SHADOWHOOK_DIR}"
  "${SHADOWHOOK_DIR}/include"
  "${SHADOWHOOK_DIR}/arch/arm64"
  "${SHADOWHOOK_DIR}/common"
  "${SHADOWHOOK_DIR}/third_party/xdl"
  "${SHADOWHOOK_DIR}/third_party/bsd"
  "${SHADOWHOOK_DIR}/third_party/lss")

# File logging is on by default; -DBOKU_LOG=OFF drops it for the smallest .so.
option(BOKU_LOG "Enable file logging" ON)

# ShadowHook debug logs go to logcat with tag "shadowhook_tag". They are
# expensive, so keep them off unless a build is being diagnosed.
option(BOKU_HOOK_DEBUG "Enable ShadowHook debug logs" OFF)

# The target names are shipped as one encrypted blob. The key is derived from
# BOKU_BUILD_SEED, so every build ships a different blob and no class or method
# name appears in the library as plaintext.
set(TARGETS_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${TARGETS_DIR}")
set(TARGETS_HEADER "${TARGETS_DIR}/targets_generated.h")
set(TARGETS_SOURCE "${TARGETS_DIR}/targets_generated.cpp")
add_custom_command(
  OUTPUT "${TARGETS_HEADER}" "${TARGETS_SOURCE}"
  COMMAND ${Python3_EXECUTABLE} "${BOKU_ROOT}/tools/gen_targets.py"
          "${BOKU_ROOT}/targets.def"
          "${TARGETS_HEADER}" "${TARGETS_SOURCE}"
          "${BOKU_BUILD_SEED}"
  DEPENDS "${BOKU_ROOT}/targets.def" "${BOKU_ROOT}/tools/gen_targets.py"
  COMMENT "Generating encrypted target table"
  VERBATIM)
add_custom_target(targets_generated DEPENDS "${TARGETS_HEADER}" "${TARGETS_SOURCE}")
set_source_files_properties("${TARGETS_SOURCE}" PROPERTIES GENERATED TRUE)

# Menu side of the template: Java bridge, feature list and the single JNI_OnLoad.
set(MENU_SOURCES
  Menu/FeatureList.cpp
  Menu/Menu.cpp
  Menu/Jni.cpp
  Menu/Setup.cpp
  Includes/Utils.cpp
)

# Ported mod. Entry point, platform bridge, shared services, then one file per
# hook group.
set(MOD_SOURCES
  "${BOKU_ROOT}/main.cpp"
  "${BOKU_ROOT}/feature_flags.cpp"
  "${BOKU_ROOT}/feature_registry.cpp"
  "${BOKU_ROOT}/jni_bridge.cpp"
  "${BOKU_ROOT}/jni_cache.cpp"
  "${BOKU_ROOT}/jni_util.cpp"
  "${BOKU_ROOT}/log.cpp"
  "${BOKU_ROOT}/player_service.cpp"
  "${BOKU_ROOT}/toast.cpp"
  "${BOKU_ROOT}/hook_manager.cpp"
  "${BOKU_ROOT}/il2cpp_api.cpp"
  "${BOKU_ROOT}/targets.cpp"
  "${BOKU_ROOT}/info.cpp"
  "${BOKU_ROOT}/hooks/action_input.cpp"
  "${BOKU_ROOT}/hooks/iap.cpp"
  "${BOKU_ROOT}/hooks/multiplayer.cpp"
  "${BOKU_ROOT}/hooks/chat.cpp"
  "${BOKU_ROOT}/hooks/sensitive.cpp"
  "${BOKU_ROOT}/hooks/text_input.cpp"
  "${BOKU_ROOT}/hooks/profile.cpp"
  "${BOKU_ROOT}/hooks/password_room.cpp"
  "${BOKU_ROOT}/hooks/skin_range.cpp"
  "${BOKU_ROOT}/hooks/about.cpp"
  "${BOKU_ROOT}/hooks/food.cpp"
  "${BOKU_ROOT}/hooks/hand.cpp"
  "${BOKU_ROOT}/hooks/privacy.cpp"
  "${BOKU_ROOT}/hooks/weapon.cpp"
  "${BOKU_ROOT}/hooks/effects.cpp"
  "${BOKU_ROOT}/hooks/knockback.cpp"
  "${BOKU_ROOT}/hooks/feces.cpp"
  "${BOKU_ROOT}/hooks/skin.cpp"
  "${BOKU_ROOT}/hooks/slots.cpp"
  "${BOKU_ROOT}/hooks/spoof.cpp"
  "${BOKU_ROOT}/hooks/room.cpp"
  "${BOKU_ROOT}/hooks/player.cpp"
)

add_library(boku SHARED
  ${MENU_SOURCES}
  ${MOD_SOURCES}
  "${TARGETS_SOURCE}"
)

target_include_directories(boku PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/Includes"
  "${BOKU_ROOT}"
  "${TARGETS_DIR}"
  "${SHADOWHOOK_DIR}/include"
)
add_dependencies(boku targets_generated)

# Every hook signature has to keep parameters it does not read, so unused
# parameters are exempt; every other warning is fatal. Only the ported mod is
# held to this: the template sources are upstream code.
set_source_files_properties(${MOD_SOURCES} "${TARGETS_SOURCE}" PROPERTIES
  COMPILE_OPTIONS "-Wall;-Wextra;-Wshadow;-Wno-unused-parameter;-Werror")
set_source_files_properties(${MENU_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")

# Size and codegen: no exceptions or RTTI, one section per function and data
# item so the linker can drop what no hook reaches. Unwind tables stay on: a
# managed exception the game itself catches has to be able to pass through a
# hook frame, and a frame without CFI would abort the process instead.
target_compile_options(boku PRIVATE
  -Oz
  -flto
  -fno-exceptions
  -fno-rtti
  -funwind-tables
  -fno-stack-protector
  -fno-ident
  -fomit-frame-pointer
  -ffunction-sections
  -fdata-sections
)

target_link_options(boku PRIVATE
  -flto
  -Wl,--gc-sections
  # Inline hook engines identify proxies by address. Safe ICF only folds
  # functions whose address is never taken, so two hook proxies stay distinct.
  -Wl,--icf=safe
  -Wl,--strip-all
  -Wl,--exclude-libs,ALL
  -Wl,--build-id=none
  -Wl,--hash-style=gnu
  -Wl,--no-rosegment
  -Wl,-z,norelro
  -Wl,-z,max-page-size=16384
  -Wl,--pack-dyn-relocs=android
)

# ShadowHook: static archive built from the vendored source. dl: dlopen,
# dlsym, dladdr. log: Android logging used by the template, ShadowHook and the
# crash handler.
target_link_libraries(boku PRIVATE shadowhook dl log)

if(BOKU_HOOK_DEBUG)
  target_compile_definitions(boku PRIVATE BOKU_HOOK_DEBUG)
endif()

if(BOKU_LOG)
  target_compile_definitions(boku PRIVATE BOKU_LOG)
endif()

target_compile_definitions(boku PRIVATE BOKU_BUILD_COMMIT="${BOKU_BUILD_COMMIT_SHORT}")

# Hidden by default; JNI_OnLoad opts back in with JNIEXPORT.
set_target_properties(boku PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  OUTPUT_NAME "boku"
)
