#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <sys/stat.h>

typedef enum {
    OK, ERROR_INVALID_ARG, ERROR_OVERFLOW, ERROR_NO_MEMORY, ERROR_FILE
} Status;

typedef struct {
    int id;
    char *name;
    char *surname;
    double salary;
} Employee;

Status report(const Status status) {
    switch (status) {
        case OK: break;
        case ERROR_INVALID_ARG: fprintf(stderr, "Error: invalid input or function argument\n"); break;
        case ERROR_OVERFLOW: fprintf(stderr, "Error: number or collection out of range\n"); break;
        case ERROR_NO_MEMORY: fprintf(stderr, "Error: cannot allocate memory\n"); break;
        case ERROR_FILE: fprintf(stderr, "Error: cannot open, read, write or close file\n"); break;
    }
    return status;
}

Status single_flag(const char *text, char *mode) {
    if (text == NULL || mode == NULL || strlen(text) != 2 ||
        (text[0] != '-' && text[0] != '/') ||
        (text[1] != 'a' && text[1] != 'd')) return ERROR_INVALID_ARG;
    *mode = text[1];
    return OK;
}

Status parse_id(const char *text, int *out) {
    if (text == NULL || out == NULL) return ERROR_INVALID_ARG;
    if (*text == '+') text++;
    if (*text == '\0') return ERROR_INVALID_ARG;
    int value = 0;
    while (*text != '\0') {
        if (*text < '0' || *text > '9') return ERROR_INVALID_ARG;
        const int digit = *text - '0';
        if (value > (INT_MAX - digit) / 10) return ERROR_OVERFLOW;
        value = value * 10 + digit;
        text++;
    }
    *out = value;
    return OK;
}

Status parse_salary(const char *text, double *out) {
    if (text == NULL || out == NULL || *text == '\0') return ERROR_INVALID_ARG;
    const char *p = text;
    int digits = 0;
    if (*p == '+' || *p == '-') p++;
    while (*p >= '0' && *p <= '9') {
        digits++; p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            digits++; p++;
        }
    }
    if (digits == 0) return ERROR_INVALID_ARG;
    if (*p != '\0') return ERROR_INVALID_ARG;
    double value = strtod(text, NULL);
    if (fabs(value) > DBL_MAX) return ERROR_OVERFLOW;
    *out = value;
    return OK;
}

Status copy_name(const char *text, char **out) {
    if (text == NULL || out == NULL || *text == '\0') return ERROR_INVALID_ARG;
    for (const char *p = text; *p != '\0'; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')))
            return ERROR_INVALID_ARG;
    }
    char *copy = malloc(strlen(text) + 1);
    if (copy == NULL) return ERROR_NO_MEMORY;
    strcpy(copy, text);
    *out = copy;
    return OK;
}

Status read_line(FILE *in, char **out, int *has_line) {
    if (in == NULL || out == NULL || has_line == NULL) return ERROR_INVALID_ARG;
    *out = NULL;
    *has_line = 0;
    int c = fgetc(in);
    if (c == EOF) return ferror(in) ? ERROR_FILE : OK;
    int length = 0, capacity = 64;
    char *line = malloc(capacity);
    if (line == NULL) return ERROR_NO_MEMORY;
    Status st = OK;
    while (c != EOF && c != '\n' && st == OK) {
        if (c == '\0') st = ERROR_INVALID_ARG;
        else {
            if (length == capacity - 1) {
                if (capacity > INT_MAX / 2) st = ERROR_OVERFLOW;
                else {
                    char *temp = realloc(line, capacity * 2);
                    if (temp == NULL) st = ERROR_NO_MEMORY;
                    else { line = temp; capacity *= 2; }
                }
            }
            if (st == OK) { line[length++] = c; c = fgetc(in); }
        }
    }
    if (ferror(in)) st = ERROR_FILE;
    if (st != OK) { free(line); return st; }
    line[length] = '\0';
    *out = line;
    *has_line = 1;
    return OK;
}

Status parse_employee(char *line, Employee *out) {
    if (line == NULL || out == NULL) return ERROR_INVALID_ARG;
    char *fields[4];
    char *token = strtok(line, " \t\r\v\f");
    for (int i = 0; i < 4; i++) {
        if (token == NULL) return ERROR_INVALID_ARG;
        fields[i] = token;
        token = strtok(NULL, " \t\r\v\f");
    }
    if (token != NULL) return ERROR_INVALID_ARG;
    Employee employee = {0, NULL, NULL, 0};
    Status st = parse_id(fields[0], &employee.id);
    if (st == OK) st = parse_salary(fields[3], &employee.salary);
    if (st == OK) st = copy_name(fields[1], &employee.name);
    if (st == OK) st = copy_name(fields[2], &employee.surname);
    if (st != OK) { free(employee.name); free(employee.surname); return st; }
    *out = employee;
    return OK;
}

Status free_employees(Employee *items, const int count) {
    if (count < 0 || (count > 0 && items == NULL)) return ERROR_INVALID_ARG;
    for (int i = 0; i < count; i++) {
        free(items[i].name);
        free(items[i].surname);
    }
    free(items);
    return OK;
}

Status read_employees(FILE *in, Employee **out, int *count) {
    if (in == NULL || out == NULL || count == NULL) return ERROR_INVALID_ARG;
    Employee *items = NULL;
    int used = 0, capacity = 0;
    Status st = OK;
    while (st == OK) {
        char *line = NULL;
        int has_line;
        st = read_line(in, &line, &has_line);
        if (st != OK || !has_line) break;
        Employee employee;
        st = parse_employee(line, &employee);
        free(line);
        if (st != OK) break;
        if (used == capacity) {
            if (capacity > INT_MAX / 2) st = ERROR_OVERFLOW;
            else {
                const int next = capacity == 0 ? 16 : capacity * 2;
                if ((size_t)next > SIZE_MAX / sizeof(Employee)) st = ERROR_OVERFLOW;
                else {
                    Employee *temp = realloc(items, (size_t)next * sizeof(Employee));
                    if (temp == NULL) st = ERROR_NO_MEMORY;
                    else { items = temp; capacity = next; }
                }
            }
        }
        if (st == OK) items[used++] = employee;
        else { free(employee.name); free(employee.surname); }
    }
    if (st != OK) { free_employees(items, used); return st; }
    *out = items;
    *count = used;
    return OK;
}

int ascending_order(const void *left, const void *right) {
    const Employee *a = left, *b = right;
    if (fabs(a->salary - b->salary) > 1e-9)
        return a->salary < b->salary ? -1 : 1;
    int order = strcmp(a->surname, b->surname);
    if (order != 0) return order < 0 ? -1 : 1;
    order = strcmp(a->name, b->name);
    if (order != 0) return order < 0 ? -1 : 1;
    return (a->id > b->id) - (a->id < b->id);
}

int descending_order(const void *left, const void *right) {
    return -ascending_order(left, right);
}

Status sort_employees(Employee *items, const int count, const char mode) {
    if (count < 0 || (count > 0 && items == NULL) ||
        (mode != 'a' && mode != 'd'))
        return ERROR_INVALID_ARG;
    for (int i = 0; i < count; i++) {
        if (items[i].id < 0 || items[i].name == NULL || items[i].surname == NULL ||
            fabs(items[i].salary) > DBL_MAX || (items[i].salary) < 0) return ERROR_INVALID_ARG;
    }
    if (count < 2) return OK;
    qsort(items, count, sizeof(Employee), mode == 'a' ? ascending_order : descending_order);
    return OK;
}

Status write_employees(FILE *out, const Employee *items, const int count) {
    if (out == NULL || count < 0 || (count > 0 && items == NULL)) return ERROR_INVALID_ARG;
    for (int i = 0; i < count; i++) {
        if (items[i].name == NULL || items[i].surname == NULL) return ERROR_INVALID_ARG;
        if (fprintf(out, "%d %s %s %.17g\n", items[i].id, items[i].name,
                    items[i].surname, items[i].salary) < 0) return ERROR_FILE;
    }
    return OK;
}
static const char *base_name(const char *path) {
    const char *name = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }
    return name;
}

Status is_same_file(const char *path1, const char *path2, int *same) {
    if (path1 == NULL || path2 == NULL || same == NULL) return ERROR_INVALID_ARG;
    *same = strcmp(base_name(path1), base_name(path2)) == 0;
    return OK;
}
int main(int argc, char *argv[]) {
    if (argc != 4 || *argv[1] == '\0' || *argv[3] == '\0') return report(ERROR_INVALID_ARG);
    if (argc != 4 || *argv[1] == '\0' || *argv[3] == '\0') return report(ERROR_INVALID_ARG);

    int same = 0;
    Status st = is_same_file(argv[1], argv[3], &same);
    if (st != OK) return report(st);
    if (same) {
        fprintf(stderr, "Error: input and output files must be different\n");
        return ERROR_INVALID_ARG;
    }
    char mode;
    st = single_flag(argv[2], &mode);
    if (st != OK) return report(st);
    FILE *in = fopen(argv[1], "rw");
    if (in == NULL) return report(ERROR_FILE);
    Employee *items = NULL;
    int count = 0;
    st = read_employees(in, &items, &count);
    if (fclose(in) == EOF && st == OK) st = ERROR_FILE;
    if (st == OK) st = sort_employees(items, count, mode);
    if (st == OK) {
        FILE *out = fopen(argv[3], "rw");
        if (out == NULL) st = ERROR_FILE;
        else {
            st = write_employees(out, items, count);
            if (fclose(out) == EOF && st == OK) st = ERROR_FILE;
        }
    }
    free_employees(items, count);
    return report(st);
}
