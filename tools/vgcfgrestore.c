/*
 * Copyright (C) 2001 Sistina Software (UK) Limited.
 *
 * This file is released under the GPL.
 */

#include "tools.h"

#include <stdio.h>

int vgcfgrestore(int argc, char **argv)
{
	if (argc != 1) {
		log_err("Please specify a *single* volume group to restore.");
		return ECMD_FAILED;
	}

	/*
	 * FIXME: overloading the -l arg for now to display a
	 * list of archive files for a particular vg
	 */
	if (arg_count(list_ARG)) {
		if (!archive_display(argv[0]))
			return ECMD_FAILED;

		return 0;
	}

	if (!(arg_count(file_ARG) ?
	      backup_restore_from_file(argv[0], arg_str_value(file_ARG, "")) :
	      backup_restore(argv[0]))) {
		log_err("Restore failed.");
		return ECMD_FAILED;
	}

	return 0;
}
