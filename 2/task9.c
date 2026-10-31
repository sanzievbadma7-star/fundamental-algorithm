#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <stdint.h>

#define MAX_DENOMINATOR 10000000000

typedef enum { OK, ERROR_INVALID_ARG, ERROR_PRECISION, ERROR_NO_MEMORY, ERROR_FILE } Status;

Status report(const Status st) {
    if (st == ERROR_INVALID_ARG) fprintf(stderr, "Error: invalid argument\n");
    else if (st == ERROR_PRECISION) fprintf(stderr, "Error: no fraction found within eps and denominator limit\n");
    else if (st == ERROR_NO_MEMORY) fprintf(stderr, "Error: cannot allocate memory\n");
    else if (st == ERROR_FILE) fprintf(stderr, "Error: output failure\n");
    return st;
}

Status recover_fraction(const double value, const double eps, int *numerator, int *denominator) {
    if (numerator == NULL || denominator == NULL || !isfinite(value) ||
        !isfinite(eps) || eps <= 0 || eps >= 0.5 || value <= 0 || value >= 1)
        return ERROR_INVALID_ARG;
    for (int q = 2; q <= MAX_DENOMINATOR; q++) {
        const int p = (int)round(value * q);
        if (p > 0 && p < q && fabs(value - (double)p / q) <= eps) {
            *numerator = p;
            *denominator = q;
            return OK;
        }
    }
    return ERROR_PRECISION;
}

Status gcd(int a, int b, int *out) {
    if (a <= 0 || b <= 0 || out == NULL) return ERROR_INVALID_ARG;
    while (b != 0) {
        const int remainder = a % b;
        a = b;
        b = remainder;
    }
    *out = a;
    return OK;
}

Status finite_fraction(const double value, const int base, const double eps, int *out) {
    if (out == NULL || base < 2 || base > 36) return ERROR_INVALID_ARG;
    int numerator, denominator;
    Status st = recover_fraction(value, eps, &numerator, &denominator);
    if (st != OK) return st;
    int divisor;
    st = gcd(numerator, denominator, &divisor);
    if (st != OK) return st;
    denominator /= divisor;
    while (denominator != 1) {
        st = gcd(denominator, base, &divisor);
        if (st != OK) return st;
        if (divisor == 1) break;
        denominator /= divisor;
    }
    *out = denominator == 1;
    return OK;
}

Status check_fractions(const int base, const double eps, const int count, int **out, ...) {
    if (out == NULL || count <= 0 || base < 2 || base > 36 ||
        !isfinite(eps) || eps <= 0 || eps >= 0.5) return ERROR_INVALID_ARG;
    if ((size_t)count > SIZE_MAX / sizeof(int)) return ERROR_INVALID_ARG;
    int *results = malloc((size_t)count * sizeof(int));
    if (results == NULL) return ERROR_NO_MEMORY;
    va_list args;
    va_start(args, out);
    Status st = OK;
    for (int i = 0; i < count && st == OK; i++)
        st = finite_fraction(va_arg(args, double), base, eps, &results[i]);
    va_end(args);
    if (st != OK) { free(results); return st; }
    *out = results;
    return OK;
}

int main(int argc, char *argv[]) {
    if (argc != 1) return report(ERROR_INVALID_ARG);
    (void)argv;
    const double values[] = {0.5, 0.1, 0.125, 0.2};
    int *results = NULL;
    Status st = check_fractions(2, 1e-7, 4, &results, 0.5, 0.1, 0.125, 0.2);
    if (st == OK) for (int i = 0; i < 4; i++)
        printf("%.3f in base 2: %s\n", values[i], results[i] ? "finite" : "infinite");
    free(results);
    const int bases[] = {3, 9, 5};
    for (int i = 0; i < 3 && st == OK; i++) {
        int finite;
        st = finite_fraction(0.33333333333, bases[i], 1e-11, &finite);
        if (st == OK) printf("0.33333333333 in base %d: %s\n", bases[i], finite ? "finite" : "infinite");
    }
    if (st == OK && (fflush(stdout) == EOF || ferror(stdout))) st = ERROR_FILE;
    return report(st);
}
