#include "simulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

#if defined(PLATFORM_WINDOWS)
static double get_time_seconds(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
#include <time.h>
static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
#endif

typedef struct {
    account_table_t* table;
    transaction_record_t* txs;
    size_t tx_count;
    atomic_size_t* current_index;
    simulation_metrics_t* metrics;
    uint64_t* latencies;
    atomic_size_t* latency_count;
    size_t max_latencies;
} worker_ctx_t;

typedef struct {
    account_table_t* table;
    account_t** sorted_accounts;
    size_t account_count;
    double expected_vault;
    atomic_bool* is_running;
    simulation_metrics_t* metrics;
} auditor_ctx_t;

static int compare_accounts_by_id(const void* a, const void* b) {
    const account_t* acc_a = *(const account_t**)a;
    const account_t* acc_b = *(const account_t**)b;
    return strcmp(acc_a->id, acc_b->id);
}

static int compare_uint64(const void* a, const void* b) {
    uint64_t arg1 = *(const uint64_t*)a;
    uint64_t arg2 = *(const uint64_t*)b;
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

static double snapshot_ordered(account_t** accounts, size_t count, size_t idx) {
    if (idx == count) {
        double sum = 0.0;
        for (size_t i = 0; i < count; i++) {
            sum += accounts[i]->balance;
        }
        return sum;
    }
    bank_mutex_lock(&accounts[idx]->lock);
    double res = snapshot_ordered(accounts, count, idx + 1);
    bank_mutex_unlock(&accounts[idx]->lock);
    return res;
}

#if defined(PLATFORM_WINDOWS)
static unsigned __stdcall worker_thread_fn(void* arg) {
#else
static void* worker_thread_fn(void* arg) {
#endif
    worker_ctx_t* ctx = (worker_ctx_t*)arg;

    while (1) {
        size_t idx = atomic_fetch_add(ctx->current_index, 1);
        if (idx >= ctx->tx_count) {
            break;
        }

        const transaction_record_t* tx = &ctx->txs[idx];
        account_t* from = account_table_find(ctx->table, tx->from_id);
        account_t* to = account_table_find(ctx->table, tx->to_id);

        if (!from || !to) {
            atomic_fetch_add((atomic_uint_fast64_t*)&ctx->metrics->missing_account_count, 1);
            atomic_fetch_add((atomic_uint_fast64_t*)&ctx->metrics->total_transactions, 1);
            continue;
        }

        double start = get_time_seconds();
        transfer_status_t status = transfer(from, to, tx->amount);
        double end = get_time_seconds();
        uint64_t us = (uint64_t)((end - start) * 1e6);

        atomic_fetch_add((atomic_uint_fast64_t*)&ctx->metrics->total_transactions, 1);

        if (status == TRANSFER_SUCCESS) {
            atomic_fetch_add((atomic_uint_fast64_t*)&ctx->metrics->successful_transfers, 1);
        } else if (status == TRANSFER_ERR_INSUFFICIENT_FUNDS) {
            atomic_fetch_add((atomic_uint_fast64_t*)&ctx->metrics->insufficient_funds_count, 1);
        } else if (status == TRANSFER_ERR_SELF_TRANSFER) {
            atomic_fetch_add((atomic_uint_fast64_t*)&ctx->metrics->self_transfer_count, 1);
        } else if (status == TRANSFER_ERR_INVALID_AMOUNT) {
            atomic_fetch_add((atomic_uint_fast64_t*)&ctx->metrics->invalid_amount_count, 1);
        }

        size_t lat_idx = atomic_fetch_add(ctx->latency_count, 1);
        if (lat_idx < ctx->max_latencies) {
            ctx->latencies[lat_idx] = us;
        }
    }
    return 0;
}

#if defined(PLATFORM_WINDOWS)
static unsigned __stdcall auditor_thread_fn(void* arg) {
#else
static void* auditor_thread_fn(void* arg) {
#endif
    auditor_ctx_t* ctx = (auditor_ctx_t*)arg;

    while (atomic_load(ctx->is_running)) {
        double audited_sum = snapshot_ordered(ctx->sorted_accounts, ctx->account_count, 0);
        bool consistent = (fabs(audited_sum - ctx->expected_vault) < 0.0001);

        atomic_fetch_add((atomic_uint_fast64_t*)&ctx->metrics->audit_checks_count, 1);
        if (!consistent) {
            atomic_fetch_add((atomic_uint_fast64_t*)&ctx->metrics->audit_violations_count, 1);
        }
        bank_sleep_ms(15);
    }
    return 0;
}

simulation_metrics_t run_simulation(
    account_table_t* table,
    transaction_record_t* txs,
    size_t tx_count,
    int workers,
    bool enable_audit
) {
    simulation_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    if (workers <= 0) workers = 4;

    // Beregn innledende samlet saldo
    double initial_vault = 0.0;
    for (size_t i = 0; i < table->count; i++) {
        initial_vault += account_get_balance(table->accounts[i]);
    }
    metrics.initial_vault_balance = initial_vault;

    // Sorter kontoer for auditør
    account_t** sorted_accounts = (account_t**)malloc(table->count * sizeof(account_t*));
    memcpy(sorted_accounts, table->accounts, table->count * sizeof(account_t*));
    qsort(sorted_accounts, table->count, sizeof(account_t*), compare_accounts_by_id);

    atomic_bool is_auditor_running;
    atomic_init(&is_auditor_running, true);

    bank_thread_t auditor_thread;
    auditor_ctx_t auditor_ctx = {
        .table = table,
        .sorted_accounts = sorted_accounts,
        .account_count = table->count,
        .expected_vault = initial_vault,
        .is_running = &is_auditor_running,
        .metrics = &metrics
    };

    if (enable_audit) {
        bank_thread_create(&auditor_thread, auditor_thread_fn, &auditor_ctx);
    }

    size_t max_latencies = (tx_count > 100000) ? 100000 : tx_count;
    uint64_t* latencies = (uint64_t*)malloc(max_latencies * sizeof(uint64_t));
    atomic_size_t latency_count;
    atomic_init(&latency_count, 0);

    atomic_size_t current_index;
    atomic_init(&current_index, 0);

    worker_ctx_t worker_ctx = {
        .table = table,
        .txs = txs,
        .tx_count = tx_count,
        .current_index = &current_index,
        .metrics = &metrics,
        .latencies = latencies,
        .latency_count = &latency_count,
        .max_latencies = max_latencies
    };

    bank_thread_t* worker_threads = (bank_thread_t*)malloc(workers * sizeof(bank_thread_t));
    double sim_start = get_time_seconds();

    for (int i = 0; i < workers; i++) {
        bank_thread_create(&worker_threads[i], worker_thread_fn, &worker_ctx);
    }

    for (int i = 0; i < workers; i++) {
        bank_thread_join(worker_threads[i]);
    }

    double sim_end = get_time_seconds();
    metrics.duration_seconds = sim_end - sim_start;

    if (enable_audit) {
        atomic_store(&is_auditor_running, false);
        bank_thread_join(auditor_thread);
    }

    // Beregn slutt-saldo i hvelvet
    double final_vault = 0.0;
    for (size_t i = 0; i < table->count; i++) {
        final_vault += account_get_balance(table->accounts[i]);
    }
    metrics.final_vault_balance = final_vault;

    if (metrics.duration_seconds > 0) {
        metrics.tps = (double)metrics.total_transactions / metrics.duration_seconds;
    }

    // Beregn latens-persentiler
    size_t actual_lat_count = atomic_load(&latency_count);
    if (actual_lat_count > max_latencies) actual_lat_count = max_latencies;

    if (actual_lat_count > 0) {
        qsort(latencies, actual_lat_count, sizeof(uint64_t), compare_uint64);
        metrics.min_us = latencies[0];
        metrics.max_us = latencies[actual_lat_count - 1];
        metrics.p50_us = latencies[(size_t)(actual_lat_count * 0.50)];
        metrics.p90_us = latencies[(size_t)(actual_lat_count * 0.90)];
        metrics.p95_us = latencies[(size_t)(actual_lat_count * 0.95)];
        metrics.p99_us = latencies[(size_t)(actual_lat_count * 0.99)];

        uint64_t sum = 0;
        for (size_t i = 0; i < actual_lat_count; i++) sum += latencies[i];
        metrics.avg_us = (double)sum / (double)actual_lat_count;
    }

    free(worker_threads);
    free(latencies);
    free(sorted_accounts);

    return metrics;
}

void print_simulation_report(const char* title, const simulation_metrics_t* m, int workers) {
    double succ_pct = m->total_transactions > 0 ? (100.0 * (double)m->successful_transfers / (double)m->total_transactions) : 0.0;

    printf("\n================================================================================\n");
    printf("  C SIMULERINGSRAPPORT: %s\n", title);
    printf("================================================================================\n");
    printf("  Arbeidstråder (parallelitet)   : %d tråder\n", workers);
    printf("  Total varighet                 : %.3f sekunder (%.1f ms)\n", m->duration_seconds, m->duration_seconds * 1000.0);
    printf("  Totalt behandlet               : %llu transaksjoner\n", (unsigned long long)m->total_transactions);
    printf("  -> Vellykkede overføringer     : %llu (%.2f%%)\n", (unsigned long long)m->successful_transfers, succ_pct);
    printf("  -> Avvist (manglende dekning)  : %llu\n", (unsigned long long)m->insufficient_funds_count);
    printf("  -> Avvist (egen-overføring)    : %llu\n", (unsigned long long)m->self_transfer_count);
    printf("  -> Avvist (ugyldig beløp)      : %llu\n", (unsigned long long)m->invalid_amount_count);
    printf("  -> Feil (ukjent konto)         : %llu\n", (unsigned long long)m->missing_account_count);
    printf("--------------------------------------------------------------------------------\n");
    printf("  Gjennomstrømming (Throughput)  : %.1f transaksjoner/sekund (TPS)\n", m->tps);
    printf("  Latens (Min / Gj.snitt / Maks) : %llu µs / %.2f µs / %llu µs\n",
           (unsigned long long)m->min_us, m->avg_us, (unsigned long long)m->max_us);
    printf("  Latens-persentiler (p50 / p95) : %llu µs / %llu µs\n",
           (unsigned long long)m->p50_us, (unsigned long long)m->p95_us);
    printf("  Latens-persentil   (p99)       : %llu µs (%.3f ms)\n",
           (unsigned long long)m->p99_us, (double)m->p99_us / 1000.0);
    printf("--------------------------------------------------------------------------------\n");
    printf("  Start-saldo bankhvelv          : %.2f kr\n", m->initial_vault_balance);
    printf("  Slutt-saldo bankhvelv          : %.2f kr\n", m->final_vault_balance);
    printf("  Differanse i hvelv             : %.4f kr\n", m->final_vault_balance - m->initial_vault_balance);
    printf("  Kontinuerlige audit-sjekker    : %llu utført, %llu avvik funnet\n",
           (unsigned long long)m->audit_checks_count, (unsigned long long)m->audit_violations_count);
    printf("================================================================================\n\n");
}
