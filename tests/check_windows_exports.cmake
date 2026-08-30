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
    "??1Exception@mustache@@"
    "??_7Exception@mustache@@6B@"
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
if(MUSTACHE_ARCHIVED_TEMPLATES)
    function(require_archive_export_count pattern expected description)
        string(REGEX MATCHALL "${pattern}" matches "${exports}")
        list(LENGTH matches actual)
        if(NOT actual EQUAL expected)
            message(FATAL_ERROR
                "Archived-template ${description}: expected ${expected} exports, found ${actual}")
        endif()
    endfunction()

    require_archive_export_count("\\?\\?0ArchivedTemplateLimits@mustache@@" 1 "limits constructor")
    require_archive_export_count(
        "\\?\\?0ArchivedTemplateException@mustache@@[^\n]*ArchivedTemplateError@" 1
        "load exception constructor")
    require_archive_export_count(
        "\\?reason@ArchivedTemplateException@mustache@@" 1
        "load exception reason method")
    require_archive_export_count("\\?\\?0ArchivedTemplate@mustache@@" 3 "constructors")
    require_archive_export_count("\\?\\?1ArchivedTemplate@mustache@@" 1 "destructor")
    require_archive_export_count("\\?\\?4ArchivedTemplate@mustache@@" 2 "assignment operators")
    require_archive_export_count("\\?\\?BArchivedTemplate@mustache@@" 1 "bool conversion")
    require_archive_export_count("\\?empty@ArchivedTemplate@mustache@@" 1 "empty method")
    require_archive_export_count("\\?loadArchivedTemplate@mustache@@" 2 "load overloads")
    require_archive_export_count("\\?serializeArchivedTemplate@mustache@@" 2 "serialize overloads")
    require_archive_export_count(
        "\\?archivedTemplateCompatibilityTag@mustache@@" 1
        "compatibility tag query")
    require_archive_export_count(
        "\\?render@Mustache@mustache@@[^\n]*ArchivedTemplate@" 2
        "member render overloads")
    require_archive_export_count(
        "\\?render@mustache@@[^\n]*ArchivedTemplate@" 2
        "free render overloads")
endif()
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
    "?archivedTemplateRoot@CompiledTemplate@"
    "?unserializeOwnedRange@"
    "?_renderNode@Renderer@"
    "?_renderChildren@Renderer@"
    "?_lookup@Renderer@"
    "?parseJSON@Data@"
    "?parsedFloating@Data@"
    "?makeStorage@Data@"
    "?requireStorage@Data@"
    "?resetMovedFrom@Node@")
if(MUSTACHE_ARCHIVED_TEMPLATES)
    list(APPEND forbidden_exports
        "ArchivedTemplateView@"
        "@cista@@"
        "MUSTACHE_CISTA_XXH_"
        "mustache_cista_xxh3_64bits_with_seed")
endif()
foreach(forbidden_export IN LISTS forbidden_exports)
    string(FIND "${exports}" "${forbidden_export}" export_position)
    if(NOT export_position EQUAL -1)
        message(FATAL_ERROR
            "Private implementation symbol leaked from mustache.dll: ${forbidden_export}")
    endif()
endforeach()
