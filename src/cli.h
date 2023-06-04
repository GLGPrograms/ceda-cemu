#ifndef CEDA_CLI_H
#define CEDA_CLI_H

#include <stdbool.h>

void cli_init(void);
void cli_start(void);
void cli_poll(void);
long cli_remaining(void);

bool cli_isQuit(void);

void cli_cleanup(void);

#endif // CEDA_CLI_H
