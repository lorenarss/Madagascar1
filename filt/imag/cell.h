/* This file is automatically generated. DO NOT EDIT! */

#ifndef _cell_h
#define _cell_h

#include <rsf.h>

void cell1_intersect (float a, float x, float dy, float p, 
		      float *sx, int *jx);
/*< intersecting a straight ray with cell boundaries >*/

float cell1_update1 (int dim, float s, float v, float *p, const float *g) ;
/*< symplectic first-order: step 1 >*/


float cell1_update2 (int dim, float s, float v, float *p, const float *g) ;
/*< symplectic first-order: step 2 >*/

void cell11_intersect2 (float a, float da, 
			const float* p, const float* g, 
			float *sp, int *jp);
/*< intersecting a straight ray with cell boundaries >*/

float cell11_update1 (int dim, float s, float v, float *p, const float *g) ;
/*< nonsymplectic first-order: step 1 >*/

float cell11_update2 (int dim, float s, float v, float *p, const float *g) ;
/*< nonsymplectic first-order: step 2 >*/

void cell_intersect (float a, float x, float dy, float p, 
		     float *sx, int *jx);
/*< intersecting a parabolic ray with cell boundaries >*/

bool cell_snap (float *z, int *iz, float eps);
/*< round to the nearest boundary >*/

float cell_update1 (int dim, float s, float v, float *p, const float *g) ;
/*< symplectic second-order: step 1 >*/


float cell_update2 (int dim, float s, float v, float *p, const float *g) ;
/*< symplectic second-order: step 2 >*/

float cell_p2a (float* p);
/*< convert ray parameter to angle >*/

#endif
