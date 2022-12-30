set(PLUGIN_NAME datacore)
set(CPPFILES src/opcodes.cpp src/maketable.cpp)

if(LINUX)
    set(CPPFILES ${CPPFILES} src/pmparser.cpp)
    add_definitions("-DUSE_PROCMAPS")
endif()

set(INCLUDES ${CSOUND_INCLUDE_DIRS} "include")

# Dependencies
    # None

# Source files
make_plugin(${PLUGIN_NAME} "${CPPFILES}")
target_include_directories(${PLUGIN_NAME} PRIVATE ${INCLUDES})
