/* SPDX-License-Identifier: LGPL-2.1+ */

#include "config.h"

#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "lxc.h"

#include "arguments.h"
#include "log.h"
#include "utils.h"

#define OPT_NO_LOCK (OPT_USAGE + 1)
#define OPT_NO_KILL (OPT_USAGE + 2)

lxc_log_define(lxc_stop, lxc);

static int my_parser(struct lxc_arguments *args, int c, char *arg);

static const struct option my_longopts[] = {
	{"reboot", no_argument, 0, 'r'},
	{"nowait", no_argument, 0, 'W'},
	{"timeout", required_argument, 0, 't'},
	{"kill", no_argument, 0, 'k'},
	{"nokill", no_argument, 0, OPT_NO_KILL},
	{"nolock", no_argument, 0, OPT_NO_LOCK},
	LXC_COMMON_OPTIONS
};

static struct lxc_arguments my_args = {
	.progname     = "lxc-stop",
	.help         = "\
--name=NAME\n\
\n\
lxc-stop stops a container with the identifier NAME\n\
\n\
Options :\n\
  -n, --name=NAME   NAME of the container\n\
  -r, --reboot      reboot the container\n\
  -W, --nowait      don't wait for shutdown or reboot to complete\n\
  -t, --timeout=T   wait T seconds before hard-stopping\n\
  -k, --kill        kill container rather than request clean shutdown\n\
      --nolock      Avoid using API locks\n\
      --nokill      Only request clean shutdown, don't force kill after timeout\n\
  --rcfile=FILE     Load configuration file FILE\n",
	.options      = my_longopts,
	.parser       = my_parser,
	.checker      = NULL,
	.log_priority = "ERROR",
	.log_file     = "none",
	.timeout      = -2,
};

static int my_parser(struct lxc_arguments *args, int c, char *arg)
{
	switch (c) {
	case 'r':
		args->reboot = 1;
		break;
	case 'W':
		args->nowait = 1;
		break;
	case 't':
		if (lxc_safe_long(arg, &args->timeout) < 0)
			return -1;
		break;
	case 'k':
		args->hardstop = 1;
		break;
	case OPT_NO_LOCK:
		args->nolock = 1;
		break;
	case OPT_NO_KILL:
		args->nokill = 1;
		break;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct lxc_container *c;
	struct lxc_log log;
	bool s;
	int ret = EXIT_FAILURE;

	if (lxc_arguments_parse(&my_args, argc, argv))
		exit(ret);

	log.name = my_args.name;
	log.file = my_args.log_file;
	log.level = my_args.log_priority;
	log.prefix = my_args.progname;
	log.quiet = my_args.quiet;
	log.lxcpath = my_args.lxcpath[0];

	if (lxc_log_init(&log))
		exit(ret);

	/* Set default timeout */
	if (my_args.timeout == -2) {
		if (my_args.hardstop)
			my_args.timeout = 0;
		else
			my_args.timeout = 60;
	}

	if (my_args.nowait)
		my_args.timeout = 0;

	/* some checks */
	if (!my_args.hardstop && my_args.timeout < -1) {
		ERROR("Invalid timeout");
		exit(ret);
	}

	if (my_args.hardstop && my_args.nokill) {
		ERROR("-k can't be used with --nokill");
		exit(ret);
	}

	if (my_args.hardstop && my_args.reboot) {
		ERROR("-k can't be used with -r");
		exit(ret);
	}

	if (my_args.hardstop && my_args.timeout) {
		ERROR("-k doesn't allow timeouts");
		exit(ret);
	}

	if (my_args.nolock && !my_args.hardstop) {
		ERROR("--nolock may only be used with -k");
		exit(ret);
	}

	c = lxc_container_new(my_args.name, my_args.lxcpath[0]);
	if (!c) {
		ERROR("Error opening container");
		goto out;
	}

	if (my_args.rcfile) {
		c->clear_config(c);

		if (!c->load_config(c, my_args.rcfile)) {
			ERROR("Failed to load rcfile");
			goto out;
		}

		c->configfile = strdup(my_args.rcfile);
		if (!c->configfile) {
			ERROR("Out of memory setting new config filename");
			goto out;
		}
	}

	if (!c->may_control(c)) {
		ERROR("Insufficent privileges to control %s", c->name);
		goto out;
	}

	if (!c->is_running(c)) {
		ERROR("%s is not running", c->name);

		/* Per our manpage we need to exit with exit code:
		 * 2: The specified container exists but was not running.
		 */
		ret = 2;
		goto out;
	}

	/* kill */
	if (my_args.hardstop) {
		ret = c->stop(c) ? EXIT_SUCCESS : EXIT_FAILURE;
		goto out;
	}

	/* reboot */
	if (my_args.reboot) {
		if (!c->reboot2(c, my_args.timeout))
			ret = EXIT_FAILURE;
		else
			ret = EXIT_SUCCESS;

		goto out;
	}

	/* shutdown */
	s = c->shutdown(c, my_args.timeout);
	if (!s) {
		if (my_args.timeout == 0)
			ret = EXIT_SUCCESS;
		else if (my_args.nokill)
			ret = EXIT_FAILURE;
		else
			ret = c->stop(c) ? EXIT_SUCCESS : EXIT_FAILURE;
	} else {
		ret = EXIT_SUCCESS;
	}

out:
	lxc_container_put(c);
	exit(ret);
}
