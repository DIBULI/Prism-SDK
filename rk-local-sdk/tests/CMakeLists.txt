add_executable(prism-rklocal-api-test api_test.cpp)
target_link_libraries(prism-rklocal-api-test PRIVATE Prism::RkLocal)
target_compile_options(prism-rklocal-api-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME prism-rklocal-api COMMAND prism-rklocal-api-test)

add_executable(prism-rklocal-api-parity-test api_parity_test.cpp)
target_link_libraries(prism-rklocal-api-parity-test PRIVATE Prism::RkLocal)
add_test(NAME prism-rklocal-api-parity COMMAND prism-rklocal-api-parity-test)

# Explicit manual execution only; CTest must never start real capture.
add_executable(prism-rklocal-hardware-test hardware_test.cpp)
target_link_libraries(prism-rklocal-hardware-test PRIVATE Prism::RkLocal)
target_compile_options(prism-rklocal-hardware-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
