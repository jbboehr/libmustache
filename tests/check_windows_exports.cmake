if(NOT DEFINED EXPORT_INSPECTOR OR NOT EXISTS "${EXPORT_INSPECTOR}")
    message(FATAL_ERROR "A valid export inspector is required")
endif()
if(NOT DEFINED MUSTACHE_DLL OR NOT EXISTS "${MUSTACHE_DLL}")
    message(FATAL_ERROR "A valid mustache DLL is required")
endif()

get_filename_component(export_inspector_name "${EXPORT_INSPECTOR}" NAME_WE)
if(export_inspector_name MATCHES "^llvm-readobj")
    set(export_inspector_arguments --coff-exports)
else()
    set(export_inspector_arguments /NOLOGO /EXPORTS)
endif()

execute_process(
    COMMAND "${EXPORT_INSPECTOR}" ${export_inspector_arguments}
        "${MUSTACHE_DLL}"
    RESULT_VARIABLE inspector_result
    OUTPUT_VARIABLE exports
    ERROR_VARIABLE inspector_error)
if(NOT inspector_result EQUAL 0)
    message(FATAL_ERROR "Export inspection failed: ${inspector_error}")
endif()

set(required_exports
    "mustache_version"
    "?empty@CompiledTemplate@mustache@@"
    "?fromJSON@Data@mustache@@"
    "?active@LambdaRenderContext@mustache@@"
    "?invoke@Lambda@mustache@@"
    "?serialize@Node@mustache@@"
    "?serializeValue@Node@mustache@@"
    "?unserializeOwned@Node@mustache@@"
    "?to_template_string@Node@mustache@@"
    "?render@Renderer@mustache@@"
    "?tokenize@Tokenizer@mustache@@"
    "?compile@mustache@@"
    "?stripWhitespace@mustache@@")
foreach(required_export IN LISTS required_exports)
    string(FIND "${exports}" "${required_export}" export_position)
    if(export_position EQUAL -1)
        message(FATAL_ERROR
            "Required public symbol is absent from mustache.dll: ${required_export}")
    endif()
endforeach()

set(forbidden_exports
    "??$"
    "JSONDataBuilder@"
    "Storage@Data@"
    "State@CompiledTemplate@"
    "State@LambdaRenderContext@"
    "TemplateStringState@"
    "?appendNodeTemplate@"
    "?unserializeOwnedRange@"
    "?_renderNode@Renderer@"
    "?_renderChildren@Renderer@"
    "?_lookup@Renderer@"
    "?parseJSON@Data@"
    "?parsedFloating@Data@"
    "?makeStorage@Data@"
    "?requireStorage@Data@"
    "?resetMovedFrom@Node@")
foreach(forbidden_export IN LISTS forbidden_exports)
    string(FIND "${exports}" "${forbidden_export}" export_position)
    if(NOT export_position EQUAL -1)
        message(FATAL_ERROR
            "Private implementation symbol leaked from mustache.dll: ${forbidden_export}")
    endif()
endforeach()
