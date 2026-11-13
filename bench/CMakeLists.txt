# Benchmark firmware: reproduces the numbers in the README "Results" section.
# See bench/README.md for what each one measures and how to read its output.
# Built like any hard-float application: no extra compiler options.

add_library(kestrel_bench_common OBJECT common/bench.c)
target_include_directories(kestrel_bench_common PUBLIC common)
target_link_libraries(kestrel_bench_common PRIVATE Kestrel::kernel Kestrel::drivers kestrel_warnings)

function(kestrel_add_bench name source)
    kestrel_add_firmware(${name} ${source} $<TARGET_OBJECTS:kestrel_bench_common>)
    target_include_directories(${name} PRIVATE common)
endfunction()

kestrel_add_bench(bench_ctx_switch ctx_switch/main.c)
target_compile_definitions(bench_ctx_switch PRIVATE USE_FPU=0)

kestrel_add_bench(bench_ctx_switch_fpu ctx_switch/main.c)
target_compile_definitions(bench_ctx_switch_fpu PRIVATE USE_FPU=1)

kestrel_add_bench(bench_isr_latency isr_latency/main.c)

kestrel_add_bench(bench_jitter jitter/main.c)
target_compile_definitions(bench_jitter PRIVATE LOAD_ENABLED=1)

kestrel_add_bench(bench_jitter_idle jitter/main.c)
target_compile_definitions(bench_jitter_idle PRIVATE LOAD_ENABLED=0)
