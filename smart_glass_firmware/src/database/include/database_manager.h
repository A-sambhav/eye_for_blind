#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define DB_MAX_PATH 256

typedef struct {
    char path[DB_MAX_PATH];
    uint32_t backup_interval_hours;
    uint32_t max_size_bytes;
    uint32_t retention_days;
    bool enable_wal;
} db_config_t;

typedef void (*db_row_callback_t)(void *user_data,
                                    int argc, char **argv, char **col_names);

typedef struct {
    uint32_t query_count;
    uint32_t write_count;
    uint32_t error_count;
    uint32_t backup_count;
    uint32_t size_bytes;
    bool initialized;
} db_stats_t;

typedef enum {
    DB_OK = 0,
    DB_ERR_NOT_INIT,
    DB_ERR_OPEN,
    DB_ERR_EXEC,
    DB_ERR_BUSY,
    DB_ERR_CORRUPT,
    DB_ERR_FULL
} db_status_t;

db_status_t db_init(const db_config_t *config);
db_status_t db_exec(const char *sql);
db_status_t db_query(const char *sql, db_row_callback_t cb, void *user_data);
db_status_t db_begin_transaction(void);
db_status_t db_commit(void);
db_status_t db_rollback(void);
db_status_t db_backup(void);
db_status_t db_vacuum(void);
db_status_t db_get_usage(uint32_t *out_used, uint32_t *out_total);
db_status_t db_get_stats(db_stats_t *out);
db_status_t db_deinit(void);

#endif /* DATABASE_MANAGER_H */
