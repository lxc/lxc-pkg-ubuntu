/* SPDX-License-Identifier: LGPL-2.1+ */

#include "config.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "lxc.h"

#include "arguments.h"
#include "log.h"

lxc_log_define(lxc_cgroup, lxc);

static int my_checker(const struct lxc_arguments *args);

static const struct option my_longopts[] = {
	LXC_COMMON_OPTIONS
};

static struct lxc_arguments my_args = {
	.progname     = "lxc-cgroup",
	.help         = "\
--name=NAME state-object [value]\n\
\n\
Get or set the value of a state object (for example, 'cpuset.cpus')\n\
in the container's cgroup for the corresponding subsystem.\n\
\n\
Options :\n\
  -n, --name=NAME      NAME of the container\n\
  --rcfile=FILE        Load configuration file FILE\n",
	.options      = my_longopts,
	.parser       = NULL,
	.checker      = my_checker,
	.log_priority = "ERROR",
	.log_file     = "none",
};

static int my_checker(const struct lxc_arguments *args)
{
	if (!args->argc) {
		ERROR("Missing state object");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	char *state_object = NULL, *value = NULL;
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

	state_object = my_args.argv[0];

	c = lxc_container_new(my_args.name, my_args.lxcpath[0]);
	if (!c)
		exit(EXIT_FAILURE);

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

	if (!c->may_control(c)) {
		ERROR("Insufficent privileges to control %s:%s", my_args.lxcpath[0], my_args.name);
		lxc_container_put(c);
		exit(EXIT_FAILURE);
	}

	if (!c->is_running(c)) {
		ERROR("'%s:%s' is not running", my_args.lxcpath[0], my_args.name);
		lxc_container_put(c);
		exit(EXIT_FAILURE);
	}

	if ((my_args.argc) > 1) {
		value = my_args.argv[1];

		if (!c->set_cgroup_item(c, state_object, value)) {
			ERROR("Failed to assign '%s' value to '%s' for '%s'",
			      value, state_object, my_args.name);
			lxc_container_put(c);
			exit(EXIT_FAILURE);
		}
	} else {
		char buffer[PATH_MAX];
		int ret;

		ret = c->get_cgroup_item(c, state_object, buffer, PATH_MAX);
		if (ret < 0) {
			ERROR("Failed to retrieve value of '%s' for '%s:%s'",
			      state_object, my_args.lxcpath[0], my_args.name);
			lxc_container_put(c);
			exit(EXIT_FAILURE);
		}

		printf("%*s\n", ret, buffer);
	}

	lxc_container_put(c);

	exit(EXIT_SUCCESS);
}
