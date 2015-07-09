cmake_minimum_required(VERSION 2.8.4)
project(1101)

set(HEADER_FILES src/common.h src/timer.h src/utils.h src/joystick.h src/midi.h src/trigger.h src/action.h src/event.h lib/json.h src/config.h)
set(SOURCE_FILES src/timer.c src/utils.c src/joystick.c src/midi.c src/trigger.c src/action.c src/event.c lib/json.c src/config.c)
set(TEST_FILES test/utils_test.c)
set(GCC_FLAGS -D_DEBUG)
set(ENV{CTEST_OUTPUT_ON_FAILURE} 1)

add_definitions(${GCC_FLAGS})
add_library(1101_LIB ${HEADER_FILES} ${SOURCE_FILES})
add_executable(1101 src/main.c)
target_link_libraries(1101 -pthread 1101_LIB)

enable_testing()
foreach(test ${TEST_FILES})
    string(REGEX REPLACE "test/([^\\.].*)\\.c" "\\1" testname ${test})
    add_executable(${testname} ${test})
    target_link_libraries(${testname} -pthread 1101_LIB)
    add_test(${test} ${testname})
endforeach(test ${TEST_FILES})

add_custom_command(TARGET 1101 POST_BUILD COMMAND ctest --output-on-failure)

configure_file(conf/import.json config/import.json COPYONLY)