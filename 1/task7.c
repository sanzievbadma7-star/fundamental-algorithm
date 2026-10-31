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

Status single_flag(const char *text, char *mode) {
    if (text == NULL || mode == NULL || strlen(text) != 2 ||
        (text[0] != '-' && text[0] != '/')) return ERROR_INVALID_ARG;
    *mode = text[1];
    return OK;
}

Status read_word(FILE *in, char **word, int *has_word) {
    if (in == NULL || word == NULL || has_word == NULL) return ERROR_INVALID_ARG;
    *has_word = 0;
    free(*word);
    *word = NULL;
    int c;
    do { c = fgetc(in); } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    if (c == EOF) return ferror(in) ? ERROR_FILE : OK;
    int length = 0, capacity = 16;
    *word = malloc(capacity);
    if (*word == NULL) return ERROR_NO_MEMORY;
    while (c != EOF && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        if (length == capacity - 1) {
            if (capacity > INT_MAX / 2) return ERROR_OVERFLOW;
            capacity *= 2;
            // При неудаче realloc старый указатель всё ещё нужен для free.
            char *temp = realloc(*word, capacity);
            if (temp == NULL) return ERROR_NO_MEMORY;
            *word = temp;
        }
        (*word)[length++] = c;
        c = fgetc(in);
    }
    (*word)[length] = '\0';
    if (ferror(in)) return ERROR_FILE;
    *has_word = 1;
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

Status lowercase(char *word) {
    if (word == NULL) return ERROR_INVALID_ARG;
    for (int i = 0; word[i]; i++) {
        if (word[i] >= 'A' && word[i] <= 'Z') word[i] += 'a' - 'A';
    }
    return OK;
}

// Для ASCII достаточно int: код символа лежит в диапазоне 0..127.
Status encode(const char *word, const int base, char **out) {
    if (word == NULL || out == NULL || (base != 4 && base != 8)) return ERROR_INVALID_ARG;
    if (strlen(word) > (INT_MAX - 1) / 4) return ERROR_OVERFLOW;
    int length = strlen(word);
    *out = malloc(length * 4 + 1);
    if (*out == NULL) return ERROR_NO_MEMORY;
    int pos = 0;
    for (int i = 0; i < length; i++) {
        int code = word[i];
        if (code < 0 || code > 127) { free(*out); *out = NULL; return ERROR_INVALID_ARG; }
        char digits[66];
        Status st = format_integer(word[i], base, digits, 66);
        if (st != OK) { free(*out); *out = NULL; return st; }
        for (int j = 0; digits[j]; j++) (*out)[pos++] = digits[j];
    }
    (*out)[pos] = '\0';
    return OK;
}

Status write_word(FILE *out, const char *word, int *first) {
    if (out == NULL || word == NULL || first == NULL) return ERROR_INVALID_ARG;
    if (!*first && fputc(' ', out) == EOF) return ERROR_FILE;
    *first = 0;
    return fputs(word, out) == EOF ? ERROR_FILE : OK;
}

Status merge(FILE *a, FILE *b, FILE *out) {
    if (a == NULL || b == NULL || out == NULL) return ERROR_INVALID_ARG;
    char *word = NULL;
    int has_a = 1, has_b = 1, first = 1;
    Status st = OK;
    while (has_a || has_b) {
        st = read_word(a, &word, &has_a);
        if (st != OK) break;
        if (has_a) st = write_word(out, word, &first);
        if (st != OK) break;
        st = read_word(b, &word, &has_b);
        if (st != OK) break;
        if (has_b) st = write_word(out, word, &first);
        if (st != OK) break;
    }
    free(word);
    return st;
}

Status transform(FILE *in, FILE *out) {
    if (in == NULL || out == NULL) return ERROR_INVALID_ARG;
    char *word = NULL;
    int has_word, first = 1, position = 1;
    Status st = OK;
    while (1) {
        st = read_word(in, &word, &has_word);
        if (st != OK || !has_word) break;
        if (position % 2 == 0) st = lowercase(word);
        char *codes = NULL;
        if (st == OK && position % 5 == 0) st = encode(word, position == 10 ? 4 : 8, &codes);
        if (st == OK) st = write_word(out, codes == NULL ? word : codes, &first);
        free(codes);
        if (st != OK) break;
        // Все правила повторяются через десять лексем.
        position = position % 10 + 1;
    }
    free(word);
    return st;
}

int main(int argc, char *argv[]) {
    char mode;
    if (argc < 2 || single_flag(argv[1], &mode) != OK ||
        (mode != 'r' && mode != 'a') || argc != (mode == 'r' ? 5 : 4)) return report(ERROR_INVALID_ARG);
    const char *output = argv[argc - 1];
    if (strcmp(argv[2], output) == 0 || (mode == 'r' && strcmp(argv[3], output) == 0))
        return report(ERROR_INVALID_ARG);
    FILE *a = fopen(argv[2], "r");
    FILE *b = mode == 'r' ? fopen(argv[3], "r") : NULL;
    FILE *out = NULL;
    Status st = a == NULL || (mode == 'r' && b == NULL) ? ERROR_FILE : OK;
    if (st == OK) {
        out = fopen(output, "wx");
        if (out == NULL) st = ERROR_FILE;
    }
    if (st == OK) st = mode == 'r' ? merge(a, b, out) : transform(a, out);
    if (a != NULL && fclose(a) != 0) st = ERROR_FILE;
    if (b != NULL && fclose(b) != 0) st = ERROR_FILE;
    if (out != NULL && fclose(out) != 0) st = ERROR_FILE;
    return report(st);
}
