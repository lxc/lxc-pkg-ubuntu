/* SPDX-License-Identifier: LGPL-2.1+ */

#ifndef __LXC_LIST_H
#define __LXC_LIST_H

#include "config.h"

#include <stdio.h>

#include "memory_utils.h"

struct lxc_list {
	void *elem;
	struct lxc_list *next;
	struct lxc_list *prev;
};

#define lxc_init_list(l)                                                       \
	{                                                                      \
		.next = l, .prev = l                                           \
	}

/*
 * Iterate through an lxc list. An example for an idiom would be:
 *
 * struct lxc_list *iterator;
 * lxc_list_for_each(iterator, list) {
 *        type *tmp;
 * 	  tmp = iterator->elem;
 * }
 */
#define lxc_list_for_each(__iterator, __list)                                  \
	for (__iterator = (__list)->next; __iterator != __list;                \
	     __iterator = __iterator->next)

/* Iterate safely through an lxc list. An example for an appropriate use case
 * would be:
 *
 * struct lxc_list *cur, *next;
 * lxc_list_for_each_safe(cur, list, next) {
 *        type *tmp;
 * 	  tmp = cur->elem;
 * }
 */
#define lxc_list_for_each_safe(__iterator, __list, __next)                     \
	for (__iterator = (__list)->next, __next = __iterator->next;           \
	     __iterator != __list; __iterator = __next, __next = __next->next)

/* Initialize list. */
static inline void lxc_list_init(struct lxc_list *list)
{
	list->elem = NULL;
	list->next = list->prev = list;
}

/* Add an element to a list. See lxc_list_add() and lxc_list_add_tail() for an
 * idiom.
 */
static inline void lxc_list_add_elem(struct lxc_list *list, void *elem)
{
	list->elem = elem;
}

/* Retrieve first element of list. */
static inline void *lxc_list_first_elem(const struct lxc_list *list)
{
	return list->next->elem;
}

/* Retrieve last element of list. */
static inline void *lxc_list_last_elem(const struct lxc_list *list)
{
	return list->prev->elem;
}

/* Determine if list is empty. */
static inline int lxc_list_empty(const struct lxc_list *list)
{
	return list == list->next;
}

/* Workhorse to be called from lxc_list_add() and lxc_list_add_tail(). */
static inline void __lxc_list_add(struct lxc_list *new, struct lxc_list *prev,
				  struct lxc_list *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/* Idiom to add an element to the beginning of an lxc list:
 *
 *	struct lxc_list *tmp = malloc(sizeof(*tmp));
 *	if (tmp == NULL)
 *		return 1;
 *	lxc_list_add_elem(tmp, elem);
 *	lxc_list_add(list, tmp);
 */
static inline void lxc_list_add(struct lxc_list *head, struct lxc_list *list)
{
	__lxc_list_add(list, head, head->next);
}

/* Idiom to add an element to the end of an lxc list:
 *
 *	struct lxc_list *tmp = malloc(sizeof(*tmp));
 *	if (tmp == NULL)
 *		return 1;
 *	lxc_list_add_elem(tmp, elem);
 *	lxc_list_add_tail(list, tmp);
 */
static inline void lxc_list_add_tail(struct lxc_list *head,
				     struct lxc_list *list)
{
	__lxc_list_add(list, head->prev, head);
}

/* Idiom to remove an element from a list:
 * struct lxc_list *cur, *next;
 * lxc_list_for_each_safe(cur, list, next) {
 * 	  lxc_list_del(cur);
 * 	  free(cur->elem);
 * 	  free(cur);
 * }
 */
static inline void lxc_list_del(struct lxc_list *list)
{
	struct lxc_list *next, *prev;

	next = list->next;
	prev = list->prev;
	next->prev = prev;
	prev->next = next;
}

/* Return length of the list. */
static inline size_t lxc_list_len(struct lxc_list *list)
{
	size_t i = 0;
	struct lxc_list *iter;

	lxc_list_for_each(iter, list) {
		i++;
	}

	return i;
}

static inline struct lxc_list *lxc_list_new(void)
{
	struct lxc_list *l;

	l = zalloc(sizeof(struct lxc_list));
	if (l)
		lxc_list_init(l);
	return l;
}

#endif /* __LXC_LIST_H */
