#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/prinfo.h>
#include <asm/uaccess.h>

void copy_into_prinfo(struct prinfo *pi, struct task_struct *p)
{
	pi->pid = p->pid;
	strcpy(pi->comm, p->comm);
	pi->parent_pid = p->parent->pid;
	if (!list_empty(&p->children))
		pi->first_child_pid = list_entry(p->children.next,
				struct task_struct, sibling)->pid;
	else
		pi->first_child_pid = 0;
	if (p->parent == p)
		pi->next_sibling_pid = 0;
	else if (!list_is_last(&p->sibling, &p->parent->children))
		pi->next_sibling_pid = list_entry(p->sibling.next,
				struct task_struct, sibling)->pid;
	else
		pi->next_sibling_pid = 0;
	pi->state = p->state;
	pi->uid = p->cred->uid;
}

int tasks_dfs_copy(struct task_struct *task, struct prinfo *pi, int nr)
{
	struct task_struct *cur = task;
	int ret = 0;
	int flag = 1;

	while (1) {
		if (thread_group_leader(cur)) {
			if (ret < nr) {
				copy_into_prinfo(pi, cur);
				pi++;
			}
			ret++;
		}

		if (!list_empty(&cur->children)) {
			cur = list_entry(cur->children.next,
					struct task_struct, sibling);
			continue;
		}
		if (!list_is_last(&cur->sibling, &cur->parent->children)) {
			cur = list_entry(cur->sibling.next,
					struct task_struct, sibling);
			continue;
		}
		if (cur->parent->parent == cur->parent)
			break;
		while (list_is_last(&cur->parent->sibling,
					&cur->parent->parent->children)) {
			cur = cur->parent;
			if (cur->parent->parent == cur->parent) {
				flag = 0;
				break;
			}
		}
		if (!flag)
			break;
		cur = list_entry(cur->parent->sibling.next,
				struct task_struct, sibling);
	}
	return ret;
}

SYSCALL_DEFINE2(ptree, struct prinfo __user *, buf, int __user *, nr) {
	unsigned long bufsz;
	struct prinfo *pi, *pi_head;
	int pnr, ret = 0;
	struct task_struct *task;

	read_lock(&tasklist_lock);
	task = &init_task;
	while (task->parent != task)
		task = task->parent;
	read_unlock(&tasklist_lock);

	/* Operate on buf and nr */
	if (!buf || !nr)
		return -EINVAL;
	if (copy_from_user(&pnr, nr, sizeof(int)))
		return -EFAULT;
	if (pnr < 1)
		return -EINVAL;
	bufsz = pnr * sizeof(struct prinfo);
	pi = kmalloc(bufsz, GFP_KERNEL);
	if (copy_from_user(pi, buf, bufsz))
		return -EFAULT;

	pi_head = pi;
	read_lock(&tasklist_lock);
	ret = tasks_dfs_copy(task, &pi[0], pnr);
	read_unlock(&tasklist_lock);

	if (copy_to_user(buf, pi_head, bufsz))
		return -EFAULT;
	if (ret < pnr)
		if (copy_to_user(nr, &ret, sizeof(int)))
			return -EFAULT;
	kfree(pi);
	return ret;
}
