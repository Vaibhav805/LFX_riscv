/*
 * Hilbert matrix condition stress for DGEMM.
 *
 * Builds H(n), computes C = H * H^T through OpenBLAS, and compares against a
 * long-double scalar reference. Hilbert matrices expose rounding sensitivity
 * without requiring a linear solve or matrix inversion.
 */

#include <cblas.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define THRESHOLD 1e-12

#ifdef __cplusplus
extern "C" {
#endif
extern char *openblas_get_config(void);
#ifdef __cplusplus
}
#endif

static const int SIZES[] = {4, 6, 8, 10, 12};
static const double COND_EST[] = {1.6e4, 1.5e7, 1.5e10, 1.6e13, 1.6e16};

static double rel_error(const long double *ref, const double *got, int n) {
    long double num = 0.0L;
    long double den = 0.0L;
    for (int i = 0; i < n; i++) {
        long double d = ref[i] - (long double)got[i];
        num += d * d;
        den += ref[i] * ref[i];
    }
    return (double)(den > 0.0L ? sqrtl(num / den) : sqrtl(num));
}

static double max_error(const long double *ref, const double *got, int n) {
    long double m = 0.0L;
    for (int i = 0; i < n; i++) {
        long double d = fabsl(ref[i] - (long double)got[i]);
        if (d > m) m = d;
    }
    return (double)m;
}

static void json_escape(FILE *f, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "condition_stress_results.json";
    const char *cfg = openblas_get_config();
    if (!cfg) cfg = "OpenBLAS config unavailable";

    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen report");
        return 1;
    }

    fprintf(f, "{\n  \"openblas_config\":\"");
    json_escape(f, cfg);
    fprintf(f, "\",\n  \"threshold\":%.17e,\n  \"results\":[\n", THRESHOLD);

    int pass = 0;
    int fail = 0;
    double worst = 0.0;
    int worst_n = 0;
    int cases = (int)(sizeof(SIZES) / sizeof(SIZES[0]));

    printf("Hilbert DGEMM condition stress\n");
    for (int c = 0; c < cases; c++) {
        int n = SIZES[c];
        double *H = malloc((size_t)n * n * sizeof(double));
        double *C = calloc((size_t)n * n, sizeof(double));
        long double *R = calloc((size_t)n * n, sizeof(long double));
        if (!H || !C || !R) {
            fprintf(stderr, "OOM for n=%d\n", n);
            fclose(f);
            return 1;
        }

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) H[i * n + j] = 1.0 / (double)(i + j + 1);
        }

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                long double sum = 0.0L;
                for (int k = 0; k < n; k++) {
                    sum += (long double)H[i * n + k] * (long double)H[j * n + k];
                }
                R[i * n + j] = sum;
            }
        }

        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans, n, n, n,
                    1.0, H, n, H, n, 0.0, C, n);

        double rel = rel_error(R, C, n * n);
        double max_abs = max_error(R, C, n * n);
        int ok = rel <= THRESHOLD;
        if (ok) pass++; else fail++;
        if (rel > worst) {
            worst = rel;
            worst_n = n;
        }

        printf("  H(%2d) cond~%.1e rel=%.4e max=%.4e [%s]\n",
               n, COND_EST[c], rel, max_abs, ok ? "PASS" : "FAIL");

        fprintf(f,
                "    {\"case\":\"hilbert_%d\",\"n\":%d,"
                "\"condition_estimate\":%.17e,\"relative_error\":%.17e,"
                "\"max_abs_error\":%.17e,\"status\":\"%s\"}%s\n",
                n, n, COND_EST[c], rel, max_abs, ok ? "PASS" : "FAIL",
                c + 1 == cases ? "" : ",");

        free(H);
        free(C);
        free(R);
    }

    fprintf(f,
            "  ],\n  \"summary\":{\"pass\":%d,\"fail\":%d,"
            "\"worst_rel_error\":%.17e,\"worst_case\":\"hilbert_%d\"}\n}\n",
            pass, fail, worst, worst_n);
    fclose(f);
    return fail == 0 ? 0 : 1;
}
