//
// $Id: variable.h,v 1.1 2006/06/27 18:41:29 nakayama Exp $
//
#ifndef VARIABLE_H
#define VARIABLE_H

#include "parameter_define.h"
#include "quaternion.h"

// omega,ux,uy,phi, phi_up $B$J$I>l$NJQ?t$r3JG<$9$k(B
typedef double ** *Value;
typedef int ** *Value_int;

typedef struct CTime {
  int ts; // time step
  double time; // time
  double dt_fluid;  // time increment
  double hdt_fluid; // 1/2 * dt
  double dt_md;  // time increment
  double hdt_md; // 1/2 * dt
} CTime;

typedef struct Particle {
  double eff_mass_ratio;
  int spec;
  double x[DIM];
  double x_previous[DIM];

  double v[DIM];
  double v_old[DIM];

  double f_hydro[DIM];
  double f_hydro_previous[DIM];
  double f_hydro1[DIM];
  double f_slip_previous[DIM];

  double fr[DIM];
  double fr_previous[DIM];

  double omega[DIM];
  double omega_old[DIM];

  double torque_hydro[DIM];
  double torque_hydro_previous[DIM];
  double torque_hydro1[DIM];
  double torque_slip_previous[DIM];

  double momentum_depend_fr[DIM];

  double QR[DIM][DIM];
  double QR_old[DIM][DIM];

  quaternion q;
  quaternion q_old;
} Particle;

typedef struct Index_range {
  int istart;
  int iend;
  int jstart;
  int jend;
  int kstart;
  int kend;
} Index_range;


#endif
