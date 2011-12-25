/*
 * memory buffer pool support
 */
#ifndef _LINUX_MEMPOOL_H
#define _LINUX_MEMPOOL_H

#include <linux/wait.h>
#include <linux/workqueue.h>

struct kmem_cache;

typedef void * (mempool_alloc_t)(gfp_t gfp_mask, void *pool_data);
typedef void (mempool_free_t)(void *element, void *pool_data);

typedef struct mempool_s {
	spinlock_t lock;
	int min_nr;		/* nr of elements at *elements */
	int curr_nr;		/* Current nr of elements at *elements */
	void **elements;

	void *pool_data;
	mempool_alloc_t *alloc;
	mempool_free_t *free;
	wait_queue_head_t wait;
} mempool_t;

extern mempool_t *mempool_create(int min_nr, mempool_alloc_t *alloc_fn,
			mempool_free_t *free_fn, void *pool_data);
extern mempool_t *mempool_create_node(int min_nr, mempool_alloc_t *alloc_fn,
			mempool_free_t *free_fn, void *pool_data, int nid);

extern int mempool_resize(mempool_t *pool, int new_min_nr, gfp_t gfp_mask);
extern void mempool_destroy(mempool_t *pool);
extern void * mempool_alloc(mempool_t *pool, gfp_t gfp_mask);
extern void mempool_free(void *element, mempool_t *pool);

/*
 * A mempool_alloc_t and mempool_free_t that get the memory from
 * a slab that is passed in through pool_data.
 */
void *mempool_alloc_slab(gfp_t gfp_mask, void *pool_data);
void mempool_free_slab(void *element, void *pool_data);
static inline mempool_t *
mempool_create_slab_pool(int min_nr, struct kmem_cache *kc)
{
	return mempool_create(min_nr, mempool_alloc_slab, mempool_free_slab,
			      (void *) kc);
}

/*
 * a mempool_alloc_t and a mempool_free_t to kmalloc and kfree the
 * amount of memory specified by pool_data
 */
void *mempool_kmalloc(gfp_t gfp_mask, void *pool_data);
void mempool_kfree(void *element, void *pool_data);
static inline mempool_t *mempool_create_kmalloc_pool(int min_nr, size_t size)
{
	return mempool_create(min_nr, mempool_kmalloc, mempool_kfree,
			      (void *) size);
}

/*
 * A mempool_alloc_t and mempool_free_t for a simple page allocator that
 * allocates pages of the order specified by pool_data
 */
void *mempool_alloc_pages(gfp_t gfp_mask, void *pool_data);
void mempool_free_pages(void *element, void *pool_data);
static inline mempool_t *mempool_create_page_pool(int min_nr, int order)
{
	return mempool_create(min_nr, mempool_alloc_pages, mempool_free_pages,
			      (void *)(long)order);
}

/*
 * Percpu mempool - mempool backed by percpu memory allocator.
 *
 * Along with the usual mempool role, because percpu allocator doesn't
 * support NOIO allocations, percpu mempool is useful as allocation buffer
 * which is filled from IO context and consumed from atomic or non-IO one.
 * To help this usage, percpu_mempool has built-in mechanism to refill the
 * pool which supports both sync and async operations.  Refer to
 * percpu_mempool_refill() for details.
 */
struct percpu_mempool {
	mempool_t		pool;
	size_t			size;		/* size of elements */
	size_t			align;		/* align of elements */
	struct work_struct	refill_work;	/* work item for async refill */
};

struct percpu_mempool *percpu_mempool_create(int min_nr, size_t size,
					     size_t align);
int percpu_mempool_refill(struct percpu_mempool *pcpu_pool, gfp_t gfp_mask);
void percpu_mempool_destroy(struct percpu_mempool *pcpu_pool);

/**
 * percpu_mempool_resize - resize an existing percpu mempool
 * @pcpu_pool:	percpu mempool to resize
 * @new_min_nr:	new minimum number of elements guaranteed to be allocated
 * @gfp_mask:	allocation mask to use
 *
 * Counterpart of mempool_resize().  If @gfp_mask doesn't contain
 * %__GFP_IO, resizing itself may succeed but the implied filling (if
 * necessary) will fail.
 */
static inline int percpu_mempool_resize(struct percpu_mempool *pcpu_pool,
					int new_min_nr, gfp_t gfp_mask)
{
	return mempool_resize(&pcpu_pool->pool, new_min_nr, gfp_mask);
}

/**
 * percpu_mempool_alloc - allocate an element from a percpu mempool
 * @pcpu_pool:	percpu mempool to allocate from
 * @gfp_mask:	allocation mask to use
 *
 * Counterpart of mempool_alloc().  If @gfp_mask doesn't contain %__GFP_IO,
 * allocation is always from the reserved pool.
 */
static inline void __percpu *
percpu_mempool_alloc(struct percpu_mempool *pcpu_pool, gfp_t gfp_mask)
{
	void *p = mempool_alloc(&pcpu_pool->pool, gfp_mask);

	return (void __percpu __force *)p;
}

/**
 * percpu_mempool_free - free an element to a percpu mempool
 * @elem:	element being freed
 * @pcpu_pool:	percpu mempool to free to
 */
static inline void percpu_mempool_free(void __percpu *elem,
				       struct percpu_mempool *pcpu_pool)
{
	void *p = (void __kernel __force *)elem;

	mempool_free(p, &pcpu_pool->pool);
}

/**
 * percpu_mempool_nr_elems - return nr of reserved elems in a percpu mempool
 * @pcpu_pool:	percpu mempool of interest
 *
 * Returns the number of reserved elements in @pcpu_pool.  Mostly useful
 * for deciding when to refill.
 */
static inline int percpu_mempool_nr_elems(struct percpu_mempool *pcpu_pool)
{
	return pcpu_pool->pool.curr_nr;
}

#endif /* _LINUX_MEMPOOL_H */
