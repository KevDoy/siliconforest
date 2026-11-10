/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Linux-semantics XArray entry points over LinuxKPI's `struct xarray`. See
 * linux/pd_xarray.h for what is implemented and what is not.
 *
 * Representation, shared with linux_xarray.c: a slot of the radix tree holds the stored
 * pointer, or PD_XA_RESERVED for an entry that is reserved (Linux: XA_ZERO_ENTRY; what
 * storing NULL in an allocating array, xa_reserve(), or xa_alloc(..., NULL, ...) leave).
 * linux_xarray.c calls the same value NULL_VALUE; tests/test_xarray.c checks that the two
 * files agree by going through both.
 */
#include <linux/pd_xarray.h>
#include <linux/err.h>
#include <vm/vm_pageout.h>

#define	PD_XA_RESERVED		((void *)0x1)
#define	PD_XA_MAX_INDEX		0xffffffffUL
#define	PD_XA_TRACK_FREE(xa)	(((xa)->xa_flags & (XA_FLAGS_ALLOC | XA_FLAGS_ALLOC1)) != 0)

static inline void *
pd_xa_in(void *entry)
{
	/* raw API value -> slot value */
	return (entry == PD_XA_ZERO_ENTRY ? PD_XA_RESERVED : entry);
}

static inline void *
pd_xa_out_raw(void *slot)
{
	return (slot == PD_XA_RESERVED ? PD_XA_ZERO_ENTRY : slot);
}

static inline void *
pd_xa_out(void *slot)
{
	return (slot == PD_XA_RESERVED ? NULL : slot);
}

/* Something a caller must not store: the reserved marker itself, or an error pointer. */
static inline bool
pd_xa_bad_entry(void *entry)
{
	return (entry == PD_XA_RESERVED || IS_ERR(entry));
}

/*
 * The radix tree allocates its nodes with GFP_NOWAIT (xa_init_flags), because xa_lock is
 * held. When that fails and the caller may sleep, do what linux_xarray.c and Linux's
 * __xas_nomem() do: drop the lock, wait, retry. Returns false when the caller may not
 * sleep, in which case the operation fails with -ENOMEM and the array is unchanged.
 */
static bool
pd_xa_nomem_wait(struct xarray *xa, gfp_t gfp)
{
	if ((gfp & M_WAITOK) == 0)
		return (false);
	xa_unlock(xa);
	vm_wait(NULL);
	xa_lock(xa);
	return (true);
}

void *
pd_xa_load(struct xarray *xa, unsigned long index)
{
	void *slot;

	if (index > PD_XA_MAX_INDEX)
		return (NULL);
	xa_lock(xa);
	slot = radix_tree_lookup(&xa->xa_head, index);
	xa_unlock(xa);
	return (pd_xa_out(slot));
}

/* Lowest present, not reserved entry in [start, max]. */
static void *
pd_xa_find_from(struct xarray *xa, unsigned long start, unsigned long max,
    unsigned long *indexp)
{
	struct radix_tree_iter iter;
	void **slot;
	void *entry = NULL;

	if (max > PD_XA_MAX_INDEX)
		max = PD_XA_MAX_INDEX;
	if (start > max)
		return (NULL);
	xa_lock(xa);
	for (iter.index = start;
	    radix_tree_iter_find(&xa->xa_head, &iter, &slot, 0) && iter.index <= max;
	    iter.index++) {
		if (*slot != PD_XA_RESERVED) {
			entry = *slot;
			*indexp = iter.index;
			break;
		}
		if (iter.index == max)
			break;
	}
	xa_unlock(xa);
	return (entry);
}

void *
pd_xa_find(struct xarray *xa, unsigned long *indexp, unsigned long max, unsigned int filter)
{
	if (filter != PD_XA_PRESENT)	/* marks are not implemented */
		return (NULL);
	return (pd_xa_find_from(xa, *indexp, max, indexp));
}

void *
pd_xa_find_after(struct xarray *xa, unsigned long *indexp, unsigned long max,
    unsigned int filter)
{
	unsigned long start = *indexp + 1;

	if (filter != PD_XA_PRESENT || start == 0)
		return (NULL);
	return (pd_xa_find_from(xa, start, max, indexp));
}

int
pd_xa_trylock(struct xarray *xa)
{
	return (mtx_trylock(&xa->xa_lock) != 0);
}

void *
pd___xa_erase(struct xarray *xa, unsigned long index)
{
	XA_ASSERT_LOCKED(xa);
	if (index > PD_XA_MAX_INDEX)
		return (NULL);
	return (pd_xa_out(radix_tree_delete(&xa->xa_head, index)));
}

/*
 * Linux __xa_cmpxchg_raw(): if the entry at index is `old`, replace it with `entry`.
 * `old`, `entry` and the result are raw: NULL is "nothing there" and PD_XA_ZERO_ENTRY a
 * reserved entry. Storing NULL removes the entry. Returns what was there, or XA_ERROR().
 */
void *
pd___xa_cmpxchg_raw(struct xarray *xa, unsigned long index, void *old, void *entry, gfp_t gfp)
{
	void *slot, *new;
	int error;

	XA_ASSERT_LOCKED(xa);
	if (index > PD_XA_MAX_INDEX || pd_xa_bad_entry(entry))
		return (XA_ERROR(-EINVAL));
	new = pd_xa_in(entry);
	for (;;) {
		slot = radix_tree_lookup(&xa->xa_head, index);
		if (pd_xa_out_raw(slot) != old)
			break;
		if (slot != NULL) {
			/* replacing or removing an existing entry allocates nothing */
			void *tmp = new;

			error = radix_tree_store(&xa->xa_head, index, &tmp);
		} else if (new != NULL) {
			error = radix_tree_insert(&xa->xa_head, index, new);
		} else {
			error = 0;	/* nothing there, nothing to store */
		}
		if (error == 0)
			break;
		if (error != -ENOMEM || !pd_xa_nomem_wait(xa, gfp))
			return (XA_ERROR(error));
		/* the lock was dropped: look again */
	}
	return (pd_xa_out_raw(slot));
}

void *
pd___xa_store(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp)
{
	void *slot;
	int error;

	XA_ASSERT_LOCKED(xa);
	if (index > PD_XA_MAX_INDEX || entry == PD_XA_ZERO_ENTRY || pd_xa_bad_entry(entry))
		return (XA_ERROR(-EINVAL));
	if (entry == NULL) {
		/* an allocating array keeps the index busy, any other array forgets it */
		if (!PD_XA_TRACK_FREE(xa))
			return (pd___xa_erase(xa, index));
		entry = PD_XA_RESERVED;
	}
	for (;;) {
		slot = entry;
		error = radix_tree_store(&xa->xa_head, index, &slot);
		if (error == 0)
			return (pd_xa_out(slot));
		if (error != -ENOMEM || !pd_xa_nomem_wait(xa, gfp))
			return (XA_ERROR(error));
	}
}

int
pd___xa_insert(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp)
{
	void *curr;

	if (entry == NULL)
		entry = PD_XA_ZERO_ENTRY;
	else if (entry == PD_XA_ZERO_ENTRY)
		return (-EINVAL);
	curr = pd___xa_cmpxchg_raw(xa, index, NULL, entry, gfp);
	if (IS_ERR(curr))
		return (PTR_ERR(curr));
	return (curr != NULL ? -EBUSY : 0);
}

/*
 * Linux __xa_alloc(): store `entry` (NULL reserves) at the lowest free index within
 * limit, -EBUSY when there is none. An XA_FLAGS_ALLOC1 array never hands out index 0.
 * Free means no entry and no reservation.
 *
 * Cost: one tree descent per occupied index between limit.min and the first gap, where
 * Linux finds the gap through its free marks. The arrays this is for (per-file VM, queue
 * and object ids of a GPU driver) hold tens of entries.
 */
int
pd___xa_alloc(struct xarray *xa, uint32_t *id, void *entry, struct xa_limit limit, gfp_t gfp)
{
	struct radix_tree_iter iter;
	void **slot;
	unsigned long index;
	int error;

	XA_ASSERT_LOCKED(xa);
	if (!PD_XA_TRACK_FREE(xa) || entry == PD_XA_ZERO_ENTRY || pd_xa_bad_entry(entry))
		return (-EINVAL);
	if (entry == NULL)
		entry = PD_XA_RESERVED;
	for (;;) {
		index = limit.min;
		if ((xa->xa_flags & XA_FLAGS_ALLOC1) != 0 && index == 0)
			index = 1;
		for (;;) {
			if (index > limit.max)
				return (-EBUSY);
			iter.index = index;
			if (!radix_tree_iter_find(&xa->xa_head, &iter, &slot, 0) ||
			    iter.index != index)
				break;		/* nothing at index */
			index++;
		}
		error = radix_tree_insert(&xa->xa_head, index, entry);
		if (error == 0) {
			*id = (uint32_t)index;
			return (0);
		}
		if (error != -ENOMEM || !pd_xa_nomem_wait(xa, gfp))
			return (error);
		/* the lock was dropped: search again */
	}
}
