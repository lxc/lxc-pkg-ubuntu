/* SPDX-License-Identifier: LGPL-2.1+ */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lxc.h"

#include "arguments.h"
#include "list.h"
#include "log.h"
#include "utils.h"

lxc_log_define(lxc_autostart, lxc);

static struct lxc_list *accumulate_list(char *input, char *delimiter, struct lxc_list *str_list);

struct lxc_list *cmd_groups_list = NULL;

static int my_parser(struct lxc_arguments *args, int c, char *arg)
{
	switch (c) {
	case 'k':
		args->hardstop = 1;
		break;
	case 'L':
		args->list = 1;
		break;
	case 'r':
		args->reboot = 1;
		break;
	case 's':
		args->shutdown = 1;
		break;
	case 'a':
		args->all = 1;
		break;
	case 'A':
		args->ignore_auto = 1;
		break;
	case 'g':
		cmd_groups_list = accumulate_list(arg, ",", cmd_groups_list);
		break;
	case 't':
		if (lxc_safe_long(arg, &args->timeout) < 0)
			return -1;
		break;
	}
	return 0;
}

static const struct option my_longopts[] = {
	{"kill", no_argument, 0, 'k'},
	{"list", no_argument, 0, 'L'},
	{"reboot", no_argument, 0, 'r'},
	{"shutdown", no_argument, 0, 's'},
	{"all", no_argument, 0, 'a'},
	{"ignore-auto", no_argument, 0, 'A'},
	{"groups", required_argument, 0, 'g'},
	{"timeout", required_argument, 0, 't'},
	{"help", no_argument, 0, 'h'},
	LXC_COMMON_OPTIONS
};

static struct lxc_arguments my_args = {
	.progname = "lxc-autostart",
	.help     = "\
\n\
lxc-autostart managed auto-started containers\n\
\n\
Options:\n\
  -k, --kill        kill the containers instead of starting them\n\
  -L, --list        list all affected containers and wait delay\n\
  -r, --reboot      reboot the containers instead of starting them\n\
  -s, --shutdown    shutdown the containers instead of starting them\n\
\n\
  -a, --all         list all auto-started containers (ignore groups)\n\
  -A, --ignore-auto ignore lxc.start.auto and select all matching containers\n\
  -g, --groups      list of groups (comma separated) to select\n\
  -t, --timeout=T   wait T seconds before hard-stopping\n",
	.options  = my_longopts,
	.parser   = my_parser,
	.checker  = NULL,
	.timeout = 60,
};

static int list_contains_entry(char *str_ptr, struct lxc_list *p1) {
	struct lxc_list *it1;

	/*
	 * If the entry is NULL or the empty string and the list
	 * is NULL, we have a match
	 */
	if (!p1 && (!str_ptr || !*str_ptr))
		return 1;

	if (!p1)
		return 0;

	lxc_list_for_each(it1, p1) {
		if (strncmp(it1->elem, str_ptr, strlen(it1->elem)) == 0)
			return 1;
	}

	return 0;
}

/* This is a variation of get_list below it. This version allows two additional
 * features. If a list is passed to it, it adds to it. It allows for empty
 * entries (i.e. "group1,,group2") generating and empty list entry.
 */
static struct lxc_list *accumulate_list(char *input, char *delimiter,
					struct lxc_list *str_list)
{
	char *workstr = NULL;
	char *workptr = NULL;
	char *next_ptr = NULL;
	struct lxc_list *worklist;
	struct lxc_list *workstr_list;

	workstr = strdup(input);
	if (!workstr)
		return NULL;

	workstr_list = str_list;
	if (!workstr_list) {
		workstr_list = malloc(sizeof(*workstr_list));
		if (!workstr_list) {
			free(workstr);
			return NULL;
		}

		lxc_list_init(workstr_list);
	}

	for (workptr = workstr; workptr; workptr = next_ptr) {
		/* We can't use strtok_r here because it collapses multiple
		 * delimiters into 1 making empty fields impossible...
		 */
		next_ptr = strchr(workptr, *delimiter);
		if (next_ptr)
			*next_ptr++ = '\0';

		/* At this point, we'd like to check to see if this group is
		 * already contained in the list and ignore it if it is...  This
		 * also helps us with any corner cases where a string begins or
		 * ends with a delimiter.
		 */
		if (list_contains_entry(workptr, workstr_list)) {
			if (*workptr)
				ERROR("Duplicate group \"%s\" in list - ignoring", workptr);
			else
				ERROR("Duplicate NULL group in list - ignoring");
		} else {
			worklist = malloc(sizeof(*worklist));
			if (!worklist)
				break;

			worklist->elem = strdup(workptr);
			if (!worklist->elem) {
				free(worklist);
				break;
			}

			lxc_list_add_tail(workstr_list, worklist);
		}
	}

	free(workstr);

	return workstr_list;
}

static struct lxc_list *get_list(char *input, char *delimiter)
{
	char *workstr = NULL;
	char *token = NULL;
	struct lxc_list *worklist;
	struct lxc_list *workstr_list;

	workstr_list = malloc(sizeof(*workstr_list));
	if (!workstr_list)
		return NULL;

	lxc_list_init(workstr_list);

	workstr = strdup(input);
	if (!workstr) {
		free(workstr_list);
		return NULL;
	}

	lxc_iterate_parts(token, workstr, delimiter) {
		worklist = malloc(sizeof(*worklist));
		if (!worklist)
			break;

		worklist->elem = strdup(token);
		if (!worklist->elem) {
			free(worklist);
			break;
		}

		lxc_list_add_tail(workstr_list, worklist);
	}

	free(workstr);

	return workstr_list;
}

static struct lxc_list *get_config_list(struct lxc_container *c, char *key)
{
	int len = 0;
	char *value = NULL;
	struct lxc_list *config_list = NULL;

	len = c->get_config_item(c, key, NULL, 0);
	if (len < 0)
		return NULL;

	value = (char *)malloc(sizeof(char) * len + 1);
	if (!value)
		return NULL;

	if (c->get_config_item(c, key, value, len + 1) != len) {
		free(value);
		return NULL;
	}

	if (strlen(value) == 0) {
		free(value);
		return NULL;
	}

	config_list = get_list(value, "\n");
	free(value);

	return config_list;
}

static int get_config_integer(struct lxc_container *c, char *key)
{
	int len = 0, ret = 0;
	char *value = NULL;

	len = c->get_config_item(c, key, NULL, 0);
	if (len < 0)
		return 0;

	value = (char *)malloc(sizeof(char) * len + 1);
	if (!value)
		return 0;

	if (c->get_config_item(c, key, value, len + 1) != len) {
		free(value);
		return 0;
	}

	if (lxc_safe_int(value, &ret) < 0)
		printf("Could not parse config item.\n");

	free(value);

	return ret;
}

static int cmporder(const void *p1, const void *p2)
{
	struct lxc_container *c1 = *(struct lxc_container **)p1;
	struct lxc_container *c2 = *(struct lxc_container **)p2;

	int c1_order = get_config_integer(c1, "lxc.start.order");
	int c2_order = get_config_integer(c2, "lxc.start.order");

	if (c1_order == c2_order)
		return strncmp(c1->name, c2->name, strlen(c1->name));

	return (c1_order - c2_order);
}

static int cmporder_reverse(const void *p1, const void *p2)
{
	return -1 * cmporder(p1, p2);
}

static int toss_list(struct lxc_list *c_groups_list)
{
	struct lxc_list *it, *next;

	if (!c_groups_list)
		return 1;

	lxc_list_for_each_safe(it, c_groups_list, next) {
		lxc_list_del(it);
		free(it->elem);
		free(it);
	}
	free(c_groups_list);

	return 1;
}

int main(int argc, char *argv[])
{
	int count = 0, failed = 0, i = 0, ret = 0;
	struct lxc_list *cmd_group;
	struct lxc_container **containers = NULL;
	struct lxc_list **c_groups_lists = NULL;
	struct lxc_log log;

	if (lxc_arguments_parse(&my_args, argc, argv))
		exit(EXIT_FAILURE);

	/* Only create log if explicitly instructed */
	if (my_args.log_file || my_args.log_priority) {
		log.name = my_args.name;
		log.file = my_args.log_file;
		log.level = my_args.log_priority;
		log.prefix = my_args.progname;
		log.quiet = my_args.quiet;
		log.lxcpath = my_args.lxcpath[0];

		if (lxc_log_init(&log))
			exit(EXIT_FAILURE);
	}

	count = list_defined_containers(my_args.lxcpath[0], NULL, &containers);

	if (count < 0)
		exit(EXIT_FAILURE);

	/* Allocate an array for our container group lists */
	if (!my_args.all)
		c_groups_lists = calloc( count, sizeof( struct lxc_list * ) );

	qsort(&containers[0], count, sizeof(struct lxc_container *),
	      (my_args.shutdown || my_args.hardstop) ? cmporder_reverse : cmporder);

	if (cmd_groups_list && my_args.all)
		ERROR("Specifying -a (all) with -g (groups) doesn't make sense. All option overrides");

	/* We need a default cmd_groups_list even for the -a
	 * case in order to force a pass through the loop for
	 * the NULL group.  This, someday, could be taken from
	 * a config file somewhere...
	 */
	if (!cmd_groups_list)
		cmd_groups_list = accumulate_list( "" , ",", NULL );

	failed = 0;
	lxc_list_for_each(cmd_group, cmd_groups_list) {
		/* Because we may take several passes through the container list
		 * We'll switch on if the container pointer is NULL and if we
		 * process a container (run it or decide to ignore it) and call
		 * lxc_container_put then we'll NULL it out and not check it
		 * again.
		 */
		for (i = 0; i < count; i++) {
			struct lxc_container *c = containers[i];

			if (!c)
				/* Skip - must have been already processed */
				continue;

			/* We haven't loaded the container groups yet so these
			 * next two checks don't need to free them if they fail.
			 * They'll fail on the first pass.
			 */
			if (!c->may_control(c)) {
				/* We're done with this container */
				if (lxc_container_put(c) > 0)
					containers[i] = NULL;

				continue;
			}

			if (!my_args.ignore_auto &&
			    get_config_integer(c, "lxc.start.auto") != 1) {
				/* We're done with this container */
				if (lxc_container_put(c) > 0)
					containers[i] = NULL;

				continue;
			}

			if (!my_args.all) {
				/* Filter by group */
				if (!c_groups_lists[i]) {
					/* Now we're loading up a container's groups */
					c_groups_lists[i] = get_config_list(c, "lxc.group");
				}

				ret = list_contains_entry(cmd_group->elem, c_groups_lists[i]);
				if (ret == 0) {
					/* Not in the target group this pass so
					 * leave in the list for subsequent
					 * passes.
					 */
					continue;
				}
			}

			/* We have a candidate container to process */
			c->want_daemonize(c, 1);

			if (my_args.shutdown) {
				/* Shutdown the container */
				if (c->is_running(c)) {
					if (my_args.list) {
						printf("%s\n", c->name);
						fflush(stdout);
					}
					else {
						if (!c->shutdown(c, my_args.timeout)) {
							if (!c->stop(c)) {
								failed++;
								ERROR("Error shutting down container: %s", c->name);
							}
						}
					}
				}
			} else if (my_args.hardstop) {
				/* Kill the container */
				if (c->is_running(c)) {
					if (my_args.list) {
						printf("%s\n", c->name);
						fflush(stdout);
					}
					else {
						if (!c->stop(c)) {
							failed++;
							ERROR("Error killing container: %s", c->name);
						}
					}
				}
			} else if (my_args.reboot) {
				/* Reboot the container */
				if (c->is_running(c)) {
					if (my_args.list) {
						printf("%s %d\n", c->name,
						       get_config_integer(c, "lxc.start.delay"));
						fflush(stdout);
					}
					else {
						if (!c->reboot(c)) {
							failed++;
							ERROR("Error rebooting container: %s", c->name);
						} else {
							sleep(get_config_integer(c, "lxc.start.delay"));
						}
					}
				}
			} else {
				/* Start the container */
				if (!c->is_running(c)) {
					if (my_args.list) {
						printf("%s %d\n", c->name,
						       get_config_integer(c, "lxc.start.delay"));
						fflush(stdout);
					}
					else {
						if (!c->start(c, 0, NULL)) {
							failed++;
							ERROR("Error starting container: %s", c->name);
						} else {
							sleep(get_config_integer(c, "lxc.start.delay"));
						}
					}
				}
			}

			/*
			 * If we get this far and we haven't hit any skip "continue"
			 * then we're done with this container...  We can dump any
			 * c_groups_list and the container itself.
			 */
			if (lxc_container_put(c) > 0)
				containers[i] = NULL;

			if (c_groups_lists) {
				toss_list(c_groups_lists[i]);
				c_groups_lists[i] = NULL;
			}
		}

	}

	/* clean up any lingering detritus, if container exists here
	 * then it must have failed to start.
	 */
	for (i = 0; i < count; i++) {
		if (containers[i])
			lxc_container_put(containers[i]);
		if (c_groups_lists && c_groups_lists[i])
			toss_list(c_groups_lists[i]);
	}

	free(c_groups_lists);
	toss_list(cmd_groups_list);
	free(containers);

	if (failed == count)
		exit(EXIT_FAILURE);	/* Total failure */
	else if (failed > 0)
		exit(2);	/* Partial failure */

	exit(EXIT_SUCCESS);
}
