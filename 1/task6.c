#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_CONVERGENCE, ERROR_NO_ROOT
} Status;

// Накопляем отрицательное число, чтобы не брать модуль LLONG_MIN.
Status parse_integer(const char *text, const int base, long long *out) {
    if (text == NULL || out == NULL || base < 2 || base > 36) return ERROR_INVALID_ARG;
    int negative = 0;
    if (*text == '-' || *text == '+') {
        negative = *text == '-';
        text++;
    }
    if (*text == '\0') return ERROR_INVALID_ARG;
    long long value = 0;
    const long long limit = negative ? LLONG_MIN : -LLONG_MAX;
    while (*text != '\0') {
        int digit;
        if (*text >= '0' && *text <= '9') digit = *text - '0';
        else if (*text >= 'A' && *text <= 'Z') digit = *text - 'A' + 10;
        else return ERROR_INVALID_ARG;
        if (digit >= base) return ERROR_INVALID_ARG;
        if (value < (limit + digit) / base) return ERROR_OVERFLOW;
        value = value * base - digit;
        text++;
    }
    *out = negative ? value : -value;
    return OK;
}

Status report(const Status status) {
    switch (status) {
        case OK: break;
        case ERROR_INVALID_ARG: fprintf(stderr, "Error: invalid input or function argument\n"); break;
        case ERROR_OVERFLOW: fprintf(stderr, "Error: number out of range\n"); break;
        case ERROR_NO_CONVERGENCE: fprintf(stderr, "Error: requested accuracy was not reached\n"); break;
        case ERROR_NO_ROOT: fprintf(stderr, "Error: interval does not bracket a root\n"); break;
    }
    return status;
}

#define MAX_VERTICES 100

Status convex(const int count, const double eps, int *out, ...) {
    if (count < 3 || count > MAX_VERTICES || out == NULL || !isfinite(eps) || eps <= 0)
        return ERROR_INVALID_ARG;
    double x[MAX_VERTICES], y[MAX_VERTICES];
    va_list args;
    va_start(args, out);
    for (int i = 0; i < count; i++) {
        x[i] = va_arg(args, double);
        y[i] = va_arg(args, double);
    }
    va_end(args);
    for (int i = 0; i < count; i++) {
        if (!isfinite(x[i]) || !isfinite(y[i])) return ERROR_INVALID_ARG;
        for (int j = 0; j < i; j++) {
            if (fabs(x[i] - x[j]) <= eps && fabs(y[i] - y[j]) <= eps)
                return ERROR_INVALID_ARG;
        }
    }
    *out = 0;
    int direction = 0;
    // У выпуклого многоугольника все вершины лежат с одной стороны
    // каждого ребра. Эта проверка отсекает и самопересечения.
    for (int i = 0; i < count; i++) {
        int j = (i + 1) % count;
        for (int k = 0; k < count; k++) {
            double cross = (x[j] - x[i]) * (y[k] - y[i]) - (y[j] - y[i]) * (x[k] - x[i]);
            if (!isfinite(cross)) return ERROR_OVERFLOW;
            int sign = 0;
            if (cross > eps) sign = 1;
            else if (cross < -eps) sign = -1;
            if (sign != 0) {
                if (direction != 0 && sign != direction) return OK;
                direction = sign;
            }
        }
    }
    *out = direction != 0;
    return OK;
}

Status polynomial(const double x, const int degree, double *out, ...) {
    if (out == NULL || !isfinite(x) || degree < 0 || degree > 10000) return ERROR_INVALID_ARG;
    va_list args;
    va_start(args, out);
    double value = va_arg(args, double);
    if (!isfinite(value)) { va_end(args); return ERROR_INVALID_ARG; }
    // Схема Горнера: вместо степеней по очереди умножаем на x.
    for (int i = 0; i < degree; i++) {
        double coefficient = va_arg(args, double);
        if (!isfinite(coefficient)) { va_end(args); return ERROR_INVALID_ARG; }
        value = value * x + coefficient;
        if (!isfinite(value)) { va_end(args); return ERROR_OVERFLOW; }
    }
    va_end(args);
    *out = value;
    return OK;
}

Status kaprekar(const long long value, const int base, int *out) {
    if (out == NULL || value < 0 || base < 2 || base > 36) return ERROR_INVALID_ARG;
    *out = 0;
    if (value == 0) return OK;
    if (value > LLONG_MAX / value) return ERROR_OVERFLOW;
    long long square = value * value;
    // Перебираем все разрезы квадрата. Правая часть должна быть ненулевой.
    for (long long divider = base; divider <= square; ) {
        long long left = square / divider, right = square % divider;
        if (right != 0 && left + right == value) { *out = 1; return OK; }
        if (divider > LLONG_MAX / base) break;
        divider *= base;
    }
    if (value == 1) *out = 1;
    return OK;
}

Status find_kaprekar(const int base, int *results, const int count, ...) {
    if (results == NULL || count <= 0 || base < 2 || base > 36) return ERROR_INVALID_ARG;
    va_list args;
    va_start(args, count);
    Status st = OK;
    for (int i = 0; i < count; i++) {
        const char *word = va_arg(args, const char *);
        long long value;
        st = parse_integer(word, base, &value);
        if (st == OK && value < 0) st = ERROR_INVALID_ARG;
        if (st == OK) st = kaprekar(value, base, &results[i]);
        if (st != OK) break;
    }
    va_end(args);
    return st;
}

// count — последний обязательный параметр, как требует условие.
Status geometric_mean(const double eps, double *out, const int count, ...) {
    if (out == NULL || count <= 0 || !isfinite(eps) || eps <= 0) return ERROR_INVALID_ARG;
    va_list args;
    va_start(args, count);
    double product = 1;
    Status st = OK;
    for (int i = 0; i < count; i++) {
        double value = va_arg(args, double);
        if (!isfinite(value) || value <= eps) { st = ERROR_INVALID_ARG; break; }
        product *= value;
        if (!isfinite(product) || product < DBL_MIN) { st = ERROR_OVERFLOW; break; }
    }
    va_end(args);
    if (st == OK) *out = pow(product, 1.0 / count);
    return st;
}

Status power(const double base, const int exponent, const double eps, double *out) {
    if (out == NULL || !isfinite(base) || !isfinite(eps) || eps <= 0) return ERROR_INVALID_ARG;
    if (exponent == 0) { *out = 1; return OK; }
    if (exponent < 0 && fabs(base) <= eps) return ERROR_INVALID_ARG;
    double half;
    Status st = power(base, exponent / 2, eps, &half);
    if (st != OK) return st;
    double value = half * half;
    if (exponent % 2 != 0) value *= exponent > 0 ? base : 1 / base;
    if (!isfinite(value) || (fabs(base) > eps && fabs(value) < DBL_MIN)) return ERROR_OVERFLOW;
    *out = value;
    return OK;
}

Status equation1(const double x, const double eps, double *out) {
    if (out == NULL || !isfinite(x) || !isfinite(eps) || eps <= 0) return ERROR_INVALID_ARG;
    *out = x * x - 2;
    return isfinite(*out) ? OK : ERROR_OVERFLOW;
}

Status equation2(const double x, const double eps, double *out) {
    if (out == NULL || !isfinite(x) || !isfinite(eps) || eps <= 0) return ERROR_INVALID_ARG;
    *out = x * x * x - x - 2;
    return isfinite(*out) ? OK : ERROR_OVERFLOW;
}

Status equation3(const double x, const double eps, double *out) {
    if (out == NULL || !isfinite(x) || !isfinite(eps) || eps <= 0) return ERROR_INVALID_ARG;
    *out = cos(x) - x;
    return OK;
}

Status dichotomy(double left, double right, const double eps,
                 Status (*function)(const double, const double, double *), double *out) {
    if (out == NULL || function == NULL || !isfinite(left) || !isfinite(right) ||
        !isfinite(eps) || eps <= 0 || right - left <= eps ||
        !isfinite(right - left)) return ERROR_INVALID_ARG;
    double first, last;
    Status st = function(left, eps, &first);
    if (st == OK) st = function(right, eps, &last);
    if (st != OK) return st;
    // На границах проверяем точный ноль: малое f(x) ещё не означает малую ошибку x.
    if (fpclassify(first) == FP_ZERO) { *out = left; return OK; }
    if (fpclassify(last) == FP_ZERO) { *out = right; return OK; }
    if (signbit(first) == signbit(last)) return ERROR_NO_ROOT;
    for (int i = 0; i < 2000; i++) {
        double mid = left / 2 + right / 2;
        if (right - left <= eps) { *out = mid; return OK; }
        if (fpclassify(mid - left) == FP_ZERO || fpclassify(right - mid) == FP_ZERO)
            return ERROR_NO_CONVERGENCE;
        double value;
        st = function(mid, eps, &value);
        if (st != OK) return st;
        if (fpclassify(value) == FP_ZERO) { *out = mid; return OK; }
        if (signbit(first) != signbit(value)) right = mid;
        else { left = mid; first = value; }
    }
    return ERROR_NO_CONVERGENCE;
}

int main(void) {
    const double eps = 1e-9;
    int result;
    double value;
    Status st = convex(4, eps, &result, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0);
    if (st != OK) return report(st);
    printf("square is convex: %d\n", result);
    st = convex(4, eps, &result, 0.0, 0.0, 2.0, 0.0, 0.5, 0.5, 0.0, 2.0);
    if (st != OK) return report(st);
    printf("concave polygon is convex: %d\n", result);
    st = polynomial(3.0, 3, &value, 2.0, -6.0, 2.0, -1.0);
    if (st != OK) return report(st);
    printf("polynomial at 3: %g\n", value);
    int flags[7];
    const char *numbers[7] = {"9", "45", "55", "99", "297", "703", "100"};
    st = find_kaprekar(10, flags, 7, "9", "45", "55", "99", "297", "703", "100");
    if (st != OK) return report(st);
    for (int i = 0; i < 7; i++) if (flags[i]) printf("Kaprekar: %s\n", numbers[i]);
    st = geometric_mean(eps, &value, 4, 1.0, 2.0, 4.0, 8.0);
    if (st != OK) return report(st);
    printf("geometric mean: %.12g\n", value);
    st = power(2, 10, eps, &value);
    if (st != OK) return report(st);
    printf("2^10: %g\n", value);
    st = power(3, -2, eps, &value);
    if (st != OK) return report(st);
    printf("3^-2: %.12g\n", value);
    // Передаём имя функции без скобок — её адрес, а не результат вызова.
    st = dichotomy(1, 2, 1e-6, equation1, &value);
    if (st != OK) return report(st);
    printf("sqrt(2), eps=1e-6: %.12g\n", value);
    st = dichotomy(1, 2, eps, equation2, &value);
    if (st != OK) return report(st);
    printf("x^3-x-2 root: %.12g\n", value);
    st = dichotomy(0, 1, eps, equation3, &value);
    if (st != OK) return report(st);
    printf("cos(x)-x root: %.12g\n", value);
    st = dichotomy(3, 4, eps, equation1, &value);
    if (st != ERROR_NO_ROOT) return report(st == OK ? ERROR_INVALID_ARG : st);
    puts("[3,4]: no sign change detected");
    return OK;
}
