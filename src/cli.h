#ifndef CEDA_CLI_H
#define CEDA_CLI_H

#include "module.h"

#include <stdbool.h>

void cli_init(CEDAModule *mod);

bool cli_isQuit(void);

#endif // CEDA_CLI_H
