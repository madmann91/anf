add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex.inc
    COMMAND ${PYTHON_EXECUTABLE} lex_gen.py > ${CMAKE_CURRENT_BINARY_DIR}/lex.inc
    DEPENDS ${PYTHON_EXECUTABLE} lex_gen.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_custom_target(lex_gen DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lex.inc)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(LIBANF_SRCS
    anf.c
    ast.c
    lex.c
    parse.c
    htable.c
    mpool.c
    scope.c
    io.c
    opt.c
    mem2reg.c
    flatten.c
    eval.c
    print.c)

set(LIBANF_HDRS
    anf.h
    adt.h
    ast.h
    lex.h
    parse.h
    hash.h
    htable.h
    lex.h
    mpool.h
    parse.h
    scope.h
    io.h
    print.h
    util.h)

add_library(libanf ${LIBANF_SRCS} ${LIBANF_HDRS} lex.inc)
add_dependencies(libanf lex_gen)
set_target_properties(libanf PROPERTIES PREFIX "")

add_executable(anf main.c)
target_link_libraries(anf libanf)

install(TARGETS anf DESTINATION bin)
install(TARGETS libanf DESTINATION lib)
install(FILES ${LIBANF_HDRS} DESTINATION include)
