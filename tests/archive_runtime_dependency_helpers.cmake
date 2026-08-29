function(mustache_is_zlib_runtime_dependency MUSTACHE_DEPENDENCY MUSTACHE_RESULT)
    get_filename_component(MUSTACHE_DEPENDENCY_NAME
        "${MUSTACHE_DEPENDENCY}" NAME)
    string(TOLOWER "${MUSTACHE_DEPENDENCY_NAME}"
        MUSTACHE_DEPENDENCY_NAME)
    if(MUSTACHE_DEPENDENCY_NAME MATCHES
            "^libz(\\.[0-9]+)*\\.(so|dylib)(\\.[0-9]+)*$" OR
            MUSTACHE_DEPENDENCY_NAME MATCHES
                "^(libz([0-9._-][^/]*)?|zlib[^/]*)\\.dll$")
        set("${MUSTACHE_RESULT}" TRUE PARENT_SCOPE)
    else()
        set("${MUSTACHE_RESULT}" FALSE PARENT_SCOPE)
    endif()
endfunction()
