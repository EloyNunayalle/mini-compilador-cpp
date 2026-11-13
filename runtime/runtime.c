/* Runtime del mini-compilador C++: string dinamico, salida y entrada.
 * No usa libstdc++. Se enlaza junto al ensamblador generado:
 *   gcc programa.s runtime/runtime.c -o programa
 *
 * Representacion de string (16 bytes, alineacion 8), igual que el layout del compilador:
 *   offset 0: char* data   (NULL mientras no se haya usado; si no, siempre termina en '\0')
 *   offset 8: int size
 *   offset 12: int capacity (incluye el '\0')
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    int size;
    int capacity;
} String;

static void str_reserve(String *s, int needed) {
    /* needed = bytes necesarios sin contar el '\0' */
    if (s->data && s->capacity >= needed + 1) return;
    int cap = s->capacity > 0 ? s->capacity : 8;
    while (cap < needed + 1) cap *= 2;
    char *p = (char *)realloc(s->data, (size_t)cap);
    if (!p) {
        fputs("runtime: sin memoria\n", stderr);
        exit(1);
    }
    if (!s->data) p[0] = '\0';
    s->data = p;
    s->capacity = cap;
}

void rt_str_reset(String *s) {
    free(s->data);
    s->data = NULL;
    s->size = 0;
    s->capacity = 0;
}

void rt_str_init(String *s) {
    s->data = NULL;
    s->size = 0;
    s->capacity = 0;
}

void rt_str_init_cstr(String *s, const char *text);
void rt_str_copy_init(String *dst, const String *src);

void rt_str_free(String *s) {
    rt_str_reset(s);
}

void rt_str_clear(String *s) {
    s->size = 0;
    if (s->data) s->data[0] = '\0';
}

void rt_str_append_buf(String *s, const char *text, int len) {
    str_reserve(s, s->size + len);
    memcpy(s->data + s->size, text, (size_t)len);
    s->size += len;
    s->data[s->size] = '\0';
}

void rt_str_append_cstr(String *s, const char *text) {
    rt_str_append_buf(s, text, (int)strlen(text));
}

void rt_str_append_char(String *s, char c) {
    rt_str_append_buf(s, &c, 1);
}

void rt_str_append_str(String *s, const String *src) {
    int len = src->size;
    if (len == 0) return;
    str_reserve(s, s->size + len);
    memmove(s->data + s->size, src->data, (size_t)len);
    s->size += len;
    s->data[s->size] = '\0';
}

void rt_str_assign_cstr(String *dst, const char *text) {
    /* text puede apuntar dentro de dst->data (por ejemplo s = s.c_str()) */
    int len = (int)strlen(text);
    str_reserve(dst, len);
    memmove(dst->data, text, (size_t)len);
    dst->size = len;
    dst->data[len] = '\0';
}

void rt_str_assign(String *dst, const String *src) {
    if (dst == src) return;
    if (src->size == 0) {
        rt_str_clear(dst);
        return;
    }
    rt_str_assign_cstr(dst, src->data);
}

void rt_str_init_cstr(String *s, const char *text) {
    rt_str_init(s);
    rt_str_assign_cstr(s, text);
}

void rt_str_copy_init(String *dst, const String *src) {
    rt_str_init(dst);
    rt_str_assign(dst, src);
}

int rt_str_size(const String *s) {
    return s->size;
}

int rt_str_empty(const String *s) {
    return s->size == 0;
}

const char *rt_str_c_str(String *s) {
    str_reserve(s, s->size);
    return s->data;
}

int rt_str_eq(const String *a, const String *b) {
    if (a->size != b->size) return 0;
    if (a->size == 0) return 1;
    return memcmp(a->data, b->data, (size_t)a->size) == 0;
}

int rt_str_eq_cstr(const String *a, const char *b) {
    size_t n = strlen(b);
    if ((size_t)a->size != n) return 0;
    if (n == 0) return 1;
    return memcmp(a->data, b, n) == 0;
}

void rt_print_int(int x) { printf("%d", x); }
void rt_print_char(char c) { putchar(c); }
void rt_print_bool(int b) { putchar(b ? '1' : '0'); }
void rt_print_double(double x) { printf("%g", x); }
void rt_print_cstr(const char *s) { fputs(s, stdout); }

void rt_print_str(const String *s) {
    if (s->size > 0) fwrite(s->data, 1, (size_t)s->size, stdout);
}

void rt_print_endl(void) {
    putchar('\n');
    fflush(stdout);
}

void rt_read_int(int *p) {
    if (scanf(" %d", p) != 1) *p = 0;
}

void rt_read_char(char *p) {
    if (scanf(" %c", p) != 1) *p = '\0';
}

void rt_read_float(float *p) {
    if (scanf(" %f", p) != 1) *p = 0.0f;
}

void rt_read_double(double *p) {
    if (scanf(" %lf", p) != 1) *p = 0.0;
}

void rt_read_string(String *s) {
    rt_str_clear(s);
    int c;
    do {
        c = getchar();
    } while (c != EOF && isspace(c));
    while (c != EOF && !isspace(c)) {
        rt_str_append_char(s, (char)c);
        c = getchar();
    }
    if (c != EOF) ungetc(c, stdin);
}
