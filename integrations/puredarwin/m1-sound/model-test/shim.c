// SPDX-License-Identifier: MIT
// Read access to the kernel model's state for the host harness (the struct is public
// in pd_speaker_model.h; this only saves mirroring its layout in Rust).
#include <stddef.h>
#include "../kernel/pd_speaker_model.h"

size_t pdshim_size(void) { return sizeof(struct pd_spk_model); }
int pdshim_fault(const struct pd_spk_model *m) { return m->fault; }
unsigned pdshim_fault_speaker(const struct pd_spk_model *m) { return m->fault_speaker; }
/* degrees, Q23.40 */
int64_t pdshim_coil(const struct pd_spk_model *m, unsigned i) { return m->spk[i].base_coil + m->spk[i].heat_coil; }
int64_t pdshim_magnet(const struct pd_spk_model *m, unsigned i) { return m->spk[i].base_magnet + m->spk[i].heat_magnet; }
int64_t pdshim_coil_hyst(const struct pd_spk_model *m, unsigned i) { return m->spk[i].t_coil_hyst; }
int64_t pdshim_magnet_hyst(const struct pd_spk_model *m, unsigned i) { return m->spk[i].t_magnet_hyst; }
int64_t pdshim_alpha_coil(const struct pd_spk_model *m) { return m->alpha_coil; }
int64_t pdshim_alpha_magnet(const struct pd_spk_model *m) { return m->alpha_magnet; }
