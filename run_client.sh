#!/bin/bash

nohup ./bin/client -u 1 -c powersave -r 500 -o 1 -t 10 > percentile_1.out 2>&1 &
nohup ./bin/client -u 2 -c powersave -r 500 -o 1 -t 10 > percentile_2.out 2>&1 &
nohup ./bin/client -u 3 -c powersave -r 500 -o 1 -t 10 > percentile_3.out 2>&1 &
nohup ./bin/client -u 4 -c powersave -r 500 -o 1 -t 10 > percentile_4.out 2>&1 &
nohup ./bin/client -u 5 -c powersave -r 500 -o 1 -t 10 > percentile_5.out 2>&1 &
nohup ./bin/client -u 6 -c powersave -r 500 -o 1 -t 10 > percentile_6.out 2>&1 &
nohup ./bin/client -u 7 -c powersave -r 500 -o 1 -t 10 > percentile_7.out 2>&1 &
nohup ./bin/client -u 8 -c powersave -r 500 -o 1 -t 10 > percentile_8.out 2>&1 &
nohup ./bin/client -u 9 -c powersave -r 500 -o 1 -t 10 > percentile_9.out 2>&1 &
nohup ./bin/client -u 10 -c powersave -r 500 -o 1 -t 10 > percentile_10.out 2>&1 &
