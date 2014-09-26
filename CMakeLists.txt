cmake_minimum_required(VERSION 2.8)
project(pushy)

# module to find hiredis
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# set some project-wide flags
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-overflow -DBOOST_SPIRIT_THREADSAFE -DBOOST_HAS_THREADS=1 -DBOOST_SPIRIT_USE_PHOENIX_V3=1 -DBOOST_ALL_DYN_LINK=1" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overflow -DBOOST_SPIRIT_THREADSAFE -DBOOST_HAS_THREADS=1 -DBOOST_SPIRIT_USE_PHOENIX_V3=1 -DBOOST_ALL_DYN_LINK=1" )

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-stdlib=libc++" COMPILER_SUPPORTS_LIBCPP)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)

if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

	if(COMPILER_SUPPORTS_LIBCPP)
    	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

	if(COMPILER_SUPPORTS_LIBCPP)
    	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

# build local dependencies
add_subdirectory(lib/push_service)
add_subdirectory(lib/redis3m)

# we require boost with some linked libraries
find_package(Boost 1.54.0 REQUIRED COMPONENTS system log thread program_options date_time log_setup)

# and of course ssl/crypto is required
find_package(OpenSSL REQUIRED)

# find hiredis
find_package(hiredis REQUIRED)

set( BINARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/pc CACHE PATH
        "Single Directory for all Binaries")

# all source files
file(GLOB_RECURSE ALL_SRC src/*.cpp)

# set includes
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories("src")
include_directories("lib/cpp-netlib-0.11.0-final")
include_directories("lib")
include_directories("lib/redis3m/include")
include_directories("lib/push_service/src")

# compile pushy itself
add_executable(pushy ${ALL_SRC})

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fpermissive" )

# link final 
target_link_libraries(pushy push_service redis3m ${HIREDIS_LIBRARIES} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES})
