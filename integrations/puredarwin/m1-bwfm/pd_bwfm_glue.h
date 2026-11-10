// SPDX-License-Identifier: MIT
// bwfm(4) as PDBwfm.cpp sees it, plain C so the c++ side never includes the openbsd headers
#ifndef PD_BWFM_GLUE_H
#define PD_BWFM_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

struct device;
struct ieee80211com;
struct pci_attach_args;

struct device *pd_bwfm_alloc(const char *);
void	pd_bwfm_free(struct device *);
int	pd_bwfm_match(struct pci_attach_args *);
int	pd_bwfm_attach(struct device *, struct pci_attach_args *);
int	pd_bwfm_attach_hook(struct device *);
int	pd_bwfm_running(struct device *);
const char *pd_bwfm_ctlwhy(struct device *);
int	pd_bwfm_down(struct device *, int inform);
int	pd_bwfm_up(struct device *);
void	pd_bwfm_set_gone(struct device *, int gone);
int	pd_bwfm_dma_resume(void);
void	pd_bwfm_unhook(struct device *);
int	pd_bwfm_detach(struct device *);
struct ieee80211com *pd_bwfm_ic(struct device *);

#ifdef __cplusplus
}
#endif

#endif
