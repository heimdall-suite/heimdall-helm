#ifndef HELM_DIAG_H
#define HELM_DIAG_H

/* Bench-only diagnostics, gathered under the CLI's single `diag` entry
   point instead of each getting its own top-level shell command -- keeps
   SHELL_MAX_COMMANDS's 16-slot table (lib/shell/shell.h) for real
   operational commands (status/dfu/help) and gives every bring-up/
   bench-only command -- inspection (`pipeline`) or hazardous (`wedge`)
   alike -- one clearly-named, documented place. See .docs/cli.md.

   `args` is whatever text followed `diag ` on the command line (e.g.
   "wedge"), same convention as a normal shell command handler's own
   `args` parameter -- just one dispatch level deeper. */
void diag_dispatch(const char *args);

#endif /* HELM_DIAG_H */
