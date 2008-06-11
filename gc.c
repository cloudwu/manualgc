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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _MSC_VER

#define bool int
#define true 1
#define false 0
typedef int intptr_t;

#else

#include <stdbool.h>
#include <stdint.h>

#endif

#define my_malloc malloc
#define my_free free
#define my_realloc realloc

#define CACHE_BITS 10

struct link {
	int number;
	int children[1];
};

struct node {
	int mark;
	union {
		struct {
			void * mem;
			struct link *children;
			void (*finalization)(void *);
		} n;
		int free;
	} u;
};

/*	   stack data
	  +----------+
	0 | level 0  |                ----> level 0 / root ( node pack )
	1 | level 1  |                --+-> level 1 ( 1 node ref + node pack )
	2 | node ref | <-bottom       --+
	3 | 2 (lv.2) |
	4 | node ref |                --+-> level 2 ( 3 node ref )
	5 | node ref |                  |
	6 | node ref |                --+
	7 | 4 (lv.3) | <-current
	8 | node ref |                --+-> level 3 ( 2 node ref )
	9 | node ref |                --+
	10|   nil    | <-top
	11|   nil    |
	  +----------+
 */

union stack_node {
	int stack;
	int number;
	int handle;
};

struct stack {
	union stack_node *data;
	int top;
	int bottom;
	int current;
};

struct hash_node {
	int id;
	struct hash_node *next;
};

struct hash_map	{
	struct hash_node **table;
	int size;
	struct hash_node *free;
	int number;
};

#define	CACHE_SIZE (1<<CACHE_BITS)

struct cache_node {
	int parent;
	int child;
};

static struct {
	struct node *pool;
	int size;
	int free;
	int mark;
	bool cache_dirty;
	struct stack stack;
	struct hash_map map;
	struct cache_node cache[CACHE_SIZE];
} E;

#define CACHE_PARENT_BITS (CACHE_BITS/3)
#define CACHE_CHILD_BITS (CACHE_BITS-CACHE_PARENT_BITS)
#define CACHE_PARENT_MASK ((1<<CACHE_PARENT_BITS) -1 )
#define CACHE_CHILD_MASK ((1<<CACHE_CHILD_BITS) -1 )
#define UNSET_MASK 0x80000000
#define UNSET_MASK_BIT(a) ((unsigned)(a)>>31)


/* Insert a parent/child handle pair into cache	*/

static bool
cache_insert(int parent,int child)
{
	int hash = (parent & CACHE_PARENT_MASK) << CACHE_CHILD_BITS | (child & CACHE_CHILD_MASK) ;
	struct cache_node *cn = &E.cache[hash];
	E.cache_dirty=true;

	if (cn->parent == -1) {
		cn->parent=parent;
		cn->child=child;
		return true;
	}
	else if (cn->parent == parent) {
		if (cn->child == child) {
			return true;
		}
		else if ((cn->child ^ child) == UNSET_MASK) {
			cn->parent = -1;
			return true;
		}
	}
	return false;
}

/* create a new handle for pointer p */

static int
node_alloc(void *p)
{
	struct node *ret;
	if (E.free==-1) {
		int sz=E.size * 2;
		int i;
		if (sz==0) {
			sz=1024;
		}

		E.pool=(struct node *)my_realloc(E.pool,sz*sizeof(struct node));
		ret=E.pool + E.size;
		ret->u.n.children=0;

		for (i=E.size+1;i<sz;i++) {
			E.pool[i].u.free=i+1;
			E.pool[i].mark=-1;
			E.pool[i].u.n.children=0;
		}
		E.pool[sz-1].u.free=-1;
		E.free=E.size+1;
		E.size=sz;
	}
	else {
		ret=E.pool + E.free;
		E.free = E.pool[E.free].u.free;
	}
	ret->u.n.mem=p;
	ret->mark=0;
	if (ret->u.n.children) {
		ret->u.n.children->number=0;
	}
	else {
		ret->u.n.children=0;
	}
	return ret-E.pool;
}

/* expand link table space . each node has a link table. */

static struct link *
link_expand(struct link *old,int sz)
{
	struct link *ret;
	if (old!=0) {
		sz+=old->number;
		if ((sz ^ old->number) <= old->number) {
			return old;
		}
	}

	sz=sz*2-1;

	ret=(struct link *)my_realloc(old,sizeof(struct link)+(sz-1)*sizeof(int));
	if (old==0) {
		ret->number=0;
	}
	return ret;
}

/* cmp function for cache sort */

static int
cache_node_cmp(const void *a,const void *b)
{
	const struct cache_node *ca=(const struct cache_node *)a;
	const struct cache_node *cb=(const struct cache_node *)b;
	if (ca->parent != cb->parent) {
		return cb->parent - ca->parent;
	}
	if (ca->parent == -1 ) {
		return 0;
	}
	return (ca->child & ~ UNSET_MASK) - (cb->child & ~UNSET_MASK);
}

/* commit cache to the node graph */

static void
cache_flush()
{
	int i;
	if (!E.cache_dirty)
		return;
	qsort(E.cache,CACHE_SIZE,sizeof(struct cache_node),cache_node_cmp);
	i=0;
	while (i<CACHE_SIZE) {
		int parent=E.cache[i].parent;
		struct cache_node *head;
		struct cache_node *next;
		struct node *node=&E.pool[parent];
		struct link *children;
		int sz;
		int j,k;

		if (parent==-1) {
			return;
		}

		head=&E.cache[i];
		next=head;
		sz=0;

		while (next->parent == parent && i<CACHE_SIZE) {
			sz += 1 - UNSET_MASK_BIT(next->child);
			++next;
			++i;
		}

		children=node->u.n.children;

		k=j=0;

		if (children) {
			while (j<children->number) {
				int child,c;
				if (head>=next) {
					goto copy_next;
				}

				child=head->child;
				c=child & ~UNSET_MASK;
				children->children[k]=children->children[j];
				if (c==children->children[j]) {
					if (child!=c) {
						--k;
					}
					head->parent=-1;
					--sz;
					++head;
				}
				else if (c < children->children[j]) {
					break;
				}
				++j;
				++k;
			}
		}
		if (sz>0) {
			children=node->u.n.children=link_expand(node->u.n.children,sz);
			assert(children);
			memmove(children->children + j + sz, children->children +j , (children->number - j) * sizeof(int));
			j+=sz;
		}

		children->number+=sz;

		while(j<children->number) {
			int child,c;
			if (head>=next) {
				goto copy_next;
			}

			child=head->child;
			c=child & ~UNSET_MASK;
			if (c==children->children[j]) {
				if (child!=c) {
					--k;
				}
				head->parent=-1;
				++head;
			}
			else if (c < children->children[j]) {
				assert(c==child);
				children->children[k]=c;
				head->parent=-1;
				++head;
				--j;
			}
			else {
				children->children[k]=children->children[j];
			}
			++j;
			++k;
		}
		while(head<next) {
			int child=head->child;
			assert((child & UNSET_MASK) == 0);
			children->children[k]=child;
			++k;
			head->parent=-1;
			++head;
		}
		children->number=k;
		continue;
	copy_next:
		if (k!=j) {
			for (;j<children->number;j++,k++) {
				children->children[k]=children->children[j];
			}
		}
		children->number=k;
	}

	E.cache_dirty=false;
}

/* add an edge into (or delete from) graph */

static void
node_add(int parent,int child)
{
	while (!cache_insert(parent,child))	{
		cache_flush();
	}
}

/* free a node for reuse */

static __inline	void
node_free(int id)
{
	E.pool[id].mark=-1;
	if (E.pool[id].u.n.children) {
		E.pool[id].u.n.children->number=0;
	}
	E.pool[id].u.free=E.free;
	E.free=id;
}

/* create a hash value for pointer p */

static __inline	int
hash(void *p)
{
	intptr_t t=(intptr_t)p;
	return (int)((t>>2) ^ (t>>16));
}

/* expand hash map space */

static void
map_expand()
{
	struct hash_node **table;
	int sz,i;
	if (E.map.size==0) {
		sz=1024;
	}
	else {
		sz=E.map.size*2;
	}
	table=(struct hash_node	**)my_malloc(sz*sizeof(struct hash_node *));
	memset(table,0,sz*sizeof(struct hash_node *));
	for (i=0;i<E.map.size;i++) {
		struct hash_node *t=E.map.table[i];
		while (t) {
			struct hash_node *tmp=t;
			void *p=E.pool[tmp->id].u.n.mem;
			int new_hash=hash(p) & (sz-1);
			t=t->next;

			tmp->next=table[new_hash];
			table[new_hash]=tmp;
		}
	}

	my_free(E.map.table);
	E.map.table=table;
	E.map.size=sz;
}

/* map a existed pointer into a handle , if not exist, create a new one */

static int
map_id(void *p)
{
	int h=hash(p);
	struct hash_node *node=E.map.table[h & (E.map.size -1)];
	while (node) {
		if (E.pool[node->id].u.n.mem==p) {
			return node->id;
		}
		node=node->next;
	}
	if (E.map.number >= E.map.size) {
		map_expand();
	}

	if (E.map.free) {
		node=E.map.free;
		E.map.free=node->next;
	}
	else {
		node=(struct hash_node *)my_malloc(sizeof(*node));
	}
	node->id=node_alloc(p);
	node->next=E.map.table[h & (E.map.size -1)];
	E.map.table[h & (E.map.size -1)]=node;

	return node->id;
}

/* Detect whether the pointer p collected . Return 0 if the p is collected, otherwise return itself	*/

void *
gc_weak(void *p)
{
	int h=hash(p);
	struct hash_node *node=E.map.table[h & (E.map.size -1)];
	while (node) {
		if (E.pool[node->id].u.n.mem==p) {
			return p;
		}
		node=node->next;
	}
	return 0;
}

/* Erase a handle from hash map */

static void
map_erase(int id)
{
	void *p=E.pool[id].u.n.mem;
	if (p) {
		int h=hash(p);
		struct hash_node **node= &E.map.table[h & (E.map.size -1)];
		struct hash_node *find;
		while ((*node)->id!=id) {
			node=&(*node)->next;
			assert(*node);
		}
		find=*node;
		*node=find->next;
		find->next=E.map.free;
		E.map.free=find;
	}
}

/* Expand stack space */

static __inline void
stack_expand()
{
	if (((E.stack.top + 1) ^ E.stack.top) > E.stack.top) {
		E.stack.data = (union stack_node *)my_realloc(E.stack.data,(E.stack.top*2+1) * sizeof(union stack_node));
	}
}

/* Push a handle into the stack */

static void
stack_push(int handle)
{
	stack_expand();
	E.stack.data[E.stack.top++].handle=handle;
}

/* gc brackets open */

void
gc_enter()
{
	stack_expand();
	E.stack.data[E.stack.top].number=E.stack.top-E.stack.current;
	E.stack.current=E.stack.top++;
}

/* gc brackets close , extend some pointers' lifetime */

void
gc_leave(void *p,...)
{
	void **head;
	if (E.stack.current >= E.stack.bottom) {
		E.stack.top = E.stack.current;
		E.stack.current -= E.stack.data[E.stack.current].number;
	}
	else {
		int parent,child;
		--E.stack.bottom;
		parent=E.stack.data[E.stack.bottom-1].stack;
		child=E.stack.data[E.stack.bottom].stack;
		node_add(parent, child | UNSET_MASK);
		node_free(child);
		E.stack.current=E.stack.bottom-1;
		E.stack.top = E.stack.current + 1;
	}

	head=&p;

	while (*head) {
		stack_push(map_id(*head));
		++head;
	}
}

/* pack the stack recursively */

static int
stack_pack_internal(int from,int to,int top)
{
	if (to < from) {
		int parent = E.stack.data[to].stack;
		while (from < top) {
			node_add(parent,E.stack.data[from].handle);
			++from;
		}
		return to+1;
	}
	else {
		int bottom=stack_pack_internal(from,to-E.stack.data[to].number,to);
		int node=node_alloc(0);
		++to;
		while (to<top) {
			node_add(node,E.stack.data[to].handle);
			++to;
		}
		node_add(E.stack.data[bottom-1].stack,node);
		E.stack.data[bottom].stack=node;
		return bottom+1;
	}
}

/* pack the value in the stack */

static void
stack_pack()
{
	int bottom=stack_pack_internal(E.stack.bottom,E.stack.current,E.stack.top);
	E.stack.top=E.stack.bottom=bottom;
	E.stack.current=bottom-1;
}

/* link or unlink two pointer */

void
gc_link(void *parent,void *prev,void *now)
{
	int parent_id;
	if (parent==0) {
		parent_id=0;
	}
	else {
		parent_id=map_id(parent);
	}
	if (prev) {
		int prev_id=map_id(prev);
		stack_push(prev_id);
		node_add(parent_id,prev_id | UNSET_MASK);
	}
	if (now) {
		node_add(parent_id,map_id(now));
	}
}

/* init struct E */

void
gc_init()
{
	int i;
	int root;

	E.pool=0;
	E.size=0;
	E.mark=1;
	E.free=-1;
	E.cache_dirty=false;
	for (i=0;i<CACHE_SIZE;i++) {
		E.cache[i].parent=-1;
	}
	E.stack.data=0;
	E.stack.top=0;

	root=node_alloc(0);
	assert(root==0);
	stack_expand();
	E.stack.data[E.stack.top++].stack=root;

	E.stack.bottom=E.stack.top;
	E.stack.current=E.stack.bottom-1;

	E.map.table=0;
	E.map.size=0;
	E.map.free=0;
	E.map.number=0;

	map_expand();
}

/* release all the resource used in gc */

void
gc_exit()
{
	int i;
	for (i=0;i<E.size;i++) {
		my_free(E.pool[i].u.n.children);
		if (E.pool[i].mark >= 0) {
			void *p=E.pool[i].u.n.mem;
			if (p && E.pool[i].u.n.finalization) {
				E.pool[i].u.n.finalization(p);
				my_free(p);
			}
		}
	}
	my_free(E.pool);
	for	(i=0;i<E.map.size;i++) {
		struct hash_node *p=E.map.table[i];
		while (p) {
			struct hash_node *n=p->next;
			my_free(p);
			p=n;
		}
	}
	my_free(E.map.table);
	while (E.map.free) {
		struct hash_node *p=E.map.free->next;
		my_free(E.map.free);
		E.map.free=p;
	}
	my_free(E.stack.data);
}

/* mark the nodes related to root */

static void
gc_mark(int root)
{
	if (E.pool[root].mark <  E.mark) {
		struct link *children=E.pool[root].u.n.children;
		E.pool[root].mark=E.mark;
		if (children) {
			int i;
			for (i=children->number-1;i>=0;i--) {
				gc_mark(children->children[i]);
			}
		}
	}
}

/* collect the memory block can no longer be otherwise accessed */

void
gc_collect()
{
	int i;
	stack_pack();
	cache_flush();
	gc_mark(0);
	for (i=0;i<E.size;i++) {
		if (E.pool[i].mark < E.mark) {
			if (E.pool[i].mark >= 0) {
				void *p=E.pool[i].u.n.mem;
				if (p && E.pool[i].u.n.finalization) {
					E.pool[i].u.n.finalization(p);
					my_free(p);
				}
				map_erase(i);
				node_free(i);
			}
		}
	}
	E.mark++;
}

/* only for debug, print all the relationship of all nodes */

void
gc_dryrun()
{
	int i;
	printf("------dry run begin----\n");
	stack_pack();
	cache_flush();
	gc_mark(0);
	for (i=0;i<E.size;i++) {
		struct link *link=E.pool[i].u.n.children;

		if (E.pool[i].mark >= E.mark) {
			printf("%d(%p) -> ",i,E.pool[i].u.n.mem);
		}
		else if (E.pool[i].mark >= 0 ) {
			printf("x %d(%p): ",i,E.pool[i].u.n.mem);
		}
		else {
			continue;
		}
		if (link) {
			int j;
			for (j=0;j<link->number;j++) {
				printf("%d,",link->children[j]);
			}
		}

		printf("\n");
	}
	E.mark++;
	printf("------dry run end------\n");
}

/* allocate a memory block , and create a node map to it. node will link to parent */

void*
gc_malloc(size_t sz,void *parent,void (*finalization)(void *))
{
	void *ret=my_malloc(sz);
	int id=map_id(ret);
	E.pool[id].u.n.finalization=finalization;
	if (parent) {
		gc_link(parent,0,ret);
	}
	else {
		stack_push(id);
	}
	return ret;
}

/* clone a memory block , this will copy all the edges linked to orginal node */

void*
gc_clone(void *from,size_t sz)
{
	int from_id=map_id(from);
	void *to=my_malloc(sz);
	int to_id=map_id(to);
	struct link *from_children=E.pool[from_id].u.n.children;
	stack_push(to_id);

	cache_flush();
	memcpy(to,from,sz);
	E.pool[to_id].u.n.finalization=E.pool[from_id].u.n.finalization;
	E.pool[to_id].u.n.children=link_expand(E.pool[to_id].u.n.children,from_children->number);
	memcpy(E.pool[to_id].u.n.children,from_children,sizeof(*from_children) + (from_children->number-1)*sizeof(int));
	return to;
}

/* realloc a memory block , all the edages linked to it will be retained */

void*
gc_realloc(void *p,size_t sz,void *parent)
{
	void *ret;
	assert(sz>0);

	if (p==0) {
		return gc_malloc(sz,parent,0);
	}

	ret=my_realloc(p,sz);
	if (ret==p) {
		return ret;
	}
	else {
		int new_id=map_id(ret);
		int old_id=map_id(p);

		struct link *tmp=E.pool[new_id].u.n.children;
		E.pool[new_id].u.n.children=E.pool[old_id].u.n.children;
		E.pool[old_id].u.n.children=tmp;

		E.pool[new_id].u.n.finalization=E.pool[old_id].u.n.finalization;

		if (parent) {
			gc_link(parent,p,ret);
		}
		else {
			stack_push(new_id);
		}
	}

	return ret;
}
