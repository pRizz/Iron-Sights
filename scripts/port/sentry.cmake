# Optional headers only: the GCC game loads the matching MSVC DLL at runtime.
set(MIDAS_SENTRY_INCLUDE_DIR "" CACHE PATH "Pinned sentry-native include directory")
set(MIDAS_SENTRY_DSN "" CACHE STRING "Public Sentry ingestion DSN (never an auth token)")
if(WIN32)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/scripts/port/sentry_config.json"
        "${CMAKE_SOURCE_DIR}/scripts/port/sentry_tools.py")
    find_program(SENTRY_PYTHON NAMES python3 python REQUIRED)
    execute_process(COMMAND ${SENTRY_PYTHON}
        ${CMAKE_SOURCE_DIR}/scripts/port/sentry_tools.py header
        --output ${CMAKE_BINARY_DIR}/generated/port_sentry_config.h
        --dsn "${MIDAS_SENTRY_DSN}" RESULT_VARIABLE sentry_header_result)
    if(NOT sentry_header_result EQUAL 0)
        message(FATAL_ERROR "Could not generate public Sentry configuration")
    endif()
    target_sources(ge PRIVATE port/crash/reporting_win.c)
    target_link_options(ge PRIVATE -Wl,--build-id)
    if(MIDAS_SENTRY_INCLUDE_DIR)
        if(NOT EXISTS "${MIDAS_SENTRY_INCLUDE_DIR}/sentry.h")
            message(FATAL_ERROR "MIDAS_SENTRY_INCLUDE_DIR must contain the pinned sentry.h")
        endif()
        set_source_files_properties(port/crash/reporting_win.c PROPERTIES
            INCLUDE_DIRECTORIES "${MIDAS_SENTRY_INCLUDE_DIR}"
            COMPILE_DEFINITIONS MIDAS_SENTRY_ENABLED=1)
    endif()
endif()
