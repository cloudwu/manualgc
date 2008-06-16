/*
 *	Copyright (c) 2008 , 
 *		Cloud Wu . All rights reserved.
 *
 *		http://www.codingnow.com
 *
 *	Use, modification and distribution are subject to the "New BSD License"
 *	as listed at <url: http://www.opensource.org/licenses/bsd-license.php >.
 */

#include "gc.h"
#include <stdlib.h>
#include <stdio.h>

struct test {
	struct test *next;
};

static void
log_ptr(void *p)
{
	printf("free %p\n",p);
}

static struct test *
new_test(struct test *parent)
{
	struct test *ret=(struct test*)gc_malloc(sizeof(struct test),parent,log_ptr);
	printf("new %p\n",ret);
	if (parent) {
		ret->next=parent->next;
		parent->next=ret;
	}
	else {
		ret->next=0;
	}
	return ret;
}

static void *
test(struct gc_weak_table *weak)
{
	struct test *p;
	int i;

	gc_enter();

	gc_enter();
		for (i=0;i<4;i++) {
			p=new_test(0);
			gc_link(weak,0,p);
		}

	/* after gc_leave , only last p leave in the stack */
	gc_leave(p,0);

	/* p will not be collected */
	gc_collect();

	p->next=new_test(p);

	gc_dryrun();

	/* p will not be exist on the stack after gc_leave , it can be collected. */
	gc_leave(p->next,0);

	gc_link(weak,0,p->next);

	return p->next;
}

static void
iterate_weak_table(struct gc_weak_table *weak)
{
	int iter=0;
	void *p;
	while ((p=gc_weak_next(weak,&iter)) != 0) {
		printf("%p is alive\n",p);
	}
}

int	
main()
{
	struct test *p;
	struct gc_weak_table *weak;
	gc_init();

	weak=gc_weak_table(0);
	p=test(weak);

	printf("%p is in weak table\n",gc_weak_next(weak,0));

	gc_collect();

	iterate_weak_table(weak);

	gc_dryrun();

	gc_exit();
	return 0;
}

