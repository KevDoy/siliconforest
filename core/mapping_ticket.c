#include "mapping_ticket.h"

/* Reject targets on which this callback could acquire an atomic-runtime lock.
 * A kernel port must also validate its compiler/interrupt-context guarantees. */
_Static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "lock-free bool atomics required");

static void pager_release(void *arg) {
    dl_map_ticket *ticket = arg;
    /* The producer promises a valid, live ticket. Deliberately no allocation,
     * store access, refcount destruction, locks or scheduling calls here.
     * Notifications are idempotent while THIS ticket remains alive. */
    atomic_store_explicit(&ticket->release_notified, true, memory_order_release);
}

int dl_map_prepare(dl_store *store, dl_buffer buffer, dl_map_request request,
                   dl_map_ticket *ticket) {
    if (!store || !ticket) return DL_INVALID;
    if (ticket->phase != DL_MAP_FRESH) return DL_BAD_STATE;
    if (!request.bytes || !request.requested_access ||
        (request.allowed_access & ~(DL_MAP_READ | DL_MAP_WRITE)) ||
        (request.requested_access & ~request.allowed_access) ||
        !(request.requested_access & DL_MAP_READ)) return DL_INVALID;
    dl_view view;
    int status = dl_store_inspect(store, buffer, &view);
    if (status != DL_OK) return status;
    uint64_t page = store->page_size;
    if (!page || (page & (page - 1)) ||
        (request.offset & (page - 1)) || (request.bytes & (page - 1)) ||
        request.offset > view.bytes || request.bytes > view.bytes - request.offset)
        return DL_INVALID;
    dl_lease hold;
    status = dl_store_hold(store, buffer, DL_CPU_MAP, &hold);
    if (status != DL_OK) return status;
    /* No remaining fallible operations. Never copy an initialized atomic. */
    ticket->store = store;
    ticket->buffer = buffer;
    ticket->hold = hold;
    ticket->request = request;
    atomic_init(&ticket->release_notified, false);
    ticket->phase = DL_MAP_PREPARED;
    return DL_OK;
}

int dl_map_publish(dl_map_ticket *ticket, dl_map_handoff *out) {
    if (!ticket || !out) return DL_INVALID;
    if (ticket->phase != DL_MAP_PREPARED) return DL_BAD_STATE;
    ticket->phase = DL_MAP_PUBLISHED;
    /* Backend must safely publish these pointers across its own threads. */
    *out = (dl_map_handoff){pager_release, ticket};
    return DL_OK;
}

int dl_map_abort(dl_map_ticket *ticket) {
    if (!ticket) return DL_INVALID;
    if (ticket->phase != DL_MAP_PREPARED) return DL_BAD_STATE;
    int status = dl_store_release_host(ticket->store, ticket->hold);
    if (status != DL_OK) return status; /* retain responsibility on failure */
    ticket->phase = DL_MAP_ABORTED;
    return DL_OK;
}

int dl_map_drain(dl_map_ticket *ticket) {
    if (!ticket) return DL_INVALID;
    if (ticket->phase == DL_MAP_DRAINED || ticket->phase == DL_MAP_ABORTED)
        return DL_STALE;
    if (ticket->phase != DL_MAP_PUBLISHED) return DL_BAD_STATE;
    if (!atomic_load_explicit(&ticket->release_notified, memory_order_acquire))
        return DL_BUSY;
    int status = dl_store_release_host(ticket->store, ticket->hold);
    if (status != DL_OK) return status;
    ticket->phase = DL_MAP_DRAINED;
    return DL_OK;
}

int dl_map_page_number(uint64_t address, unsigned shift, uint32_t *out) {
    /* Explicitly support 4/16/64 KiB granules. This is not a declaration that
     * every granule works with a particular device or kernel build. */
    if (!out || (shift != 12 && shift != 14 && shift != 16)) return DL_INVALID;
    uint64_t mask = (UINT64_C(1) << shift) - 1;
    if (address & mask) return DL_INVALID;
    uint64_t number = address >> shift;
    if (number > UINT32_MAX) return DL_OVERFLOW;
    *out = (uint32_t)number;
    return DL_OK;
}
