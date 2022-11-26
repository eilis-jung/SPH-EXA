function(compile_shader SHADERS TARGET_NAME SHADER_INCLUDE_FOLDER GENERATED_DIR GLSLANG_BIN)
    set(ALL_GENERATED_SPV_FILES "")
    set(ALL_GENERATED_CPP_FILES "")

    if(UNIX)
        execute_process(COMMAND chmod a+x ${GLSLANG_BIN})
    endif()

    set(ALL_HEADER_OUTPUT "${GENERATED_DIR}/all.h")
    set(ALL_HEADER_CONTENT "#pragma once\n")
    file(WRITE "${ALL_HEADER_OUTPUT}" ${ALL_HEADER_CONTENT})

    foreach(SHADER ${SHADERS})
        # Prepare a header name and a global variable for this shader
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        string(REPLACE "." "_" HEADER_NAME ${SHADER_NAME})
        string(TOUPPER ${HEADER_NAME} GLOBAL_SHADER_VAR)

        set(SPV_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${GENERATED_DIR}/spv/${SHADER_NAME}.spv")
        set(CPP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${GENERATED_DIR}/include/${HEADER_NAME}.h")

        add_custom_command(
            OUTPUT ${SPV_FILE}
            COMMAND ${GLSLANG_BIN} -I${SHADER_INCLUDE_FOLDER} -V100 -o ${SPV_FILE} ${SHADER}
            DEPENDS ${SHADER}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

        list(APPEND ALL_GENERATED_SPV_FILES ${SPV_FILE})
        add_custom_command(
            OUTPUT ${CPP_FILE}
            COMMAND ${CMAKE_COMMAND} -DPATH=${SPV_FILE} -DHEADER="${CPP_FILE}"
            -DGLOBAL="${GLOBAL_SHADER_VAR}" -P "${PROJECT_SOURCE_DIR}/cmake/generate_shader_header.cmake"
            DEPENDS ${SPV_FILE}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

        list(APPEND ALL_GENERATED_CPP_FILES ${CPP_FILE})
    endforeach()

    add_custom_target(${TARGET_NAME}
        DEPENDS ${ALL_GENERATED_SPV_FILES} ${ALL_GENERATED_CPP_FILES})
endfunction()