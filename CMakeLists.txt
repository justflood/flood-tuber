project(flood-tuber)


set(flood-tuber_SOURCES
    flood-tuber.cpp
    flood-tuber.h
)


add_library(flood-tuber MODULE
    ${flood-tuber_SOURCES}
)


target_link_libraries(flood-tuber PRIVATE
    libobs
)


set_target_properties(flood-tuber PROPERTIES
    FOLDER "plugins"
    PREFIX ""
)


add_custom_command(TARGET flood-tuber POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/obs-plugins/64bit"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:flood-tuber>"
        "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/obs-plugins/64bit/$<TARGET_FILE_NAME:flood-tuber>"
    VERBATIM
)


if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_custom_command(TARGET flood-tuber POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/data/obs-plugins/flood-tuber"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/data/obs-plugins/flood-tuber"
        VERBATIM
    )
endif()