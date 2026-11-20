// Запуск: .\task10.exe tests\trees.txt tests\trees_out.txt
#if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
#define __USE_MINGW_ANSI_STDIO 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/stat.h>
#endif
typedef enum { OK, ERR_ARG, ERR_RANGE, ERR_MEM, ERR_IO, ERR_EOF, ERR_NOT_FOUND, ERR_DOMAIN } Status;
static void report_error(const Status st) {
    switch (st) {
        case ERR_ARG: fprintf(stderr, "Ошибка: неверное скобочное выражение. Нужен один корень с разделёнными запятыми детьми внутри согласованных скобок.\n"); break;
        case ERR_RANGE: fprintf(stderr, "Ошибка: значение или размер памяти вне допустимого диапазона.\n"); break;
        case ERR_MEM: fprintf(stderr, "Ошибка: не удалось выделить память.\n"); break;
        case ERR_IO: fprintf(stderr, "Ошибка открытия, чтения, записи или закрытия потока.\n"); break;
        case ERR_EOF: fprintf(stderr, "Ошибка: ввод закончился раньше ожидаемого.\n"); break;
        case ERR_NOT_FOUND: fprintf(stderr, "Ошибка: запись не найдена.\n"); break;
        case ERR_DOMAIN: fprintf(stderr, "Ошибка: операция не определена для этих данных.\n"); break;
        case OK: break;
    }
}
static Status read_line(FILE *const in, char **const result) {
    if (!in || !result) return ERR_ARG;
    *result = NULL;
    size_t used = 0, capacity = 128;
    char *text = (char *)malloc(capacity);
    if (!text) return ERR_MEM;
    int c;
    while ((c = fgetc(in)) != EOF && c != '\n') {
        if (c == 0) { free(text); return ERR_ARG; }
        if (used == capacity - 1) {
            if (capacity > SIZE_MAX / 2) { free(text); return ERR_RANGE; }
            char *next = (char *)realloc(text, capacity * 2);
            if (!next) { free(text); return ERR_MEM; }
            text = next; capacity *= 2;
        }
        text[used++] = (char)c;
    }
    if (ferror(in)) { free(text); return ERR_IO; }
    if (c == EOF && !used) { free(text); return ERR_EOF; }
    if (used && text[used-1] == '\r') --used;
    text[used] = '\0'; *result = text; return OK;
}
typedef struct TreeNode {
    char *label; bool has_group; struct TreeNode *child, *last_child, *sibling, *parent;
} TreeNode;
static void free_tree(TreeNode *node) {
    while (node) {
        if (node->child) {
            TreeNode *child = node->child; node->child = NULL; node = child;
        } else {
            TreeNode *next = node->sibling ? node->sibling : node->parent;
            free(node->label); free(node); node = next;
        }
    }
}
static Status parse_tree(const char *text, TreeNode **const result) {
    if (!text || !result) return ERR_ARG;
    *result = NULL; TreeNode *parent = NULL, *last = NULL;
    bool need_label = true; Status st = OK;
    while (true) {
        while (isspace((unsigned char)*text)) ++text;
        if (!*text) break;
        if (need_label) {
            const char *start = text;
            while (*text && *text != '(' && *text != ')' && *text != ',') ++text;
            const char *end = text;
            while (end > start && isspace((unsigned char)end[-1])) --end;
            const size_t length = (size_t)(end-start);
            if (!length || (!parent && *result)) { st = ERR_ARG; break; }
            TreeNode *created = (TreeNode *)calloc(1,sizeof(*created));
            if (!created) { st = ERR_MEM; break; }
            created->label = (char *)malloc(length+1);
            if (!created->label) { free(created); st = ERR_MEM; break; }
            memcpy(created->label,start,length); created->label[length] = '\0'; created->parent = parent;
            if (parent) {
                if (parent->last_child) parent->last_child->sibling = created;
                else parent->child = created;
                parent->last_child = created;
            } else *result = created;
            last = created; need_label = false;
            continue;
        }
        if (*text == '(' && !need_label && last && !last->has_group) {
            last->has_group = true;
            parent = last; need_label = true;
        } else if (*text == ')' && parent) {
            last = parent; parent = parent->parent; need_label = false;
        } else if (*text == ',' && parent && !need_label) need_label = true;
        else { st = ERR_ARG; break; }
        ++text;
    }
    if (st == OK && (!*result || parent || need_label)) st = ERR_ARG;
    if (st != OK) { free_tree(*result); *result = NULL; }
    return st;
}
static Status print_tree(FILE *const out, const TreeNode *node) {
    if (!out || !node) return ERR_ARG;
    size_t depth = 0;
    while (node) {
        for (size_t i = 0; i < depth; ++i) if (fputs("  ",out) == EOF) return ERR_IO;
        if (fprintf(out,"%s\n",node->label) < 0) return ERR_IO;
        if (node->child) { node = node->child; ++depth; }
        else {
            while (node && !node->sibling) { node = node->parent; if (depth) --depth; }
            if (node) node = node->sibling;
        }
    }
    return OK;
}
static Status same_file(FILE *in, const char *input_path, const char *output_path, bool *same) {
    if (!in || !input_path || !output_path || !same) return ERR_ARG;
    *same = false;
    FILE *probe = fopen(output_path, "rb");
    if (!probe) return OK;
    Status st = OK;
#ifdef _WIN32
    (void)input_path;
    intptr_t first_handle = _get_osfhandle(_fileno(in));
    intptr_t second_handle = _get_osfhandle(_fileno(probe));
    BY_HANDLE_FILE_INFORMATION first_info = {0}, second_info = {0};
    if (first_handle == -1 || second_handle == -1 ||
        !GetFileInformationByHandle((HANDLE)first_handle, &first_info) ||
        !GetFileInformationByHandle((HANDLE)second_handle, &second_info)) st = ERR_IO;
    else *same = first_info.dwVolumeSerialNumber == second_info.dwVolumeSerialNumber &&
                 first_info.nFileIndexHigh == second_info.nFileIndexHigh &&
                 first_info.nFileIndexLow == second_info.nFileIndexLow;
#else
    struct stat first_info, second_info;
    if (stat(input_path, &first_info) || stat(output_path, &second_info)) st = ERR_IO;
    else *same = first_info.st_dev == second_info.st_dev && first_info.st_ino == second_info.st_ino;
#endif
    if (fclose(probe) == EOF) st = ERR_IO;
    return st;
}
int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr,"Использование: task10.exe <входной файл> <выходной файл>\n"); return 1; }
    FILE *in = fopen(argv[1],"r");
    if (!in) { report_error(ERR_IO); return 1; }
    bool identical;
    Status checked = same_file(in, argv[1], argv[2], &identical);
    if (checked != OK || identical) {
        if (checked != OK) report_error(checked);
        else fprintf(stderr, "Ошибка: входной и выходной пути ведут к одному файлу.\n");
        fclose(in);
        return 1;
    }
    FILE *temporary = tmpfile();
    if (!temporary) { fclose(in); report_error(ERR_IO); return 1; }
    Status st = OK; size_t number = 0;
    while (st == OK) {
        char *line = NULL; st = read_line(in,&line);
        if (st == ERR_EOF) { st = OK; break; }
        TreeNode *tree = NULL; ++number;
        if (st == OK) st = parse_tree(line,&tree);
        if (st == OK && fprintf(temporary,"Выражение %zu\n",number) < 0) st = ERR_IO;
        if (st == OK) st = print_tree(temporary,tree);
        if (st == OK && fputc('\n',temporary) == EOF) st = ERR_IO;
        free_tree(tree); free(line);
        if (st != OK) fprintf(stderr,"Ошибка в выражении или при выводе в строке %zu.\n",number);
    }
    if (fclose(in) == EOF) st = ERR_IO;
    if (st == OK && fseek(temporary,0,SEEK_SET)) st = ERR_IO;
    FILE *out = NULL;
    if (st == OK) { out = fopen(argv[2],"w"); if (!out) st = ERR_IO; }
    unsigned char buffer[1024]; size_t n;
    while (st == OK && (n = fread(buffer,1,sizeof(buffer),temporary)) != 0)
        if (fwrite(buffer,1,n,out) != n) st = ERR_IO;
    if (ferror(temporary)) st = ERR_IO;
    if (out && fclose(out) == EOF) st = ERR_IO;
    if (fclose(temporary) == EOF) st = ERR_IO;
    if (st != OK) report_error(st);
    return st != OK;
}
