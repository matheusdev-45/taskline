#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "cJSON.h"

#define DATA_FILE "tasks.json"
#define BUF 1024

// ANSI colors
#define C_RESET  "\x1b[0m"
#define C_RED    "\x1b[31m"
#define C_GREEN  "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE   "\x1b[34m"

// ---------- Helpers ----------
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static int write_file(const char *path, const char *data) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(data, 1, strlen(data), f);
    fclose(f);
    return 1;
}

static void current_timestamp_iso(char *out, size_t n) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(out, n, "%Y-%m-%d %H:%M", &tm);
}

static void current_timestamp_D(char *out, size_t n) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(out, n, "[%d/%m %H:%M]", &tm); // format D requested earlier
}

// load or create root JSON object
static cJSON *load_data(void) {
    char *s = read_file(DATA_FILE);
    cJSON *root = NULL;
    if (!s) {
        // create empty structure: { "lists": {} }
        root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "lists", cJSON_CreateObject());
        char *out = cJSON_Print(root);
        write_file(DATA_FILE, out);
        free(out);
        return root;
    }
    root = cJSON_Parse(s);
    free(s);
    if (!root) {
        // corrupted -> backup and recreate
        char bak[256];
        snprintf(bak, sizeof(bak), "%s.bak", DATA_FILE);
        rename(DATA_FILE, bak);
        root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "lists", cJSON_CreateObject());
        char *out = cJSON_Print(root);
        write_file(DATA_FILE, out);
        free(out);
    } else {
        // ensure "lists" exists
        if (!cJSON_GetObjectItem(root, "lists")) {
            cJSON_AddItemToObject(root, "lists", cJSON_CreateObject());
        }
    }
    return root;
}

// save root to file (pretty)
static int save_data(cJSON *root) {
    char *out = cJSON_Print(root);
    if (!out) return 0;
    int ok = write_file(DATA_FILE, out);
    free(out);
    return ok;
}

// join argv tokens from start..argc-1 into a single string separated by spaces
static char *join_args(int argc, char *argv[], int start) {
    if (start >= argc) return NULL;
    size_t total = 0;
    for (int i = start; i < argc; ++i) total += strlen(argv[i]) + 1;
    char *res = malloc(total + 1);
    if (!res) return NULL;
    res[0] = '\0';
    for (int i = start; i < argc; ++i) {
        strcat(res, argv[i]);
        if (i + 1 < argc) strcat(res, " ");
    }
    return res;
}

// ---------- Commands ----------

static void cmd_help(void) {
    printf(C_BLUE "Taskline 1.0 - Simple CLI To-Do Manager\n" C_RESET);
    printf("Usage: taskline <command> [arguments]\n\n");
    printf("Commands:\n");
    printf("  new <list>            Create a new list (list name: single token, no spaces)\n");
    printf("  add <list> <task...>  Add a task (everything after list is task, no quotes needed)\n");
    printf("  ls <list>             Show tasks in a list\n");
    printf("  lists                 Show all lists\n");
    printf("  rm <list> <num>       Remove task number (num is integer)\n");
    printf("  del <list>            Delete an entire list\n");
    printf("  help                  Show this help menu\n");
    printf("  about [en]            About (default pt, pass 'en' for English)\n\n");
    printf("Notes:\n");
    printf(" - List names cannot contain spaces in this version (use underscores if needed).\n");
    printf(" - Task text may contain spaces and is formed by joining remaining arguments.\n");
}

static void cmd_about(int english) {
    if (english) {
        printf(C_GREEN "Taskline 1.0 - About\n" C_RESET);
        printf("Taskline is a simple, local-first CLI task manager created by Matheus Campos.\n");
        printf("It stores data in ./tasks.json and is free & open-source.\n");
        printf("Features: lists, tasks, remove, colors, JSON storage.\n");
    } else {
        printf(C_GREEN "Taskline 1.0 - Sobre\n" C_RESET);
        printf("Taskline é um gerenciador de tarefas simples para terminal, criado por Matheus Campos.\n");
        printf("Armazenamento local em ./tasks.json. Livre e Open Source.\n");
        printf("Funcionalidades: listas, adicionar, remover, cores, armazenamento em JSON.\n");
    }
}

// create list (empty array)
static void cmd_new(int argc, char *argv[]) {
    if (argc < 3) {
        printf(C_YELLOW "[WARN] Usage: taskline new <list>\n" C_RESET);
        return;
    }
    const char *list = argv[2];

    cJSON *root = load_data();
    cJSON *lists = cJSON_GetObjectItem(root, "lists");
    if (cJSON_GetObjectItem(lists, list)) {
        printf(C_YELLOW "[WARN] List '%s' already exists.\n" C_RESET, list);
        cJSON_Delete(root);
        return;
    }
    cJSON_AddItemToObject(lists, list, cJSON_CreateArray());
    // set metadata? optional, we add created_at as object? keep simple

    if (save_data(root)) printf(C_GREEN "[OK] List '%s' created.\n" C_RESET, list);
    else printf(C_RED "[ERROR] Failed to save data.\n" C_RESET);
    cJSON_Delete(root);
}

// add task: list is argv[2], task = join argv[3..]
static void cmd_add(int argc, char *argv[]) {
    if (argc < 4) {
        printf(C_YELLOW "[WARN] Usage: taskline add <list> <task...>\n" C_RESET);
        return;
    }
    const char *list = argv[2];
    char *task_text = join_args(argc, argv, 3);
    if (!task_text) { printf(C_RED "[ERROR] Memory.\n" C_RESET); return; }

    cJSON *root = load_data();
    cJSON *lists = cJSON_GetObjectItem(root, "lists");
    cJSON *arr = cJSON_GetObjectItem(lists, list);
    if (!arr) {
        // create list automatically
        arr = cJSON_CreateArray();
        cJSON_AddItemToObject(lists, list, arr);
        printf(C_YELLOW "[INFO] List '%s' did not exist; created automatically.\n" C_RESET, list);
    }

    // Build task object: { "text": "...", "created": "...", "done": false }
    char ts_iso[64], ts_D[64];
    current_timestamp_iso(ts_iso, sizeof(ts_iso)); // ISO
    current_timestamp_D(ts_D, sizeof(ts_D));       // [dd/mm HH:MM]

    cJSON *task = cJSON_CreateObject();
    cJSON_AddStringToObject(task, "text", task_text);
    cJSON_AddStringToObject(task, "created", ts_iso);
    cJSON_AddStringToObject(task, "created_short", ts_D);
    cJSON_AddBoolToObject(task, "done", 0);

    cJSON_AddItemToArray(arr, task);

    if (save_data(root)) printf(C_GREEN "[OK] Task added to '%s'.\n" C_RESET, list);
    else printf(C_RED "[ERROR] Failed to save data.\n" C_RESET);

    free(task_text);
    cJSON_Delete(root);
}

// list tasks for a list
static void cmd_ls(int argc, char *argv[]) {
    if (argc < 3) {
        printf(C_YELLOW "[WARN] Usage: taskline ls <list>\n" C_RESET);
        return;
    }
    const char *list = argv[2];

    cJSON *root = load_data();
    cJSON *lists = cJSON_GetObjectItem(root, "lists");
    cJSON *arr = cJSON_GetObjectItem(lists, list);
    if (!arr || !cJSON_IsArray(arr)) {
        printf(C_RED "[ERROR] List '%s' not found.\n" C_RESET, list);
        cJSON_Delete(root);
        return;
    }
    printf(C_BLUE "=== %s ===\n" C_RESET, list);

    int idx = 1;
    cJSON *it;
    cJSON_ArrayForEach(it, arr) {
        cJSON *txt = cJSON_GetObjectItem(it, "text");
        cJSON *created_short = cJSON_GetObjectItem(it, "created_short");
        cJSON *done = cJSON_GetObjectItem(it, "done");
        const char *text = txt ? txt->valuestring : "(no text)";
        const char *created = created_short ? created_short->valuestring : "";
        int donev = (done && cJSON_IsBool(done) && done->valueint);
        if (donev)
            printf("%d. %s %s %s\n", idx++, C_GREEN "[DONE]" C_RESET, created, text);
        else
            printf("%d. %s %s\n", idx++, created, text);
    }

    cJSON_Delete(root);
}

// list all lists
static void cmd_lists(void) {
    cJSON *root = load_data();
    cJSON *lists = cJSON_GetObjectItem(root, "lists");
    printf(C_BLUE "Existing lists:\n" C_RESET);
    cJSON *child = NULL;
    child = lists->child;
    if (!child) {
        printf("  (no lists)\n");
    } else {
        while (child) {
            printf("  - %s\n", child->string);
            child = child->next;
        }
    }
    cJSON_Delete(root);
}

// remove task by index (1-based)
static void cmd_rm(int argc, char *argv[]) {
    if (argc < 4) {
        printf(C_YELLOW "[WARN] Usage: taskline rm <list> <num>\n" C_RESET);
        return;
    }
    const char *list = argv[2];
    int num = atoi(argv[3]);
    if (num <= 0) { printf(C_YELLOW "[WARN] Invalid number.\n" C_RESET); return; }

    cJSON *root = load_data();
    cJSON *lists = cJSON_GetObjectItem(root, "lists");
    cJSON *arr = cJSON_GetObjectItem(lists, list);
    if (!arr || !cJSON_IsArray(arr)) {
        printf(C_RED "[ERROR] List '%s' not found.\n" C_RESET, list);
        cJSON_Delete(root);
        return;
    }
    int size = cJSON_GetArraySize(arr);
    if (num > size) {
        printf(C_YELLOW "[WARN] Task number out of range (1..%d)\n" C_RESET, size);
        cJSON_Delete(root);
        return;
    }
    // remove (index = num-1)
    cJSON_DeleteItemFromArray(arr, num - 1);
    if (save_data(root)) printf(C_GREEN "[OK] Removed task %d from '%s'.\n" C_RESET, num, list);
    else printf(C_RED "[ERROR] Failed to save data.\n" C_RESET);
    cJSON_Delete(root);
}

// delete entire list
static void cmd_del(int argc, char *argv[]) {
    if (argc < 3) {
        printf(C_YELLOW "[WARN] Usage: taskline del <list>\n" C_RESET);
        return;
    }
    const char *list = argv[2];
    cJSON *root = load_data();
    cJSON *lists = cJSON_GetObjectItem(root, "lists");
    if (!cJSON_HasObjectItem(lists, list)) {
        printf(C_YELLOW "[WARN] List '%s' not found.\n" C_RESET, list);
        cJSON_Delete(root);
        return;
    }
    cJSON_DeleteItemFromObject(lists, list);
    if (save_data(root)) printf(C_GREEN "[OK] Deleted list '%s'.\n" C_RESET, list);
    else printf(C_RED "[ERROR] Failed to save data.\n" C_RESET);
    cJSON_Delete(root);
}

// mark done (optional)
static void cmd_done(int argc, char *argv[]) {
    if (argc < 4) {
        printf(C_YELLOW "[WARN] Usage: taskline done <list> <num>\n" C_RESET);
        return;
    }
    const char *list = argv[2];
    int num = atoi(argv[3]);
    if (num <= 0) { printf(C_YELLOW "[WARN] Invalid number.\n" C_RESET); return; }

    cJSON *root = load_data();
    cJSON *lists = cJSON_GetObjectItem(root, "lists");
    cJSON *arr = cJSON_GetObjectItem(lists, list);
    if (!arr || !cJSON_IsArray(arr)) {
        printf(C_RED "[ERROR] List '%s' not found.\n" C_RESET, list);
        cJSON_Delete(root);
        return;
    }
    int size = cJSON_GetArraySize(arr);
    if (num > size) {
        printf(C_YELLOW "[WARN] Task number out of range (1..%d)\n" C_RESET, size);
        cJSON_Delete(root);
        return;
    }
    cJSON *task = cJSON_GetArrayItem(arr, num - 1);
    if (!task) { printf(C_RED "[ERROR] Task not found.\n" C_RESET); cJSON_Delete(root); return; }
    cJSON_ReplaceItemInObject(task, "done", cJSON_CreateBool(1));
    if (save_data(root)) printf(C_GREEN "[OK] Task %d marked as done.\n" C_RESET, num);
    else printf(C_RED "[ERROR] Failed to save data.\n" C_RESET);
    cJSON_Delete(root);
}

// ---------- Main ----------
int main(int argc, char *argv[]) {

    // If no args, show help
    if (argc < 2) {
        cmd_help();
        return 0;
    }

    // map commands
    if (strcmp(argv[1], "help") == 0) {
        cmd_help();
    }
    else if (strcmp(argv[1], "about") == 0) {
        if (argc >= 3 && strcmp(argv[2], "en") == 0) cmd_about(1);
        else cmd_about(0);
    }
    else if (strcmp(argv[1], "new") == 0) {
        cmd_new(argc, argv);
    }
    else if (strcmp(argv[1], "add") == 0) {
        cmd_add(argc, argv);
    }
    else if (strcmp(argv[1], "ls") == 0) {
        cmd_ls(argc, argv);
    }
    else if (strcmp(argv[1], "lists") == 0) {
        cmd_lists();
    }
    else if (strcmp(argv[1], "rm") == 0) {
        cmd_rm(argc, argv);
    }
    else if (strcmp(argv[1], "del") == 0) {
        cmd_del(argc, argv);
    }
    else if (strcmp(argv[1], "done") == 0) {
        cmd_done(argc, argv);
    }
    else {
        printf(C_RED "[ERROR] Unknown command: %s\n" C_RESET, argv[1]);
        cmd_help();
    }

    return 0;
}

