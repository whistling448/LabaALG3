// Запуск: .\task5.exe tests\students_friend.txt tests\trace.txt
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
        case ERR_ARG: fprintf(stderr, "Ошибка: неверные данные студента или команда. Нужны неотрицательный id, латинские имя и фамилия, группа и пять оценок от 0 до 255.\n"); break;
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
static Status split_words(char *text, char **const fields, const size_t expected) {
    if (!text || !fields) return ERR_ARG;
    size_t count = 0;
    while (*text) {
        while (isspace((unsigned char)*text)) ++text;
        if (!*text) break;
        if (count == expected) return ERR_ARG;
        fields[count++] = text;
        while (*text && !isspace((unsigned char)*text)) ++text;
        if (*text) *text++ = '\0';
    }
    return count == expected ? OK : ERR_ARG;
}
static Status parse_uint(const char *text, unsigned long long *const result) {
    if (!text || !result) return ERR_ARG;
    if (*text == '+') ++text;
    if (!*text) return ERR_ARG;
    unsigned long long value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return ERR_ARG;
        const unsigned digit = (unsigned)(*text-'0');
        if (value > (ULLONG_MAX-digit)/10) return ERR_RANGE;
        value = value*10+digit;
    }
    *result = value; return OK;
}
static Status copy_text(const char *const text, char **const result) {
    if (!text || !result) return ERR_ARG;
    *result = NULL;
    const size_t length = strlen(text);
    if (length == SIZE_MAX) return ERR_RANGE;
    char *copy = (char *)malloc(length+1);
    if (!copy) return ERR_MEM;
    memcpy(copy,text,length+1); *result = copy; return OK;
}
static Status latin_name(const char *text, const bool empty_allowed) {
    if (!text || (!*text && !empty_allowed)) return ERR_ARG;
    for (; *text; ++text)
        if (!((*text >= 'A' && *text <= 'Z') || (*text >= 'a' && *text <= 'z'))) return ERR_ARG;
    return OK;
}
typedef struct {
    unsigned long long id; char *name, *surname, *group; unsigned char *marks;
} Student;
typedef int (*StudentCompare)(const void *, const void *);
static void student_destroy(Student *const student) {
    if (!student) return;
    free(student->name); free(student->surname); free(student->group); free(student->marks);
    *student = (Student){0};
}
static Status parse_student(char *const line, Student *const result) {
    if (!line || !result) return ERR_ARG;
    *result = (Student){0}; char *f[9];
    Status st = split_words(line,f,9);
    if (st == OK) st = parse_uint(f[0],&result->id);
    if (st == OK) st = latin_name(f[1],false);
    if (st == OK) st = latin_name(f[2],false);
    if (st != OK) return st;
    result->marks = (unsigned char *)malloc(5*sizeof(unsigned char));
    if (!result->marks) return ERR_MEM;
    for (size_t i = 0; i < 5 && st == OK; ++i) {
        unsigned long long value;
        st = parse_uint(f[i+4],&value);
        if (st == OK && value > UCHAR_MAX) st = ERR_RANGE;
        if (st == OK) result->marks[i] = (unsigned char)value;
    }
    if (st == OK) st = copy_text(f[1],&result->name);
    if (st == OK) st = copy_text(f[2],&result->surname);
    if (st == OK) st = copy_text(f[3],&result->group);
    if (st != OK) student_destroy(result);
    return st;
}
static Status read_students(FILE *const in, Student **const result, size_t *const count) {
    if (!in || !result || !count) return ERR_ARG;
    *result = NULL; *count = 0; size_t capacity = 0, line_number = 0;
    Status st = OK;
    while (true) {
        char *line = NULL; st = read_line(in,&line);
        if (st == ERR_EOF) { st = OK; break; }
        ++line_number;
        Student value = {0};
        if (st == OK) st = parse_student(line,&value);
        free(line);
        if (st == OK && *count == capacity) {
            const size_t next_capacity = capacity ? capacity*2 : 16;
            if (capacity > SIZE_MAX/2/sizeof(Student) || next_capacity > SIZE_MAX/sizeof(Student)) st = ERR_RANGE;
            else {
                Student *next = (Student *)realloc(*result,next_capacity*sizeof(Student));
                if (!next) st = ERR_MEM;
                else { *result = next; capacity = next_capacity; }
            }
        }
        if (st == OK) (*result)[(*count)++] = value;
        else {
            student_destroy(&value);
            fprintf(stderr,"Неверная запись студента в строке %zu.\n",line_number); break;
        }
    }
    return st;
}
static int by_id(const void *const a, const void *const b) {
    const Student *x = (const Student *)a, *y = (const Student *)b;
    return (x->id > y->id)-(x->id < y->id);
}
static int by_surname(const void *const a, const void *const b) {
    return strcmp(((const Student *)a)->surname,((const Student *)b)->surname);
}
static int by_name(const void *const a, const void *const b) {
    return strcmp(((const Student *)a)->name,((const Student *)b)->name);
}
static int by_group(const void *const a, const void *const b) {
    return strcmp(((const Student *)a)->group,((const Student *)b)->group);
}
static Status choose_compare(const char *const key, StudentCompare *const result) {
    if (!key || !result) return ERR_ARG;
    if (!strcmp(key,"id")) *result = by_id;
    else if (!strcmp(key,"name")) *result = by_name;
    else if (!strcmp(key,"surname")) *result = by_surname;
    else if (!strcmp(key,"group")) *result = by_group;
    else return ERR_ARG;
    return OK;
}
static Status matches(const Student *const student, const char *const key,
                       const char *const value, bool *const result) {
    if (!student || !key || !value || !result) return ERR_ARG;
    if (!strcmp(key,"id")) {
        unsigned long long id; Status st = parse_uint(value,&id);
        if (st != OK) return st;
        *result = student->id == id;
    } else if (!strcmp(key,"name")) *result = !strcmp(student->name,value);
    else if (!strcmp(key,"surname")) *result = !strcmp(student->surname,value);
    else if (!strcmp(key,"group")) *result = !strcmp(student->group,value);
    else return ERR_ARG;
    return OK;
}
static Status average(const Student *const student, long double *const result) {
    if (!student || !student->marks || !result) return ERR_ARG;
    unsigned long long sum = 0;
    for (size_t i = 0; i < 5; ++i) sum += student->marks[i];
    *result = (long double)sum/5; return OK;
}
static Status class_average(const Student *const students, const size_t count, long double *const result) {
    if ((!students && count) || !count || !result) return ERR_ARG;
    long double sum = 0;
    for (size_t i = 0; i < count; ++i) {
        long double value; Status st = average(&students[i],&value);
        if (st != OK) return st;
        sum += value;
    }
    *result = sum/count; return OK;
}
static Status print_student(FILE *const out, const Student *const student) {
    if (!out || !student) return ERR_ARG;
    long double mean; Status st = average(student,&mean);
    if (st != OK) return st;
    return fprintf(out,"%llu %s %s %s средний балл=%.6Lf\n",student->id,student->name,student->surname,student->group,mean) < 0 ? ERR_IO : OK;
}
static Status trace_above(FILE *const out, const Student *const students, const size_t count, const long double eps) {
    if (!out || !isfinite(eps) || eps <= 0) return ERR_ARG;
    long double mean; Status st = class_average(students,count,&mean);
    if (st != OK) return st;
    for (size_t i = 0; i < count; ++i) {
        long double value; st = average(&students[i],&value);
        if (st != OK) return st;
        if (value-mean > eps && fprintf(out,"%s %s\n",students[i].surname,students[i].name) < 0) return ERR_IO;
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
    if (argc != 3) { fprintf(stderr,"Использование: task5.exe <файл студентов> <трассировочный файл>\n"); return 1; }
    FILE *in = fopen(argv[1],"r");
    if (!in) { report_error(ERR_IO); return 1; }
    bool identical;
    Status checked = same_file(in, argv[1], argv[2], &identical);
    if (checked != OK || identical) {
        if (checked != OK) report_error(checked);
        else fprintf(stderr, "Ошибка: входной и трассировочный пути ведут к одному файлу.\n");
        fclose(in);
        return 1;
    }
    Student *students = NULL; size_t count = 0;
    Status st = read_students(in,&students,&count);
    if (fclose(in) == EOF) st = ERR_IO;
    FILE *trace = NULL;
    if (st == OK) { trace = fopen(argv[2],"w"); if (!trace) st = ERR_IO; }
    bool failed = st != OK;
    if (st != OK) report_error(st);
    else puts("Команды: find ПОЛЕ ЗНАЧЕНИЕ | sort ПОЛЕ | trace ID | above | list | help | quit; ПОЛЕ: id/name/surname/group");
    while (trace) {
        char *line = NULL; st = read_line(stdin,&line);
        if (st == ERR_EOF) break;
        if (st != OK) { report_error(st); failed = true; break; }
        if (!strcmp(line,"quit")) { free(line); break; }
        if (!strcmp(line,"list")) {
            for (size_t i = 0; i < count && st == OK; ++i) st = print_student(stdout,&students[i]);
        } else if (!strncmp(line,"sort ",5)) {
            StudentCompare compare; st = choose_compare(line+5,&compare);
            if (st == OK && count > 1) qsort(students,count,sizeof(Student),compare);
        } else if (!strncmp(line,"find ",5)) {
            char *fields[2]; st = split_words(line+5,fields,2); size_t found = 0;
            StudentCompare check;
            if (st == OK) st = choose_compare(fields[0],&check);
            if (st == OK && !strcmp(fields[0],"id")) { unsigned long long id; st = parse_uint(fields[1],&id); }
            for (size_t i = 0; i < count && st == OK; ++i) {
                bool match; st = matches(&students[i],fields[0],fields[1],&match);
                if (st == OK && match) { ++found; st = print_student(stdout,&students[i]); }
            }
            if (st == OK && !found) st = ERR_NOT_FOUND;
        } else if (!strncmp(line,"trace ",6)) {
            unsigned long long id; st = parse_uint(line+6,&id); size_t found = 0;
            for (size_t i = 0; i < count && st == OK; ++i) if (students[i].id == id) {
                ++found; st = print_student(trace,&students[i]);
            }
            if (st == OK && !found) st = ERR_NOT_FOUND;
        } else if (!strcmp(line,"above")) st = trace_above(trace,students,count,1e-12L);
        else if (!strcmp(line,"help")) puts("Во входном файле 9 полей: id, имя, фамилия, группа и пять оценок от 0 до 255. Команды trace и above записывают данные в трассировочный файл.");
        else st = ERR_ARG;
        free(line);
        if (st != OK) { report_error(st); failed = true; }
    }
    if (trace && fclose(trace) == EOF) { report_error(ERR_IO); failed = true; }
    for (size_t i = 0; i < count; ++i) student_destroy(&students[i]);
    free(students);
    const int output_error = ferror(stdout);
    if (fclose(stdout) == EOF || output_error) { report_error(ERR_IO); failed = true; }
    return failed;
}
