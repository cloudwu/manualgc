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

struct test {
	struct test *next;
};

struct test *
new_test()
{
	struct test *ret=(struct test*)gc_malloc(sizeof(struct test));
	ret->next=0;
	return ret;
}

void *
test()
{
	struct test *p;

	gc_enter();
	
	p=new_test();

	/* p and q will not be collected */
	gc_collect();
	gc_print();

	p->next=new_test();
	gc_link(p,0,p->next);

	gc_print();

	/* p will be collect after gc_leave, but p->next should not. */
	gc_leave(p->next,0);
	return p->next;
}

int 
main()
{
	gc_init();
	test();
	gc_collect();
	gc_exit();
	return 0;
}

