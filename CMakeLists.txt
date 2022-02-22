cmake_minimum_required (VERSION 3.14)
project (dx12hello-trinangle C)

set_target_properties(${TARGET_NAME} PROPERTIES C_STANDARD 17)
set_target_properties(${TARGET_NAME} PROPERTIES CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(TARGET demo)

set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${TARGET})

list(APPEND SOURCES main.c)

list(APPEND LIBRARIES d3d12.lib)
list(APPEND LIBRARIES d3dcompiler.lib)
list(APPEND LIBRARIES dxgi.lib)
list(APPEND LIBRARIES dxguid.lib)

add_executable(${TARGET} WIN32 ${SOURCES})
target_link_libraries(${TARGET} ${LIBRARIES})
target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
