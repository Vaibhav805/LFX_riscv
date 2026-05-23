/*
 * BLAS Level 1 and Level 2 validation for OpenBLAS riscv64 builds.
 *
 * Each operation uses deterministic input, a plain C reference path, and JSON
 * output so scalar and RVV-linked binaries can be compared directly.
 */

#include <cblas.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEED 1337
#define THRESHOLD 1e-10

#ifdef __cplusplus
extern "C" {
#endif
extern char *openblas_get_config(void);
#ifdef __cplusplus
}
#endif

typedef struct {
    const char *name;
    const char *level;
    double rel_error;
    double max_error;
    int pass;
} Result;

static double rnd(unsigned int *seed) {
    return ((double)rand_r(seed) / (double)RAND_MAX) * 2.0 - 1.0;
}

static void fill(double *x, int n, unsigned int *seed) {
    for (int i = 0; i < n; i++) x[i] = rnd(seed);
}

static double rel_vec(const double *ref, const double *got, int n) {
    double num = 0.0, den = 0.0;
    for (int i = 0; i < n; i++) {
        double d = ref[i] - got[i];
        num += d * d;
        den += ref[i] * ref[i];
    }
    return den > 0.0 ? sqrt(num / den) : sqrt(num);
}

static double max_vec(const double *ref, const double *got, int n) {
    double m = 0.0;
    for (int i = 0; i < n; i++) {
        double d = fabs(ref[i] - got[i]);
        if (d > m) m = d;
    }
    return m;
}

static void record(Result *r, int *nr, const char *level, const char *name,
                   double rel, double max_err) {
    r[*nr].name = name;
    r[*nr].level = level;
    r[*nr].rel_error = rel;
    r[*nr].max_error = max_err;
    r[*nr].pass = rel <= THRESHOLD;
    (*nr)++;
}

static void test_daxpy(Result *r, int *nr, unsigned int *seed) {
    const int n = 4096;
    const double alpha = -1.75;
    double *x = malloc(n * sizeof(double));
    double *y_ref = malloc(n * sizeof(double));
    double *y = malloc(n * sizeof(double));
    fill(x, n, seed);
    fill(y_ref, n, seed);
    memcpy(y, y_ref, n * sizeof(double));
    for (int i = 0; i < n; i++) y_ref[i] = alpha * x[i] + y_ref[i];
    cblas_daxpy(n, alpha, x, 1, y, 1);
    record(r, nr, "L1", "DAXPY", rel_vec(y_ref, y, n), max_vec(y_ref, y, n));
    free(x); free(y_ref); free(y);
}

static void test_ddot(Result *r, int *nr, unsigned int *seed) {
    const int n = 4096;
    double *x = malloc(n * sizeof(double));
    double *y = malloc(n * sizeof(double));
    fill(x, n, seed);
    fill(y, n, seed);
    double ref = 0.0;
    for (int i = 0; i < n; i++) ref += x[i] * y[i];
    double got = cblas_ddot(n, x, 1, y, 1);
    double rel = fabs(ref - got) / (fabs(ref) > 0.0 ? fabs(ref) : 1.0);
    record(r, nr, "L1", "DDOT", rel, fabs(ref - got));
    free(x); free(y);
}

static void test_dnrm2(Result *r, int *nr, unsigned int *seed) {
    const int n = 4096;
    double *x = malloc(n * sizeof(double));
    fill(x, n, seed);
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += x[i] * x[i];
    double ref = sqrt(sum);
    double got = cblas_dnrm2(n, x, 1);
    double rel = fabs(ref - got) / (fabs(ref) > 0.0 ? fabs(ref) : 1.0);
    record(r, nr, "L1", "DNRM2", rel, fabs(ref - got));
    free(x);
}

static void test_dscal(Result *r, int *nr, unsigned int *seed) {
    const int n = 4096;
    const double alpha = 2.25;
    double *x_ref = malloc(n * sizeof(double));
    double *x = malloc(n * sizeof(double));
    fill(x_ref, n, seed);
    memcpy(x, x_ref, n * sizeof(double));
    for (int i = 0; i < n; i++) x_ref[i] *= alpha;
    cblas_dscal(n, alpha, x, 1);
    record(r, nr, "L1", "DSCAL", rel_vec(x_ref, x, n), max_vec(x_ref, x, n));
    free(x_ref); free(x);
}

static void test_dcopy(Result *r, int *nr, unsigned int *seed) {
    const int n = 4096;
    double *x = malloc(n * sizeof(double));
    double *y = calloc((size_t)n, sizeof(double));
    fill(x, n, seed);
    cblas_dcopy(n, x, 1, y, 1);
    record(r, nr, "L1", "DCOPY", rel_vec(x, y, n), max_vec(x, y, n));
    free(x); free(y);
}

static void test_dasum(Result *r, int *nr, unsigned int *seed) {
    const int n = 4096;
    double *x = malloc(n * sizeof(double));
    fill(x, n, seed);
    double ref = 0.0;
    for (int i = 0; i < n; i++) ref += fabs(x[i]);
    double got = cblas_dasum(n, x, 1);
    double rel = fabs(ref - got) / (fabs(ref) > 0.0 ? fabs(ref) : 1.0);
    record(r, nr, "L1", "DASUM", rel, fabs(ref - got));
    free(x);
}

static void test_idamax(Result *r, int *nr, unsigned int *seed) {
    const int n = 4096;
    double *x = malloc(n * sizeof(double));
    fill(x, n, seed);
    int ref = 0;
    for (int i = 1; i < n; i++) {
        if (fabs(x[i]) > fabs(x[ref])) ref = i;
    }
    int got = (int)cblas_idamax(n, x, 1);
    record(r, nr, "L1", "IDAMAX", ref == got ? 0.0 : 1.0, ref == got ? 0.0 : 1.0);
    free(x);
}

static void test_dgemv(Result *r, int *nr, unsigned int *seed) {
    const int m = 257, n = 193;
    const double alpha = 1.25, beta = -0.5;
    double *A = malloc((size_t)m * n * sizeof(double));
    double *x = malloc(n * sizeof(double));
    double *y_ref = malloc(m * sizeof(double));
    double *y = malloc(m * sizeof(double));
    fill(A, m * n, seed); fill(x, n, seed); fill(y_ref, m, seed);
    memcpy(y, y_ref, m * sizeof(double));
    for (int i = 0; i < m; i++) {
        double sum = 0.0;
        for (int j = 0; j < n; j++) sum += A[i * n + j] * x[j];
        y_ref[i] = alpha * sum + beta * y_ref[i];
    }
    cblas_dgemv(CblasRowMajor, CblasNoTrans, m, n, alpha, A, n, x, 1, beta, y, 1);
    record(r, nr, "L2", "DGEMV", rel_vec(y_ref, y, m), max_vec(y_ref, y, m));
    free(A); free(x); free(y_ref); free(y);
}

static void test_dsymv(Result *r, int *nr, unsigned int *seed) {
    const int n = 256;
    const double alpha = -0.75, beta = 0.25;
    double *A = calloc((size_t)n * n, sizeof(double));
    double *x = malloc(n * sizeof(double));
    double *y_ref = malloc(n * sizeof(double));
    double *y = malloc(n * sizeof(double));
    fill(x, n, seed); fill(y_ref, n, seed);
    memcpy(y, y_ref, n * sizeof(double));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            double v = rnd(seed);
            A[i * n + j] = v;
            A[j * n + i] = v;
        }
    }
    for (int i = 0; i < n; i++) {
        double sum = 0.0;
        for (int j = 0; j < n; j++) sum += A[i * n + j] * x[j];
        y_ref[i] = alpha * sum + beta * y_ref[i];
    }
    cblas_dsymv(CblasRowMajor, CblasUpper, n, alpha, A, n, x, 1, beta, y, 1);
    record(r, nr, "L2", "DSYMV", rel_vec(y_ref, y, n), max_vec(y_ref, y, n));
    free(A); free(x); free(y_ref); free(y);
}

static void test_dtrmv(Result *r, int *nr, unsigned int *seed) {
    const int n = 256;
    double *A = calloc((size_t)n * n, sizeof(double));
    double *x_ref = malloc(n * sizeof(double));
    double *x = malloc(n * sizeof(double));
    fill(x_ref, n, seed);
    memcpy(x, x_ref, n * sizeof(double));
    for (int i = 0; i < n; i++) {
        for (int j = i; j < n; j++) A[i * n + j] = rnd(seed);
    }
    double *orig = malloc(n * sizeof(double));
    memcpy(orig, x_ref, n * sizeof(double));
    for (int i = 0; i < n; i++) {
        double sum = 0.0;
        for (int j = i; j < n; j++) sum += A[i * n + j] * orig[j];
        x_ref[i] = sum;
    }
    cblas_dtrmv(CblasRowMajor, CblasUpper, CblasNoTrans, CblasNonUnit, n, A, n, x, 1);
    record(r, nr, "L2", "DTRMV", rel_vec(x_ref, x, n), max_vec(x_ref, x, n));
    free(A); free(x_ref); free(x); free(orig);
}

static void test_dger(Result *r, int *nr, unsigned int *seed) {
    const int m = 257, n = 193;
    const double alpha = 0.625;
    double *A_ref = malloc((size_t)m * n * sizeof(double));
    double *A = malloc((size_t)m * n * sizeof(double));
    double *x = malloc(m * sizeof(double));
    double *y = malloc(n * sizeof(double));
    fill(A_ref, m * n, seed); fill(x, m, seed); fill(y, n, seed);
    memcpy(A, A_ref, (size_t)m * n * sizeof(double));
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) A_ref[i * n + j] += alpha * x[i] * y[j];
    }
    cblas_dger(CblasRowMajor, m, n, alpha, x, 1, y, 1, A, n);
    record(r, nr, "L2", "DGER", rel_vec(A_ref, A, m * n), max_vec(A_ref, A, m * n));
    free(A_ref); free(A); free(x); free(y);
}

static void json_escape(FILE *f, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "blas_full_results.json";
    const char *cfg = openblas_get_config();
    if (!cfg) cfg = "OpenBLAS config unavailable";
    unsigned int seed = SEED;
    Result results[16];
    int nr = 0;

    test_daxpy(results, &nr, &seed);
    test_ddot(results, &nr, &seed);
    test_dnrm2(results, &nr, &seed);
    test_dscal(results, &nr, &seed);
    test_dcopy(results, &nr, &seed);
    test_dasum(results, &nr, &seed);
    test_idamax(results, &nr, &seed);
    test_dgemv(results, &nr, &seed);
    test_dsymv(results, &nr, &seed);
    test_dtrmv(results, &nr, &seed);
    test_dger(results, &nr, &seed);

    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen report");
        return 1;
    }

    int pass = 0, fail = 0;
    double worst = 0.0;
    const char *worst_name = "none";
    fprintf(f, "{\n  \"openblas_config\":\"");
    json_escape(f, cfg);
    fprintf(f, "\",\n  \"threshold\":%.17e,\n  \"results\":[\n", THRESHOLD);

    printf("BLAS L1/L2 validation: %d operations\n", nr);
    for (int i = 0; i < nr; i++) {
        if (results[i].pass) pass++; else fail++;
        if (results[i].rel_error > worst) {
            worst = results[i].rel_error;
            worst_name = results[i].name;
        }
        printf("  %-6s %-8s rel=%.4e max=%.4e [%s]\n",
               results[i].level, results[i].name, results[i].rel_error,
               results[i].max_error, results[i].pass ? "PASS" : "FAIL");
        fprintf(f,
                "    {\"level\":\"%s\",\"operation\":\"%s\","
                "\"relative_error\":%.17e,\"max_abs_error\":%.17e,"
                "\"status\":\"%s\"}%s\n",
                results[i].level, results[i].name, results[i].rel_error,
                results[i].max_error, results[i].pass ? "PASS" : "FAIL",
                i + 1 == nr ? "" : ",");
    }

    fprintf(f,
            "  ],\n  \"summary\":{\"pass\":%d,\"fail\":%d,"
            "\"worst_rel_error\":%.17e,\"worst_case\":\"%s\"}\n}\n",
            pass, fail, worst, worst_name);
    fclose(f);
    return fail == 0 ? 0 : 1;
}
