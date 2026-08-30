if(NOT DEFINED EXPORT_INSPECTOR OR NOT EXISTS "${EXPORT_INSPECTOR}")
    message(FATAL_ERROR "A valid export inspector is required")
endif()
if(NOT DEFINED MUSTACHE_LIBRARY OR NOT EXISTS "${MUSTACHE_LIBRARY}")
    message(FATAL_ERROR "A valid mustache shared library is required")
endif()

if(MUSTACHE_OBJECT_FORMAT STREQUAL "MACHO")
    set(export_inspector_arguments -g -U)
elseif(MUSTACHE_OBJECT_FORMAT STREQUAL "ELF")
    set(export_inspector_arguments -D -g --defined-only)
else()
    message(FATAL_ERROR
        "Unsupported shared-library object format: ${MUSTACHE_OBJECT_FORMAT}")
endif()

execute_process(
    COMMAND "${EXPORT_INSPECTOR}" ${export_inspector_arguments}
        "${MUSTACHE_LIBRARY}"
    RESULT_VARIABLE inspector_result
    OUTPUT_VARIABLE exports
    ERROR_VARIABLE inspector_error)
if(NOT inspector_result EQUAL 0)
    message(FATAL_ERROR "Export inspection failed: ${inspector_error}")
endif()

string(REPLACE "\r\n" "\n" exports "${exports}")
string(REPLACE "\r" "\n" exports "${exports}")
string(REPLACE "\n" ";" export_lines "${exports}")
set(export_symbols)
foreach(export_line IN LISTS export_lines)
    string(STRIP "${export_line}" export_line)
    if(export_line MATCHES ":$")
        continue()
    endif()
    if(export_line MATCHES "([^ \t]+)$")
        list(APPEND export_symbols "${CMAKE_MATCH_1}")
    endif()
endforeach()

function(require_export_count pattern expected description)
    set(actual 0)
    foreach(export_symbol IN LISTS export_symbols)
        if(export_symbol MATCHES "${pattern}")
            math(EXPR actual "${actual} + 1")
        endif()
    endforeach()
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR
            "${description}: expected ${expected} exports, found ${actual}")
    endif()
endfunction()

require_export_count("^_?mustache_version$" 1 "mustache_version")
require_export_count("^_?mustache_version_int$" 1 "mustache_version_int")
require_export_count("^_?_ZTIN8mustache9ExceptionE$" 1 "Exception RTTI")
require_export_count("^_?_ZTVN8mustache9ExceptionE$" 1 "Exception vtable")
if(MUSTACHE_ARCHIVED_TEMPLATES)
    require_export_count(
        "^_?_ZN8mustache22ArchivedTemplateLimitsC1Ev$" 1
        "ArchivedTemplateLimits default constructor")
    require_export_count(
        "^_?_ZN8mustache25ArchivedTemplateExceptionC1ENS_21ArchivedTemplateErrorERK.*$" 1
        "ArchivedTemplateException constructor")
    require_export_count(
        "^_?_ZNK8mustache25ArchivedTemplateException6reasonEv$" 1
        "ArchivedTemplateException reason method")
    require_export_count(
        "^_?_ZN8mustache16ArchivedTemplateC1Ev$" 1
        "ArchivedTemplate default constructor")
    require_export_count(
        "^_?_ZN8mustache16ArchivedTemplateC1ERKS0_$" 1
        "ArchivedTemplate copy constructor")
    require_export_count(
        "^_?_ZN8mustache16ArchivedTemplateC1EOS0_$" 1
        "ArchivedTemplate move constructor")
    require_export_count(
        "^_?_ZN8mustache16ArchivedTemplateD1Ev$" 1
        "ArchivedTemplate destructor")
    require_export_count(
        "^_?_ZN8mustache16ArchivedTemplateaSERKS0_$" 1
        "ArchivedTemplate copy assignment")
    require_export_count(
        "^_?_ZN8mustache16ArchivedTemplateaSEOS0_$" 1
        "ArchivedTemplate move assignment")
    require_export_count(
        "^_?_ZNK8mustache16ArchivedTemplate5emptyEv$" 1
        "ArchivedTemplate empty method")
    require_export_count(
        "^_?_ZNK8mustache16ArchivedTemplatecvbEv$" 1
        "ArchivedTemplate bool conversion")
    require_export_count(
        "^_?_ZN8mustache20loadArchivedTemplate.*$" 2
        "loadArchivedTemplate overloads")
    require_export_count(
        "^_?_ZN8mustache25serializeArchivedTemplate.*$" 2
        "serializeArchivedTemplate overloads")
    require_export_count(
        "^_?_ZN8mustache32archivedTemplateCompatibilityTagEv$" 1
        "archivedTemplateCompatibilityTag")
    require_export_count(
        "^_?_ZN8mustache6render.*16ArchivedTemplate.*$" 2
        "archived-template free render overloads")
    require_export_count(
        "^_?_ZNK8mustache8Mustache6render.*16ArchivedTemplate.*$" 2
        "archived-template member render overloads")
endif()

set(forbidden_exports
    "ArchivedTemplateView"
    "N5cista"
    "archivedTemplateRoot"
    "MUSTACHE_CISTA_XXH_"
    "mustache_cista_xxh3_64bits_with_seed")
foreach(export_symbol IN LISTS export_symbols)
    foreach(forbidden_export IN LISTS forbidden_exports)
        string(FIND "${export_symbol}" "${forbidden_export}" export_position)
        if(NOT export_position EQUAL -1)
            message(FATAL_ERROR
                "Private implementation symbol leaked from the mustache shared library: ${export_symbol}")
        endif()
    endforeach()
endforeach()
