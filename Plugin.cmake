if(NOT LINUX)
    message(FATAL_ERROR "Only Linux is supported")
endif()

set(INCLUDES ${CSOUND_INCLUDE_DIRS} "include")
set(PLUGIN_NAME memread)

# Dependencies
    # None

# Source files
set(CPPFILES src/opcodes.cpp src/pmparser.cpp src/maketable.cpp)
make_plugin(${PLUGIN_NAME} "${CPPFILES}")
target_include_directories(${PLUGIN_NAME} PRIVATE ${INCLUDES})
