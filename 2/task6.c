#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <wchar.h>

typedef enum { OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE } Status;
typedef enum { NONE, HH, H, L, LL, J, Z, T, BIG_L } Length;
typedef struct { FILE *file; const char *text; int count; int error; } Input;

Status report(const Status st) {
    if (st != OK) fprintf(stderr, "Error: scanning failed (%d)\n", st);
    return st;
}

int input_get(Input *in) {
    if (in->count == INT_MAX) { in->error = 1; return EOF; }
    int c;
    if (in->file != NULL) {
        c = fgetc(in->file);
        if (ferror(in->file)) in->error = 1;
    } else c = in->text[in->count] ? (unsigned char)in->text[in->count] : EOF;
    if (c != EOF) in->count++;
    return c;
}

Status input_back(Input *in, const int c) {
    if (in == NULL) return ERROR_INVALID_ARG;
    if (c == EOF) return OK;
    if (in->count <= 0) return ERROR_INVALID_ARG;
    if (in->file != NULL && ungetc(c, in->file) == EOF) { in->error = 1; return ERROR_FILE; }
    in->count--;
    return OK;
}

Status skip_space(Input *in) {
    if (in == NULL) return ERROR_INVALID_ARG;
    int c;
    do { c = input_get(in); } while (c != EOF && isspace((unsigned char)c));
    return input_back(in, c);
}

Status roman_value(const char c, int *out) {
    if (out == NULL) return ERROR_INVALID_ARG;
    switch (c) {
        case 'I': *out = 1; break;
        case 'V': *out = 5; break;
        case 'X': *out = 10; break;
        case 'L': *out = 50; break;
        case 'C': *out = 100; break;
        case 'D': *out = 500; break;
        case 'M': *out = 1000; break;
        default: return ERROR_INVALID_ARG;
    }
    return OK;
}

Status parse_roman(const char *text, int *out) {
    if (text == NULL || *text == '\0' || out == NULL) return ERROR_INVALID_ARG;
    long long value = 0;
    for (int i = 0; text[i]; i++) {
        int current, next = 0;
        Status st = roman_value(text[i], &current);
        if (st != OK) return st;
        if (text[i + 1]) {
            st = roman_value(text[i + 1], &next);
            if (st != OK) return st;
        }
        value += current < next ? -current : current;
    }
    if (value <= 0 || value > INT_MAX) return ERROR_OVERFLOW;
    *out = (int)value;
    return OK;
}

Status parse_fibonacci(const char *text, unsigned int *out) {
    if (text == NULL || out == NULL) return ERROR_INVALID_ARG;
    const size_t length = strlen(text);
    if (length < 2 || text[length - 1] != '1') return ERROR_INVALID_ARG;
    unsigned int value = 0, current = 1, next = 2;
    for (size_t i = 0; i + 1 < length; i++) {
        if (text[i] != '0' && text[i] != '1') return ERROR_INVALID_ARG;
        if (text[i] == '1') {
            if (value > UINT_MAX - current) return ERROR_OVERFLOW;
            value += current;
        }
        if (i + 2 < length) {
            if (i + 3 < length && current > UINT_MAX - next) return ERROR_OVERFLOW;
            const unsigned int sum = i + 3 < length ? current + next : 0;
            current = next; next = sum;
        }
    }
    *out = value;
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

Status store_count(const Length length, va_list *args, const int count) {
    // Разные типы указателей обязательны по правилам scanf.
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

Status custom_token(Input *in, const char kind, const int base, char **out, int *empty_eof) {
    if (in == NULL || out == NULL || empty_eof == NULL) return ERROR_INVALID_ARG;
    Status st = skip_space(in);
    if (st != OK) return st;
    int used = 0, capacity = 32;
    char *text = malloc(capacity);
    if (text == NULL) return ERROR_NO_MEMORY;
    int c;
    *empty_eof = 0;
    while ((c = input_get(in)) != EOF) {
        int valid = 0, value;
        if (kind == 'R') valid = roman_value((char)c, &value) == OK;
        else if (kind == 'Z') valid = c == '0' || c == '1';
        else {
            if (used == 0 && (c == '+' || c == '-')) valid = 1;
            else {
                int digit = -1;
                if (c >= '0' && c <= '9') digit = c - '0';
                else if (kind == 'v' && c >= 'a' && c <= 'z') digit = c - 'a' + 10;
                else if (kind == 'V' && c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
                valid = digit >= 0 && digit < base;
            }
        }
        if (!valid) { st = input_back(in, c); break; }
        if (used == capacity - 1) {
            if (capacity > INT_MAX / 2) { st = ERROR_OVERFLOW; break; }
            char *temp = realloc(text, capacity * 2);
            if (temp == NULL) { st = ERROR_NO_MEMORY; break; }
            text = temp; capacity *= 2;
        }
        text[used++] = c;
        if (kind == 'Z' && used >= 2 && text[used - 1] == '1' && text[used - 2] == '1') break;
    }
    if (c == EOF && used == 0) *empty_eof = 1;
    if (in->error) st = ERROR_FILE;
    text[used] = '\0';
    if (st != OK) { free(text); return st; }
    *out = text;
    return OK;
}

Status standard_input(Input *in, const char **format, va_list *args,
                      int *assigned, int *completed, int *input_failure) {
    const char *start = *format, *p = start + 1;
    const int suppress = *p == '*';
    if (suppress) p++;
    while (*p >= '0' && *p <= '9') p++;
    Length length = NONE;
    if (*p == 'h') { p++; length = H; if (*p == 'h') { p++; length = HH; } }
    else if (*p == 'l') { p++; length = L; if (*p == 'l') { p++; length = LL; } }
    else if (*p == 'j') { p++; length = J; }
    else if (*p == 'z') { p++; length = Z; }
    else if (*p == 't') { p++; length = T; }
    else if (*p == 'L') { p++; length = BIG_L; }
    if (*p == '\0' || strchr("diuoxXfFeEgGaAcspn[", *p) == NULL) return ERROR_INVALID_ARG;
    const char conversion = *p++;
    if (conversion == '[') {
        if (*p == '^') p++;
        if (*p == ']') p++;
        while (*p && *p != ']') p++;
        if (*p != ']') return ERROR_INVALID_ARG;
        p++;
    }
    *format = p;
    *assigned = 0; *completed = 0; *input_failure = 0;
    if (conversion == 'n') {
        if (suppress) return ERROR_INVALID_ARG;
        Status st = store_count(length, args, in->count);
        if (st == OK) *completed = 1;
        return st;
    }
    const size_t size = (size_t)(p - start);
    char *spec = malloc(size + 3);
    if (spec == NULL) return ERROR_NO_MEMORY;
    memcpy(spec, start, size); strcpy(spec + size, "%n");
    int used = -1, result = 0;
    Status st = OK;
    // %n после спецификатора сообщает, сколько символов потребил scanf.
#define SCAN(type) do { \
    type *target = va_arg(*args, type *); \
    if (target == NULL) { st = ERROR_INVALID_ARG; break; } \
    if (in->file != NULL) result = fscanf(in->file, spec, target, &used); \
    else result = sscanf(in->text + in->count, spec, target, &used); \
} while (0)
    if (suppress) {
        if (in->file != NULL) result = fscanf(in->file, spec, &used);
        else result = sscanf(in->text + in->count, spec, &used);
    } else if (strchr("diuoxX", conversion)) {
        const int uns = strchr("uoxX", conversion) != NULL;
        switch (length) {
            case NONE: if (uns) SCAN(unsigned int); else SCAN(int); break;
            case HH: if (uns) SCAN(unsigned char); else SCAN(signed char); break;
            case H: if (uns) SCAN(unsigned short); else SCAN(short); break;
            case L: if (uns) SCAN(unsigned long); else SCAN(long); break;
            case LL: if (uns) SCAN(unsigned long long); else SCAN(long long); break;
            case J: if (uns) SCAN(uintmax_t); else SCAN(intmax_t); break;
            case Z: case T: if (uns) SCAN(size_t); else SCAN(ptrdiff_t); break;
            default: st = ERROR_INVALID_ARG;
        }
    } else if (strchr("fFeEgGaA", conversion)) {
        if (length == BIG_L) SCAN(long double);
        else if (length == L) SCAN(double);
        else if (length == NONE) SCAN(float);
        else st = ERROR_INVALID_ARG;
    } else if (conversion == 'p' && length == NONE) SCAN(void *);
    else if (strchr("cs[", conversion)) {
        if (length == L) SCAN(wchar_t);
        else if (length == NONE) SCAN(char);
        else st = ERROR_INVALID_ARG;
    } else st = ERROR_INVALID_ARG;
#undef SCAN
    free(spec);
    if (st != OK) return st;
    if (in->file != NULL && ferror(in->file)) return ERROR_FILE;
    if (used >= 0) {
        if (used > INT_MAX - in->count) return ERROR_OVERFLOW;
        in->count += used;
        *completed = 1;
        *assigned = suppress ? 0 : result;
    } else *input_failure = result == EOF;
    return OK;
}

int scan_input(Input *in, const char *format, va_list *args) {
    if (in == NULL || format == NULL || args == NULL) return EOF;
    int assigned = 0, converted = 0;
    const char *p = format;
    while (*p) {
        if (isspace((unsigned char)*p)) {
            if (skip_space(in) != OK || in->error) return assigned ? assigned : EOF;
            p++; continue;
        }
        if (*p != '%' || p[1] == '%') {
            const int literal = *p == '%' ? '%' : (unsigned char)*p;
            const int c = input_get(in);
            if (c != literal) {
                input_back(in, c);
                return c == EOF && !converted ? EOF : assigned;
            }
            p += *p == '%' ? 2 : 1;
            continue;
        }
        const int custom = (p[1] == 'R' && p[2] == 'o') || (p[1] == 'Z' && p[2] == 'r') ||
                           (p[1] == 'C' && (p[2] == 'v' || p[2] == 'V'));
        if (!custom) {
            int added, completed, input_failure;
            Status st = standard_input(in, &p, args, &added, &completed, &input_failure);
            if (st != OK) return assigned ? assigned : EOF;
            if (!completed) return input_failure && !converted ? EOF : assigned;
            assigned += added; converted = 1;
            continue;
        }
        const char kind = p[1] == 'C' ? p[2] : p[1];
        int *integer = NULL;
        unsigned int *uns = NULL;
        int base = 10;
        if (kind == 'Z') uns = va_arg(*args, unsigned int *);
        else integer = va_arg(*args, int *);
        if (p[1] == 'C') {
            base = va_arg(*args, int);
            if (base < 2 || base > 36) base = 10;
        }
        if ((kind == 'Z' && uns == NULL) || (kind != 'Z' && integer == NULL))
            return assigned ? assigned : EOF;
        char *token = NULL;
        int empty_eof;
        Status st = custom_token(in, kind, base, &token, &empty_eof);
        if (st != OK) return assigned ? assigned : EOF;
        if (kind == 'R') st = parse_roman(token, integer);
        else if (kind == 'Z') st = parse_fibonacci(token, uns);
        else st = parse_base(token, base, kind == 'V', integer);
        free(token);
        if (st != OK) return empty_eof && !converted ? EOF : assigned;
        assigned++; converted = 1; p += 3;
    }
    return assigned;
}

int overfscanf(FILE *stream, const char *format, ...) {
    if (stream == NULL || format == NULL) return EOF;
    Input in = {stream, NULL, 0, 0};
    va_list args;
    va_start(args, format);
    const int result = scan_input(&in, format, &args);
    va_end(args);
    return result;
}

int oversscanf(const char *text, const char *format, ...) {
    if (text == NULL || format == NULL) return EOF;
    Input in = {NULL, text, 0, 0};
    va_list args;
    va_start(args, format);
    const int result = scan_input(&in, format, &args);
    va_end(args);
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 1) return report(ERROR_INVALID_ARG);
    (void)argv;
    int roman = 0, lower = 0, upper = 0, ordinary = 0;
    unsigned int fibonacci = 0;
    const char *text = "MMXXVI 010011 ff FF 17";
    int count = oversscanf(text, "%Ro %Zr %Cv %CV %d", &roman, &fibonacci, &lower, 16, &upper, 16, &ordinary);
    if (count != 5) return report(ERROR_INVALID_ARG);
    printf("string: %d %u %d %d %d\n", roman, fibonacci, lower, upper, ordinary);
    FILE *file = tmpfile();
    if (file == NULL) return report(ERROR_FILE);
    Status st = OK;
    if (fputs(text, file) == EOF || fflush(file) == EOF || fseek(file, 0, SEEK_SET) != 0) st = ERROR_FILE;
    if (st == OK) {
        count = overfscanf(file, "%Ro %Zr %Cv %CV %d", &roman, &fibonacci, &lower, 16, &upper, 16, &ordinary);
        if (count != 5) st = ERROR_INVALID_ARG;
        else printf("file: %d %u %d %d %d\n", roman, fibonacci, lower, upper, ordinary);
    }
    if (fclose(file) == EOF && st == OK) st = ERROR_FILE;
    if (st == OK && (fflush(stdout) == EOF || ferror(stdout))) st = ERROR_FILE;
    return report(st);
}
