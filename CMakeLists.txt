#### Fast .stl viewer #####
# Original Project Author: Matt Keeter Copyright 2014-2024
# Author: Paul Tsouchlos Copyright 2017-2024

cmake_minimum_required(VERSION 3.16)

project(fstl)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FSTL_CHECK_FORMAT "Check source code formatting with clang-format" OFF)
if(FSTL_CHECK_FORMAT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)
    if(NOT CLANG_FORMAT_EXE)
        message(FATAL_ERROR "Could not find clang-format executable!")
    endif()

    file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS
        src/*.h
        src/*.cpp
        qt/*.h
        qt/*.cpp
        gl/*.h
        gl/*.cpp
        exe/*.h
        exe/*.cpp
    )

    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT_EXE}
            --Werror
            --style=file
            --dry-run
            ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking source code formatting"
        VERBATIM)
    add_custom_target(fix-format
        COMMAND ${CLANG_FORMAT_EXE}
            --style=file
            -i
            ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Fixing source code formatting"
        VERBATIM)
endif()

# Set the version number
set (FSTL_VERSION_MAJOR "0")
set (FSTL_VERSION_MINOR "13")
set (FSTL_VERSION_PATCH "0")
set (PROJECT_VERSION "${FSTL_VERSION_MAJOR}.${FSTL_VERSION_MINOR}.${FSTL_VERSION_PATCH}")

message(STATUS "Version: ${PROJECT_VERSION}")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#set project sources
set(Project_Sources src/app.cpp
src/animatedialog.cpp
src/backdrop.cpp
src/axis.cpp
src/canvas.cpp
src/cliexport.cpp
src/exportdialog.cpp
src/keybindingsdialog.cpp
src/offscreenrenderer.cpp
src/glmesh.cpp
src/loader.cpp
src/main.cpp
src/mesh.cpp
src/window.cpp
src/shaderlightprefs.cpp
src/statisticsdialog.cpp)

#set project headers.
set(Project_Headers src/app.h
src/animatedialog.h
src/backdrop.h
src/axis.h
src/canvas.h
src/cliexport.h
src/exportdialog.h
src/gif.h
src/keybindingsdialog.h
src/offscreenrenderer.h
src/glmesh.h
src/loader.h
src/mesh.h
src/window.h
src/shaderlightprefs.h
src/statisticsdialog.h)

#set project resources and icon resource
set(Project_Resources qt/qt.qrc gl/gl.qrc)
set(Icon_Resource exe/fstl.rc)

#set Policy CMP0072 FindOpenGL behavior
set(OpenGL_GL_PREFERENCE GLVND)

#find required packages: prefer Qt6, fall back to Qt5 (>= 5.15 for the
#Qt6-compatible APIs used throughout the code).
#Note: find_package(QT NAMES Qt6 Qt5) is NOT used because when both are
#installed the config-directory scan order (Qt5 sorts before Qt6) wins
#over the NAMES order, silently picking Qt5.
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    set(QT_VERSION ${Qt6_VERSION})
    set(FSTL_QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    set(QT_VERSION_MAJOR 5)
    set(QT_VERSION ${Qt5_VERSION})
    set(FSTL_QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL)
endif()
message(STATUS "Using Qt ${QT_VERSION}")

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

if(WIN32)
  enable_language(RC)
  add_executable(fstl WIN32 ${Project_Sources} ${Project_Headers} ${Project_Resources} ${Icon_Resource})
  if(MSVC)
    # Legacy MSVC behavior: pass the icon resource at link time
    set(Fstl_LINK_FLAGS ${CMAKE_CURRENT_SOURCE_DIR}/${Icon_Resource})
    set_target_properties(fstl PROPERTIES LINK_FLAGS ${Fstl_LINK_FLAGS})
  endif()
elseif(APPLE)
  add_executable(fstl MACOSX_BUNDLE ${Project_Sources} ${Project_Headers} ${Project_Resources} ${Icon_Resource})
else()
  add_executable(fstl ${Project_Sources} ${Project_Headers} ${Project_Resources})
endif(WIN32)

target_link_libraries(fstl ${FSTL_QT_LIBRARIES} OpenGL::GL Threads::Threads)

# Add version definitions to use within the code.
target_compile_definitions(fstl PRIVATE -DFSTL_VERSION="${PROJECT_VERSION}")

option(FSTL_BUILD_TESTS "Build the export verification test" OFF)
if(FSTL_BUILD_TESTS)
    add_executable(fstl_test_export
        test/test_export.cpp
        src/canvas.cpp src/canvas.h
        src/axis.cpp src/axis.h
        src/backdrop.cpp src/backdrop.h
        src/glmesh.cpp src/glmesh.h
        src/loader.cpp src/loader.h
        src/mesh.cpp src/mesh.h
        src/exportdialog.cpp src/exportdialog.h
        ${Project_Resources})
    target_link_libraries(fstl_test_export ${FSTL_QT_LIBRARIES} OpenGL::GL Threads::Threads)

    # Close-time teardown regression test (reuses the full app sources)
    set(Teardown_Sources ${Project_Sources})
    list(REMOVE_ITEM Teardown_Sources src/main.cpp)
    add_executable(fstl_test_teardown test/test_teardown.cpp ${Teardown_Sources} ${Project_Headers} ${Project_Resources})
    target_compile_definitions(fstl_test_teardown PRIVATE -DFSTL_VERSION="${PROJECT_VERSION}")
    target_link_libraries(fstl_test_teardown ${FSTL_QT_LIBRARIES} OpenGL::GL Threads::Threads)

    # Documentation screenshot generator (reuses the full app sources)
    add_executable(fstl_screenshot_gen test/screenshot_gen.cpp ${Teardown_Sources} ${Project_Headers} ${Project_Resources})
    target_compile_definitions(fstl_screenshot_gen PRIVATE -DFSTL_VERSION="${PROJECT_VERSION}")
    target_link_libraries(fstl_screenshot_gen ${FSTL_QT_LIBRARIES} OpenGL::GL Threads::Threads)
endif()

#installer information that is platform independent
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast viewer and exporter for STL 3D model files")
set(CPACK_PACKAGE_VERSION_MAJOR ${FSTL_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${FSTL_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${FSTL_VERSION_PATCH})

if(WIN32)

    set(QT_USE_QTMAIN true)

    if(MSVC)
        set_source_files_properties(fstl PROPERTIES LINKER_LANGUAGE "CXX")
        set_target_properties(fstl PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")

        install(TARGETS fstl DESTINATION bin COMPONENT all)

        if(QT_VERSION_MAJOR EQUAL 5)
            install(FILES
                $<TARGET_FILE:Qt5::Gui_EGL>
                $<TARGET_FILE:Qt5::Gui_GLESv2>
                $<TARGET_FILE:Qt5::Core>
                $<TARGET_FILE:Qt5::Gui>
                $<TARGET_FILE:Qt5::OpenGL>
                $<TARGET_FILE:Qt5::Widgets>
                DESTINATION bin COMPONENT all)

            #install file in the platforms directory.
            install (FILES
                ${Qt5Core_DIR}/../../../plugins/platforms/qwindows.dll
                DESTINATION bin/platforms COMPONENT all
            )
        else()
            # Qt6: deploy runtime DLLs with windeployqt at install time
            qt_generate_deploy_app_script(
                TARGET fstl
                OUTPUT_SCRIPT deploy_script
                NO_UNSUPPORTED_PLATFORM_ERROR
            )
            install(SCRIPT ${deploy_script} COMPONENT all)
        endif()

        #custom commands based on: https://gist.github.com/Rod-Persky/e6b93e9ee31f9516261b
        add_custom_command(TARGET fstl POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt${QT_VERSION_MAJOR}::Core>       $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt${QT_VERSION_MAJOR}::Gui>        $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt${QT_VERSION_MAJOR}::Widgets>    $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt${QT_VERSION_MAJOR}::OpenGL>     $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif(MSVC)

    # windows specific installer generation information
    set(CPACK_GENERATOR NSIS)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL OFF)
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN ${PROJECT_NAME})
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\fstl.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/fstl-app/fstl")
    set(CPACK_NSIS_DISPLAY_NAME "fstl ${FSTL_VERSION}")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/exe/fstl.ico")
    set(CPACK_NSIS_MUI_UNIICON  "${CMAKE_CURRENT_SOURCE_DIR}/exe/fstl.ico")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\fstl.lnk' '$INSTDIR\\\\bin\\\\fstl.exe'")
    set(CPACK_COMPONENTS_ALL all)
    if (CMAKE_CL_64)
        set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    else (CMAKE_CL_64)
        set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
    endif (CMAKE_CL_64)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME}")
    set(CPACK_SYSTEM_NAME "OSX")
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/app/fstl.icns")
else()
    install(TARGETS fstl RUNTIME DESTINATION bin)

    # Desktop integration: menu entry, icons, and STL file association
    install(FILES xdg/fstlapp-fstl.desktop DESTINATION share/applications)
    foreach(size 16 22 32 48 64 128 256)
        install(FILES xdg/icons/fstlapp-fstl_${size}x${size}.png
                DESTINATION share/icons/hicolor/${size}x${size}/apps
                RENAME fstlapp-fstl.png)
    endforeach()

    # AppStream metadata: the rich description shown by software centers
    install(FILES xdg/fstlapp-fstl.metainfo.xml DESTINATION share/metainfo)

    # Man page (gzipped per Debian policy; fall back to plain install)
    find_program(GZIP_EXECUTABLE gzip)
    if(GZIP_EXECUTABLE)
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fstl.1.gz
            COMMAND ${GZIP_EXECUTABLE} -9n -c ${CMAKE_CURRENT_SOURCE_DIR}/man/fstl.1 > ${CMAKE_CURRENT_BINARY_DIR}/fstl.1.gz
            DEPENDS man/fstl.1 VERBATIM)
        add_custom_target(manpage ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fstl.1.gz)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fstl.1.gz DESTINATION share/man/man1)
    else()
        install(FILES man/fstl.1 DESTINATION share/man/man1)
    endif()

    # Only generate package formats whose tools are available
    set(CPACK_GENERATOR "DEB")
    find_program(RPMBUILD_EXECUTABLE rpmbuild)
    if(RPMBUILD_EXECUTABLE)
        list(APPEND CPACK_GENERATOR "RPM")
    endif()

    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_PACKAGE_CONTACT "fstl-app <https://github.com/fstl-app/fstl>")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "fstl-app")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/fstl-app/fstl")
    # CPackDeb uses CPACK_PACKAGE_DESCRIPTION_SUMMARY as the synopsis and
    # indents this body itself ('.' marks paragraph breaks)
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "fstl is a very fast viewer for .stl 3D model files, designed to load
and render very high-polygon models instantly - 2 million triangles
at 60+ FPS on a mid-range laptop.
.
The viewer offers arcball rotation, panning, zoom about the cursor,
four draw modes (shaded, wireframe, surface angle, and a configurable
two-light shader), standard viewpoints, perspective or orthographic
projection, and automatic reload when the model changes on disk -
ideal alongside CAD and slicing tools.
.
Models can be exported as screenshots, rotation PNG series, and
animated turntable GIFs (looping up to 360 degrees or bouncing
between two angles), from the GUI or entirely from the command line
with configurable model color, transparent or colored backgrounds,
resolution limits, batch directory processing, and stdin pipelines.
See fstl(1) or 'fstl --help'.")
    # Derive runtime dependencies from the linked shared libraries
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    # MP4 export/recording shells out to ffmpeg at runtime
    set(CPACK_DEBIAN_PACKAGE_SUGGESTS "ffmpeg")
endif(WIN32)

include(CPack)
