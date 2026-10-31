#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <wchar.h>

typedef enum { OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE } Status;
typedef enum { NONE, HH, H, L, LL, J, Z, T, BIG_L } Length;
typedef struct { FILE *file; char *text; int count; } Output;

Status report(const Status st) {
    if (st != OK) fprintf(stderr, "Error: formatting failed (%d)\n", st);
    return st;
}

Status emit(Output *out, const char *text, const int length) {
    if (out == NULL || text == NULL || length < 0 || (out->file == NULL && out->text == NULL))
        return ERROR_INVALID_ARG;
    if (length > INT_MAX - out->count) return ERROR_OVERFLOW;
    if (out->file != NULL) {
        if (fwrite(text, 1, length, out->file) != (size_t)length) return ERROR_FILE;
    } else {
        memcpy(out->text + out->count, text, length);
        out->text[out->count + length] = '\0';
    }
    out->count += length;
    return OK;
}

Status roman(const int value, char *out) {
    if (out == NULL || value < 1 || value > 3999) return ERROR_INVALID_ARG;
    const int numbers[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    const char *letters[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    int remaining = value, pos = 0;
    for (int i = 0; i < 13; i++) while (remaining >= numbers[i]) {
        for (int j = 0; letters[i][j]; j++) out[pos++] = letters[i][j];
        remaining -= numbers[i];
    }
    out[pos] = '\0';
    return OK;
}

Status zeckendorf(unsigned int value, char *out) {
    if (out == NULL) return ERROR_INVALID_ARG;
    if (value == 0) { strcpy(out, "01"); return OK; }
    unsigned int fibonacci[sizeof(unsigned int) * CHAR_BIT * 2];
    int count = 2;
    fibonacci[0] = 1; fibonacci[1] = 2;
    while (fibonacci[count - 1] <= UINT_MAX - fibonacci[count - 2]) {
        const unsigned int next = fibonacci[count - 1] + fibonacci[count - 2];
        if (next > value) break;
        fibonacci[count++] = next;
    }
    while (count > 1 && fibonacci[count - 1] > value) count--;
    for (int i = 0; i < count; i++) out[i] = '0';
    for (int i = count - 1; i >= 0; i--) if (fibonacci[i] <= value) {
        out[i] = '1'; value -= fibonacci[i];
    }
    out[count] = '1'; out[count + 1] = '\0';
    return OK;
}

Status integer_text(int value, int base, const int upper, char *out) {
    if (out == NULL || (upper != 0 && upper != 1)) return ERROR_INVALID_ARG;
    if (base < 2 || base > 36) base = 10;
    char reverse[sizeof(int) * CHAR_BIT + 1];
    int used = 0, pos = 0;
    const int negative = value < 0;
    do {
        int digit = value % base;
        if (digit < 0) digit = -digit;
        reverse[used++] = digit < 10 ? '0' + digit : (upper ? 'A' : 'a') + digit - 10;
        value /= base;
    } while (value != 0);
    if (negative) out[pos++] = '-';
    while (used > 0) out[pos++] = reverse[--used];
    out[pos] = '\0';
    return OK;
}

Status parse_base(const char *text, int base, const int upper, int *out) {
    if (text == NULL || out == NULL) return ERROR_INVALID_ARG;
    if (base < 2 || base > 36) base = 10;
    int negative = 0;
    if (*text == '+' || *text == '-') { negative = *text == '-'; text++; }
    if (*text == '\0') return ERROR_INVALID_ARG;
    const int limit = negative ? INT_MIN : -INT_MAX;
    int value = 0;
    while (*text) {
        int digit = -1;
        if (*text >= '0' && *text <= '9') digit = *text - '0';
        else if (upper && *text >= 'A' && *text <= 'Z') digit = *text - 'A' + 10;
        else if (!upper && *text >= 'a' && *text <= 'z') digit = *text - 'a' + 10;
        if (digit < 0 || digit >= base) return ERROR_INVALID_ARG;
        if (value < (limit + digit) / base) return ERROR_OVERFLOW;
        value = value * base - digit;
        text++;
    }
    *out = negative ? value : -value;
    return OK;
}

Status memory_text(const void *value, const int size, char *out) {
    if (value == NULL || size <= 0 || out == NULL) return ERROR_INVALID_ARG;
    const unsigned char *bytes = value;
    int pos = 0;
    for (int i = 0; i < size; i++) {
        if (i > 0) out[pos++] = ' ';
        for (int bit = CHAR_BIT - 1; bit >= 0; bit--) out[pos++] = '0' + ((bytes[i] >> bit) & 1);
    }
    out[pos] = '\0';
    return OK;
}

Status store_count(const Length length, va_list *args, const int count) {
    // Разные типы указателей обязательны по правилам printf.
#define STORE(type) do { type *p = va_arg(*args, type *); if (p == NULL) return ERROR_INVALID_ARG; *p = (type)count; } while (0)
    switch (length) {
        case NONE: STORE(int); break;
        case HH: if (count > SCHAR_MAX) return ERROR_OVERFLOW; STORE(signed char); break;
        case H: if (count > SHRT_MAX) return ERROR_OVERFLOW; STORE(short); break;
        case L: STORE(long); break;
        case LL: STORE(long long); break;
        case J: STORE(intmax_t); break;
        case Z: case T: STORE(ptrdiff_t); break;
        default: return ERROR_INVALID_ARG;
    }
#undef STORE
    return OK;
}

Status consume_argument(va_list *args, const char conversion, const Length length) {
    if (strchr("diuoxX", conversion) != NULL) {
        const int uns = strchr("uoxX", conversion) != NULL;
        switch (length) {
            case NONE: if (uns) (void)va_arg(*args, unsigned int); else (void)va_arg(*args, int); break;
            case HH: case H: (void)va_arg(*args, int); break;
            case L: if (uns) (void)va_arg(*args, unsigned long); else (void)va_arg(*args, long); break;
            case LL: if (uns) (void)va_arg(*args, unsigned long long); else (void)va_arg(*args, long long); break;
            case J: if (uns) (void)va_arg(*args, uintmax_t); else (void)va_arg(*args, intmax_t); break;
            case Z: case T: if (uns) (void)va_arg(*args, size_t); else (void)va_arg(*args, ptrdiff_t); break;
            default: return ERROR_INVALID_ARG;
        }
    } else if (strchr("fFeEgGaA", conversion) != NULL) {
        if (length == BIG_L) (void)va_arg(*args, long double);
        else if (length == NONE || length == L) (void)va_arg(*args, double);
        else return ERROR_INVALID_ARG;
    } else if (conversion == 's') {
        if (length == NONE) { if (va_arg(*args, char *) == NULL) return ERROR_INVALID_ARG; }
        else if (length == L) { if (va_arg(*args, wchar_t *) == NULL) return ERROR_INVALID_ARG; }
        else return ERROR_INVALID_ARG;
    } else if (conversion == 'c') {
        if (length == L) {
#if WINT_MAX <= INT_MAX
            (void)va_arg(*args, int);
#else
            (void)va_arg(*args, wint_t);
#endif
        }
        else if (length == NONE) (void)va_arg(*args, int);
        else return ERROR_INVALID_ARG;
    } else if (conversion == 'p' && length == NONE) (void)va_arg(*args, void *);
    else return ERROR_INVALID_ARG;
    return OK;
}

Status standard_output(Output *out, const char **format, va_list *args) {
    const char *start = *format, *p = start + 1;
    while (*p && strchr("-+ #0", *p)) p++;
    int stars = 0;
    if (*p == '*') { stars++; p++; } else while (*p >= '0' && *p <= '9') p++;
    if (*p == '.') {
        p++;
        if (*p == '*') { stars++; p++; } else while (*p >= '0' && *p <= '9') p++;
    }
    Length length = NONE;
    if (*p == 'h') { p++; length = H; if (*p == 'h') { p++; length = HH; } }
    else if (*p == 'l') { p++; length = L; if (*p == 'l') { p++; length = LL; } }
    else if (*p == 'j') { p++; length = J; }
    else if (*p == 'z') { p++; length = Z; }
    else if (*p == 't') { p++; length = T; }
    else if (*p == 'L') { p++; length = BIG_L; }
    if (*p == '\0' || strchr("diuoxXfFeEgGaAcspn", *p) == NULL) return ERROR_INVALID_ARG;
    const char conversion = *p++;
    const size_t size = (size_t)(p - start);
    char *spec = malloc(size + 1);
    if (spec == NULL) return ERROR_NO_MEMORY;
    memcpy(spec, start, size); spec[size] = '\0';
    if (conversion == 'n') {
        Status st = ERROR_INVALID_ARG;
        if (stars == 0 && size == (length == NONE ? 2u : (length == HH || length == LL) ? 4u : 3u))
            st = store_count(length, args, out->count);
        free(spec);
        *format = p;
        return st;
    }
    va_list check;
    va_copy(check, *args);
    for (int i = 0; i < stars; i++) (void)va_arg(check, int);
    Status st = consume_argument(&check, conversion, length);
    va_end(check);
    if (st != OK) { free(spec); return st; }
    {
        va_list copy;
        va_copy(copy, *args);
        const int needed = vsnprintf(NULL, 0, spec, copy);
        va_end(copy);
        if (needed < 0) st = ERROR_FILE;
        else {
            char *text = malloc((size_t)needed + 1);
            if (text == NULL) st = ERROR_NO_MEMORY;
            else {
                va_copy(copy, *args);
                const int written = vsnprintf(text, (size_t)needed + 1, spec, copy);
                va_end(copy);
                st = written == needed ? emit(out, text, needed) : ERROR_FILE;
                free(text);
            }
        }
        for (int i = 0; i < stars; i++) (void)va_arg(*args, int);
        if (st == OK) st = consume_argument(args, conversion, length);
    }
    free(spec);
    *format = p;
    return st;
}

Status format_output(Output *out, const char *format, va_list *args) {
    if (out == NULL || format == NULL || args == NULL) return ERROR_INVALID_ARG;
    const char *p = format;
    Status st = OK;
    while (*p && st == OK) {
        if (*p != '%') { st = emit(out, p++, 1); continue; }
        if (p[1] == '%') { st = emit(out, "%", 1); p += 2; continue; }
        char text[sizeof(double) * (CHAR_BIT + 1) + sizeof(unsigned int) * CHAR_BIT * 2 + 64];
        if (p[1] == 'R' && p[2] == 'o') st = roman(va_arg(*args, int), text);
        else if (p[1] == 'Z' && p[2] == 'r') st = zeckendorf(va_arg(*args, unsigned int), text);
        else if (p[1] == 'C' && (p[2] == 'v' || p[2] == 'V')) {
            const int value = va_arg(*args, int), base = va_arg(*args, int);
            st = integer_text(value, base, p[2] == 'V', text);
        } else if ((p[1] == 't' && p[2] == 'o') || (p[1] == 'T' && p[2] == 'O')) {
            const char *number = va_arg(*args, const char *);
            const int base = va_arg(*args, int);
            int value;
            st = parse_base(number, base, p[1] == 'T', &value);
            if (st == OK) st = integer_text(value, 10, 0, text);
        } else if (p[1] == 'm' && p[2] && strchr("iudf", p[2])) {
            if (p[2] == 'i') {
                const int value = va_arg(*args, int);
                st = sizeof(int) == 4 ? memory_text(&value, sizeof(value), text) : ERROR_INVALID_ARG;
            } else if (p[2] == 'u') {
                const unsigned int value = va_arg(*args, unsigned int);
                st = sizeof(unsigned int) == 4 ? memory_text(&value, sizeof(value), text) : ERROR_INVALID_ARG;
            } else if (p[2] == 'd') {
                const double value = va_arg(*args, double);
                st = memory_text(&value, sizeof(value), text);
            } else {
                const float value = (float)va_arg(*args, double);
                st = memory_text(&value, sizeof(value), text);
            }
        } else { st = standard_output(out, &p, args); continue; }
        if (st == OK) st = emit(out, text, (int)strlen(text));
        p += 3;
    }
    return st;
}

int overfprintf(FILE *stream, const char *format, ...) {
    if (stream == NULL || format == NULL) return -1;
    Output out = {stream, NULL, 0};
    va_list args;
    va_start(args, format);
    const Status st = format_output(&out, format, &args);
    va_end(args);
    return st == OK ? out.count : -1;
}

int oversprintf(char *text, const char *format, ...) {
    if (text == NULL || format == NULL) return -1;
    text[0] = '\0';
    Output out = {NULL, text, 0};
    va_list args;
    va_start(args, format);
    const Status st = format_output(&out, format, &args);
    va_end(args);
    return st == OK ? out.count : -1;
}

int main(int argc, char *argv[]) {
    if (argc != 1) return report(ERROR_INVALID_ARG);
    (void)argv;
    char text[512];
    int count = oversprintf(text, "roman=%Ro fibonacci=%Zr lower=%Cv upper=%CV decimal=%to/%TO",
                           2026, 10u, 255, 16, 255, 16, "ff", 16, "FF", 16);
    if (count < 0) return report(ERROR_INVALID_ARG);
    puts(text);
    count = overfprintf(stdout, "int=%mi\nunsigned=%mu\ndouble=%md\nfloat=%mf\nstandard=%08d %.2f %s\n",
                        -1, 42u, 0.5, 0.5, 17, 3.14, "ok");
    if (count < 0 || fflush(stdout) == EOF) return report(ERROR_FILE);
    return OK;
}
