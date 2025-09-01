#ifndef CEDA_CLI_H
#define CEDA_CLI_H

#include "module.h"

#include <stdbool.h>

void cli_init(CEDAModule *mod);

bool cli_isQuit(void);

/**
 * @brief Check if the computer must be restarted (cold hard reset).
 *
 * The restart value is automatically reset upon read.
 *
 * @return true if must be restarted, false otherwise.
 */
bool cli_checkRestart(void);

#endif // CEDA_CLI_H
