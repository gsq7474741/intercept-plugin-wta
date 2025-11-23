cmake_minimum_required (VERSION 3.26)

# Define Intercept paths FIRST (before any usage)
set(INTERCEPT_CLIENT_PATH "${CMAKE_SOURCE_DIR}/intercept/src/client")
set(INTERCEPT_INCLUDE_PATH "${INTERCEPT_CLIENT_PATH}/headers" "${INTERCEPT_CLIENT_PATH}/headers/shared" "${INTERCEPT_CLIENT_PATH}/headers/client/" "${INTERCEPT_CLIENT_PATH}/headers/client/sqf")

file(GLOB_RECURSE INTERCEPT_PLUGIN_SOURCES *.h *.hpp *.c *.cpp)
list(FILTER INTERCEPT_PLUGIN_SOURCES EXCLUDE REGEX "/wta/")
SOURCE_GROUP("src" FILES ${INTERCEPT_PLUGIN_SOURCES})

file(GLOB_RECURSE WTA_CORE_SOURCES
  "wta/core/*.hpp" "wta/core/*.cpp"
  "wta/net/*.hpp"  "wta/net/*.cpp"
  "wta/world/event_bus.hpp"
  "wta/world/world_sampler.hpp"
  "wta/exec/executor.hpp"
  "wta/orchestrator/*.hpp" "wta/orchestrator/*.cpp"
)
# Exclude backup files from compilation
list(FILTER WTA_CORE_SOURCES EXCLUDE REGEX ".*_backup\\.(cpp|hpp)$")

# Explicitly add log_sink_zmq.cpp to ensure it's compiled
list(APPEND WTA_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/wta/net/log_sink_zmq.cpp")

# Debug: print all source files
message(STATUS "WTA_CORE_SOURCES: ${WTA_CORE_SOURCES}")
# Add protobuf generated sources to wta_core
add_library(wta_core STATIC ${WTA_CORE_SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})

# 确保 protobuf 代码在编译前生成
add_dependencies(wta_core generate_proto_cpp)

# Add both local sources, Intercept headers, and protobuf includes to wta_core
target_include_directories(wta_core PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${INTERCEPT_INCLUDE_PATH}
  ${CMAKE_BINARY_DIR}  # For generated protobuf headers
  ${CMAKE_BINARY_DIR}/generated  # For wta_messages.pb.h
  ${Protobuf_INCLUDE_DIRS}
)
# Link protobuf library
target_link_libraries(wta_core PUBLIC protobuf::libprotobuf)


find_package(ZeroMQ QUIET CONFIG)
if(ZeroMQ_FOUND)
    message(STATUS "ZeroMQ found: ${ZeroMQ_VERSION}")
    target_compile_definitions(wta_core PUBLIC WTA_HAVE_ZMQ=1)
    target_link_libraries(wta_core PUBLIC libzmq)
else()
    message(WARNING "ZeroMQ not found; building with stub client")
endif()

find_package(glog CONFIG REQUIRED)
target_link_libraries(wta_core PUBLIC glog::glog)
target_compile_definitions(wta_core PUBLIC WTA_HAVE_GLOG=1)

file(GLOB WTA_RUNTIME_SOURCES
  "wta/world/world_sampler_intercept.cpp"
  "wta/world/world_config.cpp"
  "wta/exec/executor_intercept.cpp"
  "wta/exec/uav_entity.cpp"
  "wta/exec/uav_controller.cpp"
  "wta/exec/task_executor.cpp"
  "wta/exec/entity_registry.cpp"
  "wta/exec/sequential_attack_test.cpp"
)
add_library(wta_runtime STATIC ${WTA_RUNTIME_SOURCES})
target_include_directories(wta_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${INTERCEPT_INCLUDE_PATH})
target_link_libraries(wta_runtime PUBLIC wta_core)

#If you want to split your source files into different directories you can do so here

#The SOURCE_GROUP string is the directory it will display as inside your visual studio.
#Here is a example of a "utilities" subdirectory.

#file(GLOB INTERCEPT_plugin_utilities_SOURCES "utilities/*.cpp" "utilities/*.hpp" "utilities/*.h")
#SOURCE_GROUP("src/utilities" FILES ${INTERCEPT_plugin_utilities_SOURCES})

#----Don't change anything below this line

# INTERCEPT_INCLUDE_PATH already defined at top of file

if(USE_64BIT_BUILD)
    set(INTERCEPT_PLUGIN_NAME "${CMAKE_PROJECT_NAME}_x64")
else()
    set(INTERCEPT_PLUGIN_NAME "${CMAKE_PROJECT_NAME}")
endif()
# Propagate target name to parent scope for packaging step
set(INTERCEPT_PLUGIN_NAME "${INTERCEPT_PLUGIN_NAME}" PARENT_SCOPE)
# And cache it globally to ensure availability during generation
set(INTERCEPT_PLUGIN_NAME "${INTERCEPT_PLUGIN_NAME}" CACHE INTERNAL "Intercept plugin target name")

add_definitions(/DINTERCEPT_NO_THREAD_SAFETY)

if(USE_ENGINE_TYPES)
    add_definitions(/DINTERCEPT_SQF_STRTYPE_RSTRING)
endif()

file(GLOB INTERCEPT_HOST_SOURCES "${INTERCEPT_CLIENT_PATH}/intercept/client/*.cpp"  "${INTERCEPT_CLIENT_PATH}/intercept/client/sqf/*.cpp" "${INTERCEPT_CLIENT_PATH}/intercept/shared/*.cpp")
SOURCE_GROUP("intercept" FILES ${INTERCEPT_HOST_SOURCES})

add_library( ${INTERCEPT_PLUGIN_NAME} SHARED ${INTERCEPT_PLUGIN_SOURCES} ${INTERCEPT_HOST_SOURCES})

# Set include directories for the plugin target
target_include_directories(${INTERCEPT_PLUGIN_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${INTERCEPT_INCLUDE_PATH})

set_target_properties(${INTERCEPT_PLUGIN_NAME} PROPERTIES PREFIX "")
set_target_properties(${INTERCEPT_PLUGIN_NAME} PROPERTIES FOLDER "${CMAKE_PROJECT_NAME}")

target_link_libraries(${INTERCEPT_PLUGIN_NAME} PRIVATE wta_core wta_runtime)

# Post-build: copy DLL to mod intercept folder (uses MOD_DIR from parent cache)
add_custom_command(TARGET ${INTERCEPT_PLUGIN_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MOD_DIR}/intercept"
  # Copy all DLLs from runtime output directory
  COMMAND powershell.exe -NoProfile -Command "Copy-Item -Path '${CMAKE_RUNTIME_OUTPUT_DIRECTORY}*.dll' -Destination '${MOD_DIR}\\intercept\\' -Force"
  COMMENT "Copying all DLLs from ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} to ${MOD_DIR}/intercept"
)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "-std=c++1z -O2 -s -fPIC -fpermissive -static-libgcc -static-libstdc++")#-march=i686 -m32
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set(CMAKE_SHARED_LINKER_FLAGS "-shared -static-libgcc -static-libstdc++")
else()
	# Add UTF-8 support for MSVC to fix encoding issues in build output
	set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MDd /Zi /Ob0 /Od /RTC1 /MP /EHsc /utf-8")
	set(CMAKE_CXX_FLAGS_RELEASE "/MT /Zi /O2 /Ob1 /EHsc /MP /utf-8") #with debug info
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/OPT:REF /DEBUG:FULL") 
endif()
