function(dolphin_inject_version_info target)
  set(INFO_PLIST_PATH "$<TARGET_BUNDLE_DIR:${target}>/Contents/Info.plist")
  # CMake generates the bundle plist at configure time. Restore it when linking
  # too, so rebuilding a deleted/cleaned bundle cannot create a version-only plist.
  get_target_property(INFO_PLIST_TEMPLATE ${target} MACOSX_BUNDLE_INFO_PLIST)
  set(BASE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist")
  configure_file("${INFO_PLIST_TEMPLATE}" "${BASE_INFO_PLIST}")
  add_custom_command(TARGET ${target}
    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy
    "${BASE_INFO_PLIST}" "${INFO_PLIST_PATH}"

    COMMAND /usr/libexec/PlistBuddy -c
    "Delete :CFBundleShortVersionString"
    "${INFO_PLIST_PATH}"
    || true

    COMMAND /usr/libexec/PlistBuddy -c
    "Delete :CFBundleLongVersionString"
    "${INFO_PLIST_PATH}"
    || true

    COMMAND /usr/libexec/PlistBuddy -c
    "Delete :CFBundleVersion"
    "${INFO_PLIST_PATH}"
    || true

    COMMAND /usr/libexec/PlistBuddy -c
    "Merge '${CMAKE_BINARY_DIR}/Source/Core/VersionInfo.plist'"
    "${INFO_PLIST_PATH}")
endfunction()
