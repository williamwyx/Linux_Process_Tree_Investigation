#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "prinfo.h"

#define MAX 100

static struct prinfo *stack[MAX];
static int stack_idx = -1;

void stack_push(struct prinfo *v)
{
	stack[++stack_idx] = v;
}

int stack_pop_indent(int parent_pid)
{
	int i;

	for (i = stack_idx; i > -1; i--) {
		if (parent_pid == stack[i]->pid)
			break;
	}
	stack_idx = i;
	return i + 1;
}

int main(int argc, char **argv)
{
	int i, j, nr = MAX;
	struct prinfo *p = malloc(nr * sizeof(struct prinfo));
	int ret = syscall(223, &p[0], &nr);

	if (ret < 0) {
		perror("error");
		return -1;
	}
	for (i = 0; i < nr; i++) {
		for (j = 0; j < stack_pop_indent(p[i].parent_pid); j++)
			printf("\t");
		stack_push(&p[i]);
		printf("%s,%d,%ld,%d,%d,%d,%ld\n", p[i].comm, p[i].pid,
				p[i].state, p[i].parent_pid,
				p[i].first_child_pid,
				p[i].next_sibling_pid, p[i].uid);
	}
	return 0;
}
