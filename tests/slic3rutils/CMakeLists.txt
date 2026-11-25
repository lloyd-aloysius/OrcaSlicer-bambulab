get_filename_component(_TEST_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
add_executable(${_TEST_NAME}_tests
    ${_TEST_NAME}_tests_main.cpp
    )

if (MSVC)
    target_link_libraries(${_TEST_NAME}_tests Setupapi.lib)
endif ()

target_link_libraries(${_TEST_NAME}_tests test_common libslic3r_gui libslic3r Catch2::Catch2WithMain)
set_property(TARGET ${_TEST_NAME}_tests PROPERTY FOLDER "tests")

if (WIN32)
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    orcaslicer_copy_dlls(COPY_DLLS "Debug" "d" output_dlls_Debug)
    elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    orcaslicer_copy_dlls(COPY_DLLS "RelWithDebInfo" "" output_dlls_Release)
    else()
    orcaslicer_copy_dlls(COPY_DLLS "Release" "" output_dlls_Release)
    endif()
endif()

catch_discover_tests(${_TEST_NAME}_tests)

if (APPLE AND ORCA_MAC_NATIVE_NETWORKING)
    add_library(native_printer_fixture MODULE ../macos/fixture_network.cpp)
    # The vendor ABI uses C symbol names with C++ string parameters and return values.
    target_compile_options(native_printer_fixture PRIVATE -Wno-return-type-c-linkage)
    target_include_directories(native_printer_fixture PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/deps_src)
    add_library(native_storage_fixture MODULE ../macos/fixture_storage.cpp)
    target_include_directories(native_storage_fixture PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/deps_src)
    add_executable(native_printer_workflow ../macos/native_workflow.cpp ../macos/file_transfer_results.cpp ../macos/native_files.cpp)
    set_target_properties(native_printer_workflow PROPERTIES MACOSX_BUNDLE FALSE)
    target_link_libraries(native_printer_workflow PRIVATE libslic3r_gui libslic3r)
    add_dependencies(native_printer_workflow native_printer_fixture native_storage_fixture)
    add_test(NAME native_printer_workflow
        COMMAND native_printer_workflow $<TARGET_FILE:native_printer_fixture> $<TARGET_FILE:native_storage_fixture>
                "${CMAKE_SOURCE_DIR}/resources/calib/filament_flow/pass1.3mf" "${CMAKE_SOURCE_DIR}/resources")
endif()
