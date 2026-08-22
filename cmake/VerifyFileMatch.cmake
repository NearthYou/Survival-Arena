if(NOT DEFINED SOURCE_FILE OR NOT DEFINED DEPLOYED_FILE)
    message(FATAL_ERROR "SOURCE_FILE and DEPLOYED_FILE are required")
endif()

if(NOT EXISTS "${SOURCE_FILE}")
    message(FATAL_ERROR "source file does not exist: ${SOURCE_FILE}")
endif()

if(NOT EXISTS "${DEPLOYED_FILE}")
    message(FATAL_ERROR "deployed file does not exist: ${DEPLOYED_FILE}")
endif()

file(SHA256 "${SOURCE_FILE}" source_hash)
file(SHA256 "${DEPLOYED_FILE}" deployed_hash)

if(NOT source_hash STREQUAL deployed_hash)
    message(FATAL_ERROR
        "deployed file is stale\n"
        "source:   ${SOURCE_FILE} (${source_hash})\n"
        "deployed: ${DEPLOYED_FILE} (${deployed_hash})"
    )
endif()

