/*
    suPHP - (c)2002-2004 Sebastian Marsching <sebastian@marsching.com>
    
    This file is part of suPHP.

    suPHP is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    suPHP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with suPHP; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "suphp.h"

void error_exit(int errcode)
{
 fprintf(stderr, "Error on executing script(%d)\n", errcode);
 _exit(errcode);
}

void error_msg_exit(int errcode, char *msg, char *file, int line)
{
 fprintf(stderr, "Error in %s on line %d: %s\n", file, line, msg);
 _exit(errcode);
}

void error_sysmsg_exit(int errcode, char *msg, char *file, int line)
{
 if (log_initialized)
  suphp_log_error("System error: %s (%s)", msg, strerror(errno));
 fprintf(stderr, "Error in %s on line %d: %s (%s)\n", file, line, msg, strerror(errno));
 _exit(errcode);
}
