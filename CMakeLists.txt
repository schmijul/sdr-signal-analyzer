cmake_minimum_required(VERSION 3.24)

project(sdr_signal_analyzer VERSION 0.1.0 LANGUAGES CXX)

option(SDR_ANALYZER_BUILD_PYTHON "Build pybind11 bindings and the Python package glue." ON)
option(SDR_ANALYZER_ENABLE_SOAPYSDR "Enable the SoapySDR live-device backend when available." ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)

if(SDR_ANALYZER_ENABLE_SOAPYSDR)
  find_package(SoapySDR CONFIG QUIET)
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

add_executable(sdr-analyzer-cli src/cli/main.cpp)
target_link_libraries(sdr-analyzer-cli PRIVATE sdr_analyzer)

if(SDR_ANALYZER_BUILD_PYTHON AND pybind11_FOUND AND Python3_FOUND)
  pybind11_add_module(_sdr_signal_analyzer python/bindings.cpp)
  target_link_libraries(_sdr_signal_analyzer PRIVATE sdr_analyzer)
  target_include_directories(_sdr_signal_analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

  set_target_properties(
    _sdr_signal_analyzer
    PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/sdr_signal_analyzer
  )
endif()

enable_testing()

add_executable(test_pipeline tests/test_pipeline.cpp)
target_link_libraries(test_pipeline PRIVATE sdr_analyzer)
target_include_directories(test_pipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME pipeline COMMAND test_pipeline)
