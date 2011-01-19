/*
 *  Hamlib Elecraft backend--support Elecraft extensions to Kenwood commands
 *  Copyright (C) 2010 by Nate Bargmann, n0nb@n0nb.us
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for 
 *  the complete text of the GNU Lesser Public License version 2.
 * 
 */

#include <string.h>
#include <stdlib.h>
#include "token.h"

#include "elecraft.h"
#include "kenwood.h"


static const struct elec_ext_id_str elec_ext_id_str_lst[] = {
	{ K20, "K20" },
	{ K21, "K21" },
	{ K22, "K22" },
	{ K23, "K23" },
	{ K30, "K30" },
	{ K31, "K31" },
	{ EXT_LEVEL_NONE, NULL },		/* end marker */
};


/* Private function declarations */
int verify_kenwood_id(RIG *rig, char *id);
int elecraft_get_extension_level(RIG *rig, const char *cmd, int *ext_level);


/* Shared backend function definitions */

/* elecraft_open()
 * 
 * First checks for ID of '017' then tests for an Elecraft radio/backend using
 * the K2; command.  Here we also test for a K3 and if that fails, assume a K2.
 * Finally, save the value for later reading.
 * 
 */
 
int elecraft_open(RIG *rig)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	int err;
	char id[KENWOOD_MAX_BUF_LEN];

	/* Actual read extension levels from radio.
	 * 
	 * The value stored in the k?_ext_lvl variables map to
	 * elec_ext_id_str_lst.level and is only written to by the
	 * elecraft_get_extension_level() private function during elecraft_open() 
	 * and thereafter shall be treated as READ ONLY!
	 */
	struct kenwood_priv_data *priv = rig->state.priv;

	/* Use check for "ID017;" to verify rig is reachable */
	err = verify_kenwood_id(rig, id);
	if (err != RIG_OK)
		return err;

	switch(rig->caps->rig_model) {
		case RIG_MODEL_K2:
			err = elecraft_get_extension_level(rig, "K2", &priv->k2_ext_lvl);
			if (err != RIG_OK)
				return err;

			rig_debug(RIG_DEBUG_ERR, "%s: K2 level is %d, %s\n", __func__, 
				priv->k2_ext_lvl, elec_ext_id_str_lst[priv->k2_ext_lvl].id);

			break;
		case RIG_MODEL_K3:
			err = elecraft_get_extension_level(rig, "K2", &priv->k2_ext_lvl);
			if (err != RIG_OK)
				return err;

			rig_debug(RIG_DEBUG_ERR, "%s: K2 level is %d, %s\n", __func__, 
				priv->k2_ext_lvl, elec_ext_id_str_lst[priv->k2_ext_lvl].id);

			err = elecraft_get_extension_level(rig, "K3", &priv->k3_ext_lvl);
			if (err != RIG_OK)
				return err;

			rig_debug(RIG_DEBUG_ERR, "%s: K3 level is %d, %s\n", __func__, 
				priv->k3_ext_lvl, elec_ext_id_str_lst[priv->k3_ext_lvl].id);
			break;
		default:
			rig_debug(RIG_DEBUG_ERR, "%s: unrecognized rig model %d\n", 
				__func__, rig->caps->rig_model);
			return -RIG_EINVAL;
	}

	return RIG_OK;
}



/* Private helper functions */

/* Tests for Kenwood ID string of "017" */

int verify_kenwood_id(RIG *rig, char *id)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !id)
		return -RIG_EINVAL;

	int err;
	char *idptr;

	/* Check for an Elecraft K2|K3 which returns "017" */
	err = kenwood_get_id(rig, id);
	if (err != RIG_OK) {
		rig_debug(RIG_DEBUG_TRACE, "%s: cannot get identification\n", __func__);
		return err;
	}

	/* ID is 'ID017;' */
	if (strlen(id) < 5) {
		rig_debug(RIG_DEBUG_TRACE, "%s: unknown ID type (%s)\n", __func__, id);
		return -RIG_EPROTO;
	}

	/* check for any white space and skip it */
	idptr = &id[2];
	if (*idptr == ' ')
		idptr++;

	if (strcmp("017", idptr) != 0) {
		rig_debug(RIG_DEBUG_TRACE, "%s: Rig (%s) is not a K2 or K3\n", __func__, id);
		return -RIG_EPROTO;
	} else
		rig_debug(RIG_DEBUG_TRACE, "%s: Rig ID is %s\n", __func__, id);

	return RIG_OK;
}


/* Determines K2 and K3 extension level */

int elecraft_get_extension_level(RIG *rig, const char *cmd, int *ext_level)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !ext_level)
		return -RIG_EINVAL;

	int err, i;
	char buf[KENWOOD_MAX_BUF_LEN];
	char *bufptr;

	err = kenwood_safe_transaction(rig, cmd, buf, KENWOOD_MAX_BUF_LEN, 4);
	if (err != RIG_OK) {
		rig_debug(RIG_DEBUG_ERR, "%s: Cannot get K2|K3 ID\n", __func__);
		return err;
	}

	/* Get extension level string */
	bufptr = &buf[0];

	for (i = 0; elec_ext_id_str_lst[i].level != EXT_LEVEL_NONE; i++) {
		if (strcmp(elec_ext_id_str_lst[i].id, bufptr) != 0)
			continue;

		if (strcmp(elec_ext_id_str_lst[i].id, bufptr) == 0) {
			*ext_level = elec_ext_id_str_lst[i].level;
			rig_debug(RIG_DEBUG_TRACE, "%s: Extension level is %d, %s\n",
			__func__, *ext_level, elec_ext_id_str_lst[i].id);
		}
	}

	return RIG_OK;
}
