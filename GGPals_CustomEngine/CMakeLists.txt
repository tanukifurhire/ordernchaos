# Define CMake version, project name
cmake_minimum_required(VERSION 3.15)
project(GGEngine)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CONFIGURATION_TYPES "DebugEditor;ReleaseEditor;ReleaseGame" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD")

# Configure ReleaseGame to inherit from Release
set(CMAKE_CXX_FLAGS_RELEASEGAME "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_RELEASEGAME "${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASEGAME "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASEGAME "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
set(CMAKE_STATIC_LINKER_FLAGS_RELEASEGAME "${CMAKE_STATIC_LINKER_FLAGS_RELEASE}")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd")

# Configure DebugEditor to inherit from Debug
set(CMAKE_CXX_FLAGS_DEBUGEDITOR "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_DEBUGEDITOR "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUGEDITOR "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
set(CMAKE_SHARED_LINKER_FLAGS_DEBUGEDITOR "${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
set(CMAKE_STATIC_LINKER_FLAGS_DEBUGEDITOR "${CMAKE_STATIC_LINKER_FLAGS_DEBUG}")

# Configure ReleaseEditor to inherit from Release (fast editor without debug overhead)
set(CMAKE_CXX_FLAGS_RELEASEEDITOR "${CMAKE_CXX_FLAGS_RELEASEGAME}")
set(CMAKE_C_FLAGS_RELEASEEDITOR "${CMAKE_C_FLAGS_RELEASEGAME}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASEEDITOR "${CMAKE_EXE_LINKER_FLAGS_RELEASEGAME}")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASEEDITOR "${CMAKE_SHARED_LINKER_FLAGS_RELEASEGAME}")
set(CMAKE_STATIC_LINKER_FLAGS_RELEASEEDITOR "${CMAKE_STATIC_LINKER_FLAGS_RELEASEGAME}")

# Define Directory structure
set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/GGEngine)
set(MATH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/GGMath)
set(NETWORK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/GGNetworking)
set(EXTERN_DIR ${ENGINE_DIR}/Extern)
set(EDITOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/GGEditor)
set(GAME_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LUAJIT_DIR ${EXTERN_DIR}/LuaJIT)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ENGINE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ENGINE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ENGINE_DIR}/bin)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUGEDITOR ${ENGINE_DIR}/lib/Debug)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASEEDITOR ${ENGINE_DIR}/lib/Release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASEGAME ${ENGINE_DIR}/lib/Release)
# Set Warning Level 4 for MSVC and equivalent for other compilers
if(MSVC)
    add_compile_options(/W4)
else()
    # For GCC/Clang - equivalent high warning level
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# GGMath (headers-only, VS project)
# Grab all headers automatically (no manual list)
file(GLOB_RECURSE GGMATH_HEADERS CONFIGURE_DEPENDS
    ${MATH_DIR}/include/GGMath/*.hpp
    ${MATH_DIR}/include/GGMath/*.h
)

# Make a STATIC target with only headers so VS shows "Header Files"
add_library(GGMath STATIC ${GGMATH_HEADERS})

# Put GGMath.lib with the other libs; empty lib is fine on MSVC
set_target_properties(GGMath PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${ENGINE_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_DEBUGEDITOR ${ENGINE_DIR}/lib/Debug
    ARCHIVE_OUTPUT_DIRECTORY_RELEASEGAME ${ENGINE_DIR}/lib/Release
    ARCHIVE_OUTPUT_DIRECTORY_RELEASEEDITOR ${ENGINE_DIR}/lib/Release
    LINKER_LANGUAGE CXX
)
target_include_directories(GGMath PUBLIC
    ${MATH_DIR}/include
)

file(GLOB_RECURSE NETWORK_SOURCES CONFIGURE_DEPENDS
    ${NETWORK_DIR}/src/*.cpp
    ${NETWORK_DIR}/src/*.c
    ${NETWORK_DIR}/include/*.h
    ${NETWORK_DIR}/include/*.hpp
)

add_library(GGNetworking STATIC ${NETWORK_SOURCES})

set_target_properties(GGNetworking PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${NETWORK_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_DEBUGEDITOR ${NETWORK_DIR}/lib/Debug
    ARCHIVE_OUTPUT_DIRECTORY_RELEASEGAME ${NETWORK_DIR}/lib/Release
    ARCHIVE_OUTPUT_DIRECTORY_RELEASEEDITOR ${NETWORK_DIR}/lib/Release
    LINKER_LANGUAGE CXX
)

target_include_directories(GGNetworking PUBLIC
    ${NETWORK_DIR}/include
)

target_compile_definitions(GGNetworking PRIVATE 
    $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING>
    $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
    $<$<CXX_COMPILER_ID:MSVC>:WIN32_LEAN_AND_MEAN>
    $<$<CXX_COMPILER_ID:MSVC>:NOGDI>
    $<$<CONFIG:DebugEditor>:_DEBUG>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:NDEBUG>
    $<$<OR:$<CONFIG:DebugEditor>,$<CONFIG:ReleaseEditor>>:GG_EDITOR_BUILD>
)

if(MSVC)
    target_compile_options(GGNetworking PRIVATE /MP)
endif()

# ---------------------------------------------------------------------------
# Steamworks SDK (optional)
#
# The SDK is not vendored: Valve's redistribution terms require accepting the
# agreement, so it cannot live in the repository. Drop the extracted SDK at
# GGEngine/Extern/steamworks_sdk and reconfigure; detection is automatic.
# Expected layout:
#
#   GGEngine/Extern/steamworks_sdk/
#     public/steam/steam_api.h
#     redistributable_bin/win64/steam_api64.lib
#     redistributable_bin/win64/steam_api64.dll
#
# Absent, everything still builds and plays: SteamPlatform, SteamLobby and
# SteamTransport each compile a stub shape that reports Steam as unavailable,
# and the game runs on the Winsock transport exactly as before. GG_STEAM_ENABLED
# is what separates the two.
#
# GG_ENABLE_STEAM=OFF forces the stub shape even with the SDK present - useful
# for confirming the kill-switch path still works before you rely on it.
# ---------------------------------------------------------------------------
option(GG_ENABLE_STEAM "Build with Steamworks integration when the SDK is present" ON)

set(STEAM_DIR ${EXTERN_DIR}/steamworks_sdk)
set(GG_STEAM_FOUND FALSE)

if(GG_ENABLE_STEAM
   AND EXISTS "${STEAM_DIR}/public/steam/steam_api.h"
   AND EXISTS "${STEAM_DIR}/redistributable_bin/win64/steam_api64.lib")
    set(GG_STEAM_FOUND TRUE)
endif()

if(GG_STEAM_FOUND)
    message(STATUS "Steamworks SDK found at ${STEAM_DIR} - Steam multiplayer enabled")

    # PUBLIC on all three: GGEngine and GGEditor include SteamPlatform.hpp and
    # SteamLobby.hpp, and GG_STEAM_ENABLED must be identical in every
    # translation unit. If it is not, SteamLobbyManager gets two different
    # sizes and layouts across the link and the resulting corruption is a
    # nightmare to attribute. This is why the define is PUBLIC, not PRIVATE.
    target_include_directories(GGNetworking PUBLIC ${STEAM_DIR}/public)
    target_link_directories(GGNetworking PUBLIC ${STEAM_DIR}/redistributable_bin/win64)
    target_link_libraries(GGNetworking PUBLIC steam_api64)
    target_compile_definitions(GGNetworking PUBLIC GG_STEAM_ENABLED)
else()
    if(GG_ENABLE_STEAM)
        message(STATUS "Steamworks SDK not found at ${STEAM_DIR} - "
                       "building without Steam (Winsock transport only)")
    else()
        message(STATUS "GG_ENABLE_STEAM=OFF - building without Steam (Winsock transport only)")
    endif()
endif()



# Fetch all engine source files (with CONFIGURE_DEPENDS)
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    ${ENGINE_DIR}/src/*.cpp
    ${ENGINE_DIR}/src/*.c
    ${ENGINE_DIR}/include/*.h
    ${ENGINE_DIR}/include/*.hpp
)


list(APPEND ENGINE_SOURCES
    ${EXTERN_DIR}/imgui/include/backends/imgui_impl_glfw.cpp
    ${EXTERN_DIR}/imgui/include/backends/imgui_impl_opengl3.cpp
)


# Creating static library for engine
add_library(GGEngine STATIC ${ENGINE_SOURCES})

set_target_properties(GGEngine PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${ENGINE_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY_DEBUGEDITOR ${ENGINE_DIR}/lib/Debug
    ARCHIVE_OUTPUT_DIRECTORY_RELEASEGAME ${ENGINE_DIR}/lib/Release
    ARCHIVE_OUTPUT_DIRECTORY_RELEASEEDITOR ${ENGINE_DIR}/lib/Release
)

target_include_directories(GGEngine PUBLIC
    ${ENGINE_DIR}/include
)

target_include_directories(GGEngine PRIVATE 
    ${EXTERN_DIR}/GLFW/include
    ${EXTERN_DIR}/glm/include
    ${EXTERN_DIR}/glew-2.1.0/include
    ${EXTERN_DIR}/SOIL2/include
    ${EXTERN_DIR}/FreeType/include
    ${MATH_DIR}/include
    ${EXTERN_DIR}/imgui/include
    ${EXTERN_DIR}/FMOD/inc
    ${LUAJIT_DIR}/include
    ${NETWORK_DIR}/include
)

target_include_directories(GGEngine SYSTEM PRIVATE
    ${EXTERN_DIR}/rapidjson
)

target_link_directories(GGEngine PRIVATE
    ${EXTERN_DIR}/GLFW/lib
    $<$<CONFIG:DebugEditor>:${EXTERN_DIR}/glew-2.1.0/lib/Debug/x64>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:${EXTERN_DIR}/glew-2.1.0/lib/Release/x64>
    ${EXTERN_DIR}/SOIL2/lib
    $<$<CONFIG:DebugEditor>:${EXTERN_DIR}/FreeType/lib/Debug/x64>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:${EXTERN_DIR}/FreeType/lib/Release/x64>
    $<$<CONFIG:DebugEditor>:${EXTERN_DIR}/imgui/lib/Debug/x64>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:${EXTERN_DIR}/imgui/lib/Release/x64>
    ${EXTERN_DIR}/FMOD/lib/x64
    ${LUAJIT_DIR}/lib/x64
    $<$<CONFIG:DebugEditor>:${NETWORK_DIR}/lib/Debug>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:${NETWORK_DIR}/lib/Release>
)

target_compile_definitions(GGEngine PRIVATE 
    GLEW_STATIC
    $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING>
    $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
    $<$<CXX_COMPILER_ID:MSVC>:WIN32_LEAN_AND_MEAN>
    $<$<CXX_COMPILER_ID:MSVC>:NOGDI>
    $<$<CONFIG:DebugEditor>:_DEBUG>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:NDEBUG>
    $<$<OR:$<CONFIG:DebugEditor>,$<CONFIG:ReleaseEditor>>:GG_EDITOR_BUILD>
)

if(MSVC)
    target_compile_options(GGEngine PRIVATE /MP)
endif()

# FIX: Link fmodL_vc for editor builds (logging enabled), fmod_vc for ReleaseGame only
target_link_libraries(GGEngine PRIVATE 
    glfw3
    $<$<CONFIG:DebugEditor>:glew32sd>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:glew32s>
    imgui
    soil2
    freetype
    $<$<OR:$<CONFIG:DebugEditor>,$<CONFIG:ReleaseEditor>>:fmodL_vc>
    $<$<CONFIG:ReleaseGame>:fmod_vc>
    lua51
)

if (WIN32)
target_link_libraries(GGEngine PRIVATE 
    opengl32
    gdi32
    user32
    kernel32
    shell32
    GGNetworking
)

set_target_properties(GGEngine PROPERTIES 
    LINK_FLAGS "/NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:MSVCRTD"
)

target_link_libraries(GGEngine PRIVATE 
    $<$<CONFIG:DebugEditor>:msvcrtd>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:msvcrt>
)
endif()

# Fetch all editor sources (with CONFIGURE_DEPENDS)
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
    ${EDITOR_DIR}/src/*.cpp
    ${EDITOR_DIR}/src/*.c
    ${EDITOR_DIR}/src/*.rc
    ${EDITOR_DIR}/include/*.h
    ${EDITOR_DIR}/include/*.hpp
)

# Application icon resource for embedding into executable
list(APPEND EDITOR_SOURCES ${ENGINE_DIR}/src/GameIcon.rc)

add_executable(GGEditor WIN32 ${EDITOR_SOURCES})

# Set different output names for Debug (editor) vs Release (game)
set_target_properties(GGEditor PROPERTIES
    OUTPUT_NAME_DEBUGEDITOR "GGEditor"
    OUTPUT_NAME_RELEASEEDITOR "GGEditor"
    OUTPUT_NAME_RELEASEGAME "OrderNChaos"
)

target_include_directories(GGEditor PRIVATE 
    ${EDITOR_DIR}/include
    ${ENGINE_DIR}/include
    ${EXTERN_DIR}/GLFW/include
    ${EXTERN_DIR}/glm/include
    ${EXTERN_DIR}/glew-2.1.0/include
    ${EXTERN_DIR}/SOIL2/include
    ${EXTERN_DIR}/FreeType/include
    ${MATH_DIR}/include
    ${EXTERN_DIR}/imgui/include
    ${EXTERN_DIR}/FMOD/inc
    ${LUAJIT_DIR}/include
)

target_include_directories(GGEditor SYSTEM PRIVATE
    ${EXTERN_DIR}/rapidjson
)

target_link_directories(GGEditor PRIVATE
    ${EXTERN_DIR}/GLFW/lib
    $<$<CONFIG:DebugEditor>:${EXTERN_DIR}/glew-2.1.0/lib/Debug/x64>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:${EXTERN_DIR}/glew-2.1.0/lib/Release/x64>
    ${EXTERN_DIR}/SOIL2/lib
    $<$<CONFIG:DebugEditor>:${EXTERN_DIR}/FreeType/lib/Debug/x64>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:${EXTERN_DIR}/FreeType/lib/Release/x64>
    $<$<CONFIG:DebugEditor>:${EXTERN_DIR}/imgui/lib/Debug/x64>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:${EXTERN_DIR}/imgui/lib/Release/x64>
    ${EXTERN_DIR}/FMOD/lib/x64
    ${LUAJIT_DIR}/lib/x64
    $<$<CONFIG:DebugEditor>:${NETWORK_DIR}/lib/Debug>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:${NETWORK_DIR}/lib/Release>
)

target_compile_definitions(GGEditor PRIVATE 
    GLEW_STATIC
    $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING>
    $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
    $<$<CXX_COMPILER_ID:MSVC>:WIN32_LEAN_AND_MEAN>
    $<$<CXX_COMPILER_ID:MSVC>:NOGDI>
    $<$<CONFIG:DebugEditor>:_DEBUG>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:NDEBUG>
    $<$<OR:$<CONFIG:DebugEditor>,$<CONFIG:ReleaseEditor>>:GG_EDITOR_BUILD>
)

if(MSVC)
    target_compile_options(GGEditor PRIVATE /MP)
endif()

target_link_libraries(GGEditor PRIVATE 
    glfw3
    $<$<CONFIG:DebugEditor>:glew32sd>
    $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:glew32s>
    soil2
    freetype
    imgui
    GGEngine
    lua51
)

if (WIN32)

    target_link_libraries(GGEditor PRIVATE 
        opengl32
        gdi32
        user32
        kernel32
        shell32
        GGNetworking
    )

    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GGEditor)

    # Run from the output directory when launched with F5, the way a shipped
    # build runs. Visual Studio otherwise starts the process in the generated
    # project directory, and the difference is not obvious because the engine's
    # "../../../GGProjects/..." asset paths happen to resolve from both - the
    # two directories sit at the same depth. Anything that is NOT reached by a
    # relative path breaks instead, and steam_appid.txt is exactly that:
    # SteamAPI_Init reads it from the working directory, so from VS it was not
    # found and Steam silently refused to initialise.
    set_target_properties(GGEditor PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:GGEditor>")

    set_target_properties(GGEditor PROPERTIES 
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:MSVCRTD"
    )

    target_link_libraries(GGEditor PRIVATE 
        $<$<CONFIG:DebugEditor>:msvcrtd>
        $<$<OR:$<CONFIG:ReleaseEditor>,$<CONFIG:ReleaseGame>>:msvcrt>
    )
endif()

# FIX: Copy only the correct FMOD DLL per config.
#   DebugEditor   -> fmodL.dll  (logging, debug sessions)
#   ReleaseEditor -> fmodL.dll  (logging still useful while authoring)
#   ReleaseGame   -> fmod.dll   (release, no logging overhead for end users)
if (WIN32)
    add_custom_command(TARGET GGEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:ReleaseGame>,${EXTERN_DIR}/FMOD/lib/x64/fmod.dll,${EXTERN_DIR}/FMOD/lib/x64/fmodL.dll>"
        $<TARGET_FILE_DIR:GGEditor>)

    add_custom_command(TARGET GGEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LUAJIT_DIR}/lib/x64/lua51.dll"
        $<TARGET_FILE_DIR:GGEditor>)

    if(GG_STEAM_FOUND)
        # steam_api64.dll has to sit beside the exe or SteamAPI_Init fails at
        # runtime with nothing useful in the log.
        add_custom_command(TARGET GGEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${STEAM_DIR}/redistributable_bin/win64/steam_api64.dll"
            $<TARGET_FILE_DIR:GGEditor>)

        # steam_appid.txt lets the game run straight from the build folder or
        # the debugger without launching through the Steam client. Written to
        # the output directory only - never committed, never packaged. Shipping
        # it would let any player spoof the App ID, which is why .gitignore
        # covers it and the packaging step must exclude it.
        add_custom_command(TARGET GGEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo 480 > "$<TARGET_FILE_DIR:GGEditor>/steam_appid.txt"
            COMMENT "Writing development steam_appid.txt (App ID 480, Spacewar) - DO NOT SHIP")
    endif()
endif()

set_property(TARGET GGEngine PROPERTY CXX_STANDARD 20)
set_property(TARGET GGEditor PROPERTY CXX_STANDARD 20)

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${ENGINE_SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${NETWORK_SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${EDITOR_SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${GGMATH_HEADERS})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")