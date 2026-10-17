add_subdirectory(AudioCommon)
add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(DiscIO)
add_subdirectory(InputCommon)
add_subdirectory(UICommon)
add_subdirectory(VideoCommon)
add_subdirectory(VideoBackends)

if(ENABLE_NOGUI)
  add_subdirectory(DolphinNoGUI)
endif()

if(ENABLE_CLI_TOOL)
  add_subdirectory(DolphinTool)
endif()

if(ENABLE_QT)
  add_subdirectory(DolphinQt)
endif()

if (APPLE OR WIN32)
  add_subdirectory(UpdaterCommon)
endif()

if (APPLE AND ENABLE_AUTOUPDATE)
  add_subdirectory(MacUpdater)
endif()

if (WIN32 AND ENABLE_AUTOUPDATE)
  add_subdirectory(WinUpdater)
endif()

if (APPLE AND ENABLE_QT)
  set(DOLPHIN_MAC_BUNDLE "$<TARGET_BUNDLE_DIR:dolphin-emu>")

  add_custom_target(build_final_bundle ALL
    DEPENDS dolphin-emu)
  
  if (ENABLE_AUTOUPDATE)
    add_dependencies(build_final_bundle MacUpdater)

    add_custom_command(TARGET build_final_bundle
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      "${DOLPHIN_MAC_BUNDLE}/Contents/Helpers/Dolphin Updater.app"

      COMMAND cp -R
      "$<TARGET_BUNDLE_DIR:MacUpdater>/."
      "${DOLPHIN_MAC_BUNDLE}/Contents/Helpers/Dolphin Updater.app")

    if (MACOS_CODE_SIGNING)
      add_custom_command(TARGET build_final_bundle
        POST_BUILD
        COMMAND "${CMAKE_SOURCE_DIR}/Tools/mac-codesign.sh"
        "-t"
        "${MACOS_CODE_SIGNING_IDENTITY}"
        "${DOLPHIN_MAC_BUNDLE}/Contents/Helpers/Dolphin Updater.app")
    endif()
  endif()

  if (MACOS_CODE_SIGNING)
    add_custom_command(TARGET build_final_bundle
      POST_BUILD
      COMMAND "${CMAKE_SOURCE_DIR}/Tools/mac-codesign.sh"
      "-t"
      "-e" "${CMAKE_SOURCE_DIR}/Source/Core/DolphinQt/DolphinEmu$<$<CONFIG:Debug>:Debug>.entitlements"
      "${MACOS_CODE_SIGNING_IDENTITY}"
      "${DOLPHIN_MAC_BUNDLE}")
  endif()
endif()
