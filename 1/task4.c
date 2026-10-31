#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE
} Status;

Status report(const Status status) {
    switch (status) {
        case OK: break;
        case ERROR_INVALID_ARG: fprintf(stderr, "Error: invalid input or function argument\n"); break;
        case ERROR_OVERFLOW: fprintf(stderr, "Error: number out of range\n"); break;
        case ERROR_NO_MEMORY: fprintf(stderr, "Error: cannot allocate memory\n"); break;
        case ERROR_FILE: fprintf(stderr, "Error: cannot read, write or open file\n"); break;
    }
    return status;
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

Status output_name(const char *input, char **out) {
    if (input == NULL || out == NULL || *input == '\0') return ERROR_INVALID_ARG;
    const char *name = input;
    for (const char *p = input; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    if (*name == '\0' || strlen(input) > INT_MAX - 5) return ERROR_INVALID_ARG;
    int prefix = name - input;
    *out = malloc(strlen(input) + 5);
    if (*out == NULL) return ERROR_NO_MEMORY;
    memcpy(*out, input, prefix);
    strcpy(*out + prefix, "out_");
    strcpy(*out + prefix + 4, name);
    return OK;
}

Status count_symbol(const int c, const char mode, long long *count) {
    if (count == NULL || *count < 0 || c < 0 || c > UCHAR_MAX ||
        (mode != 'i' && mode != 's')) return ERROR_INVALID_ARG;
    int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    int digit = c >= '0' && c <= '9';
    if ((mode == 'i' && letter) || (mode == 's' && !letter && !digit && c != ' ')) {
        if (*count == LLONG_MAX) return ERROR_OVERFLOW;
        (*count)++;
    }
    return OK;
}

Status process(FILE *in, FILE *out, const char mode) {
    if (in == NULL || out == NULL || mode == '\0' || strchr("disa", mode) == NULL)
        return ERROR_INVALID_ARG;
    int c, has_line = 0;
    long long count = 0;
    while ((c = fgetc(in)) != EOF) {
        if (mode == 'i' || mode == 's') {
            if (c == '\n') {
                if (fprintf(out, "%lld\n", count) < 0) return ERROR_FILE;
                count = 0; has_line = 0;
                continue;
            }
            has_line = 1;
            Status st = count_symbol(c, mode, &count);
            if (st != OK) return st;
        } else if (mode == 'd') {
            if ((c < '0' || c > '9') && fputc(c, out) == EOF) return ERROR_FILE;
        } else if (c >= '0' && c <= '9') {
            if (fputc(c, out) == EOF) return ERROR_FILE;
        } else {
            char text[66];
            Status st = format_integer(c, 16, text, 66);
            if (st != OK) return st;
            if (fputs(text, out) == EOF) return ERROR_FILE;
        }
    }
    if (ferror(in)) return ERROR_FILE;
    if (has_line && fprintf(out, "%lld\n", count) < 0) return ERROR_FILE;
    return OK;
}

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) return report(ERROR_INVALID_ARG);
    const char *flag = argv[1];
    int length = strlen(flag);
    if ((length != 2 && length != 3) || (flag[0] != '-' && flag[0] != '/'))
        return report(ERROR_INVALID_ARG);
    int named = length == 3;
    char mode = flag[length - 1];
    if ((named && flag[1] != 'n') || argc != 3 + named || strchr("disa", mode) == NULL)
        return report(ERROR_INVALID_ARG);
    char *generated = NULL;
    Status st = OK;
    if (!named) st = output_name(argv[2], &generated);
    if (st != OK) return report(st);
    const char *name = named ? argv[3] : generated;
    if (strcmp(argv[2], name) == 0) { free(generated); return report(ERROR_INVALID_ARG); }
    FILE *in = fopen(argv[2], "r");
    FILE *out = in == NULL ? NULL : fopen(name, "wx");
    st = in == NULL || out == NULL ? ERROR_FILE : process(in, out, mode);
    if (in != NULL && fclose(in) != 0) st = ERROR_FILE;
    if (out != NULL && fclose(out) != 0) st = ERROR_FILE;
    free(generated);
    return report(st);
}
