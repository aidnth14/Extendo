add_executable(citra_meta
    citra.rc
    main.cpp
    precompiled_headers.h
)

set_target_properties(citra_meta PROPERTIES OUTPUT_NAME "extendo")

if (BSD STREQUAL "NetBSD")
    include(DisablePaxMprotect)
    disable_pax_mprotect(citra_meta)
endif()

if (APPLE)
    set(DIST_DIR "../../dist/apple")
    set(APPLE_RESOURCES
        "${DIST_DIR}/azahar.icns"
        "${DIST_DIR}/launch_logo.png"
    )
    if (IOS)
        list(APPEND APPLE_RESOURCES "${DIST_DIR}/LaunchScreen.storyboard")
    endif()
    target_sources(citra_meta PRIVATE ${APPLE_RESOURCES})

    # Define app bundle metadata.
    include(GenerateBuildInfo)
    set(SRC_DIR "${PROJECT_SOURCE_DIR}")
    generate_build_info()
    set_target_properties(citra_meta PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${DIST_DIR}/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Extendo"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.extendo-emu.extendo"
        MACOSX_BUNDLE_BUNDLE_VERSION "${BUILD_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${BUILD_FULLNAME}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${BUILD_FULLNAME}"
        MACOSX_BUNDLE_ICON_FILE "azahar.icns"
        RESOURCE "${APPLE_RESOURCES}"
    )

    if (IOS)
        set_target_properties(citra_meta PROPERTIES
            # Have Xcode copy and sign MoltenVK into app bundle.
            XCODE_EMBED_FRAMEWORKS "${MOLTENVK_LIBRARY}"
            XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY YES
            XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
            # Support iPhone and iPad.
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        )
    endif()
endif()

# Home-screen icons and the custom UI font ship as loose files (not compiled into a .qrc) so
# they can be swapped without a rebuild — which means the build has to actually place them next
# to the binary. Without these rules the app silently falls back to blank icons and the system
# font, since nothing else copies them.
if (ENABLE_QT)
    if (APPLE AND NOT IOS)
        set(HOME_ASSET_DEST "$<TARGET_BUNDLE_CONTENT_DIR:citra_meta>/Resources")
    else()
        set(HOME_ASSET_DEST "$<TARGET_FILE_DIR:citra_meta>")
    endif()
    add_custom_command(TARGET citra_meta POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${HOME_ASSET_DEST}/home_icons"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PROJECT_SOURCE_DIR}/dist/home_screen_icons"
                "${HOME_ASSET_DEST}/home_icons"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/dist/fonts/extendo_ui.ttf"
                "${HOME_ASSET_DEST}/extendo_ui.ttf"
        COMMENT "Copying home screen icons and UI font into the app"
        VERBATIM
    )

    # macOS privacy (TCC) grants are keyed on the code signature's designated requirement. A
    # linker-signed/ad-hoc binary has no stable identity — its requirement is the cdhash, which
    # changes on every single relink, so macOS treats each rebuild as a brand new app and
    # re-prompts for Downloads/Documents access forever. Signing with a fixed local certificate
    # pins the requirement to "bundle id + cert", which stays constant across rebuilds, so the
    # permission is granted once and then remembered.
    #
    # This must run AFTER the resource copy above: codesign seals the bundle contents, so adding
    # files afterwards would invalidate the signature.
    if (APPLE AND NOT IOS AND MACOS_CODESIGN_IDENTITY)
        add_custom_command(TARGET citra_meta POST_BUILD
            COMMAND sh -c
                "codesign --force --deep --sign '${MACOS_CODESIGN_IDENTITY}' '$<TARGET_BUNDLE_DIR:citra_meta>' || echo 'warning: codesign with ${MACOS_CODESIGN_IDENTITY} failed; falling back to ad-hoc signature (macOS will re-prompt for file access after each rebuild)'"
            COMMENT "Signing app with stable local identity (${MACOS_CODESIGN_IDENTITY})"
            VERBATIM
        )
    endif()
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND MINGW)
    # TODO: Do this for all executables, not just citra_meta
    # TODO: Don't hardcode MXE directory?
    set(dllwalker_command "${CMAKE_SOURCE_DIR}/externals/dllwalker/dllwalker.rb"
                          $<TARGET_FILE:citra_meta>
                          /opt/mxe/usr/x86_64-w64-mingw32.shared/bin/
                          /opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/bin/
                          /opt/mxe/usr/x86_64-w64-mingw32.shared/x86_64-w64-mingw32/bin/)
    set(dll_list_filename "${CMAKE_CURRENT_BINARY_DIR}/required_dlls_list.txt")
    add_custom_command(TARGET citra_meta POST_BUILD
        COMMAND ${dllwalker_command} > ${dll_list_filename}
    )
    add_custom_command(TARGET citra_meta POST_BUILD
        COMMAND cat ${dll_list_filename} | xargs -I {} ${CMAKE_COMMAND} -E copy {} $<TARGET_FILE_DIR:citra_meta>
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:citra_meta>/plugins/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory /opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/plugins/ "$<TARGET_FILE_DIR:citra_meta>/plugins/"
        COMMAND_EXPAND_LISTS
        DEPENDS ${dll_list_filename}
    )
endif()

target_link_libraries(citra_meta PRIVATE citra_cli citra_common fmt)

if (ENABLE_QT)
    target_link_libraries(citra_meta PRIVATE citra_qt)
endif()

if (ENABLE_ROOM)
    target_link_libraries(citra_meta PRIVATE citra_room)
endif()

if (ENABLE_QT AND ENABLE_DISCORD_RPC)
    target_link_libraries(citra_meta PRIVATE discord-rpc)
endif()

if(WIN32)
    # compile as a win32 gui application instead of a console application
    if(MSVC)
        set_target_properties(citra_meta PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    elseif(MINGW)
        set_target_properties(citra_meta PROPERTIES LINK_FLAGS_RELEASE "-mwindows")
    endif()
endif()

if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(citra_meta PRIVATE precompiled_headers.h)
endif()

# Bundle in-place on MSVC so dependencies can be resolved by builds.
if (ENABLE_QT AND MSVC)
    include(BundleTarget)
    qt_bundle_target_in_place(citra_meta)
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS citra_meta RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()
