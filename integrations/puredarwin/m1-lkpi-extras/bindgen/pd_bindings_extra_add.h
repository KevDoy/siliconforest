
/*
 * m1-lkpi-extras: appended to ../m1-rust-kext/lkpi/include/pd_bindings_extra.h (the
 * PureDarwin additions to the reduced bindings_helper.h).
 */
#include <pd_lkpi_extras.h>

/* headers Linux reaches indirectly that the driver's direct bindings need */
#include <linux/capability.h>
#include <linux/shrinker.h>
#include <linux/seq_file.h>

/* rust/kernel/xarray.rs: the "any entry" filter of xa_find() */
const unsigned int RUST_CONST_HELPER_XA_PRESENT = PD_XA_PRESENT;

/* drivers/gpu/drm/asahi/cleanup.rs: Linux's enum wq_flags, macros in LinuxKPI */
const unsigned int RUST_CONST_HELPER_wq_flags_WQ_UNBOUND = WQ_UNBOUND;
const unsigned int RUST_CONST_HELPER_wq_flags_WQ_HIGHPRI = WQ_HIGHPRI;
const unsigned int RUST_CONST_HELPER_wq_flags_WQ_MEM_RECLAIM = WQ_MEM_RECLAIM;

/* drivers/gpu/drm/asahi/pgtable.rs: arm64 system register encoding of CTR_EL0 */
const unsigned int RUST_CONST_HELPER_SYS_CTR_EL0_Op0 = PD_SYS_CTR_EL0_Op0;
const unsigned int RUST_CONST_HELPER_SYS_CTR_EL0_Op1 = PD_SYS_CTR_EL0_Op1;
const unsigned int RUST_CONST_HELPER_SYS_CTR_EL0_CRn = PD_SYS_CTR_EL0_CRn;
const unsigned int RUST_CONST_HELPER_SYS_CTR_EL0_CRm = PD_SYS_CTR_EL0_CRm;
const unsigned int RUST_CONST_HELPER_SYS_CTR_EL0_Op2 = PD_SYS_CTR_EL0_Op2;
const unsigned int RUST_CONST_HELPER_CTR_EL0_DminLine_SHIFT = PD_CTR_EL0_DminLine_SHIFT;

