#ifndef TRANSFER_H
#define TRANSFER_H

#include "account.h"
#include <stdint.h>

typedef enum {
    TRANSFER_SUCCESS = 0,
    TRANSFER_ERR_NULL_ACCOUNT = -1,
    TRANSFER_ERR_SELF_TRANSFER = -2,
    TRANSFER_ERR_INVALID_AMOUNT = -3,
    TRANSFER_ERR_INSUFFICIENT_FUNDS = -4
} transfer_status_t;

// Utfører atomisk og vranglås-fri overføring ved deterministisk sortering av låsene.
// Begge kontoene beskyttes under hele operasjonen slik at ingen halvveis tilstand er synlig.
transfer_status_t transfer(account_t* from, account_t* to, double amount);

// Naiv låsing (låser from først, deretter to). Kun for demonstrasjon og test av deadlock!
transfer_status_t transfer_unordered(account_t* from, account_t* to, double amount);

// Returnerer en lesbar tekstbeskrivelse av statuskoden.
const char* transfer_status_to_string(transfer_status_t status);

// Henter statistikk over fullførte overføringer.
uint64_t transfer_get_successful_count(void);
uint64_t transfer_get_failed_count(void);
void transfer_reset_stats(void);

#endif // TRANSFER_H
