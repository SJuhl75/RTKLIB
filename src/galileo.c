#include "rtklib.h"

#include "NeQuickG_JRC.h"


extern void compute_sat_pos_from_az_el(const double pos[3], double az, double el, double sat_pos[3]) {

    static const double EARTH_RADIUS = 6378137.0;
    static const double SAT_RADIUS = 25000000.0; // Assume a large satellite height for now, you may need to adjust or calculate it based on other info.

    double lat = pos[0];
    double lon = pos[1];
    double h = pos[2];

    double cos_el = cos(el);
    double sin_el = sin(el);
    double cos_az = cos(az);
    double sin_az = sin(az);
    double cos_lat = cos(lat);
    double sin_lat = sin(lat);
    double cos_lon = cos(lon);
    double sin_lon = sin(lon);

    // Calculate ECEF coordinates of the observer
    double x_obs = (EARTH_RADIUS + h) * cos_lat * cos_lon;
    double y_obs = (EARTH_RADIUS + h) * cos_lat * sin_lon;
    double z_obs = (EARTH_RADIUS + h) * sin_lat;

    // Calculate rotation matrix from local tangent plane to ECEF
    double rot_matrix[3][3] = {
        {-sin_lon, -sin_lat * cos_lon, cos_lat * cos_lon},
        {cos_lon, -sin_lat * sin_lon, cos_lat * sin_lon},
        {0, cos_lat, sin_lat}
    };

    // Calculate vector from observer to satellite in local tangent plane
    double B = PI/2 + el;
    double h_sta = EARTH_RADIUS + pos[2];
    double range = h_sta*cos(B) + sqrt(SAT_RADIUS*SAT_RADIUS - h_sta*h_sta*sin(B)*sin(B));
    double local_vector[3] = {
        range * cos_el * sin_az,
        range * cos_el * cos_az,
        range * sin_el
    };

    // Rotate local vector to ECEF
    double ecef_vector[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            ecef_vector[i] += rot_matrix[i][j] * local_vector[j];
        }
    }

    // Calculate ECEF coordinates of the satellite
    double x_sat = x_obs + ecef_vector[0];
    double y_sat = y_obs + ecef_vector[1];
    double z_sat = z_obs + ecef_vector[2];

    // Calculate geodetic coordinates of the satellite
    double p = sqrt(x_sat * x_sat + y_sat * y_sat);
    double sat_lon = atan2(y_sat, x_sat);
    double sat_lat = atan2(z_sat, p); // Approximation, for more accuracy use iterative method.
    double sat_h = sqrt(x_sat * x_sat + y_sat * y_sat + z_sat * z_sat) - EARTH_RADIUS; //Another approximation.

    sat_pos[0] = sat_lat;
    sat_pos[1] = sat_lon;
    sat_pos[2] = sat_h;
}

/** \brief Compute Total Electron Content for NeQuick model
 *
 * \return 0 upon error, 1 if OK (following RTKLIB convention)
 */
extern int galioncorr(gtime_t time, const double iono_gal_coeffs[], const double *pos,
                      const double *azel, double *delay_l1, double *var) {

    double tec_tecu;
    double epoch[6];
    uint8_t month;
    double UTC;
    double sat_pos[3];  // latitude (rad), longitude (rad), height (m)
    double std_iono;
    int res = 0;
    static NeQuickG_handle nequick_handle = NEQUICKG_INVALID_HANDLE;
    static const double L1_FREQ_HZ = 1575.42e6;
    static const double ALPHA_ION = 40.3e16 / (L1_FREQ_HZ * L1_FREQ_HZ);  // m/TECU

    if (nequick_handle == NULL) {
        /* first time setup of the NeQuick handle */
        int init_ret = NeQuickG.init(NULL, NULL, &nequick_handle);  // Compiled with FTR_MODIP_CCIR_AS_CONSTANTS=1
        if (init_ret != NEQUICK_OK) {
            goto exit;
        }
    }

    time2epoch(time, epoch);
    month = (uint8_t)epoch[1];
    UTC = epoch[3] + epoch[4] / 60.0 + epoch[5] / 3600.0;

    compute_sat_pos_from_az_el(pos, azel[0], azel[1], sat_pos);

    if (NeQuickG.set_solar_activity_coefficients(nequick_handle, iono_gal_coeffs, 3) != NEQUICK_OK) {
        goto exit;
    }
    if (NeQuickG.set_time(nequick_handle, month, UTC) != NEQUICK_OK) {
        goto exit;
    }
    if (NeQuickG.set_receiver_position(nequick_handle, pos[1] * R2D, pos[0] * R2D, pos[2]) != NEQUICK_OK) {
        goto exit;
    }
    if (NeQuickG.set_satellite_position(nequick_handle, sat_pos[1] * R2D, sat_pos[0] * R2D, sat_pos[2]) != NEQUICK_OK) {
        goto exit;
    }
    if (NeQuickG.get_total_electron_content(nequick_handle, &tec_tecu) != NEQUICK_OK) {
        goto exit;
    }

    *delay_l1 = tec_tecu * ALPHA_ION;
    std_iono = 0.5 * (*delay_l1);  // Assume 50% std deviation of the ionospheric delay
    *var= std_iono * std_iono;

    trace(5,"galioncorr: pos=%.3f %.3f azel=%.3f %.3f coeffs=%.3e %.3e %.3e stec=%.3f delay_l1=%7.2f std=%7.2f\n",
        pos[0]*R2D,pos[1]*R2D,
        azel[0]*R2D,azel[1]*R2D,
        iono_gal_coeffs[0], iono_gal_coeffs[1], iono_gal_coeffs[2],
        tec_tecu, *delay_l1, std_iono);

    res = 1;
exit:
    return res;
}
