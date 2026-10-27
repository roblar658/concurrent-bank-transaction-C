#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "sync_primitives.h"
#include <stdbool.h>

#define MAX_ID_LEN 64
#define MAX_OWNER_LEN 64

// Feilkoder for konto-operasjoner
#define ACCOUNT_SUCCESS 0
#define ACCOUNT_ERR_INVALID_PARAM -1
#define ACCOUNT_ERR_INSUFFICIENT_FUNDS -2
#define ACCOUNT_ERR_INVALID_AMOUNT -3

typedef struct account_t {
    char id[MAX_ID_LEN];
    char owner[MAX_OWNER_LEN];
    double balance;
    bank_mutex_t lock;
} account_t;

// Oppretter en ny trådsikker konto. Returnerer NULL ved ugyldige parametere.
account_t* account_create(const char* id, const char* owner, double initial_balance);

// Frigjør minne og sletter kontoens synkroniseringsressurser.
void account_destroy(account_t* account);

// Returnerer kontoens unike ID.
const char* account_get_id(const account_t* account);

// Returnerer kontoeiers navn.
const char* account_get_owner(const account_t* account);

// Trådsikker avlesing av saldo. Erverver kontoens mutex slik at ingen observatør
// kan lese en halvferdig tilstand under en pågående overføring.
double account_get_balance(account_t* account);

// Frittstående innskudd på enkeltkonto (låser internt).
int account_deposit(account_t* account, double amount);

// Frittstående uttak på enkeltkonto (låser internt).
int account_withdraw(account_t* account, double amount);

// Direkte saldo-avlesing forutsetter at låsen ALLEREDE holdes av kallende tråd!
double account_get_balance_unsafe(const account_t* account);

// Innskudd som forutsetter at låsen ALLEREDE holdes av kallende tråd!
int account_deposit_unsafe(account_t* account, double amount);

// Uttak som forutsetter at låsen ALLEREDE holdes av kallende tråd!
int account_withdraw_unsafe(account_t* account, double amount);

// Validerer at et beløp er et positivt, endelig tall (ikke <= 0, NaN eller uendelig).
int account_validate_amount(double amount);

#endif // ACCOUNT_H
