// math_helper.h: the sin table everything uses

#ifndef MATH_HELPER_H
#define MATH_HELPER_H

void  MathHelper_init(void);
float MathHelper_sin(float x);
float MathHelper_cos(float x);
float MathHelper_sqrt(float x);
// an (int) cast of a float, NaN is 0 and out of range clamps
int   MathHelper_floatToInt(float x);

#endif
