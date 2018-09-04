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
    ast.h
    lex.h
    parse.h
    htable.h
    mpool.h
    scope.h
    io.h
    print.h
    util.h)

add_library(libanf ${LIBANF_SRCS} ${LIBANF_HDRS})
set_target_properties(libanf PROPERTIES PREFIX "")

add_executable(anf main.c)
target_link_libraries(anf libanf)

install(TARGETS anf DESTINATION bin)
install(TARGETS libanf DESTINATION lib)
install(FILES ${LIBANF_HDRS} DESTINATION include)
