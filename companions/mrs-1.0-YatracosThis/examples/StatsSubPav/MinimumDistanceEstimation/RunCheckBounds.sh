#!/bin/bash
#File: RunCheckBounds.sh

DATASEED=1
D=1
N=150
HOLDOUTPERCENT=0.33
MAXLEAVESEST=5 #maximum number of leaves in the function estimator
CRITLEAVES=5 #split until this number of leaves in the PQ
NUM_CHECKS=5 #collate num_checks histogram
NUM_ITERS=5 #number of iterations for "zooming-in" 

./CheckBounds $DATASEED $D $N $HOLDOUTPERCENT $MAXLEAVESEST $CRITLEAVES $NUM_CHECKS $NUM_ITERS
