// Запуск: .\task4_1.exe
#include <stdio.h>
#include "string_lib.h"

void print_string(const char *name, const String *string) {
    if (string == NULL || string->data == NULL) {
        printf("  %s: (пусто)\n", name);
        return;
    }
    printf("  %s = \"%s\" (длина %d)\n", name, string->data, string->length);
}

const char *status_text(const StringStatus status) {
    switch (status) {
        case STRING_OK:
            return "успешно";
        case STRING_NULL_POINTER:
            return "нулевой указатель";
        case STRING_TOO_LONG:
            return "строка слишком длинная";
        case STRING_MEMORY_ERROR:
            return "не удалось выделить память";
    }
    return "неизвестная ошибка";
}

void print_compare(const String *first, const String *second) {
    int order;
    int lexicographic;
    int equal;
    if (string_order_compare(first, second, &order) != STRING_OK ||
        string_lexicographic_compare(first, second, &lexicographic) != STRING_OK ||
        string_equals(first, second, &equal) != STRING_OK) {
        printf("  ошибка сравнения\n");
        return;
    }
    printf("  \"%s\" и \"%s\": порядок (длина, затем лексикографически) = %d, лексикографически = %d, равны: %s\n",
           first->data, second->data, order, lexicographic, equal ? "да" : "нет");
}

int create_all(String strings[], const char *texts[], const int count) {
    int i;
    int j;
    for (i = 0; i < count; i++) {
        if (string_create(&strings[i], texts[i]) != STRING_OK) {
            for (j = 0; j < i; j++) {
                string_destroy(&strings[j]);
            }
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {
    const char *texts[] = {"hello", "world", "abc", "abd", "ab", "hello", "", "zz"};
    const int count = sizeof(texts) / sizeof(texts[0]);
    String strings[sizeof(texts) / sizeof(texts[0])];
    String copy = {NULL, 0};
    String *clone = NULL;
    StringStatus status;
    int i;
    int failed = 0;

    (void)argv;
    if (argc != 1) {
        printf("Ошибка: программа не принимает аргументов\n");
        return 1;
    }
    if (!create_all(strings, texts, count)) {
        printf("Ошибка: не удалось создать строки\n");
        return 1;
    }

    printf("1. Создание строк из char *:\n");
    for (i = 0; i < count; i++) {
        print_string(texts[i], &strings[i]);
    }

    printf("\n2. Сравнения (-1 - меньше, 0 - равно, 1 - больше):\n");
    print_compare(&strings[2], &strings[3]);
    print_compare(&strings[3], &strings[2]);
    print_compare(&strings[4], &strings[2]);
    print_compare(&strings[1], &strings[0]);
    print_compare(&strings[0], &strings[5]);
    print_compare(&strings[6], &strings[4]);
    print_compare(&strings[7], &strings[2]);

    printf("\n3. Копирование в существующую строку:\n");
    status = string_create(&copy, "старое значение");
    if (status == STRING_OK) {
        print_string("копия до", &copy);
        status = string_copy(&copy, &strings[0]);
        print_string("копия после копирования", &copy);
        status = string_copy(&copy, &copy);
        printf("  копирование строки в саму себя: %s\n", status_text(status));
    } else {
        failed = 1;
    }

    printf("\n4. Копирование в новую строку в динамической памяти:\n");
    status = string_clone(&strings[1], &clone);
    if (status == STRING_OK) {
        print_string("новая копия", clone);
        printf("  новая копия и исходная строка - разные буферы: %s\n", clone->data != strings[1].data ? "да" : "нет");
    } else {
        failed = 1;
    }

    printf("\n5. Конкатенация:\n");
    status = string_concat(&strings[0], &strings[1]);
    print_string("hello + world", &strings[0]);
    if (status != STRING_OK) {
        failed = 1;
    }
    status = string_concat(&strings[2], &strings[2]);
    print_string("abc + abc (сама с собой)", &strings[2]);
    if (status != STRING_OK) {
        failed = 1;
    }
    status = string_concat(&strings[4], &strings[6]);
    print_string("ab + пустая строка", &strings[4]);
    if (status != STRING_OK) {
        failed = 1;
    }

    printf("\n6. Ошибочные вызовы:\n");
    printf("  string_create(NULL, \"x\"): %s\n", status_text(string_create(NULL, "x")));
    printf("  string_create(&s, NULL): %s\n", status_text(string_create(&copy, NULL)));
    printf("  string_copy(&copy, NULL): %s\n", status_text(string_copy(&copy, NULL)));
    printf("  string_clone(NULL, &clone): %s\n", status_text(string_clone(NULL, &clone)));
    printf("  string_concat(NULL, &s): %s\n", status_text(string_concat(NULL, &strings[0])));
    printf("  string_equals с NULL: %s\n", status_text(string_equals(&strings[0], NULL, &i)));

    printf("\n7. Удаление содержимого:\n");
    string_destroy(&copy);
    print_string("копия после удаления", &copy);
    printf("  сравнение с удалённой строкой: %s\n", status_text(string_order_compare(&copy, &strings[0], &i)));

    for (i = 0; i < count; i++) {
        string_destroy(&strings[i]);
    }
    if (clone != NULL) {
        string_destroy(clone);
        free(clone);
    }
    return failed ? 1 : 0;
}
