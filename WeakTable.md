Weak table is a container which can hold some weak pointers. That is to say, the pointers in the weak table can be collected by collector. We can iterate the table to know which pointers are alive.

For example,

```

struct gc_weak_table * table=gc_weak_table(0);
int i;
void *p;

for (i=0;i<10;i++) {
/* allocate a memory block of 4 bytes , and link to table */
  gc_malloc(4,table,0);
}

/* Print 10 pointers link to the table */
i=0;

while ((p=gc_weak_next(table,&i))!=0) {
  printf("%p is alive\n",p);
}

gc_collect();

/* You will see nothing link to the table after gc_collect */
i=0;

while ((p=gc_weak_next(table,&i))!=0) {
  printf("%p is alive\n",p);
}

```

Weak table usually hold only one pointer. You can use `gc_weak_next(table,0)` (pass 0 as the iterator) to get the first pointer in the table. If the return value is 0 , the pointer held by weak table has been collected, otherwise it will return the pointer you linked to it before.




