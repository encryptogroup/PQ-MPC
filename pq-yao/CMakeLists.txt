add_library(pq-yao garble-gates.cpp)

target_link_libraries(pq-yao
    PUBLIC
    emp-tool
    pq-ot
)
