cpmaddpackage(
  ut
  GITHUB_REPOSITORY
  arturbac/ut-ext
  GIT_TAG
  v2.0.1_5)
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
