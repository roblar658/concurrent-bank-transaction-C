#include "simulation.h"
#include "sync_primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char* argv[]) {
    bank_init_console_utf8();

    printf("================================================================================\n");
    printf("     C BANK SIMULERINGSMOTOR - MASSIV SAMTIDIGHET MED PREDEFINERT CSV-DATA     \n");
    printf("================================================================================\n\n");

    const char* accounts_path = "data/accounts_100.csv";
    const char* transactions_path = "data/transactions_adversarial_10k.csv";
    int workers = 16;

    if (argc >= 2) {
        accounts_path = argv[1];
    }
    if (argc >= 3) {
        transactions_path = argv[2];
    }
    if (argc >= 4) {
        int w = atoi(argv[3]);
        if (w > 0) workers = w;
    }

    printf("Laster inn data fra disk:\n");
    printf("  - Kontoer fil     : %s\n", accounts_path);
    printf("  - Transaksjoner   : %s\n", transactions_path);
    printf("  - Arbeidstråder   : %d\n\n", workers);

    account_table_t* table = csv_load_accounts(accounts_path);
    if (!table) {
        fprintf(stderr, "Feil: Kunne ikke laste kontoer fra %s\n", accounts_path);
        return 1;
    }

    size_t tx_count = 0;
    transaction_record_t* txs = csv_load_transactions(transactions_path, &tx_count);
    if (!txs) {
        fprintf(stderr, "Feil: Kunne ikke laste transaksjoner fra %s\n", transactions_path);
        account_table_destroy(table);
        return 1;
    }

    printf("Data innlastet: %zu kontoer, %zu transaksjoner.\n", table->count, tx_count);
    printf("Starter parallell simulering med sanntidsrevisjon (auditor)...\n");

    simulation_metrics_t metrics = run_simulation(table, txs, tx_count, workers, true);

    print_simulation_report(transactions_path, &metrics, workers);

    double diff = fabs(metrics.final_vault_balance - metrics.initial_vault_balance);
    if (diff > 0.0001) {
        fprintf(stderr, "KRITISK FEIL: Totalbankbalansen avviker med %.4f kr!\n", diff);
        csv_free_transactions(txs);
        account_table_destroy(table);
        return 1;
    }

    if (metrics.audit_violations_count > 0) {
        fprintf(stderr, "KRITISK FEIL: %llu inkonsistente mellomtilstander oppdaget av auditor!\n",
                (unsigned long long)metrics.audit_violations_count);
        csv_free_transactions(txs);
        account_table_destroy(table);
        return 1;
    }

    printf(">>> C SIMULERING VELLYKKET: Full konsistens, 0 vranglåser og 0 audit-avvik bekreftet!\n");

    csv_free_transactions(txs);
    account_table_destroy(table);
    return 0;
}
