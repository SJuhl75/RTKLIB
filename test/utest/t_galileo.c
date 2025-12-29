#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../../src/galileo.h"


static void test__compute_sat_pos_from_az_el(void)
{

    struct test_case
    {
        double pos[3];
        double az;
        double el;
        double expected_sat_pos[3];
    } TEST_CASES[] = {
        {
            /* Zenith looking observation */
            .pos = {0.0, 0.0, 0.0},
            .az = 0.0,
            .el = PI / 2,
            .expected_sat_pos = {0.0, 0.0, 25000000.0 - 6378137.0},
        },
        {
            /* North-looking observation */
            .pos = {0.0, 0.0, 0.0},
            .az = 0.0,
            .el = 0.0,
            .expected_sat_pos = {1.312818851305248, 0.0, 25000000.0 - 6378137.0},
        },
        {
            /* South-looking observation */
            .pos = {0.0, 0.0, 0.0},
            .az = PI,
            .el = 0.0,
            .expected_sat_pos = {-1.312818851305248, 0.0, 25000000.0 - 6378137.0},
        },
        {
            /* East-looking observation */
            .pos = {0.0, 0.0, 0.0},
            .az = PI / 2.0,
            .el = 0.0,
            .expected_sat_pos = {0.0, 1.312818851305248, 25000000.0 - 6378137.0},
        },
        {
            /* West-looking observation */
            .pos = {0.0, 0.0, 0.0},
            .az = 3.0 * PI / 2.0,
            .el = 0.0,
            .expected_sat_pos = {0.0, -1.312818851305248, 25000000.0 - 6378137.0},
        },
    };

    int n_test_cases = sizeof(TEST_CASES) / sizeof(TEST_CASES[0]);

    for (int i = 0; i < n_test_cases; i++)
    {
        struct test_case *tc = &TEST_CASES[i];
        double sat_pos[3];
        compute_sat_pos_from_az_el(tc->pos, tc->az, tc->el, sat_pos);

        assert(sat_pos[0] - tc->expected_sat_pos[0] < 1e-6);
        assert(sat_pos[1] - tc->expected_sat_pos[1] < 1e-6);
        assert(sat_pos[2] - tc->expected_sat_pos[2] < 1e-6);
    }
}

static void test__galioncorr(void)
{

    static const char *INPUT_FILENAMES[] = {
        "/src/submodules/NeQuickJRC/test/benchmark/benchmarkHighExpanded",
        "/src/submodules/NeQuickJRC/test/benchmark/benchmarkLowExpanded",
        "/src/submodules/NeQuickJRC/test/benchmark/benchmarkMidExpanded",
    };
    int n_input_files = sizeof(INPUT_FILENAMES) / sizeof(INPUT_FILENAMES[0]);

    for (int i = 0; i < n_input_files; i++)
    {
        const char *input_filename = INPUT_FILENAMES[i];
        FILE *fp = fopen(input_filename, "r");
        assert(fp != NULL);

        /* Read first line to get the coefficients */
        double iono_gal_coeffs[3];
        int n_read = fscanf(fp, "%lf %lf %lf", &iono_gal_coeffs[0], &iono_gal_coeffs[1], &iono_gal_coeffs[2]);
        assert(n_read == 3);

        /* Process each line, until there are lines and test the results */
        while (1)
        {
            uint8_t month;
            double UTC;
            double pos[3], sat_pos[3], rs[3], rr[3], rho[3];
            double tec_expected_tecu;
            double azel[2];
            double delay_l1, var;

            int n_read = fscanf(fp, "%hhu %lf %lf %lf %lf %lf %lf %lf %lf",
                                &month, &UTC, &pos[1], &pos[0], &pos[2],
                                &sat_pos[1], &sat_pos[0], &sat_pos[2], &tec_expected_tecu);
            if (n_read != 9)
            {
                break;
            }

            /* create a time_t structure from month and UTC */
            double epoch[6] = {2021, month, 1, UTC, 0, 0};
            gtime_t gtime = epoch2time(epoch);

            /* compute azimuth and elevation */
            pos[0] *= D2R;
            pos[1] *= D2R;
            sat_pos[0] *= D2R;
            sat_pos[1] *= D2R;
            pos2ecef(pos, rr);
            pos2ecef(sat_pos, rs);
            (void)geodist(rs, rr, rho);
            satazel(pos, rho, azel);

            int res = galioncorr(gtime, iono_gal_coeffs, pos, azel, &delay_l1, &var);
            assert(res == 1);
            assert(delay_l1 - tec_expected_tecu * 40.3e16 / (1575.42e6 * 1575.42e6) < 0.1);
        }

        fclose(fp);
    }
}

int main(void)
{
    test__compute_sat_pos_from_az_el();
    test__galioncorr();

    return 0;
}