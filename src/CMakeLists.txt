add_executable(patchcode
    main.cpp
    patchcode_tool.cpp
    patchcode_segments.cpp
    patchcode_file.cpp)

target_include_directories(patchcode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(diffcode
    diffcode_main.cpp)

target_include_directories(diffcode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS patchcode diffcode
    RUNTIME DESTINATION bin)
