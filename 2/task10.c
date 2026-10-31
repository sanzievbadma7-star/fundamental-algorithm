#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>

typedef enum { OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE } Status;

Status report(const Status st) {
    if (st == ERROR_INVALID_ARG) fprintf(stderr, "Error: invalid argument\n");
    else if (st == ERROR_OVERFLOW) fprintf(stderr, "Error: coefficient out of range\n");
    else if (st == ERROR_NO_MEMORY) fprintf(stderr, "Error: cannot allocate memory\n");
    else if (st == ERROR_FILE) fprintf(stderr, "Error: output failure\n");
    return st;
}

Status shift_polynomial(const double eps, const double a, double **out, const int degree, ...) {
    if (out == NULL || degree < 0 || !isfinite(eps) || eps <= 0 || !isfinite(a))
        return ERROR_INVALID_ARG;
    if (degree == INT_MAX || (size_t)(degree + 1) > SIZE_MAX / sizeof(double))
        return ERROR_OVERFLOW;
    double *coefficients = malloc((size_t)(degree + 1) * sizeof(double));
    if (coefficients == NULL) return ERROR_NO_MEMORY;
    va_list args;
    va_start(args, degree);
    Status st = OK;
    for (int i = 0; i <= degree; i++) {
        coefficients[i] = va_arg(args, double);
        if (!isfinite(coefficients[i])) st = ERROR_INVALID_ARG;
    }
    va_end(args);
    if (st != OK) { free(coefficients); return st; }
    for (int start = 0; start < degree && st == OK; start++) {
        for (int j = degree - 1; j >= start; j--) {
            coefficients[j] += a * coefficients[j + 1];
            if (!isfinite(coefficients[j])) { st = ERROR_OVERFLOW; break; }
        }
    }
    if (st != OK) { free(coefficients); return st; }
    for (int i = 0; i <= degree; i++)
        if (fabs(coefficients[i]) <= eps) coefficients[i] = 0;
    *out = coefficients;
    return OK;
}

int main(int argc, char *argv[]) {
    if (argc != 1) return report(ERROR_INVALID_ARG);
    (void)argv;

    double *r1 = NULL;
    Status st1 = shift_polynomial(1e-9, 1.0, &r1, 3, 1.0, 2.0, 3.0, 4.0);
    if (st1 == OK)
        for (int i = 0; i <= 3; i++) printf("g[%d] = %.10g\n", i, r1[i]);
    free(r1);

    double *r2 = NULL;
    Status st2 = shift_polynomial(1e-9, 3.0, &r2, 0, 5.0);
    if (st2 == OK)
        printf("g[0] = %.10g\n", r2[0]);
    free(r2);

    double *r3 = NULL;
    Status st3 = shift_polynomial(1e-9, -1.0, &r3, 2, 0.0, 0.0, 1.0);
    if (st3 == OK)
        for (int i = 0; i <= 2; i++) printf("g[%d] = %.10g\n", i, r3[i]);
    free(r3);

    double *r4 = NULL;
    Status st4 = shift_polynomial(1e-9, 1.0, &r4, 2, 1.0, -2.0, 1.0);
    if (st4 == OK)
        for (int i = 0; i <= 2; i++) printf("g[%d] = %.10g\n", i, r4[i]);
    free(r4);

    double *r5 = NULL;
    Status st5a = shift_polynomial(0.0, 1.0, &r5, 1, 1.0, 2.0);
    Status st5b = shift_polynomial(1e-9, 1e10, &r5, 1, 1e300, 1e300);
    free(r5);

    return fflush(stdout) == EOF || ferror(stdout) ? report(ERROR_FILE) : OK;
}
