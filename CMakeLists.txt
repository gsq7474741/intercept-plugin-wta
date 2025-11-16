cmake_minimum_required (VERSION 3.26)

#----Make changes here

#This is your project name. And also the filename of the resulting plugin.
# set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake")
message("CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
project (wtaPlugin)

# Export compile commands for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#This setting enables the use of the engine string type instead of converting to std::string.
#Enabling this results in better pe+rformance when handling strings to SQF commands.
option(USE_ENGINE_TYPES "USE_ENGINE_TYPES" OFF)

#----Don't change anything below this line

option(USE_64BIT_BUILD "USE_64BIT_BUILD" OFF)
set(INTERCEPT_LINK_TYPE "static")

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
	set( USE_64BIT_BUILD ON)
endif()

message("GENERATOR USED: '${CMAKE_GENERATOR}'")
message("COMPILER USED: '${CMAKE_CXX_COMPILER_ID}'")

set(CMAKE_CL_64 ${USE_64BIT_BUILD})

if(USE_64BIT_BUILD)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/win64/")
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/win32/")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Protocol Buffers
find_package(Protobuf REQUIRED)
message(STATUS "Protobuf version: ${Protobuf_VERSION}")
message(STATUS "Protobuf include: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "Protobuf libraries: ${Protobuf_LIBRARIES}")

# Generate protobuf C++ code
set(PROTO_SRC_DIR "${PROJECT_SOURCE_DIR}/proto")
set(PROTO_GEN_DIR "${PROJECT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILES
    "${PROTO_SRC_DIR}/wta_messages.proto"
)

# 使用自定义命令生成 protobuf 代码
set(PROTO_SRCS "${PROTO_GEN_DIR}/wta_messages.pb.cc")
set(PROTO_HDRS "${PROTO_GEN_DIR}/wta_messages.pb.h")

# 获取 protoc 可执行文件路径
find_program(PROTOC_EXECUTABLE protoc REQUIRED)
message(STATUS "Protoc executable: ${PROTOC_EXECUTABLE}")

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND ${PROTOC_EXECUTABLE}
    ARGS --cpp_out=${PROTO_GEN_DIR}
         --proto_path=${PROTO_SRC_DIR}
         ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    COMMENT "Generating C++ code from wta_messages.proto"
    VERBATIM
)

# 创建自定义目标以确保 protobuf 代码生成
add_custom_target(generate_proto_cpp ALL DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})

set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES GENERATED TRUE)

message(STATUS "Protobuf output dir: ${PROTO_GEN_DIR}")
message(STATUS "Generated proto sources: ${PROTO_SRCS}")
message(STATUS "Generated proto headers: ${PROTO_HDRS}")

set(CMAKE_INCLUDE_CURRENT_DIR ON) 
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

add_subdirectory(src)

find_package(GTest CONFIG REQUIRED)
enable_testing()

# Optional unit tests
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()

# ---------------- Deployment configuration (no hardcoded paths) ----------------
# 1) Where is Arma 3 installed? Configure via -DARMA3_DIR=...
set(ARMA3_DIR "" CACHE PATH "Arma 3 installation directory (e.g. C:/Program Files (x86)/Steam/steamapps/common/Arma 3)")

# 2) Mod root directory inside Arma 3. You can override entirely via -DMOD_DIR=...
#    Default uses a conventional folder name; change as needed.
set(MOD_DIR "${ARMA3_DIR}/@intercept-plugin-wta" CACHE PATH "Target mod root directory under Arma 3")

# 3) AddonBuilder.exe path (from Arma 3 Tools). Configure via -DADDON_BUILDER_EXE=...
set(ADDON_BUILDER_EXE "" CACHE FILEPATH "Path to AddonBuilder.exe from Arma 3 Tools")

# 4) Addon source directory (the folder to pack, expected to become main.pbo)
set(ADDON_SRC_DIR "${CMAKE_SOURCE_DIR}/addons/main" CACHE PATH "Addon source directory to pack")

# Optional packing flags
option(ADDON_PACKONLY "Use -packonly (no binarization)" ON)
option(ADDON_CLEAR    "Use -clear before pack" ON)

# Compose AddonBuilder args
set(_AB_ARGS)
if(ADDON_PACKONLY)
  list(APPEND _AB_ARGS "-packonly")
endif()
if(ADDON_CLEAR)
  list(APPEND _AB_ARGS "-clear")
endif()

# Step 1 & 2 的 DLL 复制钩子在 src/CMakeLists.txt 中实现（需要与目标在同一目录）。

# ---------------- Step 3: Pack addons/main -> mod/addons/main.pbo ----------------
set(MOD_ADDONS_DIR "${MOD_DIR}/addons")
set(ADDON_OUTPUT_PBO "${MOD_ADDONS_DIR}/main.pbo")

add_custom_command(
  OUTPUT "${ADDON_OUTPUT_PBO}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MOD_ADDONS_DIR}"
  COMMAND "${ADDON_BUILDER_EXE}" "${ADDON_SRC_DIR}" "${MOD_ADDONS_DIR}" ${_AB_ARGS}
  DEPENDS "${ADDON_SRC_DIR}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Packing addon: ${ADDON_SRC_DIR} -> ${ADDON_OUTPUT_PBO}"
  VERBATIM
)

add_custom_target(pack_addon
  DEPENDS "${ADDON_OUTPUT_PBO}"
)

# Ensure pack_addon runs after building the plugin (so user can build then pack in order)
if(DEFINED INTERCEPT_PLUGIN_NAME)
  add_dependencies(pack_addon ${INTERCEPT_PLUGIN_NAME})
endif()

# Aggregate target: build plugin then pack addon
add_custom_target(build_and_pack
  DEPENDS pack_addon
)