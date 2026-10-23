# One identity is shared by the embedded About screen and release metadata.
find_program(BUILDINFO_PYTHON NAMES python3 REQUIRED)
execute_process(COMMAND "${BUILDINFO_PYTHON}" "${SRC}/scripts/port/build_info.py"
    --root "${SRC}" --output "${OUT}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "Could not stamp build provenance")
endif()
