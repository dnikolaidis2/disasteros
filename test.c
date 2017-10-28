#include "util.h"
#include "kernel_sched.h"

#define MFQ_QUEUES 10

rlnode SCHED [MFQ_QUEUES];                         /* The scheduler queue */
rlnode TIMEOUT_LIST;				  /* The list of threads with a timeout */

int main(int argc, char const *argv[])
{
	for (int i = 0; i < MFQ_QUEUES; ++i)
	{
	  rlnode_init(&SCHED[i], NULL);
	}
	rlnode_init(&TIMEOUT_LIST, NULL);

	TCB tcb0 = {0};
	rlnode_init(& tcb0.sched_node, &tcb0);  /* Intrusive list node */
	TCB tcb1 = {0};
	rlnode_init(& tcb1.sched_node, &tcb1);  /* Intrusive list node */
	TCB tcb2 = {0};
	rlnode_init(& tcb2.sched_node, &tcb2);  /* Intrusive list node */
	TCB tcb3 = {0};
	rlnode_init(& tcb3.sched_node, &tcb3);  /* Intrusive list node */
	TCB tcb4 = {0};
	rlnode_init(& tcb4.sched_node, &tcb4);  /* Intrusive list node */
	TCB tcb5 = {0};
	rlnode_init(& tcb5.sched_node, &tcb5);  /* Intrusive list node */
	TCB tcb6 = {0};
	rlnode_init(& tcb6.sched_node, &tcb6);  /* Intrusive list node */
	TCB tcb7 = {0};
	rlnode_init(& tcb7.sched_node, &tcb7);  /* Intrusive list node */

	rlist_push_back(& SCHED[0], & tcb0.sched_node);
	rlist_push_back(& SCHED[0], & tcb1.sched_node);
	rlist_push_back(& SCHED[2], & tcb2.sched_node);
	rlist_push_back(& SCHED[4], & tcb3.sched_node);
	rlist_push_back(& SCHED[4], & tcb4.sched_node);
	rlist_push_back(& SCHED[4], & tcb5.sched_node);
	rlist_push_back(& SCHED[8], & tcb6.sched_node);
	rlist_push_back(& SCHED[9], & tcb7.sched_node);

	for (int i = 0; i < MFQ_QUEUES; ++i)
	{
		printf("%d\n" ,rlist_len(&SCHED[i]));
	}

	printf("\n");

	for (int i = 1; i < MFQ_QUEUES; ++i)
	{
		rlist_append(&SCHED[0], &SCHED[i]);
	}

	rlnode* list = &SCHED[0];
	rlnode* p = list->next;
	while(p->next!=list) {
		(p->tcb)->priority = 0;
		p = p->next;
	}

	printf("\n");

	for (int i = 0; i < MFQ_QUEUES; ++i)
	{
		printf("%d\n" ,rlist_len(&SCHED[i]));
	}

	return 0;
}