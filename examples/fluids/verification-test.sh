#!/bin/bash

# Copyright (c) 2017-2018, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory. LLNL-CODE-734707.
# All Rights reserved. See files LICENSE and NOTICE for details.
#
# This file is part of CEED, a collection of benchmarks, miniapps, software
# libraries and APIs for efficient high-order finite element and spectral
# element discretizations for exascale applications. For more information and
# source code availability see http://github.com/ceed.
#
# The CEED research is supported by the Exascale Computing Project 17-SC-20-SC,
# a collaborative effort of two U.S. Department of Energy organizations (Office
# of Science and the National Nuclear Security Administration) responsible for
# the planning and preparation of a capable exascale ecosystem, including
# software, applications, hardware, advanced system engineering and early
# testbed platforms, in support of the nation's exascale computing imperative.

declare -A run_flags
  run_flags[ceed]=/cpu/self/ref/serial
  run_flags[problem]=euler_vortex  # Options: "euler_vortex" and "advection2d"
  run_flags[degree]=1
  run_flags[dm_plex_box_faces]=20,20
  run_flags[ts_adapt_dt_max]=.01
  run_flags[ts_max_time]=.01

# Remove previous test results
if ! [[ -z ./verification-output/${run_flags[problem]}/*.log ]]; then
     rm -R ./verification-output/${run_flags[problem]}/*.log
fi

declare -A test_flags
  test_flags[degree_start]=1
  test_flags[degree_end]=4
  test_flags[res_start]=2
  test_flags[res_stride]=4
  test_flags[res_end]=10

for ((d=${test_flags[degree_start]}; d<=${test_flags[degree_end]}; d++)); do
    run_flags[degree]=$d
    for ((res=${test_flags[res_start]}; res<=${test_flags[res_end]}; res+=${test_flags[res_stride]})); do
        run_flags[dm_plex_box_faces]=$res,$res
        args=''
        for arg in "${!run_flags[@]}"; do
            if ! [[ -z ${run_flags[$arg]} ]]; then
                args="$args -$arg ${run_flags[$arg]}"
            fi
        done
        echo $args  &>> ./verification-output/${run_flags[problem]}/${run_flags[degree]}_${res}.log
        ./navierstokes $args  &>> ./verification-output/${run_flags[problem]}/${run_flags[degree]}_${res}.log
    done
done