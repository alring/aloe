
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <oesr.h>
#include <ctrl_skeleton.h>
#include <params.h>
#include "lte_lib/grid/base.h"

#include "lte_ctrl.h"

struct lte_cell_config cell;
struct lte_grid_config grid_tx,grid_rx;

int nof_symb_slot;

int x_mode;

itf_t pdcch_itf_out,pdcch_itf_in;

#define MAX_DCI_PACKET_SZ 128
char dci_packet[MAX_DCI_PACKET_SZ];

int mode_is_tx() {
	return (!x_mode || x_mode==2);
}
int mode_is_rx() {
	return (x_mode==1 || x_mode==2);
}

int ctrl_init() {

	pdcch_itf_out = oesr_itf_create(ctx,0,ITF_WRITE,MAX_DCI_PACKET_SZ);
	if (!pdcch_itf_out) {
		moderror("Creating PDCCH output itf\n");
		return -1;
	}

	if (param_get_int_name("mode",&x_mode)) {
		printf("mode undefined\n");
		return -1;
	}
	grid_tx.verbose = 0;
	grid_tx.subframe_idx = -1;
	grid_tx.nof_pdcch = 1;
	grid_tx.pdcch[0].nof_cce = 2;
	grid_tx.nof_pdsch = 1;

	grid_rx.subframe_idx = 1;
	grid_rx.verbose = 0;
	grid_rx.nof_pdcch = 0;
	grid_rx.subframe_idx = -1;
	grid_rx.cfi = -1;
	grid_rx.nof_pdcch = 0;
	grid_rx.nof_pdsch = 0;

	return 1;
}

int _get_param(char *name, int *address, char *mode) {
	char tmp[64];
	snprintf(tmp,64,"%s_%s",name,mode);
	if (param_get_int_name(tmp,address)) {
		return -1;
	}

	return 0;
}


int ctrl_work_(int tslot, struct lte_grid_config *grid,
		struct remote_parameters *params, char *mode) {
	int i, subframe_tmp;

	if (_get_param("cfi",&grid->cfi,mode)) {
		return -1;
	}

	if (!strcmp(mode,"tx") || grid->subframe_idx != -1) {
		grid->subframe_idx++;
	}
	if (!strcmp(mode,"rx")) {
		if (grid->subframe_idx == -1) {
			_get_param("subframe",&subframe_tmp,mode);
			if (grid->verbose) {
				printf("ts=%d subframe %s = %d\n",oesr_tstamp(ctx), mode, subframe_tmp);
			}
			if (subframe_tmp != -1) {
				grid->subframe_idx = subframe_tmp;
			}
		} else if (!(grid->subframe_idx % 5) && subframe_tmp != -1){
			subframe_tmp = -1;
			_get_param("subframe",&subframe_tmp,mode);
			if (subframe_tmp != -1) {
				if (subframe_tmp != grid->subframe_idx % NOF_SUBFRAMES_X_FRAME) {
					moderror_msg("Synchronization lost! Expected subframe %d but got %d\n",
							grid->subframe_idx,subframe_tmp);
				}
			}
		}
	}


	if (_get_param("mcs",&params->mcs,mode)) {
		return -1;
	}
	if (_get_param("nof_rbg",&grid->pdsch[0].nof_rbg,mode)) {
		return -1;
	}
	if (_get_param("rbg_mask",&grid->pdsch[0].rbg_mask,mode)) {
		return -1;
	}

	if (lte_grid_init(grid)) {
		moderror("Error initiating grid\n");
		return -1;
	}
	params->cfi = grid->cfi;
	params->nof_rbg[0] = grid->pdsch[0].nof_rbg;
	params->pdsch_mask[0] = grid->pdsch[0].rbg_mask;

	params->tbs = lte_get_tbs(params->mcs,params->nof_rbg[0]);
	params->cbs = lte_get_cbits(params->mcs,params->nof_rbg[0]);
	params->modulation = lte_get_modulation_format(params->mcs);
	params->pdcch_en[0] = 1;

	params->bits_x_slot = lte_pdsch_get_re(0,grid->subframe_idx%NOF_SUBFRAMES_X_FRAME,grid)*
			params->modulation;

	for (i=0;i<SUBFRAME_DELAY;i++) {
		params->tslot_idx[i] = (grid->subframe_idx-i)%NOF_SUBFRAMES_X_FRAME;
	}

	/*
	printf("mode=%s mcs=%d, cfi=%d sf=%d, tbs=%d, cbs=%d, mod=%d, bits=%d\n",
				mode, params->mcs, params->cfi, grid->subframe_idx,params->tbs,params->cbs,
				params->modulation,params->bits_x_slot);
	*/
	return 0;
}


int pdcch_tx(int tslot,
		struct lte_grid_config *grid, struct remote_parameters *params) {
	int i;
	int dci_len;
	for (i=0;i<grid->nof_pdcch;i++) {
		params->pdcch_E[i] = grid->pdcch[i].nof_bits;
	}
	params->pdcch_S[0] = 3*(dci_len+16);
	return 0;
}

int pdcch_rx(int tslot,
		struct lte_grid_config *grid, struct remote_parameters *params) {
	int i;
	int dci_len;
	dci_len = 21; /* TODO: Implement blind decoding. We know it's dci format 1A */
	params->pdcch_S[0] = 3*(dci_len+16);
	return 0;
}

int ctrl_work(int tslot) {
	int i;

	if (mode_is_tx()) {
		ctrl_work_(tslot,&grid_tx,&tx_params, "tx");
		if (pdcch_tx(tslot,&grid_tx,&tx_params)) {
			return -1;
		}
		tx_params.nof_pdsch = 1;
		tx_params.pdsch_mask[0] = grid_tx.pdsch[0].rbg_mask;
		tx_params.nof_rbg[0] = grid_tx.pdsch[0].nof_rbg;
	}
	if (mode_is_rx()) {
		ctrl_work_(tslot,&grid_rx,&rx_params, "rx");
		if (pdcch_rx(tslot,&grid_rx,&rx_params)) {
			return -1;
		}
		if (grid_rx.cfi > 0) {
			rx_params.nof_pdsch = 1;
			rx_params.nof_pdcch = 1;
			rx_params.pdcch_cce[0] = 2;
		}
	}
	return 0;
}