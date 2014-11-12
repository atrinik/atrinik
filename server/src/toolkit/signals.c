/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * one of those signals has been intercepted, the appropriate action will
 * be done, based on the signal's type - aborting for SIGSEGV, exiting
 * with an error code for others.
 *
 * @author Alex Tokar */

#ifdef WIN32
#define WINVER 0x502
#endif

#include <global.h>
#include <signal.h>

/**
 * Name of the API. */
#define API_NAME signals

/**
 * If 1, the API has been initialized. */
static uint8 did_init = 0;

/**
 * The signals to register. */
static const int register_signals[] = {
#ifndef WIN32
    SIGHUP,
#endif
    SIGINT,
    SIGTERM,
    SIGSEGV
};

/**
 * The signal interception handler.
 * @param signum ID of the signal being intercepted. */
static void signal_handler(int signum)
{
    /* SIGSEGV, so abort instead of exiting normally. */
    if (signum == SIGSEGV) {
        abort();
    }

    exit(1);
}

#ifdef WIN32

void windows_print_stacktrace(CONTEXT *context)
{
    STACKFRAME frame;
    int i;

    SymInitialize(GetCurrentProcess(), 0, 1);

    memset(&frame, 0, sizeof(frame));
    frame.AddrPC.Offset = context->Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context->Esp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;

    i = 0;

    while (StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(),
            GetCurrentThread(), &frame, context, 0, SymFunctionTableAccess,
            SymGetModuleBase, 0)) {
        fprintf(stderr, "%d: %p\n", i, (void *) frame.AddrPC.Offset);
        i++;
    }

    SymCleanup(GetCurrentProcess());
}

LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS *ExceptionInfo)
{
    switch (ExceptionInfo->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
        fputs("Error: EXCEPTION_ACCESS_VIOLATION\n", stderr);
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        fputs("Error: EXCEPTION_ARRAY_BOUNDS_EXCEEDED\n", stderr);
        break;
    case EXCEPTION_BREAKPOINT:
        fputs("Error: EXCEPTION_BREAKPOINT\n", stderr);
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        fputs("Error: EXCEPTION_DATATYPE_MISALIGNMENT\n", stderr);
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        fputs("Error: EXCEPTION_FLT_DENORMAL_OPERAND\n", stderr);
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        fputs("Error: EXCEPTION_FLT_DIVIDE_BY_ZERO\n", stderr);
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        fputs("Error: EXCEPTION_FLT_INEXACT_RESULT\n", stderr);
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        fputs("Error: EXCEPTION_FLT_INVALID_OPERATION\n", stderr);
        break;
    case EXCEPTION_FLT_OVERFLOW:
        fputs("Error: EXCEPTION_FLT_OVERFLOW\n", stderr);
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        fputs("Error: EXCEPTION_FLT_STACK_CHECK\n", stderr);
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        fputs("Error: EXCEPTION_FLT_UNDERFLOW\n", stderr);
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        fputs("Error: EXCEPTION_ILLEGAL_INSTRUCTION\n", stderr);
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        fputs("Error: EXCEPTION_IN_PAGE_ERROR\n", stderr);
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        fputs("Error: EXCEPTION_INT_DIVIDE_BY_ZERO\n", stderr);
        break;
    case EXCEPTION_INT_OVERFLOW:
        fputs("Error: EXCEPTION_INT_OVERFLOW\n", stderr);
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        fputs("Error: EXCEPTION_INVALID_DISPOSITION\n", stderr);
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        fputs("Error: EXCEPTION_NONCONTINUABLE_EXCEPTION\n", stderr);
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        fputs("Error: EXCEPTION_PRIV_INSTRUCTION\n", stderr);
        break;
    case EXCEPTION_SINGLE_STEP:
        fputs("Error: EXCEPTION_SINGLE_STEP\n", stderr);
        break;
    case EXCEPTION_STACK_OVERFLOW:
        fputs("Error: EXCEPTION_STACK_OVERFLOW\n", stderr);
        break;
    default:
        fputs("Error: Unrecognized Exception\n", stderr);
        break;
    }

    fputs("Stack trace:\n", stderr);

    if (EXCEPTION_STACK_OVERFLOW !=
            ExceptionInfo->ExceptionRecord->ExceptionCode) {
        windows_print_stacktrace(ExceptionInfo->ContextRecord);
    } else {
        fprintf(stderr, "%p\n", (void *) ExceptionInfo->ContextRecord->Eip);
    }

    fflush(stderr);

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

/**
 * Initialize the signals API.
 * @internal */
void toolkit_signals_init(void)
{

    TOOLKIT_INIT_FUNC_START(signals)
    {
        size_t i;

        /* Register the signals. */
        for (i = 0; i < arraysize(register_signals); i++) {
#ifdef HAVE_SIGACTION
            struct sigaction new_action, old_action;

            new_action.sa_handler = signal_handler;
            sigemptyset(&new_action.sa_mask);
            new_action.sa_flags = 0;

            sigaction(register_signals[i], NULL, &old_action);

            if (old_action.sa_handler != SIG_IGN) {
                sigaction(register_signals[i], &new_action, NULL);
            }
#else
            signal(register_signals[i], signal_handler);
#endif
        }

#ifdef WIN32
        AddVectoredExceptionHandler(1, windows_exception_handler);
#endif
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the signals API.
 * @internal */
void toolkit_signals_deinit(void)
{

    TOOLKIT_DEINIT_FUNC_START(signals)
    {
    }
    TOOLKIT_DEINIT_FUNC_END()
}
