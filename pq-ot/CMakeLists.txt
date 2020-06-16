add_library(pq-ot
    pq-ot.cpp
    pq-otmain.cpp
)

target_link_libraries(pq-ot
    PUBLIC
    SEAL::seal
    emp-tool
)
