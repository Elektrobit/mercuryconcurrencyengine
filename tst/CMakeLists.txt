project(mce_ut) 

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release) 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -O3 -Wall")

add_subdirectory(googletest)

add_executable(mce_ut
    fptr.cpp
    logging.hpp 
    thunk_ut.cpp 
    coroutine_ut.cpp 
    scheduler_ut.cpp 
    mutex_ut.cpp
    condition_variable_ut.cpp
    unbuffered_channel_ut.cpp 
    buffered_channel_ut.cpp 
    chan_ut.cpp 
    timer_ut.cpp 
    await_ut.cpp 
    threadpool_ut.cpp 
    comparison_ut.cpp
    comparison_mutex_ut.cpp
)

add_executable(mce_ut_minimal
    fptr.cpp
    logging.hpp 
    thunk_ut.cpp 
    coroutine_ut.cpp 
    scheduler_ut.cpp 
    mutex_ut.cpp
    condition_variable_ut.cpp
    unbuffered_channel_ut.cpp 
    buffered_channel_ut.cpp 
    chan_ut.cpp 
    timer_ut.cpp 
    await_ut.cpp 
    threadpool_ut.cpp 
    comparison_ut.cpp
    comparison_mutex_ut.cpp
)

target_link_libraries(mce_ut 
    gtest 
    gtest_main 
    boost_coroutine
    boost_context
    boost_thread
    mce)

target_link_libraries(mce_ut_minimal
    gtest 
    gtest_main 
    boost_coroutine
    boost_context
    boost_thread
    mce_minimal)

set(MCE_UT_LARGE_VALUE_MESSAGE_COUNT 2500)
set(MCE_UT_CONGESTED_MESSAGE_COUNT 25)

message("-- mce UNIT TEST COMPILE DEFINES -- ")
message("-DMCE_UT_LARGE_VALUE_MESSAGE_COUNT=${MCE_UT_LARGE_VALUE_MESSAGE_COUNT}")
message("-DMCE_UT_CONGESTED_MESSAGE_COUNT=${MCE_UT_CONGESTED_MESSAGE_COUNT}")
message("-DMCEBALANCERATIO=${MCEBALANCERATIO}")

target_compile_definitions(
    mce_ut 
    PRIVATE 
    -DMCE_UT_LARGE_VALUE_MESSAGE_COUNT=${MCE_UT_LARGE_VALUE_MESSAGE_COUNT}
    -DMCE_UT_CONGESTED_MESSAGE_COUNT=${MCE_UT_CONGESTED_MESSAGE_COUNT}
    -DMCEBALANCERATIO=${MCEBALANCERATIO}
)

target_compile_definitions(
    mce_ut_minimal
    PRIVATE 
    -DMCE_UT_LARGE_VALUE_MESSAGE_COUNT=${MCE_UT_LARGE_VALUE_MESSAGE_COUNT}
    -DMCE_UT_CONGESTED_MESSAGE_COUNT=${MCE_UT_CONGESTED_MESSAGE_COUNT}
    -DMCEBALANCERATIO=${MCEBALANCERATIO}
)
