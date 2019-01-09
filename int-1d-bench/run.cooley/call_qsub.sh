#!/bin/sh
qsub -n 4 -t 15 -A datascience ./submit.sh
