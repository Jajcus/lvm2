/*
 * Copyright (C) 2002-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2007 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tools.h"

const char _really_wipe[] =
    "Really WIPE LABELS from physical volume \"%s\" of volume group \"%s\" [y/n]? ";

/*
 * Decide whether it is "safe" to wipe the labels on this device.
 * 0 indicates we may not.
 */
static int pvremove_check(struct cmd_context *cmd, const char *name)
{
	struct physical_volume *pv;

	/* FIXME Check partition type is LVM unless --force is given */

	/* Is there a pv here already? */
	/* If not, this is an error unless you used -f. */
	if (!(pv = pv_read(cmd, name, 1, 0))) {
		if (arg_count(cmd, force_ARG))
			return 1;
		log_error("Physical Volume %s not found", name);
		return 0;
	}

	/*
	 * If a PV has no MDAs it may appear to be an
	 * orphan until the metadata is read off
	 * another PV in the same VG.  Detecting this
	 * means checking every VG by scanning every
	 * PV on the system.
	 */
	if (is_orphan(pv) && !dm_list_size(&pv->fid->metadata_areas_in_use) &&
	    !dm_list_size(&pv->fid->metadata_areas_ignored)) {
		if (!scan_vgs_for_pvs(cmd, 0)) {
			log_error("Rescan for PVs without metadata areas "
				  "failed.");
			goto bad;
		}
		free_pv_fid(pv);
		if (!(pv = pv_read(cmd, name, 1, 0))) {
			log_error("Failed to read physical volume %s", name);
			goto bad;
		}
	}

	/* orphan ? */
	if (is_orphan(pv)) {
		free_pv_fid(pv);
		return 1;
	}

	/* Allow partial & exported VGs to be destroyed. */
	/* we must have -ff to overwrite a non orphan */
	if (arg_count(cmd, force_ARG) < 2) {
		log_error("PV %s belongs to Volume Group %s so please use vgreduce first.", name, pv_vg_name(pv));
		log_error("(If you are certain you need pvremove, then confirm by using --force twice.)");
		goto bad;
	}

	/* prompt */
	if (!arg_count(cmd, yes_ARG) &&
	    yes_no_prompt(_really_wipe, name, pv_vg_name(pv)) == 'n') {
		log_error("%s: physical volume label not removed", name);
		goto bad;
	}

	if (arg_count(cmd, force_ARG)) {
		log_warn("WARNING: Wiping physical volume label from "
			  "%s%s%s%s", name,
			  !is_orphan(pv) ? " of volume group \"" : "",
			  !is_orphan(pv) ? pv_vg_name(pv) : "",
			  !is_orphan(pv) ? "\"" : "");
	}

	free_pv_fid(pv);
	return 1;

bad:
	free_pv_fid(pv);
	return 0;
}

static int pvremove_single(struct cmd_context *cmd, const char *pv_name,
			   void *handle __attribute__((unused)))
{
	struct device *dev;
	int ret = ECMD_FAILED;

	if (!lock_vol(cmd, VG_ORPHANS, LCK_VG_WRITE)) {
		log_error("Can't get lock for orphan PVs");
		return ECMD_FAILED;
	}

	if (!pvremove_check(cmd, pv_name))
		goto out;

	if (!(dev = dev_cache_get(pv_name, cmd->filter))) {
		log_error("%s: Couldn't find device.  Check your filters?",
			  pv_name);
		goto out;
	}

	if (!dev_test_excl(dev)) {
		/* FIXME Detect whether device-mapper is still using the device */
		log_error("Can't open %s exclusively - not removing. "
			  "Mounted filesystem?", dev_name(dev));
		goto out;
	}

	/* Wipe existing label(s) */
	if (!label_remove(dev)) {
		log_error("Failed to wipe existing label(s) on %s", pv_name);
		goto out;
	}

	if (!lvmetad_pv_gone_by_dev(dev, NULL))
		goto_out;

	log_print_unless_silent("Labels on physical volume \"%s\" successfully wiped",
				pv_name);

	ret = ECMD_PROCESSED;

out:
	unlock_vg(cmd, VG_ORPHANS);

	return ret;
}

int pvremove(struct cmd_context *cmd, int argc, char **argv)
{
	int i, r;
	int ret = ECMD_PROCESSED;

	if (!argc) {
		log_error("Please enter a physical volume path");
		return EINVALID_CMD_LINE;
	}

	for (i = 0; i < argc; i++) {
		dm_unescape_colons_and_at_signs(argv[i], NULL, NULL);
		r = pvremove_single(cmd, argv[i], NULL);
		if (r > ret)
			ret = r;
	}

	return ret;
}
