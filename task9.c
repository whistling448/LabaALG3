// Запуск: .\task9.exe tests\words.txt ' ' ','
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/stat.h>
#endif

typedef enum { OK, END, BAD, MEMORY, IO, OVERFLOW } Status;

typedef struct Node {
    char *word;
    unsigned long long count;
    struct Node *left, *right;
} Node;

typedef struct { Node *node; size_t depth; } Visit;
typedef struct { Node **place; const char *low, *high; } Slot;

Status push_visit(Visit **stack, size_t *used, size_t *capacity, Visit item) {
    Visit *p;
    size_t cap;
    if (!stack || !used || !capacity) return BAD;
    if (*used == *capacity) {
        if (*capacity > SIZE_MAX / 2 / sizeof(Visit)) return MEMORY;
        cap = *capacity ? *capacity * 2 : 16;
        p = realloc(*stack, cap * sizeof(Visit));
        if (!p) return MEMORY;
        *stack = p; *capacity = cap;
    }
    (*stack)[(*used)++] = item;
    return OK;
}

Status push_slot(Slot **stack, size_t *used, size_t *capacity, Slot item) {
    Slot *p;
    size_t cap;
    if (!stack || !used || !capacity) return BAD;
    if (*used == *capacity) {
        if (*capacity > SIZE_MAX / 2 / sizeof(Slot)) return MEMORY;
        cap = *capacity ? *capacity * 2 : 16;
        p = realloc(*stack, cap * sizeof(Slot));
        if (!p) return MEMORY;
        *stack = p; *capacity = cap;
    }
    (*stack)[(*used)++] = item;
    return OK;
}

void free_tree(Node *root) {
    while (root) {
        if (root->left) {
            Node *left = root->left;
            root->left = left->right;
            left->right = root;
            root = left;
        } else {
            Node *next = root->right;
            free(root->word);
            free(root);
            root = next;
        }
    }
}

Status read_line(FILE *file, char **line) {
    size_t n = 0, cap = 64;
    char *p = malloc(cap), *tmp;
    int c;
    if (!file || !line) return BAD;
    if (!p) return MEMORY;
    while ((c = fgetc(file)) != EOF && c != '\n') {
        if (n + 1 == cap) {
            if (cap > SIZE_MAX / 2) { free(p); return MEMORY; }
            tmp = realloc(p, cap * 2);
            if (!tmp) { free(p); return MEMORY; }
            p = tmp; cap *= 2;
        }
        p[n++] = (char)c;
    }
    if (ferror(file)) { free(p); return IO; }
    if (c == EOF && n == 0) { free(p); return END; }
    if (n && p[n - 1] == '\r') n--;
    p[n] = '\0';
    *line = p;
    return OK;
}

Status insert(Node **root, const char *word, size_t *unique) {
    Node **place, *node;
    int cmp;
    size_t length;
    if (!root || !word || !unique || !*word) return BAD;
    place = root;
    while (*place) {
        cmp = strcmp(word, (*place)->word);
        if (cmp == 0) {
            if ((*place)->count == ULLONG_MAX) return OVERFLOW;
            (*place)->count++;
            return OK;
        }
        place = cmp < 0 ? &(*place)->left : &(*place)->right;
    }
    if (*unique == SIZE_MAX) return OVERFLOW;
    length = strlen(word);
    if (length == SIZE_MAX) return MEMORY;
    node = calloc(1, sizeof(*node));
    if (!node) return MEMORY;
    node->word = malloc(length + 1);
    if (!node->word) { free(node); return MEMORY; }
    memcpy(node->word, word, length + 1);
    node->count = 1;
    *place = node;
    (*unique)++;
    return OK;
}

Status scan_words(FILE *file, const unsigned char separators[256], Node **root, size_t *unique) {
    char *word = NULL, *next;
    size_t length = 0, cap = 0;
    Status s = OK;
    int c;
    if (!file || !separators || !root || !unique) return BAD;
    while ((c = fgetc(file)) != EOF) {
        if (c == 0) { s = BAD; break; }
        if (separators[c]) {
            if (length) {
                word[length] = '\0';
                s = insert(root, word, unique);
                if (s) break;
                length = 0;
            }
        } else {
            if (length + 1 >= cap) {
                size_t nc;
                if (cap > SIZE_MAX / 2) { s = MEMORY; break; }
                nc = cap ? cap * 2 : 16;
                next = realloc(word, nc);
                if (!next) { s = MEMORY; break; }
                word = next; cap = nc;
            }
            word[length++] = (char)c;
        }
    }
    if (!s && ferror(file)) s = IO;
    if (!s && length) { word[length] = '\0'; s = insert(root, word, unique); }
    free(word);
    return s;
}

const Node *find(const Node *root, const char *word) {
    if (!word) return NULL;
    while (root) {
        int cmp = strcmp(word, root->word);
        if (!cmp) return root;
        root = cmp < 0 ? root->left : root->right;
    }
    return NULL;
}

Status collect(Node *root, Node ***array, const size_t count, size_t *depth, const Node **shortest, const Node **longest) {
    Visit *stack = NULL;
    size_t used = 0, cap = 0, n = 0;
    Status s = OK;
    if (!array || !depth || !shortest || !longest || (count && !root)) return BAD;
    *array = NULL; *depth = 0; *shortest = *longest = NULL;
    if (!count) return OK;
    if (count > SIZE_MAX / sizeof(Node *)) return MEMORY;
    *array = malloc(count * sizeof(Node *));
    if (!*array) return MEMORY;
    s = push_visit(&stack, &used, &cap, (Visit){root, 1});
    while (used && !s) {
        Visit v = stack[--used];
        size_t len = strlen(v.node->word);
        if (n == count) { s = BAD; break; }
        (*array)[n++] = v.node;
        if (v.depth > *depth) *depth = v.depth;
        if (!*shortest || len < strlen((*shortest)->word)) *shortest = v.node;
        if (!*longest || len > strlen((*longest)->word)) *longest = v.node;
        if (v.node->right) s = push_visit(&stack, &used, &cap, (Visit){v.node->right, v.depth + 1});
        if (!s && v.node->left) s = push_visit(&stack, &used, &cap, (Visit){v.node->left, v.depth + 1});
    }
    free(stack);
    if (n != count && !s) s = BAD;
    if (s) { free(*array); *array = NULL; }
    return s;
}

int by_frequency(const void *a, const void *b) {
    const Node *x = *(Node *const *)a;
    const Node *y = *(Node *const *)b;
    if (x->count != y->count) return x->count > y->count ? -1 : 1;
    return strcmp(x->word, y->word);
}

Status parse_count(const char *text, size_t *out) {
    size_t n = 0;
    if (!text || !out || !*text) return BAD;
    while (*text) {
        unsigned d;
        if (*text < '0' || *text > '9') return BAD;
        d = (unsigned)(*text++ - '0');
        if (n > (SIZE_MAX - d) / 10) return OVERFLOW;
        n = n * 10 + d;
    }
    *out = n;
    return OK;
}

Status save_tree(FILE *file, Node *root, const size_t count) {
    Node **stack;
    size_t used = 0;
    if (!file || count == SIZE_MAX || count > (SIZE_MAX / sizeof(Node *)) - 1) return BAD;
    stack = malloc((count + 1) * sizeof(Node *));
    if (!stack) return MEMORY;
    if (fwrite("BST1", 1, 4, file) != 4) { free(stack); return IO; }
    stack[used++] = root;
    while (used) {
        Node *node = stack[--used];
        unsigned char marker = node ? 1 : 0;
        if (fwrite(&marker, 1, 1, file) != 1) { free(stack); return IO; }
        if (node) {
            uint64_t n = node->count, len = strlen(node->word);
            if (fwrite(&n, sizeof(n), 1, file) != 1 || fwrite(&len, sizeof(len), 1, file) != 1 ||
                fwrite(node->word, 1, (size_t)len, file) != len) { free(stack); return IO; }
            if (used + 2 > count + 1) { free(stack); return BAD; }
            stack[used++] = node->right;
            stack[used++] = node->left;
        }
    }
    free(stack);
    return OK;
}

Status restore_tree(FILE *file, Node **root, size_t *unique) {
    char header[4];
    Slot *stack = NULL;
    size_t used = 0, cap = 0;
    Status s = OK;
    int extra;
    if (!file || !root || !unique) return BAD;
    *root = NULL; *unique = 0;
    if (fread(header, 1, 4, file) != 4 || memcmp(header, "BST1", 4)) return BAD;
    s = push_slot(&stack, &used, &cap, (Slot){root, NULL, NULL});
    while (used && !s) {
        Slot slot = stack[--used];
        unsigned char marker;
        uint64_t count, length;
        Node *node;
        if (fread(&marker, 1, 1, file) != 1) { s = BAD; break; }
        if (!marker) { *slot.place = NULL; continue; }
        if (marker != 1 || fread(&count, sizeof(count), 1, file) != 1 || fread(&length, sizeof(length), 1, file) != 1 ||
            !count || !length || length >= SIZE_MAX) { s = BAD; break; }
        node = calloc(1, sizeof(*node));
        if (!node) { s = MEMORY; break; }
        node->word = malloc((size_t)length + 1);
        if (!node->word) { free(node); s = MEMORY; break; }
        if (fread(node->word, 1, (size_t)length, file) != length || memchr(node->word, 0, (size_t)length)) {
            free(node->word); free(node); s = BAD; break;
        }
        node->word[length] = '\0';
        if (
            (slot.low && strcmp(node->word, slot.low) <= 0) || (slot.high && strcmp(node->word, slot.high) >= 0)) {
            free(node->word); free(node); s = BAD; break;
        }
        node->count = count;
        *slot.place = node;
        if (*unique == SIZE_MAX) { s = OVERFLOW; break; }
        (*unique)++;
        s = push_slot(&stack, &used, &cap, (Slot){&node->right, node->word, slot.high});
        if (!s) s = push_slot(&stack, &used, &cap, (Slot){&node->left, slot.low, node->word});
    }
    extra = fgetc(file);
    if (!s && (extra != EOF || ferror(file))) s = BAD;
    free(stack);
    if (s) { free_tree(*root); *root = NULL; *unique = 0; }
    return s;
}

Status ask(const char *label, char **line) {
    if (!label || !line) return BAD;
    printf("%s", label);
    fflush(stdout);
    return read_line(stdin, line);
}

Status same_file(FILE *in, const char *input_path, const char *output_path, int *same) {
    FILE *probe;
    Status s = OK;
    if (!in || !input_path || !output_path || !same) return BAD;
    *same = 0;
    probe = fopen(output_path, "rb");
    if (!probe) return OK;
#ifdef _WIN32
    (void)input_path;
    intptr_t first_handle = _get_osfhandle(_fileno(in));
    intptr_t second_handle = _get_osfhandle(_fileno(probe));
    BY_HANDLE_FILE_INFORMATION first_info = {0}, second_info = {0};
    if (first_handle == -1 || second_handle == -1 ||
        !GetFileInformationByHandle((HANDLE)first_handle, &first_info) ||
        !GetFileInformationByHandle((HANDLE)second_handle, &second_info)) s = IO;
    else *same = first_info.dwVolumeSerialNumber == second_info.dwVolumeSerialNumber &&
                 first_info.nFileIndexHigh == second_info.nFileIndexHigh &&
                 first_info.nFileIndexLow == second_info.nFileIndexLow;
#else
    struct stat first_info, second_info;
    if (stat(input_path, &first_info) || stat(output_path, &second_info)) s = IO;
    else *same = first_info.st_dev == second_info.st_dev && first_info.st_ino == second_info.st_ino;
#endif
    if (fclose(probe) == EOF) s = IO;
    return s;
}

Status same_input_file(const char *input_path, const char *output_path, int *same) {
    FILE *in;
    Status s;
    if (!input_path || !output_path || !same) return BAD;
    in = fopen(input_path, "rb");
    if (!in) return IO;
    s = same_file(in, input_path, output_path, same);
    if (fclose(in) == EOF) s = IO;
    return s;
}

int main(int argc, char **argv) {
    unsigned char separators[256] = {0};
    Node *root = NULL;
    size_t unique = 0;
    FILE *file;
    Status s;
    int i;
    if (argc < 3) { puts("Использование: task9.exe <текстовый файл> <разделитель1> [разделитель2 ...]"); return 1; }
    for (i = 2; i < argc; i++) {
        if (strlen(argv[i]) != 1 || !argv[i][0]) { puts("Каждый разделитель должен быть одним символом"); return 1; }
        separators[(unsigned char)argv[i][0]] = 1;
    }
    file = fopen(argv[1], "rb");
    if (!file) { printf("Не удалось открыть файл %s\n", argv[1]); return 1; }
    s = scan_words(file, separators, &root, &unique);
    if (fclose(file) != 0 && !s) s = IO;
    if (s) { printf("Ошибка чтения, код %d\n", s); free_tree(root); return 1; }
    printf("Уникальных слов: %zu\n", unique);
    for (;;) {
        char *line = NULL;
        size_t choice;
        puts("\n1 частота слова, 2 первые n слов, 3 длинное и короткое, 4 глубина, 5 сохранить, 6 восстановить, 0 выход");
        s = ask("Выбор: ", &line);
        if (s == END) break;
        if (s) { puts("Ошибка ввода"); break; }
        s = parse_count(line, &choice);
        free(line);
        if (s || choice > 6) { puts("Неверный пункт меню"); continue; }
        if (choice == 0) break;
        if (choice == 1) {
            const Node *found;
            s = ask("Слово: ", &line);
            if (s) break;
            found = find(root, line);
            printf("Частота: %llu\n", found ? found->count : 0ULL);
            free(line);
        } else if (choice >= 2 && choice <= 4) {
            Node **all = NULL;
            size_t depth;
            const Node *shortest, *longest;
            s = collect(root, &all, unique, &depth, &shortest, &longest);
            if (s) { puts("Не хватило памяти"); break; }
            if (choice == 2) {
                size_t n, j;
                s = ask("Количество слов n: ", &line);
                if (s) { free(all); break; }
                s = parse_count(line, &n);
                free(line);
                if (s || n == 0) puts("n должно быть натуральным числом");
                else {
                    if (n > unique) n = unique;
                    qsort(all, unique, sizeof(*all), by_frequency);
                    for (j = 0; j < n; j++) printf("%s: %llu\n", all[j]->word, all[j]->count);
                }
            } else if (choice == 3) {
                if (!unique) puts("Слов нет");
                else printf("Самое длинное: %s\nСамое короткое: %s\n", longest->word, shortest->word);
            } else printf("Глубина дерева: %zu\n", depth);
            free(all);
        } else {
            s = ask("Путь к файлу: ", &line);
            if (s) break;
            if (!*line) { puts("Неверный путь"); free(line); continue; }
            if (choice == 5) {
                int identical;
                s = same_input_file(argv[1], line, &identical);
                if (s || identical) {
                    puts(s ? "Не удалось проверить пути к файлам" : "Исходный текст и файл дерева совпадают");
                    free(line);
                    continue;
                }
            }
            file = fopen(line, choice == 5 ? "wb" : "rb");
            if (!file) puts("Не удалось открыть файл");
            else {
                if (choice == 5) s = save_tree(file, root, unique);
                else {
                    Node *copy = NULL;
                    size_t copied = 0;
                    s = restore_tree(file, &copy, &copied);
                    if (!s) { free_tree(root); root = copy; unique = copied; }
                }
                if (fclose(file) != 0 && !s) s = IO;
                puts(s == OK ? "Готово" : "Ошибка сохранения или восстановления");
            }
            free(line);
        }
    }
    free_tree(root);
    return 0;
}
