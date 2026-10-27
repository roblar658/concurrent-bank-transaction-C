#include "account.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int account_validate_amount(double amount) {
    if (amount <= 0.0 || isnan(amount) || isinf(amount)) {
        return ACCOUNT_ERR_INVALID_AMOUNT;
    }
    return ACCOUNT_SUCCESS;
}

account_t* account_create(const char* id, const char* owner, double initial_balance) {
    if (!id || strlen(id) == 0 || initial_balance < 0.0 || isnan(initial_balance) || isinf(initial_balance)) {
        return NULL;
    }

    account_t* acc = (account_t*)malloc(sizeof(account_t));
    if (!acc) {
        return NULL;
    }

    strncpy(acc->id, id, sizeof(acc->id) - 1);
    acc->id[sizeof(acc->id) - 1] = '\0';

    if (owner) {
        strncpy(acc->owner, owner, sizeof(acc->owner) - 1);
        acc->owner[sizeof(acc->owner) - 1] = '\0';
    } else {
        acc->owner[0] = '\0';
    }

    acc->balance = initial_balance;
    bank_mutex_init(&acc->lock);

    return acc;
}

void account_destroy(account_t* account) {
    if (!account) return;
    bank_mutex_destroy(&account->lock);
    free(account);
}

const char* account_get_id(const account_t* account) {
    return account ? account->id : NULL;
}

const char* account_get_owner(const account_t* account) {
    return account ? account->owner : NULL;
}

double account_get_balance(account_t* account) {
    if (!account) return 0.0;
    bank_mutex_lock(&account->lock);
    double b = account->balance;
    bank_mutex_unlock(&account->lock);
    return b;
}

double account_get_balance_unsafe(const account_t* account) {
    return account ? account->balance : 0.0;
}

int account_deposit_unsafe(account_t* account, double amount) {
    if (!account) return ACCOUNT_ERR_INVALID_PARAM;
    if (account_validate_amount(amount) != ACCOUNT_SUCCESS) {
        return ACCOUNT_ERR_INVALID_AMOUNT;
    }
    account->balance += amount;
    return ACCOUNT_SUCCESS;
}

int account_withdraw_unsafe(account_t* account, double amount) {
    if (!account) return ACCOUNT_ERR_INVALID_PARAM;
    if (account_validate_amount(amount) != ACCOUNT_SUCCESS) {
        return ACCOUNT_ERR_INVALID_AMOUNT;
    }
    if (account->balance < amount) {
        return ACCOUNT_ERR_INSUFFICIENT_FUNDS;
    }
    account->balance -= amount;
    return ACCOUNT_SUCCESS;
}

int account_deposit(account_t* account, double amount) {
    if (!account) return ACCOUNT_ERR_INVALID_PARAM;
    bank_mutex_lock(&account->lock);
    int res = account_deposit_unsafe(account, amount);
    bank_mutex_unlock(&account->lock);
    return res;
}

int account_withdraw(account_t* account, double amount) {
    if (!account) return ACCOUNT_ERR_INVALID_PARAM;
    bank_mutex_lock(&account->lock);
    int res = account_withdraw_unsafe(account, amount);
    bank_mutex_unlock(&account->lock);
    return res;
}
