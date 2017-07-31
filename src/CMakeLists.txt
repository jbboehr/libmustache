
set(SOURCE_FILES
        compiler.cpp
        data.cpp
        lambda.cpp
        mustache.cpp
        node.cpp
        renderer.cpp
        stack.cpp
        tokenizer.cpp
        utils.cpp
        vm.cpp)

set(HEADERS
        compiler.hpp
        data.hpp
        exception.hpp
        lambda.hpp
        mustache.hpp
        mustache_config.h
        node.hpp
        renderer.hpp
        stack.hpp
        tokenizer.hpp
        utils.hpp
        vm.hpp)

link_libraries(${LIBYAML} ${LIBJSON})

add_library(mustache ${SOURCE_FILES} ${HEADERS})

