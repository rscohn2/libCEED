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

ceed="${ceed:-/cpu/self/xsmm/blocked}"
common_args=(-ceed $ceed -problem hyperFS)
bc_args=(-bc_clamp 1,2,3,4,5,6 -bc_clamp_1_scale .6666666666666667,2.25,.6666666666666667 -bc_clamp_2_scale .6666666666666667,2.25,.6666666666666667 -bc_clamp_3_scale .6666666666666667,2.25,.6666666666666667 -bc_clamp_4_scale .6666666666666667,2.25,.6666666666666667 -bc_clamp_5_scale .6666666666666667,2.25,.6666666666666667 -bc_clamp_6_scale .6666666666666667,2.25,.6666666666666667)
materiel_args=(-nu 0.32 -E 69e6 -units_meter 100)
solver_args=(-snes_ksp_ew -snes_ksp_ew_alpha 2 -snes_rtol 1e-6)
num_meshes=7
meshes=(15 10 6 5 3 2 1)
steps=(x 75 75 100 150 250 500)
i=
for ((i = 0; i < num_meshes; i++)); do
   mesh_args=(-mesh unstructured_box_${meshes[$i]}.msh)
   max_p=6
   sol_p=
   for ((sol_p = 1; sol_p <= max_p; sol_p++)); do
      all_args=("${common_args[@]}" "${bc_args[@]}" "${materiel_args[@]}" "${mesh_args[@]}" "${solver_args[@]}" -degree $sol_p -num_steps ${steps[$sol_p]})
      echo
      echo "Running test:"
      echo mpiexec -n 32 ./elasticity "${all_args[@]}"
      mpiexec -n 32 ./elasticity "${all_args[@]}" || \
      printf "\nError in the test, error code: $?\n\n"
   done
done
