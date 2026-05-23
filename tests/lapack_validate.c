/*
 * Minimal LAPACKE validation for riscv64 LAPACK builds.
 * Checks dgesv and dgels against small systems with known solutions.
 */

#include <lapacke.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double max_abs(const double *x, const double *ref, int n) {
    double m = 0.0;
    for (int i = 0; i < n; i++) {
        double d = fabs(x[i] - ref[i]);
        if (d > m) m = d;
    }
    return m;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "lapack_validation_results.json";
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen report");
        return 1;
    }

    double A1[9] = {
        3.0, 1.0, 2.0,
        2.0, 6.0, 1.0,
        1.0, 0.0, 4.0,
    };
    double b1[3] = {11.0, 17.0, 13.0};
    double x1_ref[3] = {1.0, 2.0, 3.0};
    int ipiv[3] = {0, 0, 0};
    int info_dgesv = LAPACKE_dgesv(LAPACK_ROW_MAJOR, 3, 1, A1, 3, ipiv, b1, 1);
    double err_dgesv = max_abs(b1, x1_ref, 3);

    double A2[6] = {
        1.0, 1.0,
        1.0, 2.0,
        1.0, 3.0,
    };
    double b2[3] = {3.0, 5.0, 7.0};
    double x2_ref[2] = {1.0, 2.0};
    int info_dgels = LAPACKE_dgels(LAPACK_ROW_MAJOR, 'N', 3, 2, 1, A2, 2, b2, 1);
    double err_dgels = max_abs(b2, x2_ref, 2);

    int pass_dgesv = info_dgesv == 0 && err_dgesv < 1e-12;
    int pass_dgels = info_dgels == 0 && err_dgels < 1e-12;

    fprintf(f,
            "{\n"
            "  \"results\": [\n"
            "    {\"routine\":\"dgesv\",\"info\":%d,\"max_abs_error\":%.17e,\"status\":\"%s\"},\n"
            "    {\"routine\":\"dgels\",\"info\":%d,\"max_abs_error\":%.17e,\"status\":\"%s\"}\n"
            "  ],\n"
            "  \"summary\":{\"pass\":%d,\"fail\":%d}\n"
            "}\n",
            info_dgesv, err_dgesv, pass_dgesv ? "PASS" : "FAIL",
            info_dgels, err_dgels, pass_dgels ? "PASS" : "FAIL",
            pass_dgesv + pass_dgels, 2 - pass_dgesv - pass_dgels);
    fclose(f);

    printf("dgesv info=%d err=%.4e [%s]\n", info_dgesv, err_dgesv, pass_dgesv ? "PASS" : "FAIL");
    printf("dgels info=%d err=%.4e [%s]\n", info_dgels, err_dgels, pass_dgels ? "PASS" : "FAIL");
    return pass_dgesv && pass_dgels ? 0 : 1;
}
