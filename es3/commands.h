/*
Copyright (c) 2013, Illumina Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions 
are met:
. Redistributions of source code must retain the above copyright 
notice, this list of conditions and the following disclaimer.
. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the 
documentation and/or other materials provided with the distribution.
. Neither the name of the Illumina, Inc. nor the names of its 
contributors may be used to endorse or promote products derived from 
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
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
