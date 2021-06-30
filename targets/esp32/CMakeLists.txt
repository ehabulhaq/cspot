# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Don't override EXTRA_COMPONENT_DIRS as platformio uses it.  Instead we append
# see https://github.com/platformio/platform-espressif32/issues/341
list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common 
                                 $ENV{ADF_PATH}/components/audio_board 
                                $ENV{ADF_PATH}/components/audio_hal 
                                $ENV{ADF_PATH}/components/esp_peripherals
                                $ENV{ADF_PATH}/components/audio_sal
                                $ENV{ADF_PATH}/components/display_service
                                $ENV{ADF_PATH}/components/esp_dispatcher
                                $ENV{ADF_PATH}/components/audio_pipeline
                                $ENV{ADF_PATH}/components/esp-adf-libs
                                $ENV{ADF_PATH}/components/esp-sr
                                $ENV{MDF_PATH}/components 
                                $ENV{MDF_PATH}/components/third_party)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include($ENV{MDF_PATH}/mdf_functions.cmake)
# 

mdf_get_git_revision()

project(cspot-esp32)