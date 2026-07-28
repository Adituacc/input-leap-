# InputLeap protocol microbenchmarks. These executables are intentionally not
# registered with CTest because timing assertions are machine-dependent.

add_executable(protocol_benchmark
    ProtocolBenchmark.cpp
)

target_link_libraries(protocol_benchmark
    arch
    synlib
    io
    base
    common
    mt
    ${libs}
)
