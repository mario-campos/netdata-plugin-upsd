# SPDX-License-Identifier: GPL-3.0-or-later
# CMake module to handle fetching and installing the dashboard code

include(NetdataUtil)

function(handle_braindead_versioning_insanity prefix)
  if(IS_DIRECTORY "${prefix}/v2" AND NOT IS_DIRECTORY "${prefix}/v3")
    message(STATUS "  Fixing incorrectly versioned paths generated by poorly written CI")
    file(RENAME "${prefix}/v2" "${prefix}/v3")

    if(IS_DIRECTORY "${prefix}/v3" AND NOT IS_DIRECTORY "${prefix}/v2")
      message(STATUS "  Fixing incorrectly versioned paths generated by poorly written CI -- Done")
    else()
      message(FATAL_ERROR "Failed to fix incorrectly versioned paths")
    endif()
  endif()
endfunction()

# Bundle the dashboard code for inclusion during install.
#
# This is unfortunately complicated due to how we need to handle the
# generation of the CMakeLists file for the dashboard code.
function(bundle_dashboard)
  include(ExternalProject)

  set(dashboard_src_dir "${CMAKE_CURRENT_BINARY_DIR}/dashboard-src")
  set(dashboard_src_prefix "${dashboard_src_dir}/dist/agent")
  set(dashboard_bin_dir "${CMAKE_CURRENT_BINARY_DIR}/dashboard-bin")
  set(DASHBOARD_URL "https://app.netdata.cloud/agent.tar.gz" CACHE STRING
      "URL used to fetch the local agent dashboard code")

  message(STATUS "Preparing local agent dashboard code")

  message(STATUS "  Fetching ${DASHBOARD_URL}")
  file(DOWNLOAD
       "${DASHBOARD_URL}"
       "${CMAKE_CURRENT_BINARY_DIR}/dashboard.tar.gz"
       TIMEOUT 180
       STATUS fetch_status)

  list(GET fetch_status 0 result)

  if(result)
    message(FATAL_ERROR "Failed to fetch dashboard code")
  else()
    message(STATUS "  Fetching ${DASHBOARD_URL} -- Done")
  endif()

  message(STATUS "  Extracting dashboard code")
  extract_gzipped_tarball(
    "${CMAKE_CURRENT_BINARY_DIR}/dashboard.tar.gz"
    "${dashboard_src_dir}"
  )
  message(STATUS "  Extracting dashboard code -- Done")

  handle_braindead_versioning_insanity("${dashboard_src_prefix}")

  message(STATUS "  Generating CMakeLists.txt file for dashboard code")
  set(rules "")

  subdirlist(dash_dirs "${dashboard_src_prefix}")

  foreach(dir IN LISTS dash_dirs)
    file(GLOB files
         LIST_DIRECTORIES FALSE
         RELATIVE "${dashboard_src_dir}"
         "${dashboard_src_prefix}/${dir}/*")

    set(rules "${rules}install(FILES ${files} COMPONENT dashboard DESTINATION ${WEB_DEST}/${dir})\n")
  endforeach()

  file(GLOB files
       LIST_DIRECTORIES FALSE
       RELATIVE "${dashboard_src_dir}"
       "${dashboard_src_prefix}/*")

  set(rules "${rules}install(FILES ${files} COMPONENT dashboard DESTINATION ${WEB_DEST})\n")

  file(WRITE "${dashboard_src_dir}/CMakeLists.txt" "${rules}")
  message(STATUS "  Generating CMakeLists.txt file for dashboard code -- Done")
  add_subdirectory("${dashboard_src_dir}" "${dashboard_bin_dir}")
  message(STATUS "Preparing local agent dashboard code -- Done")
endfunction()
