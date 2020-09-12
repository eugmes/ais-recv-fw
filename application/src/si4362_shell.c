/*
 * Copyright (c) 2020 Ievgenii Meshcheriakov.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <shell/shell.h>
#include "si4362.h"

#define GET_DEVICE(shell)							\
const struct device *dev = device_get_binding("RX_1");				\
if (!dev) {									\
	shell_error(shell, "Failed to get the device");				\
	return -ENODEV;								\
}

static int cmd_cts(const struct shell *shell, size_t argc, char **argv)
{
	GET_DEVICE(shell);

	int cts = si4362_get_cts(dev);
	shell_print(shell, "%d\n", cts);
	return 0;
}

static int cmd_shutdown(const struct shell *shell, size_t argc, char **argv)
{
	GET_DEVICE(shell);

	bool new_state = 0;

	if (!strcmp(argv[1], "on")) {
		new_state = true;
	} else if (!strcmp(argv[1], "off")) {
		new_state = false;
	} else {
		shell_error(shell, "on|off");
		return -EINVAL;
	}

	shell_print(shell, "Setting SDN to %d", (int)new_state);
	return si4362_shutdown(dev, new_state);
}

SHELL_STATIC_SUBCMD_SET_CREATE(si4362_cmds,
	SHELL_CMD_ARG(cts, NULL, "Show CTS pin state", cmd_cts, 1, 0),
	SHELL_CMD_ARG(shutdown, NULL, "Set SDN pin state", cmd_shutdown, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(si4362, &si4362_cmds, "SI4362 shell commands", NULL);
