#ifndef DECLARE

#define PSS_LEN		62
#define SSS_LEN		62
#define PSS_RE		6*NOF_RE_X_OSYMB
#define SSS_RE		PSS_RE

#define PSSCELLID0 	25.0
#define PSSCELLID1 	29.0
#define PSSCELLID2 	34.0


#define PI		3.14159265

#else
int lte_pss_init(struct lte_phch_config *ch, struct lte_grid_config *config);
int lte_sss_init(struct lte_phch_config *ch, struct lte_grid_config *config);

int lte_pss_put(complex_t *sss, complex_t *output,
		struct lte_symbol *location, struct lte_grid_config *config);
int lte_sss_put(real_t *sss, complex_t *output,
		struct lte_symbol *location, struct lte_grid_config *config);

void generate_pss(complex_t *signal, int direction, struct lte_grid_config *config);
void generate_sss(real_t *signal, struct lte_grid_config *config);

#endif
