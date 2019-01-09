#!/bin/sh
NODES=`cat $COBALT_NODEFILE | wc -l`
PROCS=$((NODES * 12))
EXECU=/projects/datascience/rzamora/MPIIOBenchMarks/int-1d-bench/int_1d_bench
mpirun -f $COBALT_NODEFILE -n $PROCS $EXECU --block 1048576 --count 1 
mpirun -f $COBALT_NODEFILE -n $PROCS $EXECU --block 1024 --count 1024 
