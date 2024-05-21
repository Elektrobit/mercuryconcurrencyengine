cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release) 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -O3 -Wall")
set(MCE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/inc/mce)

project(libmce)

#-------------------------------------------------------------------------------
# mce feature configuration, user should modify these as necessary  

# development only features such as printing
set(MCE_ENABLE_DEV_FEATURES OFF)

# Define the count of threads in the background default threadpool accessed by 
# features like `mce::parallel()`. If `0` is set the library will decide how 
# many threads to spawn, preferring maximum CPU throughput.
set(MCEMAXPROCS 0)

# Define the minimum count of available threads for await tasks executed by 
# `mce::await()` (more will be temporarily spawned as necessary).
set(MCEMINAWAITPROCS 1)

# Define the default balance ratio used by balance scheduling algorithm
set(MCEBALANCERATIO 1.5)

# flag sanity logic 

# print values 
message("-- MCE LIBRARY CONFIGURATION VALUES -- ")

if(${MCE_ENABLE_DEV_FEATURES})
    message("MCE_ENABLE_DEV_FEATURES:${MCE_ENABLE_DEV_FEATURES}")
endif()

message("MCEMAXPROCS:${MCEMAXPROCS}")
message("MCEMINAWAITPROCS:${MCEMINAWAITPROCS}")
message("MCEBALANCERATIO:${MCEBALANCERATIO}")

#-------------------------------------------------------------------------------
# boost targets

# The folbaseing version values can be modified as necessary by the user
set(MCE_BOOST_MAJOR_VERSION 1)
set(MCE_BOOST_MINOR_VERSION 85)
set(MCE_BOOST_PATCH_VERSION 0)

# Additional boost variables
set(MCE_BOOST_DOT_VERSION ${MCE_BOOST_MAJOR_VERSION}.${MCE_BOOST_MINOR_VERSION}.${MCE_BOOST_PATCH_VERSION})
set(MCE_BOOST_UNDERSCORE_VERSION ${MCE_BOOST_MAJOR_VERSION}_${MCE_BOOST_MINOR_VERSION}_${MCE_BOOST_PATCH_VERSION})
set(MCE_BOOST_ROOT_TOPLEVEL boost)
set(MCE_BOOST_ROOT ${MCE_BOOST_ROOT_TOPLEVEL}/boost_${MCE_BOOST_UNDERSCORE_VERSION})
set(MCE_BOOST_TARBALL boost_${MCE_BOOST_UNDERSCORE_VERSION}.tar.gz)

# If the boost download location changes this may need to be updated by the user 
set(MCE_BOOST_DOWNLOAD_URL https://boostorg.jfrog.io/artifactory/main/release/${MCE_BOOST_DOT_VERSION}/source/${MCE_BOOST_TARBALL})
set(MCE_BOOST_BUILT_FILE boost.built)

message("MCE_BOOST_MAJOR_VERSION:${MCE_BOOST_MAJOR_VERSION}")
message("MCE_BOOST_MINOR_VERSION:${MCE_BOOST_MINOR_VERSION}")
message("MCE_BOOST_PATCH_VERSION:${MCE_BOOST_PATCH_VERSION}")
message("MCE_BOOST_ROOT:${MCE_BOOST_ROOT}")

add_custom_command(
    OUTPUT ${MCE_BOOST_BUILT_FILE}
    COMMAND python3 script/setup-boost.py --major ${MCE_BOOST_MAJOR_VERSION} --minor ${MCE_BOOST_MINOR_VERSION} --patch ${MCE_BOOST_PATCH_VERSION} --url ${MCE_BOOST_DOWNLOAD_URL} --built-file boost.built
)

message("To rebuild boost delete local file 'boost.built'")

#-------------------------------------------------------------------------------
# mce util headers
set(MCE_UTILS_HEADER_FILES
    ${MCE_INCLUDE_DIR}/function_utility.hpp
    ${MCE_INCLUDE_DIR}/atomic.hpp)

#-------------------------------------------------------------------------------
# mce

set(MCE_HEADER_FILES ${MCE_UTILS_HEADER_FILES})
set(MCE_SOURCE_FILES "")

if(${MCE_ENABLE_DEV_FEATURES})
set(MCE_HEADER_FILES 
    ${MCE_HEADER_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/dev/inc/dev_print.hpp)

set(MCE_SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/dev/src/dev_print.cpp)
endif()

set(MCE_HEADER_FILES
    ${MCE_HEADER_FILES}
    ${MCE_INCLUDE_DIR}/coroutine.hpp
    ${MCE_INCLUDE_DIR}/scheduler.hpp
    ${MCE_INCLUDE_DIR}/timer.hpp
    ${MCE_INCLUDE_DIR}/mutex.hpp
    ${MCE_INCLUDE_DIR}/condition_variable.hpp
    ${MCE_INCLUDE_DIR}/base_channel.hpp
    ${MCE_INCLUDE_DIR}/unbuffered_channel.hpp
    ${MCE_INCLUDE_DIR}/buffered_channel.hpp
    ${MCE_INCLUDE_DIR}/chan.hpp
    ${MCE_INCLUDE_DIR}/await.hpp
    ${MCE_INCLUDE_DIR}/threadpool.hpp
    ${MCE_INCLUDE_DIR}/mce.hpp
)

set(MCE_SOURCE_FILES
    ${MCE_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/src/coroutine.cpp 
    ${CMAKE_CURRENT_LIST_DIR}/src/scheduler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/timer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/await.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/threadpool.cpp
)

#-------------------------------------------------------------------------------
# mce framework static library 

# convert a list to a readable string
function (ListToString result delim)
    list(GET ARGV 2 temp)
    math(EXPR N "${ARGC}-1")
    foreach(IDX RANGE 3 ${N})
        list(GET ARGV ${IDX} STR)
        set(temp "${temp}${delim}${STR}")
    endforeach()
    set(${result} "${temp}" PARENT_SCOPE)
endfunction(ListToString)

ListToString(PRETTY_MCE_HEADER_FILES "\n" ${MCE_HEADER_FILES})
ListToString(PRETTY_MCE_SOURCE_FILES "\n" ${MCE_SOURCE_FILES})

message("-- MCE LIBRARY HEADERS -- ")
message(${PRETTY_MCE_HEADER_FILES})
message("-- MCE LIBRARY SOURCES -- ")
message(${PRETTY_MCE_SOURCE_FILES})

add_library(mce STATIC 
    ${MCE_SOURCE_FILES}
    ${MCE_HEADER_FILES}
    ${MCE_BOOST_BUILT_FILE}
)

add_library(mce_minimal STATIC 
    ${MCE_SOURCE_FILES}
    ${MCE_HEADER_FILES}
    ${MCE_BOOST_BUILT_FILE}
)

if(${MCE_ENABLE_DEV_FEATURES})
    target_include_directories(mce PUBLIC ${MCE_INCLUDE_DIR} ${MCE_BOOST_ROOT} dev/inc) 
    target_include_directories(mce_minimal PUBLIC ${MCE_INCLUDE_DIR} ${MCE_BOOST_ROOT} dev/inc) 
else()
    target_include_directories(mce PUBLIC ${MCE_INCLUDE_DIR} ${MCE_BOOST_ROOT}) 
    target_include_directories(mce_minimal PUBLIC ${MCE_INCLUDE_DIR} ${MCE_BOOST_ROOT}) 
endif()

target_link_directories(mce PUBLIC ${MCE_BOOST_ROOT}/stage/lib)

message("-- MCE LIBRARY COMPILE DEFINES -- ")
message("-DMCEMAXPROCS=${MCEMAXPROCS}")
message("-DMCEMINAWAITPROCS=${MCEMINAWAITPROCS}")
message("-DMCEBALANCERATIO=${MCEBALANCERATIO}")

target_compile_definitions(
    mce
    PRIVATE 
    -DMCEMAXPROCS=${MCEMAXPROCS}
    -DMCEMINAWAITPROCS=${MCEMINAWAITPROCS}
    -DMCEBALANCERATIO=${MCEBALANCERATIO}
)

target_compile_definitions(
    mce_minimal
    PRIVATE 
    -DMCEMAXPROCS=1
    -DMCEMINAWAITPROCS=${MCEMINAWAITPROCS}
    -DMCEBALANCERATIO=${MCEBALANCERATIO}
)

#install(TARGETS mce DESTINATION lib)
#install(FILES ${MCE_HEADER_FILES} DESTINATION include/mce)

add_subdirectory(tst EXCLUDE_FROM_ALL)
add_subdirectory(ex EXCLUDE_FROM_ALL) 
