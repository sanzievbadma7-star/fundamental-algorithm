#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_OUTPUT
} Status;

Status report(const Status status) {
    switch (status) {
        case OK: break;
        case ERROR_INVALID_ARG: fprintf(stderr, "Error: invalid input or function argument\n"); break;
        case ERROR_OVERFLOW: fprintf(stderr, "Error: number or string out of range\n"); break;
        case ERROR_NO_MEMORY: fprintf(stderr, "Error: cannot allocate memory\n"); break;
        case ERROR_OUTPUT: fprintf(stderr, "Error: cannot write output\n"); break;
    }
    return status;
}
//длина строки -l
Status string_length(const char *text, int *out) {
    if (text == NULL || out == NULL) return ERROR_INVALID_ARG;
    int length = 0;
    while (text[length] != '\0') {
        if (length == INT_MAX - 1) return ERROR_OVERFLOW;
        length++;
    }
    *out = length;
    return OK;
}
//парс флага
Status single_flag(const char *text, char *mode) {
    if (text == NULL || mode == NULL || text[0] != '-' ||
        text[1] == '\0' || text[2] != '\0') return ERROR_INVALID_ARG;
    *mode = text[1];
    return OK;
}
//парсим сид для флага -с
Status parse_seed(const char *text, unsigned int *out) {
    if (text == NULL || out == NULL) return ERROR_INVALID_ARG;
    if (*text == '+') text++;
    if (*text == '\0') return ERROR_INVALID_ARG;
    unsigned int value = 0;
    while (*text != '\0') {
        if (*text < '0' || *text > '9') return ERROR_INVALID_ARG;
        const unsigned int digit = *text - '0';
        if (value > (UINT_MAX - digit) / 10) return ERROR_OVERFLOW;
        value = value * 10 + digit;//типа 123 =1*10**2 + 2*10**1 + 3*10**0
        text++;
    }
    *out = value;
    return OK;
}
//реверс строки -r
Status reverse_string(const char *text, char **out) {
    if (text == NULL || out == NULL) return ERROR_INVALID_ARG;
    int length;
    Status st = string_length(text, &length);
    if (st != OK) return st;
    char *result = malloc(length + 1);
    if (result == NULL) return ERROR_NO_MEMORY;
    for (int i = 0; i < length; i++) result[i] = text[length - 1 - i];
    result[length] = '\0';
    *out = result;
    return OK;
}
//перевести в верхний регистр -u
Status uppercase_odd(const char *text, char **out) {
    if (text == NULL || out == NULL) return ERROR_INVALID_ARG;
    int length;
    Status st = string_length(text, &length);
    if (st != OK) return st;
    char *result = malloc(length + 1);
    if (result == NULL) return ERROR_NO_MEMORY;
    for (int i = 0; i < length; i++) {
        char c = text[i];
        if (i % 2 != 0 && c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        result[i] = c;
    }
    result[length] = '\0';
    *out = result;
    return OK;
}
//флаг -n
Status group_symbols(const char *text, char **out) {
    if (text == NULL || out == NULL) return ERROR_INVALID_ARG;
    int length;
    Status st = string_length(text, &length);
    if (st != OK) return st;
    char *result = malloc(length + 1);
    if (result == NULL) return ERROR_NO_MEMORY;
    int pos = 0;
    for (int group = 0; group < 3; group++) {
        for (int i = 0; i < length; i++) {
            const char c = text[i];
            const int kind = c >= '0' && c <= '9' ? 0 :
                ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) ? 1 : 2;
            if (kind == group) result[pos++] = c;
        }
    }
    result[pos] = '\0';
    *out = result;
    return OK;
}

Status concatenate_random(const char *first, char *const *others,
                          const int count, const unsigned int seed, char **out) {
    if (first == NULL || out == NULL || count < 0 ||
        (count > 0 && others == NULL)) return ERROR_INVALID_ARG;
    if (count == INT_MAX) return ERROR_OVERFLOW;
    int total;
    Status st = string_length(first, &total);
    if (st != OK) return st;
    for (int i = 0; i < count; i++) {
        int length;
        st = string_length(others[i], &length);
        if (st != OK) return st;
        if (length > INT_MAX - 1 - total) return ERROR_OVERFLOW;
        total += length;
    }
    const int number = count + 1;
    if ((size_t)number > SIZE_MAX / sizeof(int)) return ERROR_OVERFLOW;
    int *order = malloc((size_t)number * sizeof(int));
    if (order == NULL) return ERROR_NO_MEMORY;
    char *result = malloc(total + 1);
    if (result == NULL) { free(order); return ERROR_NO_MEMORY; }
    for (int i = 0; i < number; i++) order[i] = i;
    srand(seed);
    // Перемешиваем индексы, чтобы каждую строку использовать ровно один раз.
    for (int i = number - 1; i > 0; i--) {
        const int j = rand() % (i + 1);
        const int temp = order[i];
        order[i] = order[j];
        order[j] = temp;
    }
    int pos = 0;
    for (int i = 0; i < number; i++) {
        const char *text = order[i] == 0 ? first : others[order[i] - 1];
        for (int j = 0; text[j] != '\0'; j++) result[pos++] = text[j];
    }
    result[pos] = '\0';
    free(order);
    *out = result;
    return OK;
}

int main(int argc, char *argv[]) {
    if (argc < 3) return report(ERROR_INVALID_ARG);
    char mode;
    Status st = single_flag(argv[1], &mode);
    if (st != OK) return report(st);
    if ((mode == 'c' && argc < 4) || (mode != 'c' && argc != 3))
        return report(ERROR_INVALID_ARG);
    char *result = NULL;
    switch (mode) {
        case 'l': {
            int length;
            st = string_length(argv[2], &length);
            if (st == OK && printf("%d\n", length) < 0) st = ERROR_OUTPUT;
            break;
        }
        case 'r': st = reverse_string(argv[2], &result); break;
        case 'u': st = uppercase_odd(argv[2], &result); break;
        case 'n': st = group_symbols(argv[2], &result); break;
        case 'c': {
            unsigned int seed;
            st = parse_seed(argv[3], &seed);
            if (st == OK) st = concatenate_random(argv[2], argv + 4, argc - 4, seed, &result);
            break;
            
        }
        default: st = ERROR_INVALID_ARG;
    }
    if (st == OK && result != NULL && puts(result) == EOF) st = ERROR_OUTPUT;
    free(result);
    if (st == OK && fflush(stdout) == EOF) st = ERROR_OUTPUT;
    return report(st);
}
