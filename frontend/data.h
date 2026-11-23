#ifndef SPANG_FRONTEND_DATA_H
#define SPANG_FRONTEND_DATA_H
#include <stdint.h>
/* Exact original decoded DATA. Only newly owned palette banks12/13 are here;
 * the controller reuses existing stage tables for all its other banks. */
const uint8_t *frontend_data_bank(unsigned bank);
#endif
