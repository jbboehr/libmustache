add_executable(mustachec mustache.cpp cli.cpp cli.hpp)
target_link_libraries(mustachec PRIVATE mustache)
mustache_apply_build_options(mustachec)

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set_target_properties(mustachec PROPERTIES
        INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}")
elseif(APPLE)
    set_target_properties(mustachec PROPERTIES
        INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
else()
    set_target_properties(mustachec PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

install(TARGETS mustachec
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
