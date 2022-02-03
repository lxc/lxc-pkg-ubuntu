/* SPDX-License-Identifier: LGPL-2.1+ */

#include "config.h"

#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "lxc.h"

#include "arguments.h"
#include "log.h"

lxc_log_define(lxc_wait, lxc);

static int my_parser(struct lxc_arguments *args, int c, char *arg);
static int my_checker(const struct lxc_arguments *args);

static const struct option my_longopts[] = {
	{"state", required_argument, 0, 's'},
	{"timeout", required_argument, 0, 't'},
	LXC_COMMON_OPTIONS
};

static struct lxc_arguments my_args = {
	.progname     = "lxc-wait",
	.help         = "\
--name=NAME --state=STATE\n\
\n\
lxc-wait waits for NAME container state to reach STATE\n\
\n\
Options :\n\
  -n, --name=NAME   NAME of the container\n\
  -s, --state=STATE ORed states to wait for\n\
                    STOPPED, STARTING, RUNNING, STOPPING,\n\
                    ABORTING, FREEZING, FROZEN, THAWED\n\
  -t, --timeout=TMO Seconds to wait for state changes\n\
  --rcfile=FILE     Load configuration file FILE\n",
	.options      = my_longopts,
	.parser       = my_parser,
	.checker      = my_checker,
	.log_priority = "ERROR",
	.log_file     = "none",
	.timeout      = -1,
};

static int my_parser(struct lxc_arguments *args, int c, char *arg)
{
	switch (c) {
	case 's':
		args->states = optarg;
		break;
	case 't':
		args->timeout = atol(optarg);
		break;
	}

	return 0;
}

static int my_checker(const struct lxc_arguments *args)
{
	if (!args->states) {
		ERROR("Missing state option to wait for");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct lxc_container *c;
	struct lxc_log log;

	if (lxc_arguments_parse(&my_args, argc, argv))
		exit(EXIT_FAILURE);

	log.name = my_args.name;
	log.file = my_args.log_file;
	log.level = my_args.log_priority;
	log.prefix = my_args.progname;
	log.quiet = my_args.quiet;
	log.lxcpath = my_args.lxcpath[0];

	if (lxc_log_init(&log))
		exit(EXIT_FAILURE);

	c = lxc_container_new(my_args.name, my_args.lxcpath[0]);
	if (!c)
		exit(EXIT_FAILURE);

	if (!c->may_control(c)) {
		ERROR("Insufficent privileges to control %s", c->name);
		lxc_container_put(c);
		exit(EXIT_FAILURE);
	}

	if (my_args.rcfile) {
		c->clear_config(c);

		if (!c->load_config(c, my_args.rcfile)) {
			ERROR("Failed to load rcfile");
			lxc_container_put(c);
			exit(EXIT_FAILURE);
		}

		c->configfile = strdup(my_args.rcfile);
		if (!c->configfile) {
			ERROR("Out of memory setting new config filename");
			lxc_container_put(c);
			exit(EXIT_FAILURE);
		}
	}

	if (!c->wait(c, my_args.states, my_args.timeout)) {
		lxc_container_put(c);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
