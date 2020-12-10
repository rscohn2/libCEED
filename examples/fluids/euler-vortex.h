// Copyright (c) 2017, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-734707. All Rights
// reserved. See files LICENSE and NOTICE for details.
//
// This file is part of CEED, a collection of benchmarks, miniapps, software
// libraries and APIs for efficient high-order finite element and spectral
// element discretizations for exascale applications. For more information and
// source code availability see http://github.com/ceed.
//
// The CEED research is supported by the Exascale Computing Project 17-SC-20-SC,
// a collaborative effort of two U.S. Department of Energy organizations (Office
// of Science and the National Nuclear Security Administration) responsible for
// the planning and preparation of a capable exascale ecosystem, including
// software, applications, hardware, advanced system engineering and early
// testbed platforms, in support of the nation's exascale computing imperative.

/// @file
/// Euler traveling vortex initial condition and operator for Navier-Stokes
/// example using PETSc

// Model from:
//   On the order of accuracy and numerical performance of two classes of
//   finite volume WENO, Zhang, Zhang, and Shu (2009).

#ifndef eulervortex_h
#define eulervortex_h

#ifndef __CUDACC__
#  include <math.h>
#endif

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

#ifndef euler_context_struct
#define euler_context_struct
typedef struct EulerContext_ *EulerContext;
struct EulerContext_ {
  CeedScalar time;
  CeedScalar center[3];
  CeedScalar currentTime;
  CeedScalar vortex_strength;
  CeedScalar etv_mean_velocity[3];
};
#endif

// *****************************************************************************
// This function sets the initial conditions and the boundary conditions
//
// These initial conditions are given in terms of potential temperature and
//   Exner pressure and then converted to density and total energy.
//   Initial momentum density is zero.
//
// Initial Conditions:
//   Density     = 1
//   Pressure    = 1
//   Temperature = P / rho - (gamma - 1) vortex_strength**2
//                 exp(1 - r**2) / (8 gamma pi**2)
//   Velocity = vortex_strength exp((1 - r**2)/2.) [yc - y, x - xc, 0] / (2 pi)
//         r  = sqrt( (x - xc)**2 + (y - yc)**2 + (z - zc)**2 )
//
// Conversion to Conserved Variables:
//   E   = rho (cv T + (u u)/2 )
//
// TODO: Not sure what to do about BCs
//
// Constants:
//   cv              ,  Specific heat, constant volume
//   cp              ,  Specific heat, constant pressure
//   vortex_strength ,  Strength of vortex
//   center          ,  Location of bubble center
//   gamma  = cp / cv,  Specific heat ratio
// *****************************************************************************

// *****************************************************************************
// This helper function provides support for the exact, time-dependent solution
//   (currently not implemented) and IC formulation for Euler traveling vortex
// *****************************************************************************
static inline int Exact_Euler(CeedInt dim, CeedScalar time, const CeedScalar X[],
                           CeedInt Nf, CeedScalar q[], void *ctx) {
  // Context
  const EulerContext context = (EulerContext)ctx;
  const CeedScalar vortex_strength = context->vortex_strength;
  const CeedScalar *center = context->center; // Center of the domain
  const CeedScalar *etv_mean_velocity = context->etv_mean_velocity;

  // Setup
  const CeedScalar gamma = 1.4;
  const CeedScalar cv = 2.5; // cv computed based on Rd = 1
  const CeedScalar x = X[0], y = X[1], z = X[2]; // Coordinates
   // Vortex center
  const CeedScalar xc = center[0] + etv_mean_velocity[0] * time;
  const CeedScalar yc = center[1] + etv_mean_velocity[1] * time;

  const CeedScalar x0 = x - xc;
  const CeedScalar y0 = y - yc;
  const CeedScalar r = sqrt( x0*x0 + y0*y0 );
  const CeedScalar C = vortex_strength * exp((1. - r*r)/2.) / (2. * M_PI);
  const CeedScalar S = (gamma - 1.) * vortex_strength * vortex_strength /
                       (8.*gamma*M_PI*M_PI);

  // Exact Solutions
  const CeedScalar rho = 1.;
  const CeedScalar P = 1.;
  const CeedScalar T = P / rho - S * exp(1. - r*r);
  const CeedScalar u[3] = {etv_mean_velocity[0] - C*y0, etv_mean_velocity[1] +
                           C*x0, 0.};
  // Initial Conditions
  q[0] = rho;
  q[1] = rho * u[0];
  q[2] = rho * u[1];
  q[3] = rho * u[2];
  q[4] = rho * ( cv*T + (u[0]*u[0] + u[1]*u[1] + u[2]*u[2]) / 2. );

  return 0;
}

// *****************************************************************************
// This QFunction sets the initial conditions for Euler traveling vortex
// *****************************************************************************
CEED_QFUNCTION(ICsEuler)(void *ctx, CeedInt Q,
                      const CeedScalar *const *in, CeedScalar *const *out) {
  // Inputs
  const CeedScalar (*X)[CEED_Q_VLA] = (const CeedScalar(*)[CEED_Q_VLA])in[0];

  // Outputs
  CeedScalar (*q0)[CEED_Q_VLA] = (CeedScalar(*)[CEED_Q_VLA])out[0];
  const EulerContext context = (EulerContext)ctx;

  CeedPragmaSIMD
  // Quadrature Point Loop
  for (CeedInt i=0; i<Q; i++) {
    const CeedScalar x[] = {X[0][i], X[1][i], X[2][i]};
    CeedScalar q[5];

    Exact_Euler(3, context->time, x, 5, q, ctx);

    for (CeedInt j=0; j<5; j++)
      q0[j][i] = q[j];
  } // End of Quadrature Point Loop

  // Return
  return 0;
}

// *****************************************************************************
// This QFunction implements the following formulation of Euler equations
//   with explicit time stepping method
//
// This is 3D Euler for compressible gas dynamics in conservation
//   form with state variables of density, momentum density, and total
//   energy density.
//
// State Variables: q = ( rho, U1, U2, U3, E )
//   rho - Mass Density
//   Ui  - Momentum Density,      Ui = rho ui
//   E   - Total Energy Density,  E  = rho ( cv T + (u u) / 2 )
//
// Euler Equations:
//   drho/dt + div( U )                   = 0
//   dU/dt   + div( rho (u x u) + P I3 )  = 0
//   dE/dt   + div( (E + P) u )           = 0
//
// Equation of State:
//   P = (gamma - 1) (E - rho (u u) / 2)
//
// Constants:
//   cv              ,  Specific heat, constant volume
//   cp              ,  Specific heat, constant pressure
//   g               ,  Gravity
//   gamma  = cp / cv,  Specific heat ratio
//
// *****************************************************************************

// *****************************************************************************
// This helper function provides forcing term for Euler traveling vortex
//   manufactured solution
// *****************************************************************************
static inline int MMSforce_Euler(CeedInt dim, CeedScalar time, const CeedScalar X[],
                           CeedInt Nf, CeedScalar force[], void *ctx) {
  // Context
  const EulerContext context = (EulerContext)ctx;
  const CeedScalar vortex_strength = context->vortex_strength;
  const CeedScalar *center = context->center; // Center of the domain
  const CeedScalar *etv_mean_velocity = context->etv_mean_velocity;

  // Setup
  const CeedScalar gamma = 1.4;
  const CeedScalar cv = 2.5; // cv computed based on Rd = 1
  const CeedScalar x = X[0], y = X[1], z = X[2]; // Coordinates
   // Vortex center
  const CeedScalar xc = center[0] + etv_mean_velocity[0] * time;
  const CeedScalar yc = center[1] + etv_mean_velocity[1] * time;

  const CeedScalar x0 = x - xc;
  const CeedScalar y0 = y - yc;
  const CeedScalar r = sqrt( x0*x0 + y0*y0 );
  const CeedScalar C = vortex_strength * exp((1. - r*r)/2.) / (2. * M_PI);
  const CeedScalar S = (gamma - 1.) * vortex_strength * vortex_strength /
                       (8.*gamma*M_PI*M_PI);
  const CeedScalar u[3] = {etv_mean_velocity[0] - C*y0, etv_mean_velocity[1] +
                           C*x0, 0.};

  // Forcing term for Manufactured solution
  force[0] = 0.;
  force[1] = C * ( 2*etv_mean_velocity[1] + x0 *C );
  force[2] = -C*C*y0;
  force[3] = 0.;
  force[4] = 2.*S*cv*(x0*u[0] + y0*u[1]) + x0*y0*C*(u[0]*u[0] - u[1]*u[1]) *
             C*u[0]*u[1]*(y0*y0 - x0*x0) + 2.*C*u[0]*u[1];
  return 0;
}
// *****************************************************************************
CEED_QFUNCTION(Euler)(void *ctx, CeedInt Q,
                   const CeedScalar *const *in, CeedScalar *const *out) {
  // *INDENT-OFF*
  // Inputs
  const CeedScalar (*q)[CEED_Q_VLA] = (const CeedScalar(*)[CEED_Q_VLA])in[0],
                   (*dq)[5][CEED_Q_VLA] = (const CeedScalar(*)[5][CEED_Q_VLA])in[1],
                   (*qdata)[CEED_Q_VLA] = (const CeedScalar(*)[CEED_Q_VLA])in[2],
                   (*x)[CEED_Q_VLA] = (const CeedScalar(*)[CEED_Q_VLA])in[3];
  // Outputs
  CeedScalar (*v)[CEED_Q_VLA] = (CeedScalar(*)[CEED_Q_VLA])out[0],
             (*dv)[5][CEED_Q_VLA] = (CeedScalar(*)[5][CEED_Q_VLA])out[1];
  // Context
  const EulerContext context = (EulerContext)ctx;
  const CeedScalar currentTime = context->currentTime;
  const CeedScalar gamma  = 1.4;

  CeedPragmaSIMD
  // Quadrature Point Loop
  for (CeedInt i=0; i<Q; i++) {
    // *INDENT-OFF*
    // Setup
    // -- Interp in
    const CeedScalar rho        =   q[0][i];
    const CeedScalar u[3]       =  {q[1][i] / rho,
                                    q[2][i] / rho,
                                    q[3][i] / rho
                                   };
    const CeedScalar E          =   q[4][i];
    // -- Interp-to-Interp qdata
    const CeedScalar wdetJ      =   qdata[0][i];
    // -- Interp-to-Grad qdata
    // ---- Inverse of change of coordinate matrix: X_i,j
    // *INDENT-OFF*
    const CeedScalar dXdx[3][3] = {{qdata[1][i],
                                    qdata[2][i],
                                    qdata[3][i]},
                                   {qdata[4][i],
                                    qdata[5][i],
                                    qdata[6][i]},
                                   {qdata[7][i],
                                    qdata[8][i],
                                    qdata[9][i]}
                                  };
    // *INDENT-ON*
    const CeedScalar P  = 1.; // P = pressure
    const CeedScalar X[] = {x[0][i], x[1][i], x[2][i]};
    CeedScalar force[5];
    MMSforce_Euler(3, currentTime, X, 5, force, ctx);

    // The Physics
    for (int j=0; j<5; j++) {
      v[j][i] = force[j]; // MMS forcing term
      for (int k=0; k<3; k++)
        dv[k][j][i] = 0; // Zero dv so all future terms can safely sum into it
    }

    // -- Density
    // ---- u rho
    for (int j=0; j<3; j++)
      dv[j][0][i]  += wdetJ*(rho*u[0]*dXdx[j][0] + rho*u[1]*dXdx[j][1] +
                             rho*u[2]*dXdx[j][2]);
    // -- Momentum
    // ---- rho (u x u) + P I3
    for (int j=0; j<3; j++)
      for (int k=0; k<3; k++)
        dv[k][j+1][i]  += wdetJ*((rho*u[j]*u[0] + (j==0?P:0))*dXdx[k][0] +
                                 (rho*u[j]*u[1] + (j==1?P:0))*dXdx[k][1] +
                                 (rho*u[j]*u[2] + (j==2?P:0))*dXdx[k][2]);
    // -- Total Energy Density
    // ---- (E + P) u
    for (int j=0; j<3; j++)
      dv[j][4][i]  += wdetJ * (E + P) * (u[0]*dXdx[j][0] + u[1]*dXdx[j][1] +
                                         u[2]*dXdx[j][2]);
  } // End Quadrature Point Loop

  // Return
  return 0;
}
// *****************************************************************************

#endif // eulervortex_h
