# Define the path to the entitlements file
set(ENTITLEMENTS_FILE ${CMAKE_SOURCE_DIR}/.github/macos/entitlements.plist)
set(MACOSX_BUNDLE_ICON_FILE "AppIcon")
set(MACOSX_BUNDLE_ICON_NAME "AppIcon")

# Set bundle properties
set_target_properties(SnowboardKids2Recompiled PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "SnowboardKids2Recompiled"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.snowboardkids2recompiled"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_ICON_FILE ${MACOSX_BUNDLE_ICON_FILE}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_BINARY_DIR}/Info.plist
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${ENTITLEMENTS_FILE}
)

# The .icon package is a valid macOS icon bundle produced by Icon Composer.
set(MACOS_ICON_PACKAGE ${CMAKE_SOURCE_DIR}/icons/macos_icon.icon)
set(BUNDLED_MACOS_ICON_PACKAGE ${CMAKE_BINARY_DIR}/AppIcon.icon)
set(MACOS_ICON_OUTPUT_DIR ${CMAKE_BINARY_DIR}/macos_icon_assets)
set(MACOS_ICON_ASSETS_CAR ${MACOS_ICON_OUTPUT_DIR}/Assets.car)
set(MACOS_ICON_ICNS_FILE ${MACOS_ICON_OUTPUT_DIR}/AppIcon.icns)
set(MACOS_ICON_INFO_PLIST ${MACOS_ICON_OUTPUT_DIR}/assetcatalog_generated_info.plist)

if(NOT EXISTS ${MACOS_ICON_PACKAGE}/icon.json)
    message(FATAL_ERROR "macOS icon bundle not found at ${MACOS_ICON_PACKAGE}")
endif()

file(GLOB MACOS_ICON_ASSET_FILES CONFIGURE_DEPENDS "${MACOS_ICON_PACKAGE}/Assets/*")

add_custom_command(
        OUTPUT ${MACOS_ICON_ASSETS_CAR} ${MACOS_ICON_ICNS_FILE} ${MACOS_ICON_INFO_PLIST}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${BUNDLED_MACOS_ICON_PACKAGE}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${MACOS_ICON_PACKAGE} ${BUNDLED_MACOS_ICON_PACKAGE}
        COMMAND xattr -cr ${BUNDLED_MACOS_ICON_PACKAGE}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${MACOS_ICON_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MACOS_ICON_OUTPUT_DIR}
        COMMAND xcrun actool ${BUNDLED_MACOS_ICON_PACKAGE}
                --app-icon ${MACOSX_BUNDLE_ICON_NAME}
                --compile ${MACOS_ICON_OUTPUT_DIR}
                --output-partial-info-plist ${MACOS_ICON_INFO_PLIST}
                --minimum-deployment-target 11.0
                --platform macosx
                --target-device mac
        DEPENDS ${MACOS_ICON_PACKAGE}/icon.json ${MACOS_ICON_ASSET_FILES}
        COMMENT "Compiling Icon Composer app icon"
        VERBATIM
)

add_custom_target(prepare_macos_icon ALL DEPENDS ${MACOS_ICON_ASSETS_CAR} ${MACOS_ICON_ICNS_FILE} ${MACOS_ICON_INFO_PLIST})
add_dependencies(SnowboardKids2Recompiled prepare_macos_icon)

# Configure Info.plist
configure_file(${CMAKE_SOURCE_DIR}/.github/macos/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist @ONLY)

# Install the app bundle
install(TARGETS SnowboardKids2Recompiled BUNDLE DESTINATION .)

# Ensure the entitlements file exists
if(NOT EXISTS ${ENTITLEMENTS_FILE})
    message(FATAL_ERROR "Entitlements file not found at ${ENTITLEMENTS_FILE}")
endif()

# Post-build steps for macOS bundle
add_custom_command(TARGET SnowboardKids2Recompiled POST_BUILD
    # Copy and fix frameworks first
    COMMAND ${CMAKE_COMMAND} -D CMAKE_BUILD_TYPE=$<CONFIG> -D CMAKE_GENERATOR=${CMAKE_GENERATOR} -P ${CMAKE_SOURCE_DIR}/.github/macos/fixup_bundle.cmake

    # Copy all resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/temp_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/temp_assets $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>/Contents/Resources/assets
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/temp_assets

    # Copy the compiled icon catalog for macOS 26+ and the .icns fallback for older macOS.
    COMMAND ${CMAKE_COMMAND} -E copy ${MACOS_ICON_ASSETS_CAR} $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>/Contents/Resources/Assets.car
    COMMAND ${CMAKE_COMMAND} -E copy ${MACOS_ICON_ICNS_FILE} $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>/Contents/Resources/AppIcon.icns
    COMMAND xattr -c $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>/Contents/Resources/Assets.car
    COMMAND xattr -c $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>/Contents/Resources/AppIcon.icns
    COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>/Contents/Resources/AppIcon.icon
    COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>/Contents/Resources/macos_icon.icon

    # Copy controller database
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/recompcontrollerdb.txt $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>/Contents/Resources/

    # Set RPATH
    COMMAND install_name_tool -add_rpath "@executable_path/../Frameworks/" $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>/Contents/MacOS/SnowboardKids2Recompiled

    # Sign the bundle
    COMMAND codesign --verbose=4 --options=runtime --no-strict --sign - --entitlements ${ENTITLEMENTS_FILE} --deep --force $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>
    COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_BUNDLE_DIR:SnowboardKids2Recompiled>

    COMMENT "Performing post-build steps for macOS bundle"
    VERBATIM
)
