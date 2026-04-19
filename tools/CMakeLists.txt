cmake_minimum_required(VERSION 3.20)

project(sunshine_tools)

include_directories("${CMAKE_SOURCE_DIR}")

add_executable(dxgi-info dxgi.cpp)
set_target_properties(dxgi-info PROPERTIES CXX_STANDARD 23)
target_link_libraries(dxgi-info
        ${CMAKE_THREAD_LIBS_INIT}
        dxgi
        ${PLATFORM_LIBRARIES})
target_compile_options(dxgi-info PRIVATE ${SUNSHINE_COMPILE_OPTIONS})

add_executable(audio-info audio.cpp)
set_target_properties(audio-info PROPERTIES CXX_STANDARD 23)
target_link_libraries(audio-info
        ${Boost_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ksuser
        ${PLATFORM_LIBRARIES})
target_compile_options(audio-info PRIVATE ${SUNSHINE_COMPILE_OPTIONS})

add_executable(sunshinesvc sunshinesvc.cpp)
set_target_properties(sunshinesvc PROPERTIES CXX_STANDARD 23)
target_link_libraries(sunshinesvc
        ${CMAKE_THREAD_LIBS_INIT}
        wtsapi32
        ${PLATFORM_LIBRARIES})
target_compile_options(sunshinesvc PRIVATE ${SUNSHINE_COMPILE_OPTIONS})

add_executable(qiin-tabtip qiin-tabtip.cpp)
set_target_properties(qiin-tabtip PROPERTIES CXX_STANDARD 23)
target_compile_definitions(qiin-tabtip PRIVATE UNICODE _UNICODE)
target_link_libraries(qiin-tabtip
        ${CMAKE_THREAD_LIBS_INIT}
        shell32
        advapi32
        ole32
        oleaut32
        uuid
        ${PLATFORM_LIBRARIES})
target_compile_options(qiin-tabtip PRIVATE ${SUNSHINE_COMPILE_OPTIONS})
# Set the correct entry point and subsystem for MinGW
if(MINGW)
    # Optimize for size: -Os optimizes for size, -s strips debug symbols
    target_compile_options(qiin-tabtip PRIVATE -Os)
    target_link_options(qiin-tabtip PRIVATE
        -municode
        -s  # Strip symbols
        -Wl,--gc-sections  # Remove unused sections
    )
endif()

# Test tool: test_args — for development testing only, not packaged
add_executable(test_args test_args.cpp)
set_target_properties(test_args PROPERTIES CXX_STANDARD 23)
target_link_libraries(test_args
        ${CMAKE_THREAD_LIBS_INIT}
        ${PLATFORM_LIBRARIES})
if(WIN32)
    target_link_libraries(test_args advapi32 userenv)
endif()
target_compile_options(test_args PRIVATE ${SUNSHINE_COMPILE_OPTIONS})
# Explicitly marked not to install (development/test tool); can still be built.
# Note: not installed via cmake/packaging/windows.cmake, so it won't be packaged.