include("${CMAKE_CURRENT_LIST_DIR}/archive_runtime_dependency_helpers.cmake")

function(mustache_expect_zlib_dependency MUSTACHE_DEPENDENCY)
    mustache_is_zlib_runtime_dependency(
        "${MUSTACHE_DEPENDENCY}" MUSTACHE_IS_ZLIB)
    if(NOT MUSTACHE_IS_ZLIB)
        message(FATAL_ERROR
            "zlib runtime dependency was not recognized: ${MUSTACHE_DEPENDENCY}")
    endif()
endfunction()

function(mustache_expect_other_dependency MUSTACHE_DEPENDENCY)
    mustache_is_zlib_runtime_dependency(
        "${MUSTACHE_DEPENDENCY}" MUSTACHE_IS_ZLIB)
    if(MUSTACHE_IS_ZLIB)
        message(FATAL_ERROR
            "non-zlib runtime dependency was misclassified: ${MUSTACHE_DEPENDENCY}")
    endif()
endfunction()

mustache_expect_zlib_dependency("/usr/lib/libz.so")
mustache_expect_zlib_dependency("/usr/lib/libz.so.1")
mustache_expect_zlib_dependency("/usr/lib/libz.so.1.3.1")
mustache_expect_zlib_dependency("/usr/lib/libz.dylib")
mustache_expect_zlib_dependency("/usr/lib/libz.1.dylib")
mustache_expect_zlib_dependency("/usr/lib/libz.1.2.13.dylib")
mustache_expect_zlib_dependency("C:/runtime/zlib.dll")
mustache_expect_zlib_dependency("C:/runtime/zlib1.dll")
mustache_expect_zlib_dependency("C:/runtime/libz.dll")
mustache_expect_zlib_dependency("C:/runtime/libz-1.dll")

mustache_expect_other_dependency("/usr/lib/libzip.so")
mustache_expect_other_dependency("/usr/lib/libzstd.so.1")
mustache_expect_other_dependency("C:/runtime/libzip.dll")
mustache_expect_other_dependency("C:/runtime/zlibtool.exe")
