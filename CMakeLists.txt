cmake_minimum_required(VERSION 3.24)

project(sdr_signal_analyzer VERSION 0.1.0 LANGUAGES CXX)

option(SDR_ANALYZER_BUILD_PYTHON "Build pybind11 bindings and the Python package glue." ON)
option(SDR_ANALYZER_ENABLE_SOAPYSDR "Enable the SoapySDR live-device backend when available." ON)
option(SDR_ANALYZER_ENABLE_UHD "Enable the native UHD USRP backend when available." ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)

if(SDR_ANALYZER_ENABLE_SOAPYSDR)
  find_package(SoapySDR CONFIG QUIET)
endif()

if(SDR_ANALYZER_ENABLE_UHD)
  find_package(UHD QUIET)
endif()

find_package(Python3 COMPONENTS Interpreter Development.Module QUIET)
find_package(pybind11 CONFIG QUIET)

add_library(
  sdr_analyzer
  src/core/session.cpp
  src/dsp/fft.cpp
  src/dsp/analyzer.cpp
  src/io/metadata.cpp
  src/io/recorder.cpp
  src/sdr/rtltcp_source.cpp
  src/sdr/source_factory.cpp
  src/sdr/simulator_source.cpp
  src/sdr/replay_source.cpp
)

target_include_directories(
  sdr_analyzer
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sdr_analyzer PUBLIC Threads::Threads)

if(SoapySDR_FOUND)
  target_sources(sdr_analyzer PRIVATE src/sdr/soapy_source.cpp)
  target_link_libraries(sdr_analyzer PRIVATE SoapySDR::SoapySDR)
  target_compile_definitions(sdr_analyzer PRIVATE SDR_ANALYZER_HAVE_SOAPYSDR=1)
endif()

if(UHD_FOUND)
  target_sources(sdr_analyzer PRIVATE src/sdr/uhd_source.cpp)
  target_compile_definitions(sdr_analyzer PRIVATE SDR_ANALYZER_HAVE_UHD=1)
  if(TARGET UHD::uhd)
    target_link_libraries(sdr_analyzer PRIVATE UHD::uhd)
  else()
    target_include_directories(sdr_analyzer PRIVATE ${UHD_INCLUDE_DIRS})
    target_link_libraries(sdr_analyzer PRIVATE ${UHD_LIBRARIES})
  endif()
endif()

add_executable(sdr-analyzer-cli src/cli/main.cpp)
target_link_libraries(sdr-analyzer-cli PRIVATE sdr_analyzer)

if(SDR_ANALYZER_BUILD_PYTHON AND pybind11_FOUND AND Python3_FOUND)
  pybind11_add_module(_sdr_signal_analyzer python/bindings.cpp)
  target_link_libraries(_sdr_signal_analyzer PRIVATE sdr_analyzer)
  target_include_directories(_sdr_signal_analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

  if(SKBUILD)
    install(
      TARGETS _sdr_signal_analyzer
      LIBRARY DESTINATION sdr_signal_analyzer
      RUNTIME DESTINATION sdr_signal_analyzer
    )
  else()
    set_target_properties(
      _sdr_signal_analyzer
      PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/sdr_signal_analyzer
    )
  endif()
endif()

enable_testing()

add_executable(test_pipeline tests/test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE sdr_analyzer)
target_include_directories(test_pipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME pipeline COMMAND test_pipeline)

add_executable(test_rtltcp tests/test_rtltcp.cpp)
target_link_libraries(test_rtltcp PRIVATE sdr_analyzer Threads::Threads)
target_include_directories(test_rtltcp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME rtltcp COMMAND test_rtltcp)

add_executable(test_record_replay tests/test_record_replay.cpp)
target_link_libraries(test_record_replay PRIVATE sdr_analyzer Threads::Threads)
target_include_directories(test_record_replay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME record_replay COMMAND test_record_replay)

add_executable(test_session tests/test_session.cpp)
target_link_libraries(test_session PRIVATE sdr_analyzer Threads::Threads)
target_include_directories(test_session PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME session COMMAND test_session)

add_test(
  NAME cli_replay_eof
  COMMAND
    sdr-analyzer-cli
    --source replay
    --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tone_cf32.sigmf-data
    --meta ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tone_cf32.sigmf-meta
    --frames 2
)
set_tests_properties(cli_replay_eof PROPERTIES TIMEOUT 5)

if(SDR_ANALYZER_BUILD_PYTHON AND Python3_FOUND AND TARGET _sdr_signal_analyzer)
  add_test(
    NAME gui_validation
    COMMAND
      ${CMAKE_COMMAND} -E env
      QT_QPA_PLATFORM=offscreen
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gui_validation.py
  )
  set_tests_properties(gui_validation PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

  add_test(
    NAME screenshot_assets
    COMMAND
      ${CMAKE_COMMAND} -E env
      QT_QPA_PLATFORM=offscreen
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_screenshot_assets.py
  )
  set_tests_properties(screenshot_assets PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)

  add_test(
    NAME export_helper
    COMMAND
      ${CMAKE_COMMAND} -E env
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_export_helper.py
  )
  set_tests_properties(export_helper PROPERTIES TIMEOUT 20)

  add_test(
    NAME python_docstrings
    COMMAND
      ${CMAKE_COMMAND} -E env
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_python_docstrings.py
  )
  set_tests_properties(python_docstrings PROPERTIES TIMEOUT 20)

  add_test(
    NAME diagnostics_api
    COMMAND
      ${CMAKE_COMMAND} -E env
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_diagnostics.py
  )
  set_tests_properties(diagnostics_api PROPERTIES TIMEOUT 20)

  add_test(
    NAME release_validation_script
    COMMAND
      ${CMAKE_COMMAND} -E env
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_validation_script.py
  )
  set_tests_properties(release_validation_script PROPERTIES TIMEOUT 20)
endif()

if(Python3_FOUND)
  add_test(
    NAME cli_export
    COMMAND
      ${CMAKE_COMMAND} -E env
      SDR_ANALYZER_CLI=$<TARGET_FILE:sdr-analyzer-cli>
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_export.py
  )
  set_tests_properties(cli_export PROPERTIES TIMEOUT 20)

  add_test(
    NAME demo
    COMMAND
      ${CMAKE_COMMAND} -E env
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python:$<TARGET_FILE_DIR:_sdr_signal_analyzer>
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_demo.py
  )
  set_tests_properties(demo PROPERTIES TIMEOUT 20)
endif()
