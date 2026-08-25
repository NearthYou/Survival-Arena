if(NOT DEFINED WORKFLOW_FILE OR NOT EXISTS "${WORKFLOW_FILE}")
    message(FATAL_ERROR "WORKFLOW_FILE must point to the CI workflow")
endif()

file(READ "${WORKFLOW_FILE}" WORKFLOW_CONTENT)
string(REPLACE "\r\n" "\n" WORKFLOW_CONTENT "${WORKFLOW_CONTENT}")

foreach(REQUIRED_FRAGMENT "  push:" "  pull_request:")
    string(FIND "${WORKFLOW_CONTENT}" "${REQUIRED_FRAGMENT}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "CI workflow is missing: ${REQUIRED_FRAGMENT}")
    endif()
endforeach()

set(EXPECTED_PUSH_BLOCK "  push:\n    branches:\n      - main\n")
string(REGEX MATCH
    "  push:\n    branches:\n(      - [^\n]+\n)+"
    ACTUAL_PUSH_BLOCK
    "${WORKFLOW_CONTENT}")
if(NOT ACTUAL_PUSH_BLOCK STREQUAL EXPECTED_PUSH_BLOCK)
    message(FATAL_ERROR
        "feature branch push duplicates pull_request CI; push must target main only")
endif()
