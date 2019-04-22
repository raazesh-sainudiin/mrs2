#!/bin/bash
#File: RunMappedGaussianMDE.sh

DATASEED=1
D=1
N=100
MAXLEAVESEST=100 #maximum number of leaves in the function estimator
CRITLEAVES=100 #split until this number of leaves in the PQ
NUM_CHECKS=10 #collate the num_check-th histogram
NUM_ITERS=5 #number of iterations for "zooming-in" 

./MappedGaussianMDE $DATASEED $D $N $MAXLEAVESEST $CRITLEAVES $NUM_CHECKS $NUM_ITERS