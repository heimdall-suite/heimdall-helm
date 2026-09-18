#include "shell.h"

#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

typedef struct {
    const char *name;
    const char *help;
    ShellCommandHandler handler;
} ShellCommand;

static ShellCommand commandTable[SHELL_MAX_COMMANDS];
static uint32_t commandCount = 0;

static ShellReadFn readFn = NULL;
static ShellWriteFn writeFn = NULL;

static char lineBuf[SHELL_MAX_LINE_LEN];
static uint32_t lineLen = 0;

static void print_help(const char *args);

/* Parses one complete input line into a command name and its argument
   string, then calls the matching registered handler (or prints an
   "unknown command" message if none match). */
static void dispatch_line(char *line) {
    /* Skip leading whitespace, split at the first space into name/args. */
    while (*line == ' ') {
        line++;
    }
    if (*line == '\0') {
        return;
    }

    char *args = line;
    while (*args != '\0' && *args != ' ') {
        args++;
    }
    if (*args == ' ') {
        *args = '\0';
        args++;
        while (*args == ' ') {
            args++;
        }
    }

    for (uint32_t i = 0; i < commandCount; i++) {
        if (strcmp(commandTable[i].name, line) == 0) {
            commandTable[i].handler(args);
            return;
        }
    }

    shell_print("unknown command: ");
    shell_print(line);
    shell_print(" (try \"help\")\r\n");
}

/* Handler for the built-in `help` command: prints every registered
   command's name and help text, one per line. */
static void print_help(const char *args) {
    (void)args;
    for (uint32_t i = 0; i < commandCount; i++) {
        shell_print(commandTable[i].name);
        shell_print(" - ");
        shell_print(commandTable[i].help);
        shell_print("\r\n");
    }
}

void shell_init(ShellReadFn newReadFn, ShellWriteFn newWriteFn) {
    readFn = newReadFn;
    writeFn = newWriteFn;
    commandCount = 0;
    lineLen = 0;
    shell_register("help", "list available commands", print_help);
}

bool shell_register(const char *name, const char *help, ShellCommandHandler handler) {
    if (commandCount >= SHELL_MAX_COMMANDS) {
        return false;
    }
    commandTable[commandCount].name = name;
    commandTable[commandCount].help = help;
    commandTable[commandCount].handler = handler;
    commandCount++;
    return true;
}

void shell_print(const char *str) {
    if (writeFn != NULL) {
        writeFn((const uint8_t *)str, (uint32_t)strlen(str));
    }
}

void shell_task(void *arg) {
    (void)arg;

    for (;;) {
        uint8_t byte;
        if (readFn == NULL || readFn(&byte, 1) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (byte == '\r' || byte == '\n') {
            shell_print("\r\n");
            lineBuf[lineLen] = '\0';
            dispatch_line(lineBuf);
            lineLen = 0;
        } else if (byte == 0x7F || byte == 0x08) { /* backspace/DEL */
            if (lineLen > 0) {
                lineLen--;
                shell_print("\b \b");
            }
        } else if (byte >= 0x20 && byte < 0x7F && lineLen < SHELL_MAX_LINE_LEN - 1) {
            lineBuf[lineLen++] = (char)byte;
            writeFn(&byte, 1);
        }
    }
}
