add_executable(anf_test test.c)
target_link_libraries(anf_test libanf)
target_include_directories(anf_test PUBLIC ../src)

add_test(NAME core      COMMAND anf_test)

add_test(NAME literals  COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/literals.anf)
add_test(NAME functions COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/functions.anf)
add_test(NAME loops     COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/loops.anf)
add_test(NAME ops       COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/ops.anf)
add_test(NAME pow       COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/pow.anf)
add_test(NAME lambda    COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/lambda.anf)

add_test(NAME params    COMMAND anf --must-fail ${PROJECT_SOURCE_DIR}/test/invalid/params.anf)
add_test(NAME comment   COMMAND anf --must-fail ${PROJECT_SOURCE_DIR}/test/invalid/comment.anf)
