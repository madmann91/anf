add_executable(lex_gen lex_gen.c)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex.inc
    COMMAND $<TARGET_FILE:lex_gen> > ${CMAKE_CURRENT_BINARY_DIR}/lex.inc
    DEPENDS lex_gen
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_custom_target(lex_inc DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lex.inc)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(LIBANF_SRCS
    ast.c
    lex.c
    log.c
    parse.c
    bind.c
    check.c
    htable.c
    mod.c
    mpool.c
    scope.c
    io.c
    node.c
    print.c
    type.c
    util.c)

set(LIBANF_HDRS
    adt.h
    ast.h
    lex.h
    log.h
    parse.h
    bind.h
    check.h
    hash.h
    htable.h
    lex.h
    mod.h
    mpool.h
    parse.h
    scope.h
    io.h
    node.h
    print.h
    type.h
    util.h)

add_library(libanf ${LIBANF_SRCS} ${LIBANF_HDRS} lex.inc)
add_dependencies(libanf lex_inc)
set_target_properties(libanf PROPERTIES PREFIX "")

add_executable(anf main.c)
target_link_libraries(anf libanf)

install(TARGETS anf DESTINATION bin)
install(TARGETS libanf DESTINATION lib)
install(FILES ${LIBANF_HDRS} DESTINATION include)
