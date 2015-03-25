Some example codes show how to use **gc** to implement a _vector_ of int .

```

struct vector {
	size_t size;
	int *data;
};

struct vector*
vector_create(void *parent)
{
	struct vector *vec;

	vec=(struct vector *)gc_malloc(sizeof(*vec),parent,0);
	vec->size=0;
	vec->data=0;

	return vec;
}

int * 
vector_expand(struct vector *vec,size_t sz)
{
	if (((vec->size + sz) ^ vec->size) > vec->size) {
		sz+=vec->size;
		vec->data=gc_realloc(vec->data,(sz*2-1)*sizeof(int),vec);
	}

	return vec->data;
}

void
vector_pushback(struct vector *vec,int a)
{
	vector_expand(vec,1)[vec->size++]=a;
}

```