project(mce_ex) 

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release) 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -O3 -Wall")

set(EX_LIBS 
    boost_coroutine
    boost_context
    boost_thread
    pthread 
    mce)

#------------------------------------------------------------------------------
# script runnable examples  

set(EXAMPLE_NUMS 
    001
    002
    003
    004
    005
    006
    007
    008
    009
    010
    012 
    013 
    014
    018
    019
    020
    021
    022
    028
    )

foreach(example_num ${EXAMPLE_NUMS})
    set(EXAMPLES ${EXAMPLES} "example_${example_num}")
endforeach()

function(add_example_executable example)
    add_executable(${example} src/${example}.cpp)
    target_link_libraries(${example} ${EX_LIBS})
endfunction() 

foreach(example ${EXAMPLES})
    add_example_executable(${example})
endforeach()

#------------------------------------------------------------------------------
# human only examples 

set(HUMAN_ONLY_EXAMPLE_NUMS 
    001
    )

foreach(example_num ${HUMAN_ONLY_EXAMPLE_NUMS})
    set(HUMAN_ONLY_EXAMPLES ${HUMAN_ONLY_EXAMPLES} "human_only_example_${example_num}")
endforeach()

function(add_human_only_example_executable example)
    add_executable(${example} src/${example}.cpp)
    target_link_libraries(${example} ${EX_LIBS})
endfunction() 

foreach(example ${HUMAN_ONLY_EXAMPLES})
    add_human_only_example_executable(${example})
endforeach()

#------------------------------------------------------------------------------

add_custom_target(mce_ex)
add_dependencies(mce_ex ${EXAMPLES} ${HUMAN_ONLY_EXAMPLES})

