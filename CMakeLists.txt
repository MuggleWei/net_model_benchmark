cmake_minimum_required (VERSION 3.18.6)
project(net_model_benchmark)

################################
# general config
################################

message("-- use c compiler ${CMAKE_C_COMPILER}")
message("-- use c++ compiler ${CMAKE_CXX_COMPILER}")

# set compile parameter
if (${CMAKE_C_COMPILER_ID} STREQUAL GNU)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
elseif (${CMAKE_C_COMPILER_ID} MATCHES Clang)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-missing-field-initializers")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-missing-field-initializers")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS=1 -D_UNICODE -DUNICODE)
	add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
	add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

message("-- c compiler support features: ")
foreach(feature ${CMAKE_C_COMPILE_FEATURES})
	message("support feature: ${feature}")
endforeach()

# set output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# for LSP
if (NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# set use folder in vs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################
# options
################################

set(CMAKE_BUILD_TYPE "Release" CACHE STRING
	"Build type (Debug, RelWithDebInfo, Release, MinSizeRel or Coverage)")
set(BUILD_SANITIZER OFF CACHE BOOL "build sanitizer")

################################
# deps
################################

include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/_deps)

message("# fetch mugglec")
FetchContent_Declare(
	mugglec
	GIT_REPOSITORY https://github.com/MuggleWei/mugglec.git
	GIT_TAG v1.3.5
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(mugglec)

message("# fetch haclog")
FetchContent_Declare(
	haclog
	GIT_REPOSITORY https://github.com/MuggleWei/haclog.git
	GIT_TAG v0.0.5
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(haclog)

################################
# netmodel
################################
FILE(GLOB_RECURSE netmodel_src_h "${CMAKE_CURRENT_LIST_DIR}/src/netmodel/*.h")
FILE(GLOB_RECURSE netmodel_src_c "${CMAKE_CURRENT_LIST_DIR}/src/netmodel/*.c")
add_library(netmodel STATIC
	${netmodel_src_h}
	${netmodel_src_c})
target_include_directories(netmodel PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(netmodel mugglec haclog)
set_target_properties(netmodel PROPERTIES DEBUG_POSTFIX d)
target_compile_definitions(netmodel PUBLIC HACLOG_HOLD_LOG_MACRO)

################################
# benchmarks
################################

function(add_app name folder)
	message("# add app: ${name}")

	file(GLOB_RECURSE tmp_h ${folder}/*.h)
	file(GLOB_RECURSE tmp_c ${folder}/*.c)
	file(GLOB_RECURSE tmp_cpp ${folder}/*.cpp)
	file(GLOB_RECURSE tmp_cc ${folder}/*.cc)

	if (MSVC OR MINGW)
		add_executable(${name} ${tmp_h} ${tmp_c} ${tmp_cpp} ${tmp_cc})
		set_target_properties(${name}
			PROPERTIES
			FOLDER "example"
			VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
		)
	else()
		add_executable(${name} ${tmp_c} ${tmp_cpp} ${tmp_cc})
		if (APPLE)
			set_target_properties(${name}
				PROPERTIES
				INSTALL_RPATH "@executable_path/../lib"
			)
		elseif (UNIX)
			set_target_properties(${name}
				PROPERTIES
				INSTALL_RPATH "\$ORIGIN/../lib"
			)
		endif()
	endif(MSVC OR MINGW)
	set_target_properties(${name} PROPERTIES DEBUG_POSTFIX d)

	target_include_directories(${name} PUBLIC ${folder})
	target_link_libraries(${name} netmodel)
	if (NOT (${CMAKE_C_COMPILER_ID} STREQUAL MSVC))
		target_link_libraries(${name} m)
	endif()

	if ((${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU") OR
		(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang"))
		target_compile_options(${name} PUBLIC -save-temps=obj)
	endif()

	if (BUILD_SANITIZER)
		if ((${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU") OR
			(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang"))
			target_compile_options(${name} PUBLIC
				-fsanitize=undefined
				-fsanitize=address
				-fsanitize=leak)
			target_link_options(${name} PUBLIC
				-fsanitize=undefined
				-fsanitize=address
				-fsanitize=leak)
		endif()
	endif()
endfunction()

set(example_root_dir ${CMAKE_CURRENT_LIST_DIR}/example)
FILE(GLOB subdirs RELATIVE ${example_root_dir} ${example_root_dir}/*)
FOREACH(subdir ${subdirs})
	IF(IS_DIRECTORY ${example_root_dir}/${subdir})
		add_app(${subdir} ${example_root_dir}/${subdir})
	ENDIF()
ENDFOREACH()
