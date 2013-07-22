
add_executable(ingler ingler.c)
install(PROGRAMS ingler ingler.cputemp DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
