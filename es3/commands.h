#ifndef COMMANDS_H
#define COMMANDS_H

#include "common.h"
#include "context.h"
#include "agenda.h"

namespace es3 {
	extern int term_width;

	int do_rsync(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help);

	int do_test(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help);
	int do_touch(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help);

	int do_rm(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help);
	int do_publish(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help);	
	int do_du(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help);
	int do_ls(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help);
	
	int do_cat(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help);
    int do_lsr(context_ptr context, const stringvec& params,
             agenda_ptr ag, bool help);
    int do_mass_rm(context_ptr context, const stringvec& params,
             agenda_ptr ag, bool help);
}; //namespace es3

#endif //COMMANDS_H
