/*
 * Copyright (c) 2012, Ismael Gomez-Miguelez <ismael.gomez@tsc.upc.edu>.
 * This file is part of ALOE++ (http://flexnets.upc.edu/)
 *
 * ALOE++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ALOE++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with ALOE++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include "params.h"

/** Stores in the address of the second parameter the scalar integer value of the
 * parameter name.
 * Tries to parse the parameter as an integer. Read param_addr() for supported formats.
 * \returns 0 on success or -1 on error.
 */
int param_get_int_name(char *name, int *value) {
	pmid_t id = param_id(name);
	if (id == NULL) {
		return -1;
	}
	return (param_get_int(id,value)==-1)?-1:0;
}

/** Stores in the address of the second parameter the scalar float value of the
 * parameter name.
 * Tries to parse the parameter as an float. Read param_addr() for supported formats.
 * \returns 0 on success or -1 on error.
 */
int param_get_float_name(char *name, float *value) {
	pmid_t id = param_id(name);
	if (id == NULL) {
		return -1;
	}
	return (param_get_float(id,value)==-1)?-1:0;
}

/** see param_id() */
int param_get_int(pmid_t id, int *value) {
	param_type_t type;
	int size;
	int tmp = *value;
	int max_size = sizeof(int);

	if ((size = param_get(id,value,max_size,&type)) == -1) {
		*value = tmp;
		return -1;
	}
	if (size != max_size) {
		*value = tmp;
		return 0;
	}
	if (type != INT) {
		*value = tmp;
		return 0;
	}
	return 1;
}

/** see param_id() */
int param_get_float(pmid_t id, float *value) {
	param_type_t type;
	int size;
	int max_size = sizeof(float);

	if ((size = param_get(id,value,max_size,&type)) == -1) {
		return -1;
	}
	if (size != max_size) {
		return 0;
	}
	if (type != FLOAT) {
		return 0;
	}
	return 1;
}


