# Benchmark firmware: reproduces the numbers in the README "Results" section.
# See bench/README.md for what each one measures and how to read its output.

# Benchmark C code must not touch the FPU: GCC would otherwise use S/D
# registers for 64-bit copies, give extra tasks FPU state and change what
# bench_ctx_switch_fpu measures. Only its explicit `vmov` uses the FPU.
set(KESTREL_BENCH_OPTIONS -mgeneral-regs-only)

add_library(kestrel_bench_common OBJECT common/bench.c)
target_include_directories(kestrel_bench_common PUBLIC common)
target_compile_options(kestrel_bench_common PRIVATE ${KESTREL_BENCH_OPTIONS})
target_link_libraries(kestrel_bench_common PRIVATE Kestrel::kernel Kestrel::drivers kestrel_warnings)

function(kestrel_add_bench name source)
    kestrel_add_firmware(${name} ${source} $<TARGET_OBJECTS:kestrel_bench_common>)
    target_include_directories(${name} PRIVATE common)
    set_source_files_properties(${source} TARGET_DIRECTORY ${name}
        PROPERTIES COMPILE_OPTIONS "${KESTREL_BENCH_OPTIONS}")
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
