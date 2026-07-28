#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "database_manager.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define MAX_TABLES 4
#define MAX_COLUMNS 4
#define MAX_ROWS 24
#define MAX_BACKUPS 2
#define MAX_ROW_DATA 128
#define MAX_TOKEN 48
#define MAX_TOKENS 16
#define SCHEMA_VERSION 1

typedef enum { COL_TYPE_INT, COL_TYPE_TEXT, COL_TYPE_FLOAT, COL_TYPE_BLOB } col_type_t;

typedef struct {
    char name[32];
    col_type_t type;
} column_def_t;

typedef struct {
    char name[32];
    column_def_t columns[MAX_COLUMNS];
    uint8_t column_count;
    uint8_t rows[MAX_ROWS][MAX_ROW_DATA];
    uint16_t row_sizes[MAX_ROWS];
    uint16_t row_count;
    uint16_t next_id;
    bool active;
} table_t;

static struct {
    db_config_t config;
    table_t tables[MAX_TABLES];
    uint8_t table_count;
    SemaphoreHandle_t lock;
    uint32_t query_count;
    uint32_t write_count;
    uint32_t error_count;
    uint32_t backup_count;
    uint32_t size_bytes;
    bool transaction_active;
    bool initialized;
    uint8_t backup_idx;
} db;

static const char *const SYSTEM_SCHEMA[] = {
    "CREATE TABLE IF NOT EXISTS schema_version (version INT)",
    "CREATE TABLE IF NOT EXISTS reminders (id INT PRIMARY KEY, text TEXT, "
        "trigger_type INT, repeat_type INT, repeat_interval INT, "
        "start_ts INT, end_ts INT, latitude REAL, longitude REAL, "
        "radius REAL, acknowledged INT, active INT, priority INT)",
    "CREATE TABLE IF NOT EXISTS people (id INT PRIMARY KEY, name TEXT, "
        "embedding BLOB, memory TEXT, last_seen INT, frequency INT, "
        "familiarity REAL, relationship INT, trusted INT)",
    "CREATE TABLE IF NOT EXISTS config (key TEXT PRIMARY KEY, value TEXT)",
    "CREATE TABLE IF NOT EXISTS navigation_history (id INT PRIMARY KEY, "
        "dest_lat REAL, dest_lon REAL, start_ts INT, end_ts INT, "
        "distance REAL, status INT)",
    "CREATE TABLE IF NOT EXISTS event_log (id INT PRIMARY KEY, "
        "timestamp INT, module TEXT, severity INT, message TEXT)",
};

static const int SYSTEM_SCHEMA_COUNT = sizeof(SYSTEM_SCHEMA) / sizeof(SYSTEM_SCHEMA[0]);

static table_t *find_table(const char *name)
{
    for (uint8_t i = 0; i < db.table_count; i++) {
        if (db.tables[i].active && strcmp(db.tables[i].name, name) == 0)
            return &db.tables[i];
    }
    return NULL;
}

static int tokenize(const char *sql, char tokens[MAX_TOKENS][MAX_TOKEN])
{
    int count = 0;
    const char *p = sql;
    while (*p && count < MAX_TOKENS) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        if (*p == '(' || *p == ')' || *p == ',' || *p == ';') {
            tokens[count][0] = *p;
            tokens[count][1] = '\0';
            count++;
            p++;
            continue;
        }
        int ti = 0;
        if (*p == '\'') {
            p++;
            while (*p && *p != '\'' && ti < MAX_TOKEN - 1)
                tokens[count][ti++] = *p++;
            tokens[count][ti] = '\0';
            if (*p == '\'') p++;
            count++;
            continue;
        }
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' &&
               *p != '\r' && *p != '(' && *p != ')' &&
               *p != ',' && *p != ';' && ti < MAX_TOKEN - 1)
            tokens[count][ti++] = *p++;
        tokens[count][ti] = '\0';
        if (ti > 0) count++;
    }
    return count;
}

static int find_column(table_t *t, const char *name)
{
    for (uint8_t i = 0; i < t->column_count; i++)
        if (strcmp(t->columns[i].name, name) == 0) return i;
    return -1;
}

static db_status_t exec_create(const char *sql)
{
    char buf[512];
    strncpy(buf, sql, sizeof(buf) - 1);
    char *p = buf;

    char *table_name = NULL;
    char *after_name = NULL;

    if (strstr(p, "CREATE")) p += 6;
    while (*p == ' ') p++;
    if (strstr(p, "TABLE")) p += 5;
    while (*p == ' ') p++;
    if (strstr(p, "IF")) {
        char *ifp = strstr(p, "EXISTS");
        if (ifp) p = ifp + 6;
    }
    while (*p == ' ') p++;
    table_name = p;
    while (*p && *p != ' ' && *p != '(') p++;
    if (*p == ' ') { *p = '\0'; p++; }
    if (*p == '(') after_name = p;

    if (!table_name || !after_name) return DB_ERR_EXEC;
    if (find_table(table_name)) return DB_OK;

    if (db.table_count >= MAX_TABLES) return DB_ERR_FULL;
    table_t *t = &db.tables[db.table_count];
    memset(t, 0, sizeof(*t));
    strncpy(t->name, table_name, sizeof(t->name) - 1);

    char *col = after_name + 1;
    while (*col && *col != ')') {
        while (*col == ' ' || *col == ',' || *col == '(') col++;
        if (*col == ')') break;
        char *col_name = col;
        while (*col && *col != ' ') col++;
        if (*col == ' ') { *col = '\0'; col++; }
        while (*col == ' ') col++;
        char *col_type_str = col;
        while (*col && *col != ' ' && *col != ',' && *col != ')') col++;
        char saved = *col;
        *col = '\0';

        if (t->column_count < MAX_COLUMNS) {
            strncpy(t->columns[t->column_count].name, col_name,
                    sizeof(t->columns[t->column_count].name) - 1);
            if (strcmp(col_type_str, "INT") == 0 || strcmp(col_type_str, "PRIMARY") == 0)
                t->columns[t->column_count].type = COL_TYPE_INT;
            else if (strcmp(col_type_str, "TEXT") == 0)
                t->columns[t->column_count].type = COL_TYPE_TEXT;
            else if (strcmp(col_type_str, "REAL") == 0)
                t->columns[t->column_count].type = COL_TYPE_FLOAT;
            else if (strcmp(col_type_str, "BLOB") == 0)
                t->columns[t->column_count].type = COL_TYPE_BLOB;
            t->column_count++;
        }
        *col = saved;
        if (strstr(col_name, "PRIMARY") || strstr(col_type_str, "PRIMARY")) {
            while (*col && *col != ',' && *col != ')') col++;
        }
    }
    t->active = true;
    t->next_id = 1;
    db.table_count++;
    db.size_bytes += sizeof(table_t);
    return DB_OK;
}

static db_status_t exec_insert(const char *sql)
{
    char buf[512];
    strncpy(buf, sql, sizeof(buf) - 1);
    char *p = buf;

    if (strstr(p, "INSERT")) p += 6;
    while (*p == ' ') p++;
    if (strstr(p, "INTO")) p += 4;
    while (*p == ' ') p++;
    char *table_name = p;
    while (*p && *p != ' ' && *p != '(') p++;
    if (*p == ' ') { *p = '\0'; p++; }

    table_t *t = find_table(table_name);
    if (!t) return DB_ERR_EXEC;
    if (t->row_count >= MAX_ROWS) return DB_ERR_FULL;

    int values_start = -1;
    int col_names_start = -1;
    char tokens[MAX_TOKENS][MAX_TOKEN];
    int tc = tokenize(p, tokens);
    for (int i = 0; i < tc; i++) {
        if (strcmp(tokens[i], "VALUES") == 0) {
            values_start = i + 1;
        } else if (strcmp(tokens[i], "(") == 0 && col_names_start < 0 && values_start < 0) {
            col_names_start = i;
        }
    }

    int row_idx = t->row_count;
    memset(t->rows[row_idx], 0, MAX_ROW_DATA);
    int offset = 0;

    for (int i = values_start; i < tc && i < 100; i++) {
        if (strcmp(tokens[i], "(") == 0) continue;
        if (strcmp(tokens[i], ")") == 0) break;
        if (strcmp(tokens[i], ",") == 0) continue;

        if (*tokens[i] >= '0' && *tokens[i] <= '9') {
            int ival = atoi(tokens[i]);
            memcpy(t->rows[row_idx] + offset, &ival, sizeof(int));
            offset += sizeof(int);
        } else if (strchr(tokens[i], '.') && tokens[i][0] >= '0' && tokens[i][0] <= '9') {
            float fval = atof(tokens[i]);
            memcpy(t->rows[row_idx] + offset, &fval, sizeof(float));
            offset += sizeof(float);
        } else {
            int len = strlen(tokens[i]);
            if (len > MAX_ROW_DATA - offset - 1) len = MAX_ROW_DATA - offset - 1;
            memcpy(t->rows[row_idx] + offset, tokens[i], len);
            offset += len + 1;
        }
    }
    t->row_sizes[row_idx] = offset;
    t->row_count++;
    db.write_count++;
    db.size_bytes += offset + 4;
    return DB_OK;
}

static db_status_t exec_select(const char *sql, db_row_callback_t cb, void *user_data)
{
    char buf[512];
    strncpy(buf, sql, sizeof(buf) - 1);
    char *p = buf;

    char tokens[MAX_TOKENS][MAX_TOKEN];
    int tc = tokenize(p, tokens);
    if (tc < 4) return DB_ERR_EXEC;

    int from_idx = -1, where_idx = -1;
    for (int i = 0; i < tc; i++) {
        if (strcmp(tokens[i], "FROM") == 0) from_idx = i;
        if (strcmp(tokens[i], "WHERE") == 0) where_idx = i;
    }
    if (from_idx < 0) return DB_ERR_EXEC;

    table_t *t = find_table(tokens[from_idx + 1]);
    if (!t) return DB_ERR_EXEC;

    int where_col = -1;
    const char *where_val = NULL;
    if (where_idx > 0 && where_idx + 2 < tc) {
        where_col = find_column(t, tokens[where_idx + 1]);
        if (where_col >= 0) where_val = tokens[where_idx + 2];
    }

    for (uint16_t r = 0; r < t->row_count; r++) {
        if (where_col >= 0 && where_val) {
            char col_str[64];
            int offset = 0;
            for (int c = 0; c < where_col; c++) {
                if (t->columns[c].type == COL_TYPE_INT) offset += sizeof(int);
                else if (t->columns[c].type == COL_TYPE_FLOAT) offset += sizeof(float);
                else offset += strlen((char *)t->rows[r] + offset) + 1;
            }
            memcpy(col_str, t->rows[r] + offset, sizeof(col_str) - 1);
            col_str[sizeof(col_str) - 1] = '\0';
            if (strcmp(col_str, where_val) != 0) continue;
        }

        if (cb) {
            char *values[16];
            char col_names[16][32];
            int offset = 0;
            int argc = t->column_count;
            for (int c = 0; c < argc; c++) {
                strncpy(col_names[c], t->columns[c].name, 32);
                char val_str[64];
                if (t->columns[c].type == COL_TYPE_INT) {
                    int ival;
                    memcpy(&ival, t->rows[r] + offset, sizeof(int));
                    snprintf(val_str, sizeof(val_str), "%d", ival);
                    offset += sizeof(int);
                } else if (t->columns[c].type == COL_TYPE_FLOAT) {
                    float fval;
                    memcpy(&fval, t->rows[r] + offset, sizeof(float));
                    snprintf(val_str, sizeof(val_str), "%.2f", fval);
                    offset += sizeof(float);
                } else {
                    strncpy(val_str, (char *)t->rows[r] + offset, sizeof(val_str) - 1);
                    offset += strlen((char *)t->rows[r] + offset) + 1;
                }
                values[c] = val_str;
            }
            cb(user_data, argc, values, (char **)col_names);
        }
    }
    db.query_count++;
    return DB_OK;
}

static db_status_t exec_delete(const char *sql)
{
    char buf[512];
    strncpy(buf, sql, sizeof(buf) - 1);
    char *p = buf;

    char tokens[MAX_TOKENS][MAX_TOKEN];
    int tc = tokenize(p, tokens);
    if (tc < 3) return DB_ERR_EXEC;

    int from_idx = -1, where_idx = -1;
    for (int i = 0; i < tc; i++) {
        if (strcmp(tokens[i], "FROM") == 0) from_idx = i;
        if (strcmp(tokens[i], "WHERE") == 0) where_idx = i;
    }
    if (from_idx < 0) return DB_ERR_EXEC;

    table_t *t = find_table(tokens[from_idx + 1]);
    if (!t) return DB_ERR_EXEC;

    int where_col = -1;
    const char *where_val = NULL;
    if (where_idx > 0 && where_idx + 2 < tc) {
        where_col = find_column(t, tokens[where_idx + 1]);
        if (where_col >= 0) where_val = tokens[where_idx + 2];
    }

    uint16_t deleted = 0;
    for (uint16_t r = 0; r < t->row_count;) {
        bool match = true;
        if (where_col >= 0 && where_val) {
            char col_str[64];
            int offset = 0;
            for (int c = 0; c < where_col; c++) {
                if (t->columns[c].type == COL_TYPE_INT) offset += sizeof(int);
                else if (t->columns[c].type == COL_TYPE_FLOAT) offset += sizeof(float);
                else offset += strlen((char *)t->rows[r] + offset) + 1;
            }
            memcpy(col_str, t->rows[r] + offset, sizeof(col_str) - 1);
            if (strcmp(col_str, where_val) != 0) match = false;
        }
        if (match) {
            for (uint16_t k = r; k < t->row_count - 1; k++) {
                t->row_sizes[k] = t->row_sizes[k + 1];
                memcpy(t->rows[k], t->rows[k + 1], t->row_sizes[k + 1]);
            }
            t->row_count--;
            deleted++;
        } else {
            r++;
        }
    }
    if (deleted > 0) db.write_count++;
    return DB_OK;
}

db_status_t db_init(const db_config_t *config)
{
    if (config == NULL) return DB_ERR_NOT_INIT;
    memset(&db, 0, sizeof(db));
    db.config = *config;
    if (db.config.backup_interval_hours == 0) db.config.backup_interval_hours = 24;
    if (db.config.max_size_bytes == 0) db.config.max_size_bytes = 104857600;
    if (db.config.retention_days == 0) db.config.retention_days = 7;

    db.lock = xSemaphoreCreateMutex();
    if (db.lock == NULL) return DB_ERR_OPEN;

    xSemaphoreTake(db.lock, portMAX_DELAY);
    for (int i = 0; i < SYSTEM_SCHEMA_COUNT; i++) {
        exec_create(SYSTEM_SCHEMA[i]);
    }

    table_t *sv = find_table("schema_version");
    if (sv) {
        char insert_sql[256];
        snprintf(insert_sql, sizeof(insert_sql),
                 "INSERT INTO schema_version VALUES (%d)", SCHEMA_VERSION);
        exec_insert(insert_sql);
    }

    db.size_bytes += sizeof(db);
    db.initialized = true;
    xSemaphoreGive(db.lock);

    log_info("db", "Initialized path=%s backup=%uh max=%lu retention=%d d",
             db.config.path, db.config.backup_interval_hours,
             (unsigned long)db.config.max_size_bytes, db.config.retention_days);
    return DB_OK;
}

db_status_t db_exec(const char *sql)
{
    if (!db.initialized || sql == NULL) return DB_ERR_NOT_INIT;
    xSemaphoreTake(db.lock, portMAX_DELAY);

    db_status_t ret = DB_OK;
    char upper[512];
    strncpy(upper, sql, sizeof(upper) - 1);
    for (char *p = upper; *p; p++) *p = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;

    if (strstr(upper, "CREATE")) {
        ret = exec_create(sql);
    } else if (strstr(upper, "INSERT")) {
        ret = exec_insert(sql);
    } else if (strstr(upper, "DELETE")) {
        ret = exec_delete(sql);
    } else if (strstr(upper, "DROP")) {
        char *tn = strstr(upper, "TABLE");
        if (tn) {
            tn += 5;
            while (*tn == ' ') tn++;
            if (strstr(tn, "IF")) {
                char *ex = strstr(tn, "EXISTS");
                if (ex) tn = ex + 6;
            }
            while (*tn == ' ') tn++;
            char *end = tn;
            while (*end && *end != ' ' && *end != ';') end++;
            *end = '\0';
            table_t *t = find_table(tn);
            if (t) {
                memset(t, 0, sizeof(*t));
                t->active = false;
            }
        }
    } else {
        ret = DB_ERR_EXEC;
        db.error_count++;
    }

    if (ret == DB_OK && (strstr(upper, "INSERT") || strstr(upper, "DELETE") ||
                          strstr(upper, "UPDATE"))) {
        db.write_count++;
    }
    xSemaphoreGive(db.lock);
    return ret;
}

db_status_t db_query(const char *sql, db_row_callback_t cb, void *user_data)
{
    if (!db.initialized || sql == NULL) return DB_ERR_NOT_INIT;
    xSemaphoreTake(db.lock, portMAX_DELAY);
    char upper[512];
    strncpy(upper, sql, sizeof(upper) - 1);
    for (char *p = upper; *p; p++) *p = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;

    db_status_t ret = DB_OK;
    if (strstr(upper, "SELECT")) {
        ret = exec_select(sql, cb, user_data);
    } else {
        ret = DB_ERR_EXEC;
        db.error_count++;
    }
    xSemaphoreGive(db.lock);
    return ret;
}

db_status_t db_begin_transaction(void)
{
    if (!db.initialized) return DB_ERR_NOT_INIT;
    xSemaphoreTake(db.lock, portMAX_DELAY);
    if (db.transaction_active) {
        xSemaphoreGive(db.lock);
        return DB_ERR_BUSY;
    }
    db.transaction_active = true;
    xSemaphoreGive(db.lock);
    return DB_OK;
}

db_status_t db_commit(void)
{
    if (!db.initialized) return DB_ERR_NOT_INIT;
    xSemaphoreTake(db.lock, portMAX_DELAY);
    if (!db.transaction_active) {
        xSemaphoreGive(db.lock);
        return DB_ERR_EXEC;
    }
    db.transaction_active = false;
    xSemaphoreGive(db.lock);
    return DB_OK;
}

db_status_t db_rollback(void)
{
    if (!db.initialized) return DB_ERR_NOT_INIT;
    xSemaphoreTake(db.lock, portMAX_DELAY);
    if (!db.transaction_active) {
        xSemaphoreGive(db.lock);
        return DB_ERR_EXEC;
    }
    db.transaction_active = false;
    xSemaphoreGive(db.lock);
    return DB_OK;
}

db_status_t db_backup(void)
{
    if (!db.initialized) return DB_ERR_NOT_INIT;
    db.backup_count++;
    db.backup_idx = (db.backup_idx + 1) % MAX_BACKUPS;
    log_info("db", "Backup %u completed", db.backup_count);
    return DB_OK;
}

db_status_t db_vacuum(void)
{
    if (!db.initialized) return DB_ERR_NOT_INIT;
    for (uint8_t i = 0; i < db.table_count; i++) {
        if (!db.tables[i].active) {
            for (uint8_t j = i; j < db.table_count - 1; j++)
                db.tables[j] = db.tables[j + 1];
            db.table_count--;
            i--;
        }
    }
    log_info("db", "Vacuum completed, %u tables active", db.table_count);
    return DB_OK;
}

db_status_t db_get_usage(uint32_t *out_used, uint32_t *out_total)
{
    if (!db.initialized) return DB_ERR_NOT_INIT;
    xSemaphoreTake(db.lock, portMAX_DELAY);
    if (out_used) *out_used = db.size_bytes;
    if (out_total) *out_total = db.config.max_size_bytes;
    xSemaphoreGive(db.lock);
    return DB_OK;
}

db_status_t db_get_stats(db_stats_t *out)
{
    if (!db.initialized || out == NULL) return DB_ERR_NOT_INIT;
    xSemaphoreTake(db.lock, portMAX_DELAY);
    out->query_count = db.query_count;
    out->write_count = db.write_count;
    out->error_count = db.error_count;
    out->backup_count = db.backup_count;
    out->size_bytes = db.size_bytes;
    out->initialized = db.initialized;
    xSemaphoreGive(db.lock);
    return DB_OK;
}

db_status_t db_deinit(void)
{
    db.initialized = false;
    return DB_OK;
}
