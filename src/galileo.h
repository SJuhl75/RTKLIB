#ifndef GALILEO_H
#define GALILEO_H

#include "rtklib.h"

/*
Function to compute satellite position from observer position, azimuth, and elevation

pos: [latitude (rad), longitude (rad), height (m)]
az: azimuth (rad)
el: elevation (rad)
sat_pos: [latitude (rad), longitude (rad), height (m)]
*/
EXPORT extern void compute_sat_pos_from_az_el(const double pos[3], double az, double el, double sat_pos[3]);

#endif // GALILEO_H