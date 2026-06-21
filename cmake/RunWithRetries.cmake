if(NOT DEFINED RUN_EXECUTABLE)
    message(FATAL_ERROR "RUN_EXECUTABLE must be set")
endif()

if(NOT DEFINED RUN_ATTEMPTS)
    set(RUN_ATTEMPTS 1)
endif()

if(NOT DEFINED RUN_DELAY_SECONDS)
    set(RUN_DELAY_SECONDS 0)
endif()

foreach(attempt RANGE 1 ${RUN_ATTEMPTS})
    execute_process(
        COMMAND "${RUN_EXECUTABLE}" ${RUN_ARGUMENTS}
        WORKING_DIRECTORY "${RUN_WORKING_DIRECTORY}"
        RESULT_VARIABLE run_result
    )

    if(run_result EQUAL 0)
        return()
    endif()

    if(attempt LESS RUN_ATTEMPTS)
        message(WARNING "Command failed with exit code ${run_result} on attempt ${attempt}; retrying.")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep "${RUN_DELAY_SECONDS}")
    endif()
endforeach()

message(FATAL_ERROR "Command failed with exit code ${run_result} after ${RUN_ATTEMPTS} attempt(s): ${RUN_EXECUTABLE} ${RUN_ARGUMENTS}")
