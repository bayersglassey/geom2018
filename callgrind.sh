# NOTE: use `callgrind_control -i on` once you want to start tracing
valgrind --tool=callgrind --instr-atstart=no -- bin/demo "$@"
