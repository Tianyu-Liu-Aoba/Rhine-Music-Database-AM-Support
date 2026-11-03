cmake_minimum_required(VERSION 3.24)
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.4")
endif()
# æ­£å¼ç‰ˆæœ¬åœ¨æž„å»ºå‰æ³¨å…¥ï¼›å¼€å‘ç‰ˆä»…ä¿ç•™ç³»ç»Ÿèµ„æºæ‰€éœ€çš„æ•°å­—å ä½ã€‚
set(RHINE_VERSION
    "0.0.0"
    CACHE STRING "Application release version")
option(RHINE_RELEASE_BUILD "Build a versioned public release" OFF)
if(NOT RHINE_VERSION MATCHES "^(0|[1-9][0-9]*)[.](0|[1-9][0-9]*)[.](0|[1-9][0-9]*)$")
    message(FATAL_ERROR "RHINE_VERSION must use major.minor.patch")
endif()
project(
    RhineMusicDatabase
    VERSION ${RHINE_VERSION}
    # C is required by the vendored glslang/SPIRV-Tools that compiles the FidelityFX shader permutations at
    # build time.
    LANGUAGES CXX C)
# åœç”¨D3D12å…¥å£
set(RHINE_ENABLE_D3D12_BACKEND OFF)
add_compile_definitions(RHINE_ENABLE_D3D12_BACKEND=$<BOOL:${RHINE_ENABLE_D3D12_BACKEND}>)
set(RHINE_APPLICATION_ID "rhine.music.database")
find_package(Git QUIET)
set(RHINE_COMMIT_SHA "unknown")
set(RHINE_COMMIT_TITLE "æœªçŸ¥æäº¤")
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" log -1 --format=%h --abbrev=12
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE commitSha
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -c i18n.logOutputEncoding=UTF-8 log -1 --format=%s
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE commitTitle
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET ENCODING UTF-8)
    if(commitSha AND commitTitle)
        set(RHINE_COMMIT_SHA "${commitSha}")
        set(RHINE_COMMIT_TITLE "${commitTitle}")
    endif()
endif()
# æäº¤æ ‡é¢˜å¯èƒ½åŒ…å«å¼•å·å’Œåæ–œæ ï¼Œå†™å…¥ç”Ÿæˆå¤´æ–‡ä»¶å‰å…ˆè½¬ä¹‰ C++ å­—ç¬¦ä¸²ã€‚
string(REPLACE "\\" "\\\\" RHINE_COMMIT_TITLE "${RHINE_COMMIT_TITLE}")
string(REPLACE "\"" "\\\"" RHINE_COMMIT_TITLE "${RHINE_COMMIT_TITLE}")
configure_file(Qt/BuildIdentity.h.in "${CMAKE_CURRENT_BINARY_DIR}/BuildIdentity.h" @ONLY)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(
    Qt6 6.12 REQUIRED
    COMPONENTS Core
               Gui
               Qml
               Quick
               Network
               Concurrent
               ShaderTools
               Multimedia)
if(WIN32 OR (APPLE AND NOT IOS))
    find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets)
endif()
if(NOT TARGET Qt6::GuiPrivate)
    find_package(Qt6GuiPrivate REQUIRED)
endif()
if(NOT TARGET Qt6::MultimediaPrivate)
    find_package(Qt6MultimediaPrivate REQUIRED)
endif()
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
# æ ‡ç­¾è¯»å–
include(FetchContent)
set(BUILD_TESTING
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_BINDINGS
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS
    OFF
    CACHE BOOL "" FORCE)
FetchContent_Declare(
    utfcpp
    URL "${CMAKE_CURRENT_SOURCE_DIR}/vendor/utfcpp/utfcpp-4.0.6.tar.gz"
    URL_HASH SHA256=6920a6a5d6a04b9a89b2a89af7132f8acefd46e0c2a7b190350539e9213816c0)
FetchContent_MakeAvailable(utfcpp)
set(utf8cpp_INCLUDE_DIR
    "${utfcpp_SOURCE_DIR}/source"
    CACHE PATH "" FORCE)
FetchContent_Declare(
    taglib
    URL "${CMAKE_CURRENT_SOURCE_DIR}/vendor/taglib/taglib-2.1.1.tar.gz"
    URL_HASH SHA256=bd57924496a272322d6f9252502da4e620b6ab9777992e8934779ebd64babd6e)
FetchContent_MakeAvailable(taglib)
# AMD FidelityFXï¼ˆFSR3 ç»„ä»¶ + Vulkan åŽç«¯ï¼ŒDX12 ä»…åœ¨ Windows ä¸‹æŽ¥å…¥ï¼‰ã€‚ SDK ç›®æ ‡ç”± RhineMusicDatabase é“¾æŽ¥ï¼Œè§æ–‡ä»¶æœ«å°¾çš„
# FidelityFX::SDK æ®µã€‚
include(cmake/FidelityFX.cmake)
# è‡ªç»´æŠ¤æºç å’Œé”å®šçš„äºŒç»´ç åº“æž„æˆç‹¬ç«‹åº”ç”¨ç›®æ ‡ã€‚
set(RHINE_APP_SOURCES
    Qt/src/AppleCatalog.cpp
    Qt/src/AppleCatalog.h
    Qt/src/AppleData.cpp
    Qt/src/AppleData.h
    Qt/src/AppleMusic.cpp
    Qt/src/AppleMusic.h
    Qt/src/MediaSession.cpp
    Qt/src/MediaSession.h
    Qt/src/AppleQueue.cpp
    Qt/src/AppleQueue.h
    Qt/src/NowPlayingClient.cpp
    Qt/src/NowPlayingClient.h
    Qt/src/AppUpdater.cpp
    Qt/src/AppUpdater.h
    Qt/src/AudioPlayer.cpp
    Qt/src/AudioPlayer.h
    Qt/src/HttpAudioStream.cpp
    Qt/src/HttpAudioStream.h
    Qt/src/PlaybackDiagnostics.cpp
    Qt/src/PlaybackDiagnostics.h
    Qt/src/AppStorage.cpp
    Qt/src/AppStorage.h
    Qt/src/DiscGeometry.cpp
    Qt/src/DiscGeometry.h
    Qt/src/FrameProfiler.cpp
    Qt/src/FrameProfiler.h
    Qt/src/ProcessMetrics.cpp
    Qt/src/ProcessMetrics.h
    Qt/src/NativeCards.cpp
    Qt/src/NativeCards.h
    Qt/src/NativePicking.cpp
    Qt/src/NativePicking.h
    Qt/src/UiMotion.cpp
    Qt/src/UiMotion.h
    Qt/tests/NativeHostChecks.cpp
    Qt/tests/NativeMusicChecks.cpp
    Qt/tests/SessionChecks.cpp
    Qt/src/Icon.cpp
    Qt/src/main.cpp
    Qt/src/PlaybackConnection.cpp
    Qt/src/PlaybackConnection.h
    Qt/src/Host.cpp
    Qt/src/Host.h
    Qt/src/MusicApi.cpp
    Qt/src/MusicApi.h
    Qt/src/LyricsController.cpp
    Qt/src/LyricsController.h
    Qt/src/MusicSession.cpp
    Qt/src/MusicPlayer.cpp
    Qt/src/MusicPlayer.h
    Qt/src/PlaybackQueue.cpp
    Qt/src/PlaybackQueue.h
    Qt/src/MusicData.cpp
    Qt/src/MusicData.h
    Qt/src/LocalMusicLibrary.cpp
    Qt/src/LocalMusicLibrary.h
    Qt/src/LocalCollections.cpp
    Qt/src/LocalCollections.h
    Qt/src/MusicLocal.cpp
    Qt/src/MusicSiren.cpp
    Qt/src/NativeHudLocal.cpp
    Qt/src/MusicLibrary.cpp
    Qt/src/MusicLibrary.h
    Qt/src/NativeHud.h
    Qt/src/NativeHud.cpp
    Qt/src/NativeHudController.cpp
    Qt/src/NativeHudMusic.cpp
    Qt/src/NativeHudInput.cpp
    Qt/src/NativeHudDetail.cpp
    Qt/src/SongDetailCard.cpp
    Qt/src/CollectionDetailCard.cpp
    Qt/src/NativeHudPlayer.cpp
    Qt/src/PlayerBar.h
    Qt/src/PlaybackQueueCard.h
    Qt/src/PlaybackQueueCard.cpp
    Qt/src/PlayerBar.cpp
    Qt/src/NativeHudLibrary.cpp
    Qt/src/NativeHudSearch.cpp
    Qt/src/NativeHudDiscover.cpp
    Qt/src/SettingsSchema.cpp
    Qt/src/SettingsSchema.h
    Qt/src/SettingsCard.cpp
    Qt/src/SettingsCard.h
    Qt/src/NativeHudSettings.cpp
    Qt/src/NativeHudDesktopLyrics.cpp
    Qt/src/DesktopLyricsLayout.cpp
    Qt/src/DesktopLyricsLayout.h
    Qt/src/DesktopLyricsAppearance.cpp
    Qt/src/DesktopLyricsAppearance.h
    Qt/src/DesktopLyricsSettingsPage.cpp
    Qt/src/DesktopLyricsSettingsPage.h
    Qt/src/HudScroll.h
    Qt/src/DiscoverMusicCard.cpp
    Qt/src/DiscoverArtwork.cpp
    Qt/src/NativeHudModal.cpp
    Qt/src/NativeHudBoot.cpp
    Qt/src/SceneTypes.h
    Qt/src/SceneAppearance.h
    Qt/src/SceneAppearance.cpp
    Qt/src/NativeApplication.h
    Qt/src/NativeApplication.cpp
    Qt/src/NativeScene.h
    Qt/src/NativeScene.cpp
    Qt/src/NativeSceneUpdate.cpp
    Qt/src/NativeSceneCamera.cpp
    Qt/src/NativeSceneFrame.cpp
    Qt/src/NativeSceneInput.cpp
    Qt/src/NativeViewer.h
    Qt/src/NativeViewer.cpp
    Qt/src/SceneMath.h
    Qt/src/SceneMath.cpp
    Qt/src/SceneDrag.h
    Qt/src/SceneDrag.cpp
    Qt/src/SceneMotion.h
    Qt/src/SceneMotion.cpp
    Qt/src/BootTimeline.cpp
    Qt/src/MusicDirectory.cpp
    Qt/src/MusicAccount.cpp
    Qt/src/MusicMutation.cpp
    Qt/src/MusicDiscover.cpp
    Qt/src/MusicDiscover.h
    Qt/src/Platform.cpp
    Qt/src/Engine.cpp
    Qt/src/Engine.h
    Qt/src/DeferredWindowTransition.cpp
    Qt/src/DeferredWindowTransition.h
    Qt/src/Hud.cpp
    Qt/src/Hud.h
    Qt/src/RhiScene.cpp
    Qt/src/RhiScene.h
    Qt/src/render/MotionPass.cpp
    Qt/src/render/MotionPass.h
    Qt/src/render/ScenePostProcess.cpp
    Qt/src/render/ScenePostProcess.h
    Qt/src/fsr/Fsr3Backend.cpp
    Qt/src/fsr/Fsr3Backend.h
    Qt/src/render/SceneLayerHost.cpp
    Qt/src/render/SceneLayerHost.h
    Qt/src/render/PresentationWindow.cpp
    Qt/src/render/PresentationWindow.h
    Qt/src/fg/FgBackend.cpp
    Qt/src/fg/FgBackend.h
    Qt/src/fg/FgConfig.h
    Qt/src/render/PresentationBackend.h
    Qt/src/render/RhiPresentationBackend.cpp
    Qt/src/render/PresentationShell.cpp
    Qt/src/render/PresentationShell.h
    Qt/src/Glyphs.cpp
    Qt/src/Glyphs.h
    vendor/qrcodegen/qrcodegen.cpp)
if(APPLE)
    list(APPEND RHINE_APP_SOURCES Qt/src/render/SceneTransformHistory.h
         Qt/src/render/MetalFramePresentation.h Qt/src/render/MetalFrameQueue.h)
endif()
list(TRANSFORM RHINE_APP_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
endif()
# åŽŸç”Ÿä¸šåŠ¡å’Œåœºæ™¯ç”±åº”ç”¨ä¸Žç‹¬ç«‹å›žå½’å…±ç”¨ï¼Œæµ‹è¯•ä¸éœ€è¦å¤åˆ¶æ•´å¥—æºç ç›®æ ‡ã€‚ FSR ä¸Ž FG åŽç«¯åªè¢«æ¸²æŸ“å™¨ä½¿ç”¨ï¼Œè·Ÿ RhiScene ä¸€èµ·ç•™åœ¨åº”ç”¨ç›®æ ‡é‡Œï¼Œé¿å…æŠŠ FidelityFX ä¸Ž Vulkan
# é“¾æŽ¥è¿›æ¯ä¸ªå›žå½’æµ‹è¯•ã€‚è¿‡æ»¤æ¡ä»¶å¿…é¡»åŒæ—¶è¦†ç›– /fsr/ ä¸Ž /fg/ï¼šè¿™ä¸¤å¤„çš„å®žçŽ°ä¾èµ– Qt6::Quick ä¸Ž GuiPrivateï¼Œè€Œ RhineCore åªé“¾
# Core/Gui/Network/Concurrent/Multimediaã€‚
set(RHINE_CORE_SOURCES ${RHINE_APP_SOURCES})
list(FILTER RHINE_CORE_SOURCES EXCLUDE REGEX
     "/(main|Engine|Icon|RhiScene|Hud|FrameProfiler)\\.(cpp|h)$|/tests/|/fsr/|/fg/|/render/")
list(REMOVE_ITEM RHINE_APP_SOURCES ${RHINE_CORE_SOURCES})
# æ¡Œé¢åŽå°åªé“¾æŽ¥æ’­æ”¾æºç ï¼›åœºæ™¯ã€å­—å½¢ã€èµ„æ–™åº“ä¸Ž HUD ä¸è¿›å…¥æœåŠ¡äºŒè¿›åˆ¶ã€‚
set(RHINE_SYSTEM_CORE RhineCore)
if(NOT IOS)
    set(RHINE_PLAYBACK_SOURCES ${RHINE_CORE_SOURCES})
    list(
        FILTER
        RHINE_PLAYBACK_SOURCES
        INCLUDE
        REGEX
        "/(AppUpdater|AppStorage|AudioPlayer|HttpAudioStream|PlaybackDiagnostics|PlaybackConnection|Host|MusicApi|MusicPlayer|PlaybackQueue|LyricsController|Platform|AppleCatalog|AppleData|AppleMusic|AppleQueue|MediaSession|NowPlayingClient|MusicData)\\.(cpp|h)$|/qrcodegen\\.cpp$"
    )
    list(REMOVE_ITEM RHINE_CORE_SOURCES ${RHINE_PLAYBACK_SOURCES})
    add_library(RhinePlaybackCore STATIC ${RHINE_PLAYBACK_SOURCES})
    target_include_directories(
        RhinePlaybackCore
        PUBLIC Qt/src
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" vendor/qrcodegen)
    target_compile_definitions(
        RhinePlaybackCore
        PUBLIC RHINE_APPLICATION_ID="${RHINE_APPLICATION_ID}" RHINE_VERSION="${PROJECT_VERSION}"
               RHINE_RELEASE_BUILD=$<BOOL:${RHINE_RELEASE_BUILD}>)
    target_link_libraries(
        RhinePlaybackCore
        PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent Qt6::Multimedia
        PRIVATE Qt6::MultimediaPrivate)
    if(MSVC)
        target_compile_options(RhinePlaybackCore PRIVATE /utf-8)
    endif()
    set(RHINE_SYSTEM_CORE RhinePlaybackCore)
endif()
add_library(RhineCore STATIC ${RHINE_CORE_SOURCES})
target_include_directories(
    RhineCore
    PUBLIC Qt/src
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" vendor/qrcodegen "${taglib_SOURCE_DIR}/taglib"
            "${taglib_SOURCE_DIR}/taglib/toolkit" "${taglib_BINARY_DIR}")
target_compile_definitions(
    RhineCore PUBLIC RHINE_APPLICATION_ID="${RHINE_APPLICATION_ID}" RHINE_VERSION="${PROJECT_VERSION}"
                     RHINE_RELEASE_BUILD=$<BOOL:${RHINE_RELEASE_BUILD}>)
target_link_libraries(
    RhineCore
    PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent Qt6::Multimedia
    PRIVATE Qt6::MultimediaPrivate tag)
if(NOT IOS)
    target_link_libraries(RhineCore PUBLIC RhinePlaybackCore)
endif()
add_subdirectory(Qt)
target_link_libraries(RhineMusicDatabase PRIVATE RhineCore)
# ä½“ç§¯ä¼˜åŒ–ï¼šéšåŒ…çš„ 97 ä¸ªåº“éƒ½æ˜¯ Qt ä¸Žå‘è¡Œç‰ˆçš„ strip äºŒè¿›åˆ¶ï¼Œç¼–è¯‘å‚æ•°æ— ä»Žä¸‹æ‰‹ï¼Œèƒ½è‡ªå®šä¹‰çš„åªæœ‰è¿™ä¸¤ä¸ª
# ç›®æ ‡ã€‚Release å¼€ LTOï¼Œè®©é“¾æŽ¥æœŸåšè·¨ç¿»è¯‘å•å…ƒå†…è”ä¸Žæ­»ä»£ç æ¶ˆé™¤ã€‚
# åˆ»æ„ä¸åŠ  -fvisibility=hiddenï¼šå¯æ‰§è¡Œæ–‡ä»¶æœ¬æ¥å°±ä¸å¯¼å‡ºç¬¦å·ï¼Œçœä¸åˆ° 50 KBï¼Œå´å¯èƒ½æŒ¡ä½ dlsym
# ä¸€ç±»çš„è‡ªçœè·¯å¾„ï¼Œæ”¶ç›Šä¸Žé£Žé™©å€’æŒ‚ã€‚
#
# LTO è¦ä¸‰æ ·ä¸œè¥¿åŒæ—¶åœ¨ä½ï¼šç¼–è¯‘å™¨æ’ä»¶ï¼ˆGCC çš„ liblto_plugin.soï¼‰ã€å¸¦æ’ä»¶çš„å½’æ¡£å™¨
# ï¼ˆgcc-ar/gcc-ranlibâ€”â€”RhineCore æ˜¯é™æ€åº“ï¼ŒCMake å»ºåº“æ—¶ä¼šå¼ºåˆ¶æ¢æˆè¿™ä¸¤ä¸ªï¼‰ï¼Œä»¥åŠèƒ½åŠ è½½æ’ä»¶çš„
# é“¾æŽ¥å™¨ã€‚ç¼ºä»»ä¸€é¡¹ check_ipo_supported åªè¿”å›ž NOï¼Œäº§ç‰©æ‚„æ‚„å¤§ 1.5 MB è€Œæž„å»ºç…§å¸¸æˆåŠŸï¼Œå› æ­¤è¿™é‡Œ
# é»˜è®¤æ‰“è­¦å‘ŠæŠŠé™çº§æš´éœ²å‡ºæ¥ï¼›RHINE_REQUIRE_LTO=1 æŠŠå®ƒå‡çº§ä¸ºé…ç½®æœŸé”™è¯¯ï¼ŒCI çš„ Linux ä»»åŠ¡ç”¨è¿™ä¸ªå¼€å…³ã€‚
set(RHINE_REQUIRE_LTO OFF)
if(DEFINED ENV{RHINE_REQUIRE_LTO} AND "$ENV{RHINE_REQUIRE_LTO}" MATCHES "^(1|[Oo][Nn]|[Tt][Rr][Uu][Ee]|[Yy][Ee][Ss])$")
    set(RHINE_REQUIRE_LTO ON)
endif()
include(CheckIPOSupported)
check_ipo_supported(RESULT RHINE_IPO_SUPPORTED OUTPUT RHINE_IPO_ERROR LANGUAGES CXX)
if(RHINE_IPO_SUPPORTED)
    set_target_properties(
        RhineCore
        RhineMusicDatabase
        PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    message(STATUS "IPO/LTO enabled for Release (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
elseif(RHINE_REQUIRE_LTO)
    message(
        FATAL_ERROR
            "RHINE_REQUIRE_LTO is set but IPO/LTO is unavailable.\n"
            "Install a plugin-capable toolchain (GCC: gcc/g++/binutils provide liblto_plugin.so and"
            " gcc-ar), or unset RHINE_REQUIRE_LTO to accept a larger Release binary.\n${RHINE_IPO_ERROR}")
else()
    message(
        WARNING
            "IPO/LTO unavailable, Release builds are not optimized across translation units"
            " (expect ~1.5 MB larger binaries).\n${RHINE_IPO_ERROR}")
endif()
# Linux å†æŒ‰å‡½æ•°/æ•°æ®åˆ‡æ®µï¼Œé…åˆ --gc-sections å›žæ”¶ä¸»ç¨‹åºé‡ŒçœŸæ­£åˆ°ä¸äº†çš„éƒ¨åˆ†ã€‚
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(RhineCore PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(RhineMusicDatabase PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(RhineMusicDatabase PRIVATE -Wl,--gc-sections)
endif()
# FSR åªæŒ‚åœ¨åº”ç”¨ç›®æ ‡ä¸Šï¼šè®¾ç½®é¡µè¦æŒ‰ç¼–è¯‘æœŸèƒ½åŠ›ç½®ç°ï¼Œæ‰€ä»¥æŠŠå¯ç”¨æ€§ä¹Ÿä¼ æˆå®ã€‚ FSR1 æ˜¯çº¯ç€è‰²å™¨ï¼ˆEASU+RCASï¼‰ï¼Œä»»ä½•åŽç«¯éƒ½èƒ½ç”¨ï¼›FSR3 éœ€è¦ Vulkan æˆ– D3D12ã€‚
if(TARGET FidelityFX::SDK)
    target_link_libraries(RhineMusicDatabase PRIVATE FidelityFX::SDK)
endif()
if(RHINE_FFX_VK_AVAILABLE AND RHINE_FFX_SHADER_BLOBS_AVAILABLE)
    target_compile_definitions(RhineMusicDatabase PRIVATE RHINE_FSR3_VULKAN=1)
    # è®¾ç½®é¡µç¼–åœ¨ RhineCore é‡Œï¼Œåªå‡­"åŽç«¯å"çŒœä¸å‡ºæž„å»ºæ˜¯å¦çœŸç¼–è¿›äº† FSR3ï¼šåŒä¸€ä¸ªåŽç«¯ åœ¨ä¸åŒæž„å»ºé…ç½®ä¸‹å¯èƒ½å¯ç”¨ä¹Ÿå¯èƒ½ä¸å¯ç”¨ã€‚æŠŠèƒ½åŠ›å®ä¸€å¹¶ä¼ ç»™ RhineCoreï¼Œè®©ç½®ç°é€»è¾‘
    # æŒ‰å®žé™…ç¼–è¯‘ç»“æžœåˆ¤æ–­ã€‚è¿™é‡Œåªä¼ å®ï¼Œä¸é“¾æŽ¥ FidelityFXã€‚
    target_compile_definitions(RhineCore PRIVATE RHINE_FSR3_VULKAN=1)
    # ä¿æŒ Vulkan å‘ˆçŽ°åŽŸæœ‰çš„æž„å»ºé—¨æŽ§ï¼ŒåªæŠŠè®¾å¤‡/äº¤æ¢é“¾æ“ä½œç§»å…¥é€‚é…å™¨ã€‚
    target_compile_definitions(RhineMusicDatabase PRIVATE RHINE_VULKAN_PRESENTATION=1)
    target_sources(RhineMusicDatabase PRIVATE Qt/src/fg/VulkanPresenter.cpp Qt/src/fg/VulkanPresenter.h
                                              Qt/src/render/VulkanPresentationBackend.cpp)
endif()
if(RHINE_FFX_DX12_AVAILABLE)
    target_compile_definitions(RhineMusicDatabase PRIVATE RHINE_FSR3_DX12=1)
    target_compile_definitions(RhineCore PRIVATE RHINE_FSR3_DX12=1)
endif()
set_source_files_properties(Qt/qml/Main.qml TARGET_DIRECTORY RhineMusicDatabase PROPERTIES QT_RESOURCE_ALIAS
                                                                                           Main.qml)
target_include_directories(RhineMusicDatabase PRIVATE vendor/qrcodegen Qt/src)
qt_add_resources(
    RhineMusicDatabase
    "rhine_animation"
    PREFIX
    "/animation"
    BASE
    "Assets/animation"
    FILES
    Assets/animation/boot-tracks.json)
qt_add_resources(
    RhineMusicDatabase
    "rhine_icons"
    PREFIX
    "/icons"
    BASE
    "Assets/icons"
    FILES
    Assets/icons/icon-16.png
    Assets/icons/icon-32.png
    Assets/icons/icon-64.png
    Assets/icons/icon-128.png
    Assets/icons/icon-256.png
    Assets/icons/icon-512.png
    Assets/icons/icon-1024.png)
# ç€è‰²å™¨æŒ‰åŽŸé€šé“é¡ºåºæ‰“åŒ…ï¼Œæ‰€æœ‰åŽç«¯ä½¿ç”¨åŒä¸€ä»½å…¬å¼ã€‚ GLSL 130 å…¼å®¹ Windows éšåŒ…çš„è½¯ä»¶ OpenGLï¼Œå…¶ä»–åŽç«¯ä»ä¿ç•™åŽŸæœ‰ç‰ˆæœ¬ã€‚
qt_add_shaders(
    RhineMusicDatabase
    "rhine_shaders"
    PREFIX
    "/shaders"
    BASE
    "Qt/shaders"
    GLSL
    "130,150,300es"
    HLSL
    50
    MSL
    12
    FILES
    Qt/shaders/quad.vert
    Qt/shaders/copyColor.frag
    Qt/shaders/aaFragment.frag
    Qt/shaders/taa.frag
    Qt/shaders/motion.vert
    Qt/shaders/velocity.frag
    Qt/shaders/fsr1_easu.frag
    Qt/shaders/fsr1_rcas.frag
    Qt/shaders/fgComposite.vert
    Qt/shaders/fgComposite.frag)
# å”¯ä¸€åœºæ™¯çš„çƒ˜ç„™æè´¨ä¸Žæœ€ç»ˆåˆæˆç€è‰²å™¨
set(RHINE_OPTICAL_SHADER_DEFINES)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR APPLE)
    list(APPEND RHINE_OPTICAL_SHADER_DEFINES RHINE_SINGLE_PASS_MOTION=1)
endif()
if(APPLE)
    list(APPEND RHINE_OPTICAL_SHADER_DEFINES RHINE_METAL_MOTION=1)
endif()
# E05aï¼šç´§å‡‘å®žä¾‹ï¼ˆ80 å­—èŠ‚ï¼‰çš„ç¬¬äºŒå¥—ç€è‰²å™¨çƒ˜ç„™æ”¾åœ¨ç‹¬ç«‹å­ç›®å½•ï¼š
# qt_add_shaders æŠŠ .qsb å†™åˆ°å½“å‰äºŒè¿›åˆ¶ç›®å½•çš„ .qsb/ ä¸‹ï¼ŒåŒä¸€ç›®å½•å¯¹åŒä¸€ä»½æºç çƒ˜ä¸¤æ¬¡ä¼šæ’žè§„åˆ™ã€‚
add_subdirectory(Qt/shaders/compact)
if(APPLE)
    add_subdirectory(Qt/shaders/metal-temporal)
endif()
qt_add_shaders(
    RhineMusicDatabase
    "rhine_enhancement_shaders"
    PREFIX
    "/optical"
    BASE
    "Qt/shaders/enhancement"
    DEFINES
    ${RHINE_OPTICAL_SHADER_DEFINES}
    GLSL
    "130,150,300es"
    HLSL
    50
    MSL
    12
    FILES
    Qt/shaders/enhancement/opticalGeometry.vert
    Qt/shaders/enhancement/opticalQuad.vert
    Qt/shaders/enhancement/opticalSurface.frag
    Qt/shaders/enhancement/opticalFinalFragment.frag
    Qt/shaders/enhancement/opticalWarp.frag
    Qt/shaders/enhancement/opticalWarpEarly.frag
    Qt/shaders/enhancement/opticalWarpLate.frag
    Qt/shaders/enhancement/opticalWarpDebug.frag
    Qt/shaders/enhancement/opticalSurfaceNoMrt.frag)
target_compile_definitions(RhineMusicDatabase PRIVATE RHINE_APPLICATION_ID="${RHINE_APPLICATION_ID}"
                                                      $<$<CONFIG:Debug>:QT_QML_DEBUG>)
target_link_libraries(
    RhineMusicDatabase
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::GuiPrivate
            Qt6::Qml
            Qt6::Quick
            Qt6::Network
            Qt6::Concurrent
            Qt6::Multimedia)
# å¹³å°åˆ†æ”¯åªå¤„ç†åŽŸç”Ÿèƒ½åŠ›ã€éƒ¨ç½²èº«ä»½åŠé“¾æŽ¥ä¾èµ–ã€‚
if(NOT IOS)
    target_sources(RhineMusicDatabase PRIVATE Qt/tests/PlaybackServiceChecks.cpp
                                              Qt/tests/WindowTransitionChecks.cpp)
    if(WIN32 OR (APPLE AND NOT IOS))
        find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets)
        target_sources(RhineMusicDatabase PRIVATE Qt/src/DesktopTray.cpp Qt/src/DesktopTray.h)
        target_link_libraries(RhineMusicDatabase PRIVATE Qt6::Widgets)
    endif()
    # åŽå°å¯æ‰§è¡Œæ–‡ä»¶ä¸é“¾æŽ¥ Qt Quickã€QML æˆ–ä»»ä½•æ¸²æŸ“èµ„æºã€‚
    qt_add_executable(
        RhinePlaybackService
        WIN32
        Qt/src/playback_main.cpp
        Qt/src/PlaybackService.cpp
        Qt/src/PlaybackService.h
        Qt/src/InterfaceSession.cpp
        Qt/src/InterfaceSession.h)
    target_link_libraries(RhinePlaybackService PRIVATE RhinePlaybackCore)
    target_include_directories(RhinePlaybackService PRIVATE Qt/src)
    if(APPLE OR WIN32)
        target_sources(
            RhineMusicDatabase
            PRIVATE Qt/src/DesktopLyricsView.cpp
                    Qt/src/DesktopLyricsView.h
                    Qt/src/DesktopLyricsWindow.cpp
                    Qt/src/DesktopLyricsWindow.h
                    Qt/src/DesktopLyricsSettings.cpp
                    Qt/src/DesktopLyricsSettings.h
                    Qt/src/DesktopLyricsController.cpp
                    Qt/src/DesktopLyricsController.h)
    endif()
    if(WIN32)
        target_sources(RhineMusicDatabase PRIVATE Qt/src/DesktopLyricsWindowWindows.cpp)
        target_link_libraries(RhineMusicDatabase PRIVATE user32)
    endif()
    if(APPLE)
        target_sources(RhineMusicDatabase PRIVATE Qt/src/InterfaceWindowMac.mm
                                                  Qt/src/DesktopLyricsWindowMac.mm)
        target_compile_options(RhinePlaybackService PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
        target_link_libraries(RhinePlaybackService PRIVATE "-framework AppKit")
    endif()
    qt_add_resources(
        RhinePlaybackService
        "service_icons"
        PREFIX
        "/icons"
        BASE
        "Assets/icons"
        FILES
        Assets/icons/icon-256.png)
    if(MSVC)
        target_compile_options(RhinePlaybackService PRIVATE /utf-8)
    endif()
    if(APPLE)
        # helper è·Ÿéšä¸»åŒ…ä¸€èµ·ç­¾åå’Œéƒ¨ç½²ï¼›ä¸»ç›®æ ‡çš„æ‰“åŒ…é’©å­æ‰§è¡Œå‰å¿…é¡»å·²å®Œæˆæž„å»ºã€‚
        set_target_properties(
            RhinePlaybackService
            PROPERTIES MACOSX_BUNDLE TRUE
                       MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Qt/PlaybackServiceInfo.plist.in"
                       RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:RhineMusicDatabase>/../Helpers")
    elseif(UNIX)
        set_target_properties(RhinePlaybackService PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")
        install(TARGETS RhinePlaybackService RUNTIME DESTINATION bin)
    endif()
    add_dependencies(RhineMusicDatabase RhinePlaybackService)
endif()
if(APPLE)
    target_sources(RhineMusicDatabase PRIVATE Qt/src/render/MetalFxBackend.mm Qt/src/render/MetalFxBackend.h
                                              Qt/src/render/MetalTemporalResolve.cpp
                                              Qt/src/render/MetalTemporalResolve.h
                                              Qt/src/render/FrameGenerator.h Qt/src/render/SceneResources.h)
    target_link_libraries(RhineMusicDatabase PRIVATE "-framework Metal")
    # iOS æ¨¡æ‹Ÿå™¨ SDK ä¸å¸¦ MetalFXï¼Œä»æž„å»ºå…±ç”¨å‘ˆçŽ°å±‚å¹¶æŠ¥å‘ŠåŽŸç”Ÿæ•ˆæžœä¸å¯ç”¨ã€‚
    find_library(RHINE_METALFX_FRAMEWORK MetalFX)
    if(RHINE_METALFX_FRAMEWORK)
        target_link_libraries(RhineMusicDatabase PRIVATE "-weak_framework MetalFX")
    endif()
    target_compile_options(RhineMusicDatabase PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
endif()
if(IOS)
    target_sources(RhineCore PRIVATE Qt/src/PlatformIOS.mm Qt/src/BridgeProcessIOS.cpp Qt/src/BridgeProcess.h)
    target_link_libraries(RhineCore PRIVATE "-framework Security" "-framework UIKit" "-framework AVFAudio"
                                            "-framework MediaPlayer")
    target_compile_options(RhineCore PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    include(Qt/IOS.cmake)
elseif(APPLE)
    target_sources(${RHINE_SYSTEM_CORE} PRIVATE Qt/src/Platform.mm)
    target_link_libraries(${RHINE_SYSTEM_CORE} PRIVATE "-framework Security" "-framework AppKit"
                                                       "-framework MediaPlayer")
    target_compile_options(${RHINE_SYSTEM_CORE} PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    set_target_properties(
        RhineMusicDatabase
        PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER ${RHINE_APPLICATION_ID}
                   MACOSX_BUNDLE_BUNDLE_NAME "RHINE MUSIC DATABASE"
                   MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Qt/Info.plist.in")
elseif(WIN32)
    target_sources(RhineMusicDatabase PRIVATE Qt/src/WindowsGraphics.cpp Qt/src/WindowsTaskbar.cpp
                                              Qt/src/WindowsRoundedCorners.cpp
                                              Qt/tests/WindowsTaskbarChecks.cpp)
    target_link_libraries(RhineMusicDatabase PRIVATE dxgi ole32 uuid gdi32 dwmapi)
    if(RHINE_ENABLE_D3D12_BACKEND)
        target_link_libraries(RhineMusicDatabase PRIVATE d3d12)
    endif()
    # ffx_backend_vk é“¾æŽ¥äº† Vulkan å¯¼å…¥åº“ï¼Œè€Œè¯¥ä¾èµ– PUBLIC ä¼ æ’­åˆ°æœ¬ç›®æ ‡ï¼ŒäºŽæ˜¯ vulkan-1.dll æˆäº†åŠ è½½æœŸä¾èµ–ã€‚è¿™ä¸Ž README æ‰¿è¯ºçš„ã€ŒVulkan
    # ä¸å¯ç”¨æ—¶å›žé€€å¹³å°é»˜è®¤ã€ ç›´æŽ¥çŸ›ç›¾ï¼šç¼ºå°‘ Vulkan è¿è¡Œåº“çš„æœºå™¨ï¼ˆCI runnerã€è™šæ‹Ÿæœºã€è¿œç¨‹æ¡Œé¢ï¼‰ä¼šåœ¨è¿›ç¨‹å¯åŠ¨ é˜¶æ®µå°±æ‹¿åˆ° 0xc0000135ï¼Œmain.cpp é‡Œçš„
    # vulkanDeviceAvailable() æŽ¢æµ‹å’Œå›žé€€é€»è¾‘ æ ¹æœ¬æ²¡æœºä¼šæ‰§è¡Œã€‚æ”¹æˆå»¶è¿ŸåŠ è½½ï¼šä¸è°ƒç”¨ Vulkan å°±ä¸åŠ è½½ï¼ŒæŽ¢æµ‹å¤±è´¥å°±æ­£å¸¸å›žé€€ã€‚
    if(MSVC AND RHINE_FFX_VK_AVAILABLE)
        target_link_options(RhineMusicDatabase PRIVATE "/DELAYLOAD:vulkan-1.dll")
        target_link_libraries(RhineMusicDatabase PRIVATE delayimp)
    endif()
    configure_file(Qt/WindowsIcon.rc.in "${CMAKE_CURRENT_BINARY_DIR}/WindowsIcon.rc" @ONLY)
    target_sources(RhineMusicDatabase PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/WindowsIcon.rc")
    target_link_libraries(RhineCore PRIVATE psapi)
    target_sources(${RHINE_SYSTEM_CORE} PRIVATE Qt/src/PlatformWindows.cpp)
    target_link_libraries(${RHINE_SYSTEM_CORE} PRIVATE crypt32 shell32 user32 windowsapp runtimeobject
                                                       Qt6::MultimediaPrivate)
    target_compile_options(RhineMusicDatabase PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_compile_options(RhineCore PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
elseif(UNIX)
    # Linux å¤ç”¨åŒä¸€ä»½åŽŸç”Ÿåœºæ™¯å®žçŽ°ï¼›å‡­æ®ã€åè®®äº¤ä»˜å’Œåª’ä½“é›†æˆç”±æœ¬å¹³å°æ–‡ä»¶è¡¥é½ã€‚
    target_sources(${RHINE_SYSTEM_CORE} PRIVATE Qt/src/PlatformLinux.cpp)
    # æ˜¾å¡é€‰æ‹©ï¼šWindows é‚£æ¡ç›´è·¯æ˜¯ DXGI + QT_D3D_ADAPTER_INDEXï¼ŒLinux åªèƒ½æŠŠåºå·ç¿»è¯‘æˆ é©±åŠ¨è®¤è¯†çš„çŽ¯å¢ƒå˜é‡ï¼Œä¸”å¿…é¡»åœ¨ QGuiApplication
    # ä¹‹å‰å®Œæˆï¼Œæ‰€ä»¥å•ç‹¬æˆæ–‡ä»¶ã€åªæŒ‚ä¸»ç¨‹åºã€‚
    target_sources(RhineMusicDatabase PRIVATE Qt/src/LinuxGraphics.cpp)
    # Vulkan å‘ˆçŽ°èƒ½åŠ›æŽ¢æµ‹ï¼ˆDRI3ï¼‰ï¼šåªæŒ‚ä¸»ç¨‹åºï¼ŒX11 ç›¸å…³é€»è¾‘ä¸è¿›å…¥æ’­æ”¾æœåŠ¡ã€‚
    target_sources(RhineMusicDatabase PRIVATE Qt/src/LinuxX11.cpp Qt/src/LinuxX11.h)
    # å®‰è£…åŽçš„ç¨‹åºæŒ‰ AppImage å‰ç¼€æŸ¥æ‰¾åŒçº§ Qt è¿è¡Œæ—¶ï¼›æž„å»ºæ ‘ä»ç”¨ CMake ç”Ÿæˆçš„ kit è·¯å¾„ï¼Œ å¦åˆ™ CI ä¸­è£…åœ¨ç‹¬ç«‹ç›®å½•çš„ Qt ä¼šåœ¨è¿è¡Œæµ‹è¯•æ—¶æ‰¾ä¸åˆ°ã€‚
    set_target_properties(RhineMusicDatabase PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")
    install(TARGETS RhineMusicDatabase RUNTIME DESTINATION bin)
else()
    message(FATAL_ERROR "Supported desktop platforms are macOS, Linux and Windows")
endif()
# Qt Creator çš„æ™®é€šæž„å»ºä¹Ÿäº¤ä»˜å®Œæ•´åº”ç”¨ï¼Œä¸èƒ½ä¾èµ–è¿è¡Œç›®å½•é‡Œçš„ output/resourcesã€‚
option(RHINE_PACKAGE_APP "Package resources and Qt libraries after a macOS CMake build" ON)
if(APPLE
   AND NOT IOS
   AND RHINE_PACKAGE_APP)
    find_program(RHINE_NPM_EXECUTABLE NAMES npm REQUIRED)
    get_filename_component(RHINE_QT_KIT "${Qt6_DIR}/../../.." ABSOLUTE)
    # è„šæœ¬ã€å­—ä½“å’Œåœºæ™¯æ•°æ®æ”¹å˜æ—¶é‡æ–°é“¾æŽ¥ï¼Œç¡®ä¿ POST_BUILD é‡æ–°ç”Ÿæˆéƒ¨ç½²èµ„æºã€‚
    file(
        GLOB_RECURSE
        RHINE_RESOURCE_INPUTS
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*"
        "${CMAKE_CURRENT_SOURCE_DIR}/MusicBridge/client/*"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*")
    list(
        APPEND RHINE_RESOURCE_INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/package.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/MusicBridge/worker.cjs" "${CMAKE_CURRENT_SOURCE_DIR}/MusicBridge/qq.cjs"
        "${CMAKE_CURRENT_SOURCE_DIR}/MusicBridge/package-lock.json")
    set_property(
        TARGET RhineMusicDatabase
        APPEND
        PROPERTY LINK_DEPENDS ${RHINE_RESOURCE_INPUTS})
    add_custom_command(
        TARGET RhineMusicDatabase
        POST_BUILD
        COMMAND "${RHINE_NPM_EXECUTABLE}" run package:cmake -- "$<TARGET_BUNDLE_DIR:RhineMusicDatabase>"
                "${RHINE_QT_KIT}" "$<CONFIG>"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Packaging standalone music panel resources and Qt runtime"
        VERBATIM)
endif()
qt_finalize_executable(RhineMusicDatabase)
# æ¡Œé¢å›žå½’åŒ…å«å­è¿›ç¨‹å’Œæ¡Œé¢è·¯å¾„ï¼ŒiOS åªæž„å»ºæ­£å¼åº”ç”¨ã€‚
if(IOS)
    return()
endif()
enable_testing()
qt_add_executable(RhinePlatformRegression Qt/tests/PlatformRegression.cpp Qt/src/AppStorage.cpp
                  Qt/src/AudioPlayer.cpp Qt/src/HttpAudioStream.cpp Qt/src/ProcessMetrics.cpp)
target_include_directories(RhinePlatformRegression PRIVATE Qt/src)
target_link_libraries(RhinePlatformRegression PRIVATE Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::Network
                                                      Qt6::MultimediaPrivate)
if(WIN32)
    target_sources(RhinePlatformRegression PRIVATE Qt/src/PlatformWindows.cpp)
    target_link_libraries(
        RhinePlatformRegression
        PRIVATE Qt6::Qml
                Qt6::Network
                crypt32
                shell32
                user32
                windowsapp
                runtimeobject)
    target_compile_definitions(RhinePlatformRegression PRIVATE RHINE_APPLICATION_ID="${RHINE_APPLICATION_ID}")
    target_compile_options(RhinePlatformRegression PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
elseif(UNIX)
    # å‡­æ®æ–‡ä»¶çš„ä½ç½®å’Œæƒé™éšå¹³å°ä¸åŒï¼Œå›žå½’æµ‹è¯•ç›´æŽ¥ä½¿ç”¨çœŸå®žå®žçŽ°è€Œä¸æ˜¯æ¡©ã€‚
    target_sources(RhinePlatformRegression PRIVATE Qt/src/PlatformLinux.cpp)
endif()
add_test(NAME qt_platform_regression COMMAND RhinePlatformRegression
                                             "${CMAKE_CURRENT_BINARY_DIR}/platform-regression")
set_tests_properties(qt_platform_regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
qt_add_executable(RhineInputMethodRegression Qt/tests/InputMethodChecks.cpp
                  Qt/src/render/PresentationWindow.cpp Qt/src/render/PresentationWindow.h)
# æµ‹è¯•åœ¨å†…å­˜ä¸­åˆ›å»º QMLï¼›åŠ¨æ€ Qt kit ç›´æŽ¥åŠ è½½æ’ä»¶ï¼Œæ— éœ€é€’å½’æ‰«æä»“åº“å’Œéƒ¨ç½²äº§ç‰©ã€‚
set_target_properties(RhineInputMethodRegression PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
target_include_directories(RhineInputMethodRegression PRIVATE Qt/src)
target_link_libraries(RhineInputMethodRegression PRIVATE Qt6::Quick)
if(MSVC)
    target_compile_options(RhineInputMethodRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_input_method_regression COMMAND RhineInputMethodRegression)
set_tests_properties(qt_input_method_regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
qt_add_executable(RhineAudioPlaybackRegression Qt/tests/AudioPlaybackChecks.cpp)
target_link_libraries(RhineAudioPlaybackRegression PRIVATE Qt6::Core Qt6::Multimedia)
if(MSVC)
    target_compile_options(RhineAudioPlaybackRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_audio_playback_regression COMMAND RhineAudioPlaybackRegression)
# å¸§ç”Ÿæˆçš„çº¯é€»è¾‘ï¼ˆçŽ¯å¢ƒå˜é‡è§£æžã€èƒ½åŠ›å¿«ç…§ã€è®¾ç½®é¡µä¸‰æ€ã€å¸§è¾“å…¥æŠ•é€’ï¼‰ä¸ç¢° GPUï¼Œ æ‰€ä»¥å…¨å¹³å°éƒ½ç¼–ã€éƒ½è·‘ï¼šé—¨æŽ§å†™é”™çš„åŽæžœæ˜¯"è¯¥ç½®ç°çš„æ”¾å¼€äº†"ï¼Œåªæœ‰ç”¨ä¾‹æ‹¦å¾—ä½ã€‚
qt_add_executable(RhineFgCapabilityRegression Qt/tests/FgCapabilityChecks.cpp)
target_include_directories(RhineFgCapabilityRegression PRIVATE Qt/src)
target_link_libraries(RhineFgCapabilityRegression PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(RhineFgCapabilityRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_fg_capability_regression COMMAND RhineFgCapabilityRegression)

# æ¸²æŸ“ç­–ç•¥é‡Œèƒ½ç¦»çº¿åˆ¤å®šçš„é‚£ä¸€åŠï¼ˆGPU è®¡æ—¶å››æ€ã€æ—¶é—´æˆ³ç™»è®°ã€AA æ¨¡å¼æ˜¯å¦éœ€è¦è¿åŠ¨çŸ¢é‡ï¼‰æ˜¯ header-only çº¯é€»è¾‘ï¼Œ
# ä¸ç¢° QRhi/çª—å£ï¼Œæ‰€ä»¥ä¸‰å¹³å°éƒ½ç¼–ã€éƒ½è·‘ï¼šçœŸå€¼è¡¨å†™é”™ä¸ä¼šå´©ï¼Œåªä¼šè®©é¢æ¿æŠŠ"æ²¡è¯·æ±‚"è¯´æˆ"æ²¡æ ·æœ¬"ï¼Œåªæœ‰ç”¨ä¾‹æ‹¦å¾—ä½ã€‚
qt_add_executable(RhineRenderPolicyRegression Qt/tests/RenderChecks.cpp)
target_include_directories(RhineRenderPolicyRegression PRIVATE Qt/src)
target_link_libraries(RhineRenderPolicyRegression PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(RhineRenderPolicyRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_render_policy_regression COMMAND RhineRenderPolicyRegression)

if(APPLE)
    # æ¸²æŸ“å¸§åŽ†å²ä¸ä¾èµ–çª—å£ï¼Œè¦†ç›–ç¨³å®š ID é‡æŽ’ã€é›†åˆå¢žåˆ ã€çŸ©é˜µæˆªæ–­ä¸Žé‡ç½®ã€‚
    qt_add_executable(RhineSceneTransformHistoryRegression Qt/tests/SceneTransformHistoryChecks.cpp)
    target_include_directories(RhineSceneTransformHistoryRegression PRIVATE Qt/src)
    target_link_libraries(RhineSceneTransformHistoryRegression PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME qt_scene_transform_history_regression COMMAND RhineSceneTransformHistoryRegression)
endif()

# å¿«é€Ÿæ»šåŠ¨çš„å†·çº¹ç†å‡†å…¥ï¼ˆB01ï¼‰ä¸Žå°é¢éœ€æ±‚è´¦æœ¬ï¼ˆB03ï¼‰æ˜¯ header-only çº¯é€»è¾‘ï¼Œä¸ç¢° QRhi/çª—å£ï¼š
# è¾¹ç•Œå·® 1msã€é€»è¾‘å–æ¶ˆæå‰å½’è¿˜åé¢éƒ½ä¸ä¼šå´©ï¼Œåªä¼šè®©ç•Œé¢å˜æ…¢æˆ–å¹¶å‘æ…¢æ…¢å¤±æŽ§ï¼Œåªèƒ½é ç”¨ä¾‹æ‹¦ã€‚
# è®¡æ—¶å™¨ç”¨ä¾‹éœ€è¦äº‹ä»¶å¾ªçŽ¯ï¼Œæ‰€ä»¥åœ¨ main é‡Œå»º QCoreApplicationï¼ˆä¸å»º GUIï¼‰ã€‚
qt_add_executable(RhineMusicTextureAdmissionRegression Qt/tests/MusicTextureAdmissionChecks.cpp)
target_include_directories(RhineMusicTextureAdmissionRegression PRIVATE Qt/src)
target_link_libraries(RhineMusicTextureAdmissionRegression PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(RhineMusicTextureAdmissionRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_music_texture_admission_regression COMMAND RhineMusicTextureAdmissionRegression)

# C04ï¼šç”¨"å‘å¸ƒï¼æ¶ˆè´¹"åºå·åˆå¹¶è¿‡é‡å®Œæ•´ step æ˜¯ header-only çº¯ç­–ç•¥ï¼Œä¸è¯»æ—¶é’Ÿã€ä¸æŽ¥äº‹ä»¶å¾ªçŽ¯ã€‚
# å†™é”™çš„åŽæžœä¸æ˜¯å´©æºƒï¼šè¦ä¹ˆç™½ç™½å¤šç®—ä¸€ä»½ä¼šè¢«è¦†ç›–çš„ä¸­é—´çŠ¶æ€ï¼Œè¦ä¹ˆåŠ¨ç”»åœåœ¨åŠè·¯ï¼Œåªèƒ½é ç”¨ä¾‹æ‹¦ã€‚
qt_add_executable(RhineFramePacingRegression Qt/tests/FramePacingChecks.cpp)
target_include_directories(RhineFramePacingRegression PRIVATE Qt/src)
target_link_libraries(RhineFramePacingRegression PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(RhineFramePacingRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_frame_pacing_regression COMMAND RhineFramePacingRegression)

# D06ï¼šVulkan èµ„æºç­–ç•¥çš„çº¯ç”¨ä¾‹ï¼ˆarena æ­¥é•¿/åŠ¨æ€åç§»/å®žä¾‹æ± /æ·±åº¦æ¶ˆè´¹è€…/ç¼“å­˜ä»£å·ï¼‰ã€‚
# åªä¾èµ– Qt6::Coreï¼Œä¸å»º QGuiApplicationï¼Œä¹Ÿä¸ç¢° VkDeviceâ€”â€”ç¦»çº¿å°±èƒ½è·‘ã€‚
qt_add_executable(RhineVulkanResourceRegression Qt/tests/VulkanResourceChecks.cpp)
target_include_directories(RhineVulkanResourceRegression PRIVATE Qt/src)
target_link_libraries(RhineVulkanResourceRegression PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(RhineVulkanResourceRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_vulkan_resource_regression COMMAND RhineVulkanResourceRegression)

qt_add_executable(RhineSceneLifetimeRegression Qt/tests/SceneLifetimeChecks.cpp)
target_include_directories(RhineSceneLifetimeRegression PRIVATE Qt/src)
target_link_libraries(RhineSceneLifetimeRegression PRIVATE Qt6::Core)
add_test(NAME qt_scene_lifetime_regression COMMAND RhineSceneLifetimeRegression)

# Linux æ˜¾å¡é€‰æ‹©é‡Œèƒ½ç¦»çº¿éªŒè¯çš„éƒ¨åˆ†ï¼šPCI åœ°å€è§£æžã€åºå· â†’ çŽ¯å¢ƒå˜é‡çš„æ˜ å°„ã€åºå·å¤±æ•ˆæ—¶çš„å›žé€€ã€‚ å†™é”™çš„åŽæžœæ˜¯"é€‰é”™å¡"æˆ–"é…ç½®å¤±æ•ˆåŽä»æ³¨å…¥å˜é‡"ï¼Œåªèƒ½é ç”¨ä¾‹æ‹¦ã€‚ä¸‰å¹³å°éƒ½ç¼–ï¼Œ sysfs
# ç›¸å…³ç”¨ä¾‹åœ¨æºç é‡ŒæŒ‰å¹³å°è£å‰ªã€‚
qt_add_executable(RhineGpuChoiceRegression Qt/tests/LinuxGpuChecks.cpp Qt/src/LinuxGraphics.cpp
                  Qt/src/AppStorage.cpp)
target_include_directories(RhineGpuChoiceRegression PRIVATE Qt/src)
target_link_libraries(RhineGpuChoiceRegression PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(RhineGpuChoiceRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_gpu_choice_regression COMMAND RhineGpuChoiceRegression)

# X11 å‘ˆçŽ°èƒ½åŠ›åˆ¤å®šï¼šç¼º DRI3 æ—¶ä¸èƒ½ä¸Š Vulkanï¼ˆå¦åˆ™ Qt åˆ›å»ºäº¤æ¢é“¾å¤±è´¥åŽç›´æŽ¥ abortï¼‰ã€‚
# çº¯å‡½æ•° + æ—  DISPLAY çš„æŽ¢æµ‹å†’çƒŸï¼Œä¸éœ€è¦ X æœåŠ¡å™¨ï¼Œä¸‰å¹³å°éƒ½ç¼–ã€‚
qt_add_executable(RhineLinuxDisplayRegression Qt/tests/LinuxDisplayChecks.cpp Qt/src/LinuxX11.cpp)
target_include_directories(RhineLinuxDisplayRegression PRIVATE Qt/src)
target_link_libraries(RhineLinuxDisplayRegression PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(RhineLinuxDisplayRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_linux_display_regression COMMAND RhineLinuxDisplayRegression)

qt_add_executable(RhineAudioStreamRegression Qt/tests/AudioStreamChecks.cpp Qt/src/AudioPlayer.cpp
                  Qt/src/HttpAudioStream.cpp)
target_include_directories(RhineAudioStreamRegression PRIVATE Qt/src)
target_link_libraries(RhineAudioStreamRegression PRIVATE Qt6::Core Qt6::Multimedia Qt6::Network)
if(MSVC)
    target_compile_options(RhineAudioStreamRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_audio_stream_regression COMMAND RhineAudioStreamRegression)
add_test(NAME qt_offline COMMAND RhineMusicDatabase --self-test)
set_tests_properties(
    qt_offline
    PROPERTIES ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen;RHINE_RESOURCES=${CMAKE_CURRENT_SOURCE_DIR}/output/resources")

qt_add_executable(RhineGlyphsRegression Qt/tests/GlyphsChecks.cpp)
target_link_libraries(RhineGlyphsRegression PRIVATE RhineCore)
if(MSVC)
    target_compile_options(RhineGlyphsRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_glyphs_regression COMMAND RhineGlyphsRegression
                                           "${CMAKE_CURRENT_SOURCE_DIR}/output/resources")
if(WIN32)
    # offscreen æ’ä»¶ä¸ä½¿ç”¨ Windows å­—ä½“åº“ï¼Œç¼ºå­—å›žé€€éœ€è¦åŽŸç”Ÿå¹³å°æ’ä»¶ï¼›æµ‹è¯•æœ¬èº«ä¸åˆ›å»ºçª—å£ã€‚
    set_tests_properties(qt_glyphs_regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
else()
    set_tests_properties(qt_glyphs_regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

qt_add_executable(RhineNativeHudRegression Qt/tests/NativeHudChecks.cpp)
target_compile_definitions(RhineNativeHudRegression PRIVATE RHINE_HUD_REGRESSION_TEST_MAIN)
target_link_libraries(RhineNativeHudRegression PRIVATE RhineCore)
if(MSVC)
    target_compile_options(RhineNativeHudRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_native_hud_regression COMMAND RhineNativeHudRegression
                                               "${CMAKE_CURRENT_SOURCE_DIR}/output/resources")
set_tests_properties(qt_native_hud_regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(RhineUpdaterRegression Qt/tests/AppUpdaterChecks.cpp)
target_link_libraries(RhineUpdaterRegression PRIVATE RhineCore)
if(MSVC)
    target_compile_options(RhineUpdaterRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_updater_regression COMMAND RhineUpdaterRegression)
set_tests_properties(qt_updater_regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(RhineLocalMusicRegression Qt/tests/LocalMusicChecks.cpp)
target_link_libraries(RhineLocalMusicRegression PRIVATE RhineCore)
if(MSVC)
    target_compile_options(RhineLocalMusicRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_local_music_regression COMMAND RhineLocalMusicRegression
                                                "${CMAKE_CURRENT_SOURCE_DIR}/output/resources")
set_tests_properties(qt_local_music_regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(RhinePlaybackQueueRegression Qt/tests/PlaybackQueueChecks.cpp)
target_link_libraries(RhinePlaybackQueueRegression PRIVATE RhineCore)
if(MSVC)
    target_compile_options(RhinePlaybackQueueRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_playback_queue_regression COMMAND RhinePlaybackQueueRegression)

# Apple Music å…¬å¼€ç›®å½•ï¼šæ¡ç›®æ˜ å°„ã€åˆ†é¡µè¾¹ç•Œä¸Žåªè¯»æŽ¥å£çš„ç¦»çº¿å›žå½’ã€‚
qt_add_executable(RhineAppleMusicRegression Qt/tests/AppleMusicChecks.cpp)
target_link_libraries(RhineAppleMusicRegression PRIVATE RhineCore)
if(MSVC)
    target_compile_options(RhineAppleMusicRegression PRIVATE /utf-8)
endif()
add_test(NAME qt_apple_music_regression COMMAND RhineAppleMusicRegression)

# FG é—¸é—¨æŽ¢é’ˆï¼šéªŒè¯ã€ŒsetQueueSubmitParams å¯¹ç¦»å±å¸§ç”Ÿæ•ˆã€ä¸Žã€Œå…±äº« device ä¸Šèƒ½å»ºäº¤æ¢é“¾ã€ã€‚ éœ€è¦çœŸå®ž GPU + æ˜¾ç¤ºæœåŠ¡ï¼ˆæˆ– xvfb + lavapipeï¼‰ï¼ŒCI runner æ²¡æœ‰
# GPUï¼Œæ‰€ä»¥**ä¸è¿›é»˜è®¤ CTest é›†åˆ**ï¼šæž„å»ºåŽç”¨ `output/qt-build/RhineFgGateProbe` æ‰‹åŠ¨è¿è¡Œï¼Œç”¨å®ƒç»™å‡º "FG æž¶æž„æˆä¸æˆç«‹"çš„ç¡¬ç»“è®ºã€‚
if(RHINE_FFX_VK_AVAILABLE AND TARGET Qt6::GuiPrivate)
    # EXCLUDE_FROM_ALLï¼šè¿™æ˜¯æŽ’éšœç”¨çš„ä¸€æ¬¡æ€§æŽ¢é’ˆï¼Œä¸è¯¥å ç”¨é»˜è®¤æž„å»ºä¸Ž CI çš„æ—¶é—´ã€‚ éœ€è¦æ—¶æ˜¾å¼ cmake --build output/qt-build --target
    # RhineFgGateProbeã€‚
    qt_add_executable(RhineFgGateProbe Qt/tests/FgGateProbe.cpp)
    set_target_properties(RhineFgGateProbe PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(RhineFgGateProbe PRIVATE Qt/src)
    target_sources(RhineFgGateProbe PRIVATE Qt/src/fg/VulkanPresenter.cpp Qt/src/fg/VulkanPresenter.h)
    target_compile_definitions(RhineFgGateProbe PRIVATE RHINE_FFX_VK_AVAILABLE=1)
    target_link_libraries(RhineFgGateProbe PRIVATE Qt6::Quick Qt6::GuiPrivate)
    if(MSVC)
        target_compile_options(RhineFgGateProbe PRIVATE /utf-8)
    else()
        # ä¿ç•™ç¬¦å·ï¼šæŽ¢é’ˆå´©åœ¨ Qt å†…éƒ¨æ—¶ï¼Œåªæœ‰å¸¦ -g æ‰èƒ½çœ‹å‡ºæ˜¯å“ªä¸€æ­¥ã€‚
        target_compile_options(RhineFgGateProbe PRIVATE -g)
        target_link_options(RhineFgGateProbe PRIVATE -g)
    endif()
endif()
