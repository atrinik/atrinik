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

#ifdef HAVE_SIGACTION
#include <execinfo.h>
#endif

/**
 * The signals to register. */
static const int register_signals[] = {
#ifndef WIN32
    SIGHUP,
#endif
    SIGSEGV,
    SIGFPE,
    SIGILL,
    SIGTERM,
    SIGABRT
};

TOOLKIT_API();

/**
 * The signal interception handler.
 * @param signum ID of the signal being intercepted.
 */
static void simple_signal_handler(int signum)
{
    if (signum == SIGABRT) {
#ifdef WIN32
        RaiseException(STATUS_ACCESS_VIOLATION, 0, 0, 0);
#else
        return;
#endif
    }

    /* SIGSEGV, so abort instead of exiting normally. */
    if (signum == SIGSEGV) {
        abort();
    }

    exit(1);
}

#if defined(WIN32) || defined(HAVE_SIGACTION)
#ifdef WIN32
static LONG WINAPI signal_handler(EXCEPTION_POINTERS *ExceptionInfo)
#else
static void signal_handler(int sig, siginfo_t *siginfo, void *context)
#endif
{
    struct tm *tm;
    static time_t t = 0;
    char path[MAX_BUF], date[MAX_BUF], *homedir;
    FILE *fp;

#ifndef WIN32
    homedir = getenv("HOME");
#else
    homedir = getenv("APPDATA");
#endif

    if (t == 0) {
        t = time(NULL);
    }

    tm = localtime(&t);

    strftime(VS(date), "%Y_%m_%d_%H-%M-%S", tm);
    snprintf(VS(path), "%s/.atrinik/"EXECUTABLE"-traceback-%s.txt", homedir,
            date);
    fp = fopen(path, "a");

    if (fp == NULL) {
        snprintf(VS(path), EXECUTABLE"-traceback-%s.txt", date);
        fp = fopen(path, "a");

        if (fp == NULL) {
            fp = stderr;
        }
    }

#ifdef WIN32
    switch (ExceptionInfo->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
        fputs("Error: EXCEPTION_ACCESS_VIOLATION\n", fp);
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        fputs("Error: EXCEPTION_ARRAY_BOUNDS_EXCEEDED\n", fp);
        break;
    case EXCEPTION_BREAKPOINT:
        fputs("Error: EXCEPTION_BREAKPOINT\n", fp);
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        fputs("Error: EXCEPTION_DATATYPE_MISALIGNMENT\n", fp);
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        fputs("Error: EXCEPTION_FLT_DENORMAL_OPERAND\n", fp);
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        fputs("Error: EXCEPTION_FLT_DIVIDE_BY_ZERO\n", fp);
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        fputs("Error: EXCEPTION_FLT_INEXACT_RESULT\n", fp);
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        fputs("Error: EXCEPTION_FLT_INVALID_OPERATION\n", fp);
        break;
    case EXCEPTION_FLT_OVERFLOW:
        fputs("Error: EXCEPTION_FLT_OVERFLOW\n", fp);
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        fputs("Error: EXCEPTION_FLT_STACK_CHECK\n", fp);
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        fputs("Error: EXCEPTION_FLT_UNDERFLOW\n", fp);
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        fputs("Error: EXCEPTION_ILLEGAL_INSTRUCTION\n", fp);
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        fputs("Error: EXCEPTION_IN_PAGE_ERROR\n", fp);
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        fputs("Error: EXCEPTION_INT_DIVIDE_BY_ZERO\n", fp);
        break;
    case EXCEPTION_INT_OVERFLOW:
        fputs("Error: EXCEPTION_INT_OVERFLOW\n", fp);
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        fputs("Error: EXCEPTION_INVALID_DISPOSITION\n", fp);
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        fputs("Error: EXCEPTION_NONCONTINUABLE_EXCEPTION\n", fp);
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        fputs("Error: EXCEPTION_PRIV_INSTRUCTION\n", fp);
        break;
    case EXCEPTION_SINGLE_STEP:
        fputs("Error: EXCEPTION_SINGLE_STEP\n", fp);
        break;
    case EXCEPTION_STACK_OVERFLOW:
        fputs("Error: EXCEPTION_STACK_OVERFLOW\n", fp);
        break;
    default:
        fputs("Error: Unrecognized Exception\n", fp);
        break;
    }
#else
    switch (sig) {
    case SIGSEGV:
        fputs("Caught SIGSEGV: Segmentation Fault\n", fp);
        break;
    case SIGFPE:
        switch (siginfo->si_code) {
        case FPE_INTDIV:
            fputs("Caught SIGFPE: (integer divide by zero)\n", fp);
            break;
        case FPE_INTOVF:
            fputs("Caught SIGFPE: (integer overflow)\n", fp);
            break;
        case FPE_FLTDIV:
            fputs("Caught SIGFPE: (floating-point divide by zero)\n", fp);
            break;
        case FPE_FLTOVF:
            fputs("Caught SIGFPE: (floating-point overflow)\n", fp);
            break;
        case FPE_FLTUND:
            fputs("Caught SIGFPE: (floating-point underflow)\n", fp);
            break;
        case FPE_FLTRES:
            fputs("Caught SIGFPE: (floating-point inexact result)\n", fp);
            break;
        case FPE_FLTINV:
            fputs("Caught SIGFPE: (floating-point invalid operation)\n", fp);
            break;
        case FPE_FLTSUB:
            fputs("Caught SIGFPE: (subscript out of range)\n", fp);
            break;
        default:
            fputs("Caught SIGFPE: Arithmetic Exception\n", fp);
            break;
        }
        break;
    case SIGILL:
        switch (siginfo->si_code) {
        case ILL_ILLOPC:
            fputs("Caught SIGILL: (illegal opcode)\n", fp);
            break;
        case ILL_ILLOPN:
            fputs("Caught SIGILL: (illegal operand)\n", fp);
            break;
        case ILL_ILLADR:
            fputs("Caught SIGILL: (illegal addressing mode)\n", fp);
            break;
        case ILL_ILLTRP:
            fputs("Caught SIGILL: (illegal trap)\n", fp);
            break;
        case ILL_PRVOPC:
            fputs("Caught SIGILL: (privileged opcode)\n", fp);
            break;
        case ILL_PRVREG:
            fputs("Caught SIGILL: (privileged register)\n", fp);
            break;
        case ILL_COPROC:
            fputs("Caught SIGILL: (coprocessor error)\n", fp);
            break;
        case ILL_BADSTK:
            fputs("Caught SIGILL: (internal stack error)\n", fp);
            break;
        default:
            fputs("Caught SIGILL: Illegal Instruction\n", fp);
            break;
        }
        break;
    case SIGTERM:
        fputs("Caught SIGTERM: a termination request was sent to the program\n",
                fp);
        break;
    case SIGABRT:
        fputs("Caught SIGABRT: usually caused by an abort() or assert()\n", fp);
        break;
    default:
        break;
    }
#endif

#ifdef WIN32
    fputs("Stack trace:\n", fp);

    if (EXCEPTION_STACK_OVERFLOW !=
            ExceptionInfo->ExceptionRecord->ExceptionCode) {
        STACKFRAME frame;
        int i;

        SymInitialize(GetCurrentProcess(), 0, 1);

        memset(&frame, 0, sizeof(frame));
        frame.AddrPC.Offset = ExceptionInfo->ContextRecord->Eip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrStack.Offset = ExceptionInfo->ContextRecord->Esp;
        frame.AddrStack.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = ExceptionInfo->ContextRecord->Ebp;
        frame.AddrFrame.Mode = AddrModeFlat;

        i = 0;

        while (StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(),
                GetCurrentThread(), &frame, ExceptionInfo->ContextRecord, 0,
                SymFunctionTableAccess, SymGetModuleBase, 0)) {
            fprintf(fp, "%d: %p\n", i, (void *) frame.AddrPC.Offset);
            i++;
        }

        SymCleanup(GetCurrentProcess());
    } else {
        fprintf(fp, "%p\n", (void *) ExceptionInfo->ContextRecord->Eip);
    }
#else
    {
        void *stack_traces[64];
        int i, trace_size = 0;
        char **messages = NULL;

        trace_size = backtrace(stack_traces, 64);
        messages = backtrace_symbols(stack_traces, trace_size);

        for (i = 0; i < trace_size; i++) {
            fprintf(fp, "%d: %s\n", i, messages[i]);
        }

        if (messages) {
            free(messages);
        }
    }
#endif

    if (fp == stderr) {
        fflush(stderr);
    } else {
        fclose(fp);
    }

#ifdef WIN32
    return EXCEPTION_EXECUTE_HANDLER;
#else
    simple_signal_handler(sig);
#endif
}

#endif

TOOLKIT_INIT_FUNC(signals)
{
    size_t i;
#ifdef HAVE_SIGACTION
    stack_t ss;

    ss.ss_sp = malloc(SIGSTKSZ * 4);

    if (ss.ss_sp == NULL) {
        log_error("OOM.");
        abort();
    }

    ss.ss_size = SIGSTKSZ * 4;
    ss.ss_flags = 0;

    if (sigaltstack(&ss, NULL) != 0) {
        LOG(ERROR, "Could not set up alternate stack.");
        exit(1);
    }
#endif

    /* Register the signals. */
    for (i = 0; i < arraysize(register_signals); i++) {
#ifdef HAVE_SIGACTION
        struct sigaction sig_action;

        sig_action.sa_sigaction = signal_handler;
        sigemptyset(&sig_action.sa_mask);
        sig_action.sa_flags = SA_SIGINFO | SA_ONSTACK;

        if (sigaction(register_signals[i], &sig_action, NULL) != 0) {
            LOG(ERROR, "Could not register signal: %d",
                    register_signals[i]);
            exit(1);
        }
#else
        signal(register_signals[i], simple_signal_handler);
#endif
    }

#ifdef WIN32
    AddVectoredExceptionHandler(1, signal_handler);
#endif
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(signals)
{
}
TOOLKIT_DEINIT_FUNC_FINISH
