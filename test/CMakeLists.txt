add_executable(anf_test test.c)
target_link_libraries(anf_test libanf)
target_include_directories(anf_test PUBLIC ../src)

add_test(NAME core      COMMAND anf_test)
add_test(NAME literals  COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/literals.anf)
add_test(NAME functions COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/functions.anf)
add_test(NAME loops     COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/loops.anf)
add_test(NAME ops       COMMAND anf ${PROJECT_SOURCE_DIR}/test/valid/ops.anf)
