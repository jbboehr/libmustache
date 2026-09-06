# Copy the CMake inputs used by nested build tests. Only inspect files directly
# inside known source directories, so in-source build outputs cannot cause a
# test to recursively copy its own scratch directories.
function(mustache_copy_cmake_test_source source destination)
    file(MAKE_DIRECTORY "${destination}")
    file(COPY
        "${source}/CMakeLists.txt"
        "${source}/mustache.pc.in"
        DESTINATION "${destination}")
    foreach(directory IN ITEMS cmake src src/archive src/archive/xxh3 benchmarks
            tests tests/fixtures tests/cmake-consumer tests/cmake-subproject
            vendor/cista vendor/xxhash)
        file(GLOB inputs LIST_DIRECTORIES FALSE
            "${source}/${directory}/CMakeLists.txt"
            "${source}/${directory}/*.cmake"
            "${source}/${directory}/*.cpp"
            "${source}/${directory}/*.hpp"
            "${source}/${directory}/*.h"
            "${source}/${directory}/*.in"
            "${source}/${directory}/*.map")
        list(FILTER inputs EXCLUDE REGEX
            "/(mustache_config\\.h|Makefile\\.in|CTestTestfile\\.cmake|cmake_install\\.cmake)$")
        file(MAKE_DIRECTORY "${destination}/${directory}")
        if(inputs)
            file(COPY ${inputs} DESTINATION "${destination}/${directory}")
        endif()
    endforeach()
endfunction()
