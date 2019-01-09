#!/bin/sh
NODES=`cat $COBALT_NODEFILE | wc -l`
PROCS=$((NODES * 12))
EXECU=/projects/datascience/rzamora/MPIIOBenchMarks/int-1d-bench/int_1d_bench
mpirun -f $COBALT_NODEFILE -n $PROCS $EXECU --block 134217728 --count 1
rm file.*
# Case from sergios plot: (?)
mpirun -f $COBALT_NODEFILE -n $PROCS $EXECU --block 2097152 --count 64
rm file.*
mpirun -f $COBALT_NODEFILE -n $PROCS $EXECU --block 16777216 --count 1
rm file.*
mpirun -f $COBALT_NODEFILE -n $PROCS $EXECU --block 262144 --count 64
rm file.*
