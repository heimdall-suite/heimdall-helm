#ifndef HELM_CLI_H
#define HELM_CLI_H

/* CLI base (github.com/heimdall-suite/heimdall-helm issue #1): wires
   lib/shell's generic command console over lib/usb_cdc's transport and
   registers this project's own commands (`status`, `dfu` -- see
   lib/bootloader). Hardware-independent itself (see lib/README.md's
   "hardware-independent logic lives here too" section) -- it only calls
   through those libs' interfaces, never touches a register directly.
   Compiled out entirely on boards with HELM_FEATURE_CLI 0 (no USB CDC
   transport ported yet); call cli_start() from main() gated on that same
   macro, matching every other module's registration pattern. */
void cli_start(void);

#endif /* HELM_CLI_H */
