cpmaddpackage(
  NAME
  ut-ext
  GITHUB_REPOSITORY
  arturbac/ut-ext
  VERSION
  2.0.1
  # tag v2.0.1_5, pinned to its commit - a tag can be moved
  GIT_TAG
  6309cc3da0daf21626ceb9f91a803d0aedceb769)
find_package(ut-ext REQUIRED)

function(add_ut_test source_file_name)
  get_filename_component(test_executable_name ${source_file_name} NAME_WE)

  add_executable(${test_executable_name} ${source_file_name})
  target_link_libraries(${test_executable_name} PRIVATE
                       eht
                        Boost::ut
                       )
  target_compile_options(${test_executable_name} PUBLIC -Wno-global-constructors )
  add_test(NAME "${test_executable_name}"
           COMMAND ${test_executable_name})
  # add_dependencies(unit_tests ${test_executable_name})
endfunction()

add_ut_test(value_calculation_ut.cc)
add_ut_test(db_ut.cc)
add_ut_test(event_guard_ut.cc)
add_ut_test(biology_ut.cc)
add_ut_test(surface_nav_ut.cc)
add_ut_test(picture_records_ut.cc)
add_ut_test(backup_ut.cc)
add_ut_test(names_ut.cc)
add_ut_test(json_io_ut.cc)
add_ut_test(eddn_ut.cc)
add_ut_test(colonisation_ut.cc)
add_ut_test(fleet_ut.cc)

# ipc overlaya nie zalezy od eht, wiec ma wlasny cel zamiast add_ut_test
add_executable(overlay_ipc_ut overlay_ipc_ut.cc)
target_link_libraries(overlay_ipc_ut PRIVATE eht_overlay_ipc Boost::ut)
target_compile_options(overlay_ipc_ut PUBLIC -Wno-global-constructors)
add_test(NAME "overlay_ipc_ut" COMMAND overlay_ipc_ut)
add_ut_test(on_foot_ut.cc)
add_ut_test(bar_sales_ut.cc)
add_ut_test(territory_ut.cc)
add_ut_test(glare_ut.cc)
add_ut_test(sensors_ut.cc)
add_ut_test(evidence_ut.cc)
add_ut_test(orbit_position_ut.cc)
add_ut_test(schema_version_ut.cc)
add_ut_test(network_incident_ut.cc)
add_ut_test(server_link_ut.cc)
add_ut_test(instance_players_ut.cc)
add_ut_test(graphics_profile_ut.cc)
add_ut_test(credits_ut.cc)
add_ut_test(planet_face_ut.cc)
add_ut_test(port_model_ut.cc)
add_ut_test(face_store_ut.cc)

# the recorder of eht_vision lives in its own library beside eht
add_ut_test(hot_drop_ut.cc)
add_ut_test(vision_ut.cc)
target_link_libraries(vision_ut PRIVATE eht_vision_core)
