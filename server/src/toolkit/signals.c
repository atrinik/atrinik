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
 * This API, when imported, will register the signals defined in
 * ::register_signals (SIGSEGV, SIGINT, etc) for interception. When any
 * one of those signals has been intercepted, the ::handler_func will be
 * called, if it has been registered using signals_register_handler_func()
 * after importing the API. This handler function is completely optional,
 * and should only be used for freeing data allocated by the application.
 *
 * When the handler function (if any) finished, the appropriate action
 * will be done, based on the signal's type - aborting for SIGSEGV,
 * exiting with an error code for others.
 *
 * @author Alex Tokar */

#include <global.h>
#include <signal.h>

/**
 * The function to call when intercepting a signal. */
static signals_handler_func handler_func;

/**
 * The signals to register. */
static const int register_signals[] =
{
	SIGINT, SIGTERM, SIGHUP, SIGSEGV
};

/**
 * The signal interception handler.
 * @param signum ID of the signal being intercepted. */
static void signal_handler(int signum)
{
	/* If a handler function has been defined, call it first. */
	if (handler_func)
	{
		handler_func();
	}

	/* SIGSEGV, so abort instead of exiting normally. */
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

		/* No handler function. */
		handler_func = NULL;

		/* Register the signals. */
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

/**
 * Register a handler function, which will be called when intercepting a
 * signal.
 * @param func The handler function to register. */
void signals_register_handler_func(signals_handler_func func)
{
	handler_func = func;
}
