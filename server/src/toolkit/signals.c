/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the Free Software           *
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
*                                                                       *
* The author can be reached at admin@atrinik.org                        *
************************************************************************/

/**
 * @file
 * Signals API.
 *
 * @author Alex Tokar */

#include <global.h>
#include <signal.h>

static void (*handler_func)(void);

static const int register_signals[] =
{
	SIGINT, SIGTERM, SIGHUP, SIGSEGV
};

static void signal_handler(int signum)
{
	if (handler_func)
	{
		handler_func();
	}

	if (signum == SIGSEGV)
	{
		abort();
	}

	exit(1);
}

/**
 * Initialize the signals API.
 * @internal */
void toolkit_signals_init(void)
{
	TOOLKIT_INIT_FUNC_START(signals)
	{
		size_t i;
		struct sigaction new_action, old_action;

		handler_func = NULL;

		for (i = 0; i < arraysize(register_signals); i++)
		{
			new_action.sa_handler = signal_handler;
			sigemptyset(&new_action.sa_mask);
			new_action.sa_flags = 0;

			sigaction(register_signals[i], NULL, &old_action);

			if (old_action.sa_handler != SIG_IGN)
			{
				sigaction(register_signals[i], &new_action, NULL);
			}
		}
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the signals API.
 * @internal */
void toolkit_signals_deinit(void)
{
}

void signals_set_handler_func(void (*func)(void))
{
	handler_func = func;
}
