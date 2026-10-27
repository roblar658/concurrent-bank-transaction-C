#ifndef CSV_LOADER_H
#define CSV_LOADER_H

#include "account.h"
#include <stddef.h>

typedef struct {
    char tx_id[32];
    char from_id[64];
    char to_id[64];
    double amount;
} transaction_record_t;

typedef struct {
    account_t** accounts;
    size_t count;
    size_t capacity;
} account_table_t;

account_table_t* csv_load_accounts(const char* filepath);
account_t* account_table_find(const account_table_t* table, const char* id);
void account_table_destroy(account_table_t* table);

transaction_record_t* csv_load_transactions(const char* filepath, size_t* out_count);
void csv_free_transactions(transaction_record_t* records);

#endif // CSV_LOADER_H
