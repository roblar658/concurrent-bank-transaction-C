#ifndef SIMULATION_H
#define SIMULATION_H

#include "csv_loader.h"
#include "transfer.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint64_t total_transactions;
    uint64_t successful_transfers;
    uint64_t insufficient_funds_count;
    uint64_t self_transfer_count;
    uint64_t invalid_amount_count;
    uint64_t missing_account_count;

    uint64_t audit_checks_count;
    uint64_t audit_violations_count;

    double initial_vault_balance;
    double final_vault_balance;

    double duration_seconds;
    double tps;

    uint64_t min_us;
    double avg_us;
    uint64_t p50_us;
    uint64_t p90_us;
    uint64_t p95_us;
    uint64_t p99_us;
    uint64_t max_us;
} simulation_metrics_t;

simulation_metrics_t run_simulation(
    account_table_t* table,
    transaction_record_t* txs,
    size_t tx_count,
    int workers,
    bool enable_audit
);

void print_simulation_report(const char* title, const simulation_metrics_t* metrics, int workers);

#endif // SIMULATION_H
