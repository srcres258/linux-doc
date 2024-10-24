/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2023 Advanced Micro Devices, Inc. */

#ifndef _VFIO_DEV_H_
#define _VFIO_DEV_H_

#include <linux/pci.h>
#include <linux/vfio_pci_core.h>

#include "dirty.h"
#include "lm.h"

enum pds_vfio_reset_type {
	PDS_VFIO_HOST_RESET = 0,
	PDS_VFIO_DEVICE_RESET = 1,
};

struct pds_vfio_pci_device {
	struct vfio_pci_core_device vfio_coredev;

	struct pds_vfio_lm_file *save_file;
	struct pds_vfio_lm_file *restore_file;
	struct pds_vfio_dirty dirty;
	struct mutex state_mutex; /* protect migration state */
	enum vfio_device_mig_state state;
	struct notifier_block nb;

	int vf_id;
	u16 client_id;
};

const struct vfio_device_ops *pds_vfio_ops_info(void);
struct pds_vfio_pci_device *pds_vfio_pci_drvdata(struct pci_dev *pdev);
void pds_vfio_reset(struct pds_vfio_pci_device *pds_vfio,
		    enum vfio_device_mig_state state);

struct pci_dev *pds_vfio_to_pci_dev(struct pds_vfio_pci_device *pds_vfio);
struct device *pds_vfio_to_dev(struct pds_vfio_pci_device *pds_vfio);

#endif /* _VFIO_DEV_H_ */
