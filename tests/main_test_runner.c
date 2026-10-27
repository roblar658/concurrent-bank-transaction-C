#include "account.h"
#include "transfer.h"
#include "csv_loader.h"
#include "simulation.h"
#include "sync_primitives.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdatomic.h>

#if defined(PLATFORM_WINDOWS)
static double get_time_ms(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return ((double)counter.QuadPart * 1000.0) / (double)freq.QuadPart;
}
#else
#include <time.h>
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}
#endif

static int g_tests_passed = 0;
static int g_tests_failed = 0;

typedef void (*test_func_t)(void);

static void run_test(const char* name, test_func_t fn) {
    double start = get_time_ms();
    printf("  %-70s ", name);
    fflush(stdout);
    fn();
    double elapsed = get_time_ms() - start;
    printf("[PASS] (%.1f ms)\n", elapsed);
    g_tests_passed++;
}

// 1. Vellykket overføring
static void test_successful_transfer(void) {
    account_t* a = account_create("ACC-001", "Alice", 1000.0);
    account_t* b = account_create("ACC-002", "Bob", 500.0);
    assert(a && b);

    transfer_status_t status = transfer(a, b, 350.0);
    assert(status == TRANSFER_SUCCESS);
    assert(fabs(account_get_balance(a) - 650.0) < 0.001);
    assert(fabs(account_get_balance(b) - 850.0) < 0.001);

    account_destroy(a);
    account_destroy(b);
}

// 2. Overføring av hele saldoen
static void test_full_balance_transfer(void) {
    account_t* a = account_create("ACC-001", "Alice", 1000.0);
    account_t* b = account_create("ACC-002", "Bob", 500.0);

    transfer_status_t status = transfer(a, b, 1000.0);
    assert(status == TRANSFER_SUCCESS);
    assert(fabs(account_get_balance(a) - 0.0) < 0.001);
    assert(fabs(account_get_balance(b) - 1500.0) < 0.001);

    account_destroy(a);
    account_destroy(b);
}

// 3. Utilstrekkelig dekning
static void test_insufficient_funds(void) {
    account_t* a = account_create("ACC-001", "Alice", 1000.0);
    account_t* b = account_create("ACC-002", "Bob", 500.0);

    transfer_status_t status = transfer(a, b, 1000.01);
    assert(status == TRANSFER_ERR_INSUFFICIENT_FUNDS);
    assert(fabs(account_get_balance(a) - 1000.0) < 0.001);
    assert(fabs(account_get_balance(b) - 500.0) < 0.001);

    account_destroy(a);
    account_destroy(b);
}

// 4. Ugyldige beløp
static void test_invalid_amounts(void) {
    account_t* a = account_create("ACC-001", "Alice", 1000.0);
    account_t* b = account_create("ACC-002", "Bob", 500.0);

    assert(transfer(a, b, 0.0) == TRANSFER_ERR_INVALID_AMOUNT);
    assert(transfer(a, b, -50.0) == TRANSFER_ERR_INVALID_AMOUNT);
    assert(transfer(a, b, NAN) == TRANSFER_ERR_INVALID_AMOUNT);
    assert(transfer(a, b, INFINITY) == TRANSFER_ERR_INVALID_AMOUNT);
    assert(transfer(a, b, -INFINITY) == TRANSFER_ERR_INVALID_AMOUNT);

    account_destroy(a);
    account_destroy(b);
}

// 5. Null-pekere for kontoer
static void test_null_accounts(void) {
    account_t* a = account_create("ACC-001", "Alice", 1000.0);

    assert(transfer(NULL, a, 100.0) == TRANSFER_ERR_NULL_ACCOUNT);
    assert(transfer(a, NULL, 100.0) == TRANSFER_ERR_NULL_ACCOUNT);
    assert(transfer(NULL, NULL, 100.0) == TRANSFER_ERR_NULL_ACCOUNT);

    account_destroy(a);
}

// 6. Egen-overføring (Self Transfer)
static void test_self_transfer(void) {
    account_t* a = account_create("ACC-001", "Alice", 1000.0);
    assert(transfer(a, a, 100.0) == TRANSFER_ERR_SELF_TRANSFER);
    assert(fabs(account_get_balance(a) - 1000.0) < 0.001);
    account_destroy(a);
}

// 7. Uavhengighet av rekkefølge på ID (lavere->høyere og høyere->lavere)
static void test_order_independence(void) {
    account_t* a = account_create("ACC-001", "Alice", 1000.0);
    account_t* b = account_create("ACC-002", "Bob", 1000.0);

    assert(transfer(a, b, 200.0) == TRANSFER_SUCCESS);
    assert(transfer(b, a, 500.0) == TRANSFER_SUCCESS);

    assert(fabs(account_get_balance(a) - 1300.0) < 0.001);
    assert(fabs(account_get_balance(b) - 700.0) < 0.001);

    account_destroy(a);
    account_destroy(b);
}

// --- CONCURRENCY TESTS ---

typedef struct {
    account_t* a;
    account_t* b;
    int count;
    bool forward;
    atomic_bool* start_flag;
} cross_transfer_arg_t;

#if defined(PLATFORM_WINDOWS)
static unsigned __stdcall cross_transfer_worker(void* arg) {
#else
static void* cross_transfer_worker(void* arg) {
#endif
    cross_transfer_arg_t* ca = (cross_transfer_arg_t*)arg;
    while (!atomic_load(ca->start_flag)) {
#if defined(PLATFORM_WINDOWS)
        YieldProcessor();
#endif
    }
    for (int i = 0; i < ca->count; i++) {
        if (ca->forward) {
            transfer(ca->a, ca->b, 10.0);
        } else {
            transfer(ca->b, ca->a, 10.0);
        }
    }
    return 0;
}

// 8. Deadlock-frihet ved samtidige kryss-overføringer (A->B og B->A)
static void test_deadlock_prevention(void) {
    account_t* a = account_create("ACC-100", "Alice", 1000000.0);
    account_t* b = account_create("ACC-200", "Bob", 1000000.0);

    int thread_count = 16;
    int transfers_per_thread = 2000;
    bank_thread_t threads[16];
    cross_transfer_arg_t args[16];
    atomic_bool start_flag;
    atomic_init(&start_flag, false);

    for (int i = 0; i < thread_count; i++) {
        args[i].a = a;
        args[i].b = b;
        args[i].count = transfers_per_thread;
        args[i].forward = (i % 2 == 0);
        args[i].start_flag = &start_flag;
        bank_thread_create(&threads[i], cross_transfer_worker, &args[i]);
    }

    atomic_store(&start_flag, true);

    for (int i = 0; i < thread_count; i++) {
        bank_thread_join(threads[i]);
    }

    double total = account_get_balance(a) + account_get_balance(b);
    assert(fabs(total - 2000000.0) < 0.001);

    account_destroy(a);
    account_destroy(b);
}

// 9. Atomisitet & Isolasjon: Lesere observerer aldri halvferdig tilstand
typedef struct {
    account_t* a;
    account_t* b;
    atomic_bool* is_running;
} writer_arg_t;

typedef struct {
    account_t* a;
    account_t* b;
    atomic_bool* is_running;
    atomic_uint_fast64_t* total_reads;
    atomic_uint_fast64_t* violations;
} reader_arg_t;

#if defined(PLATFORM_WINDOWS)
static unsigned __stdcall atomicity_writer(void* arg) {
#else
static void* atomicity_writer(void* arg) {
#endif
    writer_arg_t* wa = (writer_arg_t*)arg;
    int step = 0;
    while (atomic_load(wa->is_running)) {
        if (step++ % 2 == 0) {
            transfer(wa->a, wa->b, 25.0);
        } else {
            transfer(wa->b, wa->a, 25.0);
        }
    }
    return 0;
}

#if defined(PLATFORM_WINDOWS)
static unsigned __stdcall atomicity_reader(void* arg) {
#else
static void* atomicity_reader(void* arg) {
#endif
    reader_arg_t* ra = (reader_arg_t*)arg;
    account_t* first = ra->a;
    account_t* second = ra->b;
    if (strcmp(ra->a->id, ra->b->id) > 0) {
        first = ra->b;
        second = ra->a;
    }

    while (atomic_load(ra->is_running)) {
        bank_mutex_lock(&first->lock);
        bank_mutex_lock(&second->lock);

        double sum = first->balance + second->balance;
        atomic_fetch_add(ra->total_reads, 1);

        if (fabs(sum - 100000.0) > 0.001) {
            atomic_fetch_add(ra->violations, 1);
        }

        bank_mutex_unlock(&second->lock);
        bank_mutex_unlock(&first->lock);
    }
    return 0;
}

static void test_atomicity_and_isolation(void) {
    account_t* a = account_create("ACC-ISO-1", "Alice", 50000.0);
    account_t* b = account_create("ACC-ISO-2", "Bob", 50000.0);

    atomic_bool is_running;
    atomic_init(&is_running, true);
    atomic_uint_fast64_t total_reads;
    atomic_init(&total_reads, 0);
    atomic_uint_fast64_t violations;
    atomic_init(&violations, 0);

    bank_thread_t writers[4];
    bank_thread_t readers[4];
    writer_arg_t w_args[4];
    reader_arg_t r_args[4];

    for (int i = 0; i < 4; i++) {
        w_args[i].a = a;
        w_args[i].b = b;
        w_args[i].is_running = &is_running;
        bank_thread_create(&writers[i], atomicity_writer, &w_args[i]);

        r_args[i].a = a;
        r_args[i].b = b;
        r_args[i].is_running = &is_running;
        r_args[i].total_reads = &total_reads;
        r_args[i].violations = &violations;
        bank_thread_create(&readers[i], atomicity_reader, &r_args[i]);
    }

    bank_sleep_ms(1500);
    atomic_store(&is_running, false);

    for (int i = 0; i < 4; i++) {
        bank_thread_join(writers[i]);
        bank_thread_join(readers[i]);
    }

    assert(atomic_load(&total_reads) > 10000);
    assert(atomic_load(&violations) == 0);

    account_destroy(a);
    account_destroy(b);
}

// 10. Sirkulære overføringer i ring (Dining Philosophers)
typedef struct {
    account_t** accounts;
    int n;
    int thread_id;
    int transfers;
    atomic_bool* start_flag;
} ring_arg_t;

#if defined(PLATFORM_WINDOWS)
static unsigned __stdcall ring_worker(void* arg) {
#else
static void* ring_worker(void* arg) {
#endif
    ring_arg_t* ra = (ring_arg_t*)arg;
    while (!atomic_load(ra->start_flag)) {
#if defined(PLATFORM_WINDOWS)
        YieldProcessor();
#endif
    }
    for (int i = 0; i < ra->transfers; i++) {
        int from_idx = (ra->thread_id + i) % ra->n;
        int to_idx = (from_idx + 1) % ra->n;
        transfer(ra->accounts[from_idx], ra->accounts[to_idx], 5.0);
    }
    return 0;
}

static void test_circular_ring_transfers(void) {
    int n = 10;
    account_t* accounts[10];
    for (int i = 0; i < n; i++) {
        char id[32];
        snprintf(id, sizeof(id), "ACC-%03d", i);
        accounts[i] = account_create(id, "User", 10000.0);
    }

    int threads = 16;
    int per_thread = 1000;
    bank_thread_t th[16];
    ring_arg_t r_args[16];
    atomic_bool start_flag;
    atomic_init(&start_flag, false);

    for (int i = 0; i < threads; i++) {
        r_args[i].accounts = accounts;
        r_args[i].n = n;
        r_args[i].thread_id = i;
        r_args[i].transfers = per_thread;
        r_args[i].start_flag = &start_flag;
        bank_thread_create(&th[i], ring_worker, &r_args[i]);
    }

    atomic_store(&start_flag, true);
    for (int i = 0; i < threads; i++) {
        bank_thread_join(th[i]);
    }

    double total = 0.0;
    for (int i = 0; i < n; i++) {
        total += account_get_balance(accounts[i]);
        account_destroy(accounts[i]);
    }

    assert(fabs(total - 100000.0) < 0.001);
}

// 11. Rettferdighet (Fairness & Starvation Prevention)
typedef struct {
    account_t* client;
    account_t* central;
    atomic_bool* is_running;
    atomic_uint_fast64_t* tx_count;
} fair_arg_t;

#if defined(PLATFORM_WINDOWS)
static unsigned __stdcall fairness_worker(void* arg) {
#else
static void* fairness_worker(void* arg) {
#endif
    fair_arg_t* fa = (fair_arg_t*)arg;
    while (atomic_load(fa->is_running)) {
        transfer(fa->client, fa->central, 1.0);
        atomic_fetch_add(fa->tx_count, 1);
        transfer(fa->central, fa->client, 1.0);
    }
    return 0;
}

static void test_fairness_and_starvation(void) {
    account_t* central = account_create("ACC-CENTRAL", "Central", 10000000.0);
    int threads = 8;
    account_t* clients[8];
    atomic_uint_fast64_t counts[8];
    bank_thread_t th[8];
    fair_arg_t args[8];
    atomic_bool is_running;
    atomic_init(&is_running, true);

    for (int i = 0; i < threads; i++) {
        char id[32];
        snprintf(id, sizeof(id), "ACC-CLIENT-%d", i);
        clients[i] = account_create(id, "Client", 1000000.0);
        atomic_init(&counts[i], 0);

        args[i].client = clients[i];
        args[i].central = central;
        args[i].is_running = &is_running;
        args[i].tx_count = &counts[i];
        bank_thread_create(&th[i], fairness_worker, &args[i]);
    }

    bank_sleep_ms(1500);
    atomic_store(&is_running, false);

    for (int i = 0; i < threads; i++) {
        bank_thread_join(th[i]);
    }

    uint64_t min_tx = UINT64_MAX;
    uint64_t max_tx = 0;
    for (int i = 0; i < threads; i++) {
        uint64_t c = atomic_load(&counts[i]);
        if (c < min_tx) min_tx = c;
        if (c > max_tx) max_tx = c;
        account_destroy(clients[i]);
    }
    account_destroy(central);

    assert(min_tx > 100); // Ingen tråd må være sultet ut
    double ratio = (double)min_tx / (double)max_tx;
    assert(ratio > 0.15); // Balansert fordeling
}

// 12. Predefinerte CSV-datasett simuleringstester
static void test_csv_simulation_benchmark(void) {
    account_table_t* table = csv_load_accounts("data/accounts_100.csv");
    assert(table != NULL);
    size_t count = 0;
    transaction_record_t* txs = csv_load_transactions("data/transactions_benchmark_10k.csv", &count);
    assert(txs != NULL);

    simulation_metrics_t m = run_simulation(table, txs, count, 16, true);
    assert(m.total_transactions == 10000);
    assert(m.audit_violations_count == 0);
    assert(fabs(m.final_vault_balance - m.initial_vault_balance) < 0.0001);

    csv_free_transactions(txs);
    account_table_destroy(table);
}

static void test_csv_simulation_adversarial(void) {
    account_table_t* table = csv_load_accounts("data/accounts_100.csv");
    assert(table != NULL);
    size_t count = 0;
    transaction_record_t* txs = csv_load_transactions("data/transactions_adversarial_10k.csv", &count);
    assert(txs != NULL);

    simulation_metrics_t m = run_simulation(table, txs, count, 20, true);
    assert(m.total_transactions == 10000);
    assert(m.audit_violations_count == 0);
    assert(fabs(m.final_vault_balance - m.initial_vault_balance) < 0.0001);

    csv_free_transactions(txs);
    account_table_destroy(table);
}

int main(void) {
    bank_init_console_utf8();

    printf("================================================================================\n");
    printf("  C CONCURRENT BANK TRANSFER - TEST SUITE & VERIFIKASJON (WINDOWS NATIVE)       \n");
    printf("================================================================================\n\n");

    run_test("1.  Vellykket overføring oppdaterer begge kontoer", test_successful_transfer);
    run_test("2.  Overføring av hele saldoen reduserer avsender til 0", test_full_balance_transfer);
    run_test("3.  Utilstrekkelig dekning avvises og bevarer saldo", test_insufficient_funds);
    run_test("4.  Ugyldige beløp (0, negative, NaN, Inf) avvises", test_invalid_amounts);
    run_test("5.  Null-pekere for kontoer avvises", test_null_accounts);
    run_test("6.  Egen-overføring (Self Transfer) avvises", test_self_transfer);
    run_test("7.  Uavhengighet av konto-ID rekkefølge (lavere/høyere)", test_order_independence);
    run_test("8.  Deadlock-frihet ved samtidige kryss-overføringer (A<->B)", test_deadlock_prevention);
    run_test("9.  Atomisitet & Isolasjon: Lesere ser aldri mellomtilstand", test_atomicity_and_isolation);
    run_test("10. Sirkulære overføringer i ring (Dining Philosophers)", test_circular_ring_transfers);
    run_test("11. Rettferdighet (Fairness & Starvation Prevention)", test_fairness_and_starvation);
    run_test("12. CSV-simulering: Benchmark 10k transaksjoner", test_csv_simulation_benchmark);
    run_test("13. CSV-simulering: Adversarial 10k deadlock-feller", test_csv_simulation_adversarial);

    printf("\n================================================================================\n");
    printf("  TESTRESULTAT: %d bestått, %d feilet.\n", g_tests_passed, g_tests_failed);
    printf("================================================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
