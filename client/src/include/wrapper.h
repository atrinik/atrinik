/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
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

#if !defined(__WRAPPER_H)
#define __WRAPPER_H

/* include here the hardware depend headers */
#ifdef __WIN_32
#include "win32.h"
#elif __LINUX
#include <cflinux.h>
#define _malloc(__d,__s) malloc(__d)
#endif

#if !defined(HAVE_STRICMP)
#define stricmp(_s1_,_s2_) strcasecmp(_s1_,_s2_)
#endif

#if !defined(HAVE_STRNICMP)
#define strnicmp(_s1_,_s2_,_nrof_) strncasecmp(_s1_,_s2_,_nrof_)
#endif


#define MAX_METASTRING_BUFFER 128*2013

typedef enum _LOGLEVEL
{
        LOG_MSG,
        LOG_ERROR,
        LOG_DEBUG
} _LOGLEVEL;
#define LOGLEVEL LOG_DEBUG
extern void LOG (int logLevel, char *format, ...);

extern char * GetCacheDirectory(void);
extern char * GetGfxUserDirectory(void);
extern char * GetBitmapDirectory(void);
extern char * GetSfxDirectory(void);
extern char * GetMediaDirectory(void);
extern char * GetIconDirectory(void);

extern Boolean SYSTEM_Start(void);
extern Boolean SYSTEM_End(void);
int attempt_fullscreen_toggle(SDL_Surface **surface, uint32 *flags);
uint32 get_video_flags(void);
void parse_metaserver_data(char *info);

#if defined(HAVE_STRNICMP)
#else
#if !defined(HAVE_STRNCASECMP)
int strncasecmp(char *s1, char *s2, int n);
#endif
#endif

#if defined(HAVE_STRICMP)
#else
#if !defined(HAVE_STRCASECMP)
int strcasecmp(char *s1, char*s2);
#endif
#endif

#endif
