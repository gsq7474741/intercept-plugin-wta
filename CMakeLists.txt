cmake_minimum_required (VERSION 3.26)

#----Make changes here

#This is your project name. And also the filename of the resulting plugin.
project (wtaPlugin)

#This setting enables the use of the engine string type instead of converting to std::string.
#Enabling this results in better performance when handling strings to SQF commands.
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

set(CMAKE_INCLUDE_CURRENT_DIR ON) 
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

add_subdirectory(src)

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