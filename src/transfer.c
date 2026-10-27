#include "transfer.h"
#include <string.h>
#include <stdatomic.h>

static atomic_uint_fast64_t g_successful_transfers = 0;
static atomic_uint_fast64_t g_failed_transfers = 0;

transfer_status_t transfer(account_t* from, account_t* to, double amount) {
    if (!from || !to) {
        return TRANSFER_ERR_NULL_ACCOUNT;
    }

    if (account_validate_amount(amount) != ACCOUNT_SUCCESS) {
        return TRANSFER_ERR_INVALID_AMOUNT;
    }

    // 1. Deterministisk låseordning basert på unik konto-ID (strcmp)
    int cmp = strcmp(from->id, to->id);
    if (cmp == 0) {
        return TRANSFER_ERR_SELF_TRANSFER;
    }

    account_t* first = from;
    account_t* second = to;
    if (cmp > 0) {
        first = to;
        second = from;
    }

    // 2. Erverv låser i streng rekkefølge (lavere ID først)
    bank_mutex_lock(&first->lock);
    bank_mutex_lock(&second->lock);

    transfer_status_t status = TRANSFER_SUCCESS;

    // 3. Valider dekning mens BEGGE kontoer holdes trygt låst
    if (from->balance < amount) {
        status = TRANSFER_ERR_INSUFFICIENT_FUNDS;
        atomic_fetch_add(&g_failed_transfers, 1);
    } else {
        // 4. Atomisk uttak og innskudd
        from->balance -= amount;
        to->balance += amount;
        atomic_fetch_add(&g_successful_transfers, 1);
    }

    // 5. Frigjør låser i omvendt rekkefølge
    bank_mutex_unlock(&second->lock);
    bank_mutex_unlock(&first->lock);

    return status;
}

transfer_status_t transfer_unordered(account_t* from, account_t* to, double amount) {
    if (!from || !to) {
        return TRANSFER_ERR_NULL_ACCOUNT;
    }
    if (strcmp(from->id, to->id) == 0) {
        return TRANSFER_ERR_SELF_TRANSFER;
    }
    if (account_validate_amount(amount) != ACCOUNT_SUCCESS) {
        return TRANSFER_ERR_INVALID_AMOUNT;
    }

    // Naiv låsing: Låser 'from' først, deretter 'to' uansett ID
    bank_mutex_lock(&from->lock);
    bank_mutex_lock(&to->lock);

    transfer_status_t status = TRANSFER_SUCCESS;
    if (from->balance < amount) {
        status = TRANSFER_ERR_INSUFFICIENT_FUNDS;
    } else {
        from->balance -= amount;
        to->balance += amount;
    }

    bank_mutex_unlock(&to->lock);
    bank_mutex_unlock(&from->lock);

    return status;
}

const char* transfer_status_to_string(transfer_status_t status) {
    switch (status) {
        case TRANSFER_SUCCESS: return "SUCCESS";
        case TRANSFER_ERR_NULL_ACCOUNT: return "ERR_NULL_ACCOUNT";
        case TRANSFER_ERR_SELF_TRANSFER: return "ERR_SELF_TRANSFER";
        case TRANSFER_ERR_INVALID_AMOUNT: return "ERR_INVALID_AMOUNT";
        case TRANSFER_ERR_INSUFFICIENT_FUNDS: return "ERR_INSUFFICIENT_FUNDS";
        default: return "ERR_UNKNOWN";
    }
}

uint64_t transfer_get_successful_count(void) {
    return atomic_load(&g_successful_transfers);
}

uint64_t transfer_get_failed_count(void) {
    return atomic_load(&g_failed_transfers);
}

void transfer_reset_stats(void) {
    atomic_store(&g_successful_transfers, 0);
    atomic_store(&g_failed_transfers, 0);
}
