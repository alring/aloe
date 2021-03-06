#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "oesr.h"
#include "rtdal.h"
#include "params.h"
#include "skeleton.h"
#include "ctrl_pkt.h"


#define MAX_INPUTS 		30
#define MAX_OUTPUTS 	30
#define MAX_VARIABLES 	50
#define MAX_PARAMETERS 	50

extern int input_sample_sz;
extern int output_sample_sz;
extern const int nof_input_itf;
extern const int nof_output_itf;
extern const int input_max_samples;
extern const int output_max_samples;

static int mem_ok=0, log_ok=0, check_ok=0;

itf_t inputs[MAX_INPUTS], outputs[MAX_OUTPUTS];
itf_t ctrl_in;
/*
var_t parameters[MAX_PARAMETERS];
int nof_parameters;
*/
struct ctrl_in_pkt ctrl_in_buffer;

#define CTRL_IN_BUFFER_SZ	sizeof(struct ctrl_in_pkt)

user_var_t *user_vars;
var_t vars[MAX_VARIABLES];
int nof_vars;

log_t mlog;
counter_t counter;

void *input_ptr[MAX_INPUTS], *output_ptr[MAX_OUTPUTS];
int rcv_len[MAX_INPUTS], snd_len[MAX_OUTPUTS];

void *ctx;

void init_memory() {
	memset(inputs,0,sizeof(itf_t)*MAX_INPUTS);
	memset(rcv_len,0,sizeof(int)*MAX_INPUTS);

	memset(outputs,0,sizeof(itf_t)*MAX_OUTPUTS);
	memset(snd_len,0,sizeof(int)*MAX_OUTPUTS);

	memset(vars,0,sizeof(var_t)*MAX_VARIABLES);

	ctrl_in = NULL;
	nof_vars=0;
	mlog=NULL;
	counter=NULL;

}

int check_configuration(void *ctx) {
	moddebug("nof_input=%d, nof_output=%d\n",nof_input_itf,nof_output_itf);

	if (nof_input_itf > MAX_INPUTS) {
		moderror_msg("Maximum number of input interfaces is %d. The module uses %d. "
				"Increase MAX_INPUTS in oesr/src/skeleton/skeleton.c and recompile ALOE\n",
				MAX_INPUTS,nof_input_itf);
		return -1;
	}
	if (nof_output_itf > MAX_OUTPUTS) {
		moderror_msg("Maximum number of output interfaces is %d. The module uses %d. "
				"Increase MAX_OUTPUTS in oesr/src/skeleton/skeleton.c and recompile ALOE\n",
				MAX_OUTPUTS,nof_output_itf);
		return -1;
	}

	user_vars = NULL;
	nof_vars = 0;

	return 0;
}

int init_interfaces(void *ctx) {
	int i;


	for (i=0;i<nof_output_itf;i++) {
		if (outputs[i] == NULL) {
			outputs[i] = oesr_itf_create(ctx, i, ITF_WRITE, output_max_samples*output_sample_sz);
			if (outputs[i] == NULL) {
				if (oesr_error_code(ctx) == OESR_ERROR_NOTFOUND) {
					moddebug("Caution output port %d not connected,\n",i);
				} else {
					moderror_msg("Error creating output port %d\n",i);
					oesr_perror("oesr_itf_create\n");
					return -1;
				}
			} else {
				moddebug("output_%d=0x%x\n",i,outputs[i]);
			}
		}
	}

	if (oesr_itf_nofinputs(ctx) > nof_input_itf) {
		if (!ctrl_in) {
			/* try to create control interface */
			ctrl_in = oesr_itf_create(ctx, oesr_itf_nofinputs(ctx)-1, ITF_READ, CTRL_IN_BUFFER_SZ);
			if (ctrl_in) {
				moddebug("Created control port\n",0);
			}
		}
	}

	moddebug("configuring %d inputs and %d outputs %d %d %d\n",nof_input_itf,nof_output_itf,inputs[0],input_max_samples,input_sample_sz);
	for (i=0;i<nof_input_itf;i++) {
		if (inputs[i] == NULL) {
			inputs[i] = oesr_itf_create(ctx, i, ITF_READ, input_max_samples*input_sample_sz);
			if (inputs[i] == NULL) {
				if (oesr_error_code(ctx) == OESR_ERROR_NOTREADY) {
					moddebug("input %d not ready\n",i);
					return 0;
				} else {
					oesr_perror("creating input interface\n");
					return -1;
				}
			} else {
				moddebug("input_%d=0x%x\n",i,inputs[i]);
			}
		}
	}


	return 1;
}

void close_interfaces(void *ctx) {
	int i;
	moddebug("nof_input=%d, nof_output=%d\n",nof_input_itf,nof_output_itf);

	if (ctrl_in) {
		if (oesr_itf_close(ctrl_in)) {
			oesr_perror("oesr_itf_close");
		}
	}

	for (i=0;i<nof_input_itf;i++) {
		moddebug("input_%d=0x%x\n",i,inputs[i]);
		if (inputs[i]) {
			if (oesr_itf_close(inputs[i])) {
				oesr_perror("oesr_itf_close");
			}
		}
	}
	for (i=0;i<nof_output_itf;i++) {
		moddebug("output_%d=0x%x\n",i,outputs[i]);
		if (outputs[i]) {
			if (oesr_itf_close(outputs[i])) {
				oesr_perror("oesr_itf_close");
			}
		}
	}

}

int init_variables(void *ctx) {
	int i;

	moddebug("nof_vars=%d\n",nof_vars);

	for (i=0;nof_vars;i++) {
		moddebug("var %d\n",i);
		vars[i]=oesr_var_create(ctx, user_vars[i].name, user_vars[i].value, user_vars[i].size);
		if (!vars[i]) {
			oesr_perror("oesr_var_create\n");
			moderror_msg("variable name=%s size=%d\n",user_vars[i].name, user_vars[i].size);
			return -1;
		}
	}

/*	nof_parameters = oesr_var_param_list(ctx,parameters,MAX_PARAMETERS);
	if (nof_parameters == -1) {
		oesr_perror("oesr_var_param_list");
		return -1;
	}
*/
	return 0;
}

void close_variables(void *ctx) {
	int i;
	moddebug("nof_vars=%d\n",nof_vars);
	for (i=0;i<nof_vars;i++) {
		if (vars[i]) {
			if (oesr_var_close(ctx, vars[i])) {
				oesr_perror("oesr_var_close\n");
			}
		}
	}
}

int init_log(void *ctx) {
	mlog = oesr_log_create(ctx, "default");
	moddebug("log=0x%x\n",mlog);
	if (!mlog) {
		oesr_perror("oesr_log_create\n");
		return -1;
	}
	return 0;
}

void close_log(void *ctx) {
	moddebug("log=0x%x\n",mlog);
	if (!mlog) return;
	if (oesr_log_close(mlog)) {
		oesr_perror("oesr_counter_close\n");
	}
}

int init_counter(void *ctx) {
	counter = oesr_counter_create(ctx, "work");
	moddebug("counter=0x%x\n",counter);
	if (!counter) {
		oesr_perror("oesr_counter_create\n");
		return -1;
	}
	return 0;
}

void close_counter(void *ctx) {
	moddebug("counter=0x%x\n",counter);
	if (!counter) return;
	if (oesr_counter_close(counter)) {
		oesr_perror("oesr_counter_close\n");
	}
}

int Init(void *_ctx) {
	int n;
	ctx = _ctx;

	moddebug("enter ts=%d\n",oesr_tstamp(ctx));

	if (!mem_ok) {
		init_memory();
		mem_ok = 1;
	}

	if (!log_ok) {
		mlog = NULL;
		if (USE_LOG) {
			if (init_log(ctx)) {
				return -1;
			}
		}
		log_ok = 1;
	}

/*	if (init_counter(ctx)) {
		return -1;
	}
*/
	if (init_variables(ctx)) {
		return -1;
	}

	moddebug("calling initialize, ts=%d\n",oesr_tstamp(ctx));

	/* this is the module initialize function */
	if (initialize()) {
		moddebug("error initializing module\n",oesr_tstamp(ctx));
		return -1;
	}

	if (!check_ok) {
		if (check_configuration(ctx)) {
			return -1;
		}
		check_ok = 1;
	}

	n = init_interfaces(ctx);
	if (n == -1) {
		return -1;
	} else if (n == 0) {
		return 0;
	}

	moddebug("exit ts=%d\n",oesr_tstamp(ctx));
	return 1;
}

int Stop(void *_ctx) {
	ctx = _ctx;

	moddebug("enter ts=%d\n",oesr_tstamp(ctx));

	stop();

	moddebug("module stoped ok ts=%d\n",oesr_tstamp(ctx));

	close_counter(ctx);
	close_variables(ctx);
	close_interfaces(ctx);

	moddebug("exit ts=%d\n",oesr_tstamp(ctx));
	oesr_exit(ctx);
	return 0;
}

int process_ctrl_packet(void) {
	moddebug("Received ctrl packet to %d, size %d\n",ctrl_in_buffer.pm_idx,ctrl_in_buffer.size);
	if (oesr_var_param_set_value_idx(ctx,ctrl_in_buffer.pm_idx,ctrl_in_buffer.value,
			ctrl_in_buffer.size) == -1) {
		oesr_perror("Error setting control parameter\n");
		return -1;
	}

	return 0;
}

int Run(void *_ctx) {
	ctx = _ctx;
	int tstamp = oesr_tstamp(ctx);
	int i;
	int n;

	if (ctrl_in) {
		do {
			n = oesr_itf_read(ctrl_in, &ctrl_in_buffer, CTRL_IN_BUFFER_SZ,oesr_tstamp(ctx));
			if (n == -1) {
				oesr_perror("oesr_itf_read");
				return -1;
			} else if (n>0) {
				if (process_ctrl_packet()) {
					moderror("Error processing control packet\n");
					return -1;
				}
			}
		} while(n>0);
	}

	for (i=0;i<nof_input_itf;i++) {
		if (!inputs[i]) {
			input_ptr[i] = NULL;
			rcv_len[i] = 0;
		} else {
			n = oesr_itf_ptr_get(inputs[i], &input_ptr[i], &rcv_len[i], tstamp);
			if (n == 0) {
			} else if (n == -1) {
				oesr_perror("oesr_itf_get");
				return -1;
			} else {
				itfdebug("[ts=%d] received %d bytes\n",rtdal_time_slot(),rcv_len[i]);
				rcv_len[i] /= input_sample_sz;
			}
		}
	}
	for (i=0;i<nof_output_itf;i++) {
		if (!outputs[i]) {
			output_ptr[i] = NULL;
		} else {
			n = oesr_itf_ptr_request(outputs[i], &output_ptr[i]);
			if (n == 0) {
				moderror_msg("[ts=%d] no packets available in output interface %d\n",rtdal_time_slot(),i);
			} else if (n == -1) {
				oesr_perror("oesr_itf_request");
				return -1;
			}
		}
	}

	memset(snd_len,0,sizeof(int)*nof_output_itf);

	n = work(input_ptr,output_ptr);
	if (n<0) {
		return -1;
	}

	memset(rcv_len,0,sizeof(int)*nof_input_itf);

	for (i=0;i<nof_output_itf;i++) {
		if (!snd_len[i] && output_ptr[i]) {
			snd_len[i] = n*output_sample_sz;
		}
	}

	for (i=0;i<nof_input_itf;i++) {
		if (input_ptr[i]) {
			n = oesr_itf_ptr_release(inputs[i]);
			if (n == 0) {
				itfdebug("[ts=%d] packet from interface %d not released\n",rtdal_time_slot(),i);
			} else if (n == -1) {
				oesr_perror("oesr_itf_ptr_release\n");
				return -1;
			}
		}
	}
	for (i=0;i<nof_output_itf;i++) {
		if (output_ptr[i] && snd_len[i]) {
			n = oesr_itf_ptr_put(outputs[i],snd_len[i],tstamp);
			if (n == 0) {
				itfdebug("[ts=%d] no space left in output interface %d\n",rtdal_time_slot(),i);
			} else if (n == -1) {
				oesr_perror("oesr_itf_ptr_put\n");
				return -1;
			}
		}
	}

	moddebug("exit Run\n",0);
	return 0;
}


int get_input_samples(int idx) {
	if (idx<0 || idx>nof_input_itf)
			return -1;
	return rcv_len[idx];
}

int set_output_samples(int idx, int len) {
	if (idx<0 || idx>nof_output_itf)
			return -1;
	snd_len[idx] = len*output_sample_sz;
	return 0;
}

int param_get(pmid_t id, void *ptr, int max_size, param_type_t *type) {
	if (type) {
		*type = (param_type_t) oesr_var_param_type(ctx,(var_t) id);
	}
	int n = oesr_var_param_get_value(ctx, (var_t) id, ptr, max_size);
	if (n == -1) {
		/* keep quiet in this case */
		if (oesr_error_code(ctx) != OESR_ERROR_INVAL) {
			oesr_perror("oesr_var_param_value\n");
		}
	}
	return n;
}

pmid_t param_id(char *name) {
	return (pmid_t) oesr_var_param_get(ctx,name);
}



int param_remote_set_ptr(void *out_ptr, int param_idx, void *value, int value_sz) {
	struct ctrl_in_pkt *pkt;

	pkt = out_ptr;
	if (value_sz > CTRL_PKT_VALUE_SZ) {
		return -1;
	}
	if (!pkt) {
		moddebug("output packet not ready, remote parameter %d won't be sent\n",param_idx);
		return -1;
	}
	pkt->pm_idx = param_idx;
	pkt->size = value_sz;
	memcpy(pkt->value,value,value_sz);
	return sizeof(struct ctrl_in_pkt);
}

int param_remote_set(void **out_ptr, int module_idx, int param_idx, void *value, int value_sz) {
	int n;
	if (module_idx < 0 || module_idx > nof_output_itf) {
		return -1;
	}
	n = param_remote_set_ptr(out_ptr[module_idx], param_idx,value,value_sz);
	if (n == -1) {
		return -1;
	}
	set_output_samples(module_idx,n/output_sample_sz);
	return 0;
}


