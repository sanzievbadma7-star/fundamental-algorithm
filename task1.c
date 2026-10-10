#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW
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
    }
    return status;
}

Status single_flag(const char *text, char *mode) {
    if (text == NULL || mode == NULL || strlen(text) != 2 ||
        (text[0] != '-' && text[0] != '/')) return ERROR_INVALID_ARG;
    *mode = text[1];
    return OK;
}

Status format_integer(long long value, const int base, char *out, const int capacity) {
    if (out == NULL || base < 2 || base > 36 || capacity < 66) return ERROR_INVALID_ARG;
    char reverse[65];
    int count = 0, pos = 0;
    const int negative = value < 0;
    // Остаток по модулю основания мал даже для LLONG_MIN.
    do {
        int digit = value % base;
        if (digit < 0) digit = -digit;
        reverse[count++] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        value /= base;
    } while (value != 0);
    if (negative) out[pos++] = '-';
    while (count > 0) out[pos++] = reverse[--count];
    out[pos] = '\0';
    return OK;
}

Status multiples(const long long x, int *values, int *count) {
    if (x == 0 || values == NULL || count == NULL) return ERROR_INVALID_ARG;
    *count = 0;
    for (int i = 1; i <= 100; i++) if (i % x == 0) values[(*count)++] = i;
    return OK;
}

Status prime(const long long x, int *kind) {
    if (x <= 0 || kind == NULL) return ERROR_INVALID_ARG;
    *kind = x == 1 ? 2 : 0;
    // i <= x/i равносильно i*i <= x, но без переполнения.
    for (long long i = 2; i <= x / i; i++) {
        if (x % i == 0) { *kind = 1; break; }
    }
    return OK;
}

Status sum_to(const long long x, long long *out) {
    if (x < 0 || out == NULL) return ERROR_INVALID_ARG;
    if (x == LLONG_MAX) return ERROR_OVERFLOW;
    long long a = x, b = x + 1;
    if (a % 2 == 0) a /= 2;
    else b /= 2;
    if (a != 0 && b > LLONG_MAX / a) return ERROR_OVERFLOW;
    *out = a * b;
    return OK;
}

Status factorial(const long long x, long long *out) {
    if (x < 0 || out == NULL) return ERROR_INVALID_ARG;
    if (x > 20) return ERROR_OVERFLOW;
    *out = 1;
    for (int i = 2; i <= x; i++) *out *= i;
    return OK;
}

Status powers(const int x, long long table[10][10]) {
    if (x < 1 || x > 10 || table == NULL) return ERROR_INVALID_ARG;
    for (int base = 1; base <= 10; base++) {
        long long value = 1;
        for (int exponent = 0; exponent < x; exponent++) {
            value *= base;
            table[exponent][base - 1] = value;
        }
    }
    return OK;
}

int main(int argc, char *argv[]) {
    if (argc != 3) return report(ERROR_INVALID_ARG);
    long long x;
    char mode;
    Status st = parse_integer(argv[1], 10, &x);
    if (st == OK) st = single_flag(argv[2], &mode);
    if (st != OK) return report(st);
    switch (mode) {
        case 'h': {
            int values[100], count;
            st = multiples(x, values, &count);
            if (st == OK) {
                if (count == 0) puts("no multiples up to 100");
                for (int i = 0; i < count; i++) printf("%d%c", values[i], i + 1 == count ? '\n' : ' ');
            }
            break;
        }
        case 'p': {
            int kind;
            st = prime(x, &kind);
            if (st == OK) puts(kind == 0 ? "prime" : kind == 1 ? "composite" : "neither prime nor composite");
            break;
        }
        case 's': {
            char text[66];
            st = format_integer(x, 16, text, 66);
            if (st == OK) {
                int start = text[0] == '-';
                for (int i = start; text[i]; i++) printf("%c%c", text[i], text[i + 1] ? ' ' : '\n');
            }
            break;
        }
        case 'e': {
            long long table[10][10];
            if (x < 1 || x > 10) st = ERROR_INVALID_ARG;
            else st = powers(x, table);
            if (st == OK) for (int i = 0; i < x; i++) {
                for (int j = 0; j < 10; j++) printf("%lld%c", table[i][j], j == 9 ? '\n' : ' ');
            }
            break;
        }
        case 'a':
        case 'f': {
            long long value;
            st = mode == 'a' ? sum_to(x, &value) : factorial(x, &value);
            if (st == OK) printf("%lld\n", value);
            break;
        }
        default: st = ERROR_INVALID_ARG;
    }
    return report(st);
}
