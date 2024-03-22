cmake_minimum_required(VERSION 3.21) # Minimum CEF requirement, but building with the latest cmake is strongly recommended
project(Bolt VERSION 0.8 LANGUAGES C CXX)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Create a runtime directory structure in build dir to allow running Bolt without "make install"
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "staging-${CMAKE_BUILD_TYPE}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "staging-${CMAKE_BUILD_TYPE}")
# Generate compile_commands.json, for use by language servers for highlighting/autocomplete/etc
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Tell git to ignore everything in the new build directory
file(GENERATE OUTPUT .gitignore CONTENT "*")

# e.g. becomes "libbolt-plugin.so" on linux
set(BOLT_PLUGIN_LIB_NAME "bolt-plugin")

# Cache variables for all builds: variables that show up in CMake gui and CMake -L
# Some linux/mac specific cache vars are defined later
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef/dist" CACHE PATH "Root external CEF binary distribution")
set(BOLT_SKIP_LIBRARIES CACHE BOOL "Skip building the Bolt RS3 plugin api")
set(BOLT_META_NAME "BoltLauncher" CACHE STRING "Name of application in metadata. Flatpak build sets this to com.adamcake.Bolt")

if(NOT WIN32)
    include(GNUInstallDirs)

    # defines DESTINATION of shell script (relative to CMAKE_INSTALL_PREFIX, unless set to an absolute path)
    # things like flatpak need to override this
    set(BOLT_BINDIR "${CMAKE_INSTALL_BINDIR}" CACHE PATH "Set custom path for the launch wrapper script")

    # as above but lib instead of bin
    set(BOLT_LIBDIR "${CMAKE_INSTALL_LIBDIR}" CACHE PATH "Set installation root for CEF and Bolt itself. Defaults to lib/bolt-launcher")

    if(CMAKE_INSTALL_PREFIX STREQUAL "/")
        set(BOLT_FULL_LIBDIR "/${BOLT_LIBDIR}")
    else()
        set(BOLT_FULL_LIBDIR "${CMAKE_INSTALL_PREFIX}/${BOLT_LIBDIR}")
    endif()

    # defines DESTINATION of metadata like icons (relative to CMAKE_INSTALL_PREFIX, unless set to an absolute path)
    set(BOLT_SHAREDIR "${CMAKE_INSTALL_DATADIR}" CACHE PATH "Set custom share target")
endif()

    string(ASCII 74 97 103 101 120 32 108 97 117 110 99 104 101 114 BOLT_PROGRAM_NAME)
    string(ASCII 82 117 110 101 83 99 97 112 101 BOLT_GAME_NAME)
    string(TOLOWER "${BOLT_GAME_NAME}" BOLT_GAME_NAME_LOWER)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/share/bolt.desktop.in"
        "bolt.desktop"
        @ONLY)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/share/bolt.game.desktop.in"
        "bolt.game.desktop"
        @ONLY)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/share/metainfo.xml.in"
        "metainfo.xml"
        @ONLY)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/bolt-run.sh.in"
        "bolt-run.sh"
        @ONLY)

    # auto-generated helper for running from build dir; this file does not get installed
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/bolt.sh.in"
        "bolt.sh"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        @ONLY)

# source html directory
if(NOT BOLT_HTML_DIR)
    set(BOLT_HTML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/html")
endif()

# libcef C++ wrapper
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# Copy CEF runtime files to build tree
# Invokes an additional cmake script
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy-cef-runtime.cmake.in"
    "copy-cef-runtime.cmake"
    @ONLY)
add_custom_target(copy_cef_runtime
    ALL
    COMMAND ${CMAKE_COMMAND}
    -P"${CMAKE_CURRENT_BINARY_DIR}/copy-cef-runtime.cmake")

# select an implementation for OS-specific behaviour of launcher window
if(WIN32)
    set(WINDOW_LAUNCHER_OS_SPECIFIC src/browser/window_launcher_win.cxx)
else()
    set(WINDOW_LAUNCHER_OS_SPECIFIC src/browser/window_launcher_posix.cxx)
endif()

# compile an auto-generator, then use it to auto-generate a C++ file containing icon data
set(BOLT_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/icon")
add_executable(icon_gen icon/generator.cxx modules/lodepng/lodepng.cpp)
set_target_properties(icon_gen PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
add_custom_command(
    OUTPUT client_cmake_gen.cxx
    DEPENDS icon_gen ${BOLT_ICON_DIR}/16.png ${BOLT_ICON_DIR}/32.png ${BOLT_ICON_DIR}/64.png ${BOLT_ICON_DIR}/256.png
    COMMAND icon_gen "${CMAKE_CURRENT_SOURCE_DIR}" "${BOLT_ICON_DIR}" ">client_cmake_gen.cxx"
)

# compile the generator for embedded launcher file contents
if(NOT BOLT_DEV_LAUNCHER_DIRECTORY)
    add_executable(html_gen html/generator.cxx src/mime.cxx)
    set_target_properties(html_gen PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
    file(GLOB_RECURSE BOLT_HTML_FILE_LIST FOLLOW_SYMLINKS LIST_DIRECTORIES false CONFIGURE_DEPENDS "${BOLT_HTML_DIR}/*")
    add_custom_command(
        OUTPUT file_manager_launcher_gen.cxx
        DEPENDS html_gen ${BOLT_HTML_FILE_LIST}
        COMMAND html_gen "${CMAKE_CURRENT_SOURCE_DIR}" "${BOLT_HTML_DIR}" ${BOLT_HTML_FILE_LIST} ">file_manager_launcher_gen.cxx"
    )
    set(BOLT_FILE_MANAGER_LAUNCHER_GEN file_manager_launcher_gen.cxx)
endif()

# This line needs to be updated manually with any new/deleted object files; cmake discourages GLOBbing source files
add_executable(bolt
    modules/fmt/src/format.cc src/main.cxx src/browser.cxx src/browser/app.cxx src/browser/client.cxx
    src/browser/resource_handler.cxx src/browser/window_launcher.cxx ${WINDOW_LAUNCHER_OS_SPECIFIC}
    src/mime.cxx src/file_manager/directory.cxx client_cmake_gen.cxx ${BOLT_FILE_MANAGER_LAUNCHER_GEN}
)

# Various build properties
target_include_directories(bolt PUBLIC ${CEF_ROOT} modules/fmt/include)
set_target_properties(bolt PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
target_link_libraries(bolt PUBLIC libcef_dll_wrapper)
if(UNIX AND NOT APPLE)
    set_target_properties(bolt PROPERTIES INSTALL_RPATH "$ORIGIN")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/libcef.so")
    target_link_libraries(bolt PUBLIC "X11")
    target_link_libraries(bolt PUBLIC "xcb")
    target_link_libraries(bolt PUBLIC "archive")
elseif(MSVC)
    target_compile_options(bolt PUBLIC $<$<CONFIG:>:/MT> $<$<CONFIG:Debug>:/MTd> $<$<CONFIG:Release>:/MT>)
    set_target_properties(bolt PROPERTIES WIN32_EXECUTABLE TRUE)
    # note parentheses instead of brackets below - this variable is for substitution by Visual Studio, not cmake
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/$(ConfigurationName)/libcef.lib")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/$(ConfigurationName)/cef_sandbox.lib")
elseif(WIN32)
    # note: this is not used on Windows if compiler is MSVC; tht above section is used instead
    set_target_properties(bolt PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/libcef.lib")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/cef_sandbox.lib")
elseif(APPLE)
    # TODO: mac support
endif()

# compilation setting for enabling chromium dev tools
if(BOLT_DEV_SHOW_DEVTOOLS)
    target_compile_definitions(bolt PUBLIC BOLT_DEV_SHOW_DEVTOOLS)
endif()

# flag indicating flathub is creating this build, used only for some minor UI changes
if(BOLT_FLATHUB_BUILD)
    target_compile_definitions(bolt PUBLIC BOLT_FLATHUB_BUILD=1)
else()
    target_compile_definitions(bolt PUBLIC BOLT_FLATHUB_BUILD=0)
endif()

# compilation setting for directory-watching for launcher UI
if(BOLT_DEV_LAUNCHER_DIRECTORY)
    target_compile_definitions(bolt PUBLIC BOLT_DEV_LAUNCHER_DIRECTORY="${BOLT_HTML_DIR}")
endif()

# fake java wrapper for running hdos
if(NOT WIN32)
    add_executable(java-proxy src/fake_java.c)
    install(TARGETS java-proxy DESTINATION ${BOLT_LIBDIR}/bolt-launcher)
endif()

# Bolt uses GTK on all platforms, but it must specifically use gtk3
# to avoid symbols conflicting with CEF's GTK usage on Unix platforms
if(NOT BOLT_USE_SNI)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
target_include_directories(bolt PUBLIC ${GTK3_INCLUDE_DIRS})
target_link_libraries(bolt PUBLIC ${GTK3_LIBRARIES})
if(MSVC)
    target_link_directories(bolt PUBLIC ${GTK3_LIBRARY_DIRS})
endif()
endif()

# Snitray
if(BOLT_USE_SNI)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB2 REQUIRED glib-2.0 gio-2.0 dbusmenu-glib-0.4)
    add_library(snitray src/snitray/snitray.c)
    target_include_directories(snitray PRIVATE ${GLIB2_INCLUDE_DIRS})
    target_link_libraries(snitray PRIVATE ${GLIB2_LIBRARIES})
    target_link_libraries(bolt PRIVATE snitray)
    install(FILES src/snitray/StatusNotifierItem.xml DESTINATION "${BOLT_LIBDIR}/bolt-launcher/snitray")
    target_compile_definitions(bolt PRIVATE USE_SNI)
    target_compile_definitions(snitray PRIVATE
        BOLT_SNI_XML_PATH="${BOLT_FULL_LIBDIR}/bolt-launcher/snitray/StatusNotifierItem.xml"
        BOLT_SNI_META_NAME="${BOLT_META_NAME}")
endif()

# Install commands for target dir layout - as per https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md
if(APPLE)
    # TODO: mac support
else()
    install(TARGETS bolt DESTINATION ${BOLT_LIBDIR}/bolt-launcher)
    install(DIRECTORY "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/" "${CEF_ROOT}/Resources/" DESTINATION ${BOLT_LIBDIR}/bolt-launcher USE_SOURCE_PERMISSIONS)
endif()

# Build helper libraries
if(NOT BOLT_SKIP_LIBRARIES)
    # todo: anything other than linux
    if(UNIX AND NOT APPLE)
        add_library(${BOLT_PLUGIN_LIB_NAME} SHARED src/library/so/main.c src/library/plugin.c src/library/gl.c src/library/rwlock_posix.c modules/hashmap/hashmap.c)
        install(TARGETS ${BOLT_PLUGIN_LIB_NAME} DESTINATION "${BOLT_LIBDIR}")
    endif()
    target_include_directories(${BOLT_PLUGIN_LIB_NAME} PUBLIC "${CEF_ROOT}" "${BOLT_LUAJIT_INCLUDE_DIR}" modules/fmt/include)
    target_link_libraries(${BOLT_PLUGIN_LIB_NAME} luajit-5.1)
    target_compile_definitions(bolt PUBLIC BOLT_LIB_NAME="${BOLT_PLUGIN_LIB_NAME}")
endif()

# Finally, install shell script and metadata
if(NOT WIN32)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/bolt-run.sh" RENAME bolt DESTINATION ${BOLT_BINDIR})
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bolt.desktop" DESTINATION "${BOLT_SHAREDIR}/applications/" RENAME "${BOLT_META_NAME}.desktop")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bolt.game.desktop" DESTINATION "${BOLT_SHAREDIR}/applications/" RENAME "${BOLT_META_NAME}.${BOLT_GAME_NAME}.desktop")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/metainfo.xml" DESTINATION "${BOLT_SHAREDIR}/metainfo/" RENAME "${BOLT_META_NAME}.metainfo.xml")
    install(FILES "${BOLT_ICON_DIR}/16.png" DESTINATION "${BOLT_SHAREDIR}/icons/hicolor/16x16/apps/" RENAME "${BOLT_META_NAME}.png")
    install(FILES "${BOLT_ICON_DIR}/32.png" DESTINATION "${BOLT_SHAREDIR}/icons/hicolor/32x32/apps/" RENAME "${BOLT_META_NAME}.png")
    install(FILES "${BOLT_ICON_DIR}/64.png" DESTINATION "${BOLT_SHAREDIR}/icons/hicolor/64x64/apps/" RENAME "${BOLT_META_NAME}.png")
    install(FILES "${BOLT_ICON_DIR}/256.png" DESTINATION "${BOLT_SHAREDIR}/icons/hicolor/256x256/apps/" RENAME "${BOLT_META_NAME}.png")
endif()
