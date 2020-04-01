include(cmake/source_of_randomness.cmake)

set(sources
    emp-tool.cpp
    circuits/float32_add.cpp
    circuits/float32_cos.cpp
    circuits/float32_div.cpp
    circuits/float32_eq.cpp
    circuits/float32_le.cpp
    circuits/float32_leq.cpp
    circuits/float32_mul.cpp
    circuits/float32_sin.cpp
    circuits/float32_sq.cpp
    circuits/float32_sqrt.cpp
    circuits/float32_sub.cpp
    circuits/float32_exp2.cpp
    circuits/float32_exp.cpp
    circuits/float32_ln.cpp
    circuits/float32_log2.cpp
)

add_library(emp-tool ${sources})
target_include_directories(emp-tool PUBLIC ${GMP_INCLUDE_DIR})
target_link_libraries(emp-tool
    PUBLIC
    ${GMP_LIBRARIES}
    ${GMPXX_LIBRARIES}
)
