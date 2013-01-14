

#include "sndcard.h"
#include "x5400.h"
#include "uhd.h"

/** Use this structure to configure available dacs and their functions
 * (terminate with null)
 */
struct dac_interface system_dacs[] = {

#ifdef HAVE_UHD
		/** USRP board*/
		{	"uhd",
			uhd_readcfg,
			uhd_init,
			uhd_close
		},

#endif

#ifdef HAVE_JACK
		/** soundcard dac */
		{	"soundcard",
			sndcard_readcfg,
			sndcard_init,
			sndcard_close
		},
#endif

#ifdef HAVE_X5
		/** x5-400 board dac */
		{	"x5-400",
			x5_readcfg,
			x5_init,
			x5_close
		},
#endif
		/** add here your dac */

		/* end of list */
		{NULL,NULL,NULL}};

