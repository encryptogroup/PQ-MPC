set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(pqot test-pqot.cpp)
target_link_libraries(pqot pq-ot)

macro (add_test _name)
	add_executable(${_name} "test-${_name}.cpp")
    target_link_libraries(${_name} pq-yao) 
endmacro()

add_test(pqyao)
add_test(int)
add_test(bit)
add_test(float)
