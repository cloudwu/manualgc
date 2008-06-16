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
test()
{
	struct test *p;
	int i;

	gc_enter();

	gc_enter();
		for (i=0;i<4;i++) {
			p=new_test(0);
		}

	/* after gc_leave , only last p leave in the stack */
	gc_leave(p,0);

	/* p will not be collected */
	gc_collect();

	p->next=new_test(p);

	gc_dryrun();

	/* p will not be exist on the stack after gc_leave , it can be collected. */
	gc_leave(p->next,0);

	return p->next;
}

int	
main()
{
	struct test *p;
	gc_init();

	p=test();

	gc_collect();

	gc_exit();
	return 0;
}

