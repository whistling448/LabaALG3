// Запуск: .\task7.exe tests\livers_friend.txt
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
typedef enum { OK, ERR_ARG, ERR_RANGE, ERR_MEM, ERR_IO, ERR_EOF, ERR_NOT_FOUND, ERR_DOMAIN } Status;
static void report_error(const Status st) {
    switch (st) {
        case ERR_ARG: fprintf(stderr, "Ошибка: неверные данные жителя или команда. Проверь латинские имя и фамилию, дату, пол M/W, неотрицательный доход и номер записи.\n"); break;
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
static Status split_pipe(char *text, char **const fields, const size_t expected) {
    if (!text || !fields || !expected) return ERR_ARG;
    size_t count = 0;
    fields[count++] = text;
    for (; *text; ++text) if (*text == '|') {
        if (count == expected) return ERR_ARG;
        *text = '\0'; fields[count++] = text+1;
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
static Status parse_real(const char *const text, long double *const result) {
    if (!text || !result || !*text) return ERR_ARG;
    const char *p = text;
    if (*p == '+' || *p == '-') ++p;
    bool digit = false, dot = false, nonzero = false;
    for (; *p && *p != 'e' && *p != 'E'; ++p) {
        if (*p == '.' && !dot) { dot = true; continue; }
        if (*p < '0' || *p > '9') return ERR_ARG;
        digit = true; if (*p != '0') nonzero = true;
    }
    if (!digit) return ERR_ARG;
    if (*p) {
        ++p; if (*p == '+' || *p == '-') ++p;
        if (!*p) return ERR_ARG;
        for (; *p; ++p) if (*p < '0' || *p > '9') return ERR_ARG;
    }
    char *end;
    const long double value = strtold(text, &end);
    if (*end || isnan(value) || fabsl(value) == HUGE_VALL || (value == 0 && nonzero)) return ERR_RANGE;
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
typedef struct { int day, month, year, hour, minute, second; long long key; } Date;
static Status make_date(const int day, const int month, const int year,
                        const int hour, const int minute, const int second, Date *const result) {
    if (!result || year < 1 || year > 9999 || month < 1 || month > 12 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return ERR_ARG;
    const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = year%4 == 0 && (year%100 != 0 || year%400 == 0);
    const int maximum = days[month-1] + (month == 2 && leap);
    if (day < 1 || day > maximum) return ERR_ARG;
    long long years = year-1;
    long long total = years*365 + years/4 - years/100 + years/400;
    for (int m = 1; m < month; ++m) total += days[m-1] + (m == 2 && leap);
    total += day-1;
    *result = (Date){day,month,year,hour,minute,second,((total*24+hour)*60+minute)*60+second};
    return OK;
}
static Status parse_date(const char *const text, const char separator, Date *const result) {
    if (!text || !result || strlen(text) != 19 || text[2] != separator || text[5] != separator ||
        text[10] != ' ' || text[13] != ':' || text[16] != ':') return ERR_ARG;
    for (size_t i = 0; i < 19; ++i)
        if (i != 2 && i != 5 && i != 10 && i != 13 && i != 16 && !isdigit((unsigned char)text[i])) return ERR_ARG;
    return make_date((text[0]-'0')*10+text[1]-'0',(text[3]-'0')*10+text[4]-'0',
        (text[6]-'0')*1000+(text[7]-'0')*100+(text[8]-'0')*10+text[9]-'0',
        (text[11]-'0')*10+text[12]-'0',(text[14]-'0')*10+text[15]-'0',
        (text[17]-'0')*10+text[18]-'0',result);
}
typedef struct Liver {
    char *surname, *name, *patronymic; Date birth; char gender;
    long double income; struct Liver *next;
} Liver;
typedef struct History { Liver *snapshot; struct History *next; } History;
static void free_livers(Liver *list) {
    while (list) {
        Liver *next = list->next;
        free(list->surname); free(list->name); free(list->patronymic); free(list); list = next;
    }
}
static Status insert_liver(Liver **const list, Liver *const value) {
    if (!list || !value || value->next) return ERR_ARG;
    Liver **position = list;
    while (*position && (*position)->birth.key >= value->birth.key) position = &(*position)->next;
    value->next = *position; *position = value; return OK;
}
static Status parse_liver(char *const text, Liver **const result) {
    if (!text || !result) return ERR_ARG;
    *result = NULL; char *f[6]; Status st = split_pipe(text,f,6);
    if (st == OK) st = latin_name(f[0],false);
    if (st == OK) st = latin_name(f[1],false);
    if (st == OK) st = latin_name(f[2],true);
    if (st == OK && (strlen(f[3]) != 10 || strlen(f[4]) != 1 || !strchr("MW",f[4][0]))) st = ERR_ARG;
    if (st != OK) return st;
    char date[20]; snprintf(date,sizeof(date),"%s 00:00:00",f[3]);
    Liver *value = (Liver *)calloc(1,sizeof(*value));
    if (!value) return ERR_MEM;
    st = parse_date(date,'.',&value->birth);
    if (st == OK) st = parse_real(f[5],&value->income);
    if (st == OK && value->income < 0) st = ERR_ARG;
    value->gender = f[4][0];
    if (st == OK) st = copy_text(f[0],&value->surname);
    if (st == OK) st = copy_text(f[1],&value->name);
    if (st == OK) st = copy_text(f[2],&value->patronymic);
    if (st != OK) { free_livers(value); return st; }
    *result = value; return OK;
}
static Status copy_livers(const Liver *source, Liver **const result) {
    if (!result) return ERR_ARG;
    *result = NULL; Liver **tail = result; Status st = OK;
    for (; source && st == OK; source = source->next) {
        Liver *value = (Liver *)calloc(1,sizeof(*value));
        if (!value) { st = ERR_MEM; break; }
        value->birth = source->birth; value->gender = source->gender; value->income = source->income;
        st = copy_text(source->surname,&value->surname);
        if (st == OK) st = copy_text(source->name,&value->name);
        if (st == OK) st = copy_text(source->patronymic,&value->patronymic);
        if (st != OK) { free_livers(value); break; }
        *tail = value; tail = &value->next;
    }
    if (st != OK) { free_livers(*result); *result = NULL; }
    return st;
}
static Status liver_position(Liver **const list, const unsigned long long index, Liver ***const result) {
    if (!list || !result || !index) return ERR_ARG;
    Liver **position = list;
    for (unsigned long long i = 1; i < index && *position; ++i) position = &(*position)->next;
    if (!*position) return ERR_NOT_FOUND;
    *result = position; return OK;
}
static Status change_liver(Liver **const list, History **const history, size_t *const changes,
                            const char mode, const unsigned long long index, Liver *const replacement) {
    if (!list || !history || !changes || !mode || !strchr("ade",mode) || ((mode=='a'||mode=='e') && (!replacement || replacement->next))) return ERR_ARG;
    if (*changes == SIZE_MAX) return ERR_RANGE;
    Liver **position = NULL;
    Status st = mode == 'a' ? OK : liver_position(list,index,&position);
    if (st != OK) return st;
    History *saved = (History *)calloc(1,sizeof(*saved));
    if (!saved) return ERR_MEM;
    st = copy_livers(*list,&saved->snapshot);
    if (st != OK) { free(saved); return st; }
    if (position) {
        Liver *removed = *position; *position = removed->next; removed->next = NULL; free_livers(removed);
    }
    if (mode != 'd') insert_liver(list,replacement);
    saved->next = *history; *history = saved; ++*changes; return OK;
}
static void free_history(History *history) {
    while (history) { History *next = history->next; free_livers(history->snapshot); free(history); history = next; }
}
static Status undo(Liver **const list, History **const history, size_t *const changes, size_t *const undone) {
    if (!list || !history || !changes || !undone) return ERR_ARG;
    *undone = *changes/2;
    if (!*undone) return OK;
    for (size_t i = 0; i < *undone; ++i) {
        if (!*history) return ERR_ARG;
        History *saved = *history; *history = saved->next;
        free_livers(*list); *list = saved->snapshot; free(saved);
    }
    free_history(*history); *history = NULL; *changes = 0; return OK;
}
static Status liver_matches(const Liver *const value, const char *const key, const char *const text,
                             const long double eps, bool *const result) {
    if (!value || !key || !text || !result || !isfinite(eps) || eps <= 0) return ERR_ARG;
    if (!strcmp(key,"surname")) *result = !strcmp(value->surname,text);
    else if (!strcmp(key,"name")) *result = !strcmp(value->name,text);
    else if (!strcmp(key,"patronymic")) *result = !strcmp(value->patronymic,text);
    else if (!strcmp(key,"gender")) {
        if (strlen(text) != 1 || !strchr("MW",text[0])) return ERR_ARG;
        *result = value->gender == text[0];
    } else if (!strcmp(key,"birth")) {
        if (strlen(text) != 10) return ERR_ARG;
        char buffer[20]; snprintf(buffer,sizeof(buffer),"%s 00:00:00",text); Date date;
        Status st = parse_date(buffer,'.',&date); if (st != OK) return st;
        *result = value->birth.key == date.key;
    } else if (!strcmp(key,"income")) {
        long double income; Status st = parse_real(text,&income);
        if (st != OK || income < 0) return ERR_ARG;
        *result = fabsl(value->income-income) <= eps;
    } else return ERR_ARG;
    return OK;
}
static Status print_liver(FILE *const out, const Liver *const value) {
    if (!out || !value) return ERR_ARG;
    return fprintf(out,"%s|%s|%s|%02d.%02d.%04d|%c|%.*Lg\n",value->surname,value->name,value->patronymic,
        value->birth.day,value->birth.month,value->birth.year,value->gender,DECIMAL_DIG,value->income) < 0 ? ERR_IO : OK;
}
static Status read_livers(FILE *const in, Liver **const list) {
    if (!in || !list) return ERR_ARG;
    while (true) {
        char *line = NULL; Status st = read_line(in,&line);
        if (st == ERR_EOF) return OK;
        Liver *value = NULL;
        if (st == OK) st = parse_liver(line,&value);
        free(line);
        if (st != OK) return st;
        insert_liver(list,value);
    }
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr,"Использование: task7.exe <файл жителей>\n"); return 1; }
    FILE *in = fopen(argv[1],"r");
    if (!in) { report_error(ERR_IO); return 1; }
    Liver *list = NULL; History *history = NULL; size_t changes = 0;
    Status st = read_livers(in,&list); if (fclose(in) == EOF) st = ERR_IO;
    if (st != OK) { report_error(st); free_livers(list); return 1; }
    puts("Команды: list | find ПОЛЕ ЗНАЧЕНИЕ | findall УСЛОВИЯ | add ЗАПИСЬ | edit НОМЕР ЗАПИСЬ | delete НОМЕР | undo | save | help | quit");
    bool failed = false;
    while (true) {
        char *line = NULL; st = read_line(stdin,&line);
        if (st == ERR_EOF) break;
        if (st != OK) { report_error(st); failed = true; break; }
        if (!strcmp(line,"quit")) { free(line); break; }
        if (!strcmp(line,"list")) {
            size_t number = 0;
            for (const Liver *v = list; v && st == OK; v = v->next) {
                if (printf("%zu ",++number) < 0) st = ERR_IO;
                if (st == OK) st = print_liver(stdout,v);
            }
        } else if (!strncmp(line,"add ",4) || !strncmp(line,"edit ",5) || !strncmp(line,"delete ",7)) {
            const char mode = line[0] == 'a' ? 'a' : (line[0] == 'e' ? 'e' : 'd');
            char *record = line+(mode=='a' ? 4 : (mode=='e' ? 5 : 7)); unsigned long long index = 0;
            if (mode == 'e') {
                char *space = strchr(record,' ');
                if (!space) st = ERR_ARG;
                else { *space = '\0'; st = parse_uint(record,&index); record = space+1; }
            } else if (mode == 'd') st = parse_uint(record,&index);
            Liver *replacement = NULL;
            if (st == OK && mode != 'd') st = parse_liver(record,&replacement);
            if (st == OK) st = change_liver(&list,&history,&changes,mode,index,replacement);
            if (st != OK) free_livers(replacement);
        } else if (!strcmp(line,"undo")) {
            size_t undone = 0; st = undo(&list,&history,&changes,&undone);
            if (st == OK && printf("Отменено изменений: %zu\n",undone) < 0) st = ERR_IO;
        } else if (!strncmp(line,"findall ",8)) {
            char *criteria[6]; st = split_pipe(line+8,criteria,6);
            const char *keys[] = {"surname","name","patronymic","birth","gender","income"};
            Liver sample = {"","","",{0},'M',0,NULL};
            for (size_t i = 0; i < 6 && st == OK; ++i) if (strcmp(criteria[i],"*")) {
                bool match; st = liver_matches(&sample,keys[i],criteria[i],1e-12L,&match);
            }
            size_t found = 0;
            for (const Liver *v = list; v && st == OK; v = v->next) {
                bool all = true;
                for (size_t i = 0; i < 6 && st == OK && all; ++i) if (strcmp(criteria[i],"*"))
                    st = liver_matches(v,keys[i],criteria[i],1e-12L,&all);
                if (st == OK && all) { ++found; st = print_liver(stdout,v); }
            }
            if (st == OK && !found) st = ERR_NOT_FOUND;
        } else if (!strncmp(line,"find ",5)) {
            char *key = line+5, *space = strchr(key,' ');
            if (!space) st = ERR_ARG;
            else {
                *space = '\0'; size_t found = 0;
                const char *keys[] = {"surname","name","patronymic","birth","gender","income"}; bool valid = false;
                for (size_t i = 0; i < 6; ++i) if (!strcmp(key,keys[i])) valid = true;
                if (!valid) st = ERR_ARG;
                for (const Liver *v = list; v && st == OK; v = v->next) {
                    bool match; st = liver_matches(v,key,space+1,1e-12L,&match);
                    if (st == OK && match) { ++found; st = print_liver(stdout,v); }
                }
                if (st == OK && !found) st = ERR_NOT_FOUND;
            }
        } else if (!strcmp(line,"save")) {
            puts("Путь к выходному файлу:"); char *path = NULL; st = read_line(stdin,&path);
            FILE *out = NULL;
            if (st == OK) { out = fopen(path,"w"); if (!out) st = ERR_IO; }
            for (const Liver *v = list; v && st == OK; v = v->next) st = print_liver(out,v);
            if (out && fclose(out) == EOF) st = ERR_IO;
            free(path);
        } else if (!strcmp(line,"help")) puts("Запись: фамилия|имя|отчество|дд.мм.гггг|M/W|доход. Отчество может быть пустым. В findall шесть полей через |, символ * означает любое значение. Список упорядочен от младших к старшим; undo отменяет floor(N/2) последних успешных изменений.");
        else st = ERR_ARG;
        free(line);
        if (st != OK) { report_error(st); failed = true; }
    }
    free_livers(list); free_history(history);
    const int output_error = ferror(stdout);
    if (fclose(stdout) == EOF || output_error) { report_error(ERR_IO); failed = true; }
    return failed;
}
