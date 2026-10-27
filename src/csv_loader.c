#include "csv_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char* trim_whitespace(char* str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

account_table_t* csv_load_accounts(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        return NULL;
    }

    account_table_t* table = (account_table_t*)malloc(sizeof(account_table_t));
    if (!table) {
        fclose(f);
        return NULL;
    }
    table->capacity = 128;
    table->count = 0;
    table->accounts = (account_t**)malloc(table->capacity * sizeof(account_t*));
    if (!table->accounts) {
        free(table);
        fclose(f);
        return NULL;
    }

    char line[512];
    bool is_header = true;

    while (fgets(line, sizeof(line), f)) {
        char* trimmed = trim_whitespace(line);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') {
            continue;
        }

        if (is_header) {
            is_header = false;
            if (strstr(trimmed, "balance") != NULL || strstr(trimmed, "id") != NULL) {
                continue;
            }
        }

        char* token = strtok(trimmed, ",");
        if (!token) continue;
        char id[64];
        strncpy(id, trim_whitespace(token), sizeof(id) - 1);
        id[sizeof(id) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        char owner[64];
        strncpy(owner, trim_whitespace(token), sizeof(owner) - 1);
        owner[sizeof(owner) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        double balance = strtod(trim_whitespace(token), NULL);

        account_t* acc = account_create(id, owner, balance);
        if (!acc) continue;

        if (table->count >= table->capacity) {
            table->capacity *= 2;
            table->accounts = (account_t**)realloc(table->accounts, table->capacity * sizeof(account_t*));
        }
        table->accounts[table->count++] = acc;
    }

    fclose(f);
    return table;
}

account_t* account_table_find(const account_table_t* table, const char* id) {
    if (!table || !id) return NULL;
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->accounts[i]->id, id) == 0) {
            return table->accounts[i];
        }
    }
    return NULL;
}

void account_table_destroy(account_table_t* table) {
    if (!table) return;
    for (size_t i = 0; i < table->count; i++) {
        account_destroy(table->accounts[i]);
    }
    free(table->accounts);
    free(table);
}

transaction_record_t* csv_load_transactions(const char* filepath, size_t* out_count) {
    if (!out_count) return NULL;
    *out_count = 0;

    FILE* f = fopen(filepath, "r");
    if (!f) {
        return NULL;
    }

    size_t capacity = 1024;
    size_t count = 0;
    transaction_record_t* records = (transaction_record_t*)malloc(capacity * sizeof(transaction_record_t));
    if (!records) {
        fclose(f);
        return NULL;
    }

    char line[512];
    bool is_header = true;

    while (fgets(line, sizeof(line), f)) {
        char* trimmed = trim_whitespace(line);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') {
            continue;
        }

        if (is_header) {
            is_header = false;
            if (strstr(trimmed, "from") != NULL || strstr(trimmed, "to") != NULL) {
                continue;
            }
        }

        char* token = strtok(trimmed, ",");
        if (!token) continue;
        char tx_id[32];
        strncpy(tx_id, trim_whitespace(token), sizeof(tx_id) - 1);
        tx_id[sizeof(tx_id) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        char from_id[64];
        strncpy(from_id, trim_whitespace(token), sizeof(from_id) - 1);
        from_id[sizeof(from_id) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        char to_id[64];
        strncpy(to_id, trim_whitespace(token), sizeof(to_id) - 1);
        to_id[sizeof(to_id) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        double amount = strtod(trim_whitespace(token), NULL);

        if (count >= capacity) {
            capacity *= 2;
            records = (transaction_record_t*)realloc(records, capacity * sizeof(transaction_record_t));
        }

        strncpy(records[count].tx_id, tx_id, sizeof(records[count].tx_id) - 1);
        strncpy(records[count].from_id, from_id, sizeof(records[count].from_id) - 1);
        strncpy(records[count].to_id, to_id, sizeof(records[count].to_id) - 1);
        records[count].amount = amount;
        count++;
    }

    fclose(f);
    *out_count = count;
    return records;
}

void csv_free_transactions(transaction_record_t* records) {
    if (records) {
        free(records);
    }
}
