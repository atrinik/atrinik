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

#include <include.h>

unsigned long hashbmap(char *str, int tablesize)
{
    unsigned long hash = 0;
    int i = 0, rot = 0;
    char *p;

    for (p = str; i < MAXHASHSTRING && *p; p++, i++) {
        hash ^= (unsigned long) *p << rot;
        rot += 2;
        if (rot >= ((int) sizeof(long) - (int) sizeof(char)) * 8)
            rot = 0;
    }
    return (hash % tablesize);
}

_bmaptype *find_bmap(char *name)
{
  _bmaptype *at;
  unsigned long index;

  if (name == NULL)
    return (_bmaptype *) NULL;

  index=hashbmap(name, BMAPTABLE);
  for(;;) {
    at = bmap_table[index];
    if (at==NULL) /* not in our bmap list */
		return NULL;
    if (!strcmp(at->name,name))
      return at;
    if(++index>=BMAPTABLE)
      index=0;
  }
}

void add_bmap(_bmaptype *at)
 {
  int index=hashbmap(at->name, BMAPTABLE),org_index=index;

  for(;;) {

  if(bmap_table[index] && !strcmp(bmap_table[index]->name,at->name))
  {
	  LOG(LOG_ERROR,"ERROR: add_bmap(): double use of bmap name %s\n",at->name);
  }
  if(bmap_table[index]==NULL) {
      bmap_table[index]=at;
      return;
    }
    if(++index==BMAPTABLE)
      index=0;
    if(index==org_index)
	  LOG(LOG_ERROR,"ERROR: add_bmap(): bmaptable to small\n");
  }
}

void FreeMemory(void **p)
{
        if(p==NULL)
                return;
        if(*p != NULL)
                free(*p);
        *p=NULL;
}

char * show_input_string(char *text, struct _Font *font, int wlen)
{
    register int i, j,len;

    static char buf[MAX_INPUT_STR];
    strcpy(buf, text);

    len = strlen(buf);
    while (len >= CurrentCursorPos)
    {
        buf[len + 1] = buf[len];
        len--;
    }
    buf[CurrentCursorPos] = '_';

    for (len = 25,i = CurrentCursorPos; i >= 0; i--)
    {
        if (!buf[i])
            continue;
        if (len + font->c[(int) (buf[i])].w + font->char_offset >= wlen)
        {
            i--;
            break;
        }
        len += font->c[(int) (buf[i])].w + font->char_offset;
    }

    len -= 25;
    for (j = CurrentCursorPos; j <= (int) strlen(buf); j++)
    {
        if (len + font->c[(int) (buf[j])].w + font->char_offset >= wlen)
        {
            break;
        }
        len += font->c[(int) (buf[j])].w + font->char_offset;
    }
    buf[j] = 0;

    return(&buf[++i]);
}

int read_substr_char(char *srcstr, char *desstr, int *sz, char ct)
{
        register unsigned char c;
        register int s=0;

        desstr[0]=0;
        for(;s<1023;)
        {
                c = *(desstr+s++) =*(srcstr+*sz); /* get character*/
                if(c==0x0d)
                        continue;
                if(c==0)
                        return(-1);
                if (c == 0x0a || c == ct)  /* if it END or WHITESPACE..*/
                        break; /* have a single word! (or not...)*/
                        (*sz)++; /* point to next source char */
        }
        *(desstr+(--s)) = 0; /* terminate all times with 0, */
                                        /* s: string length*/
        (*sz)++; /*point to next source charakter return(s);*/
        return s;
}

void * _my_malloc(size_t blen, char *info)
{
    LOG(LOG_DEBUG, "Malloc(): size %d info: %s\n", blen, info);
    return malloc(blen);
}

/*
 * Based on (n+1)^2 = n^2 + 2n + 1
 * given that	1^2 = 1, then
 *		2^2 = 1 + (2 + 1) = 1 + 3 = 4
 * 		3^2 = 4 + (4 + 1) = 4 + 5 = 1 + 3 + 5 = 9
 * 		4^2 = 9 + (6 + 1) = 9 + 7 = 1 + 3 + 5 + 7 = 16
 *		...
 * In other words, a square number can be express as the sum of the
 * series n^2 = 1 + 3 + ... + (2n-1)
 */
int isqrt(int n)
{
	int result, sum, prev;
	result = 0;
	prev = sum = 1;
	while (sum <= n) {
		prev += 2;
		sum += prev;
		++result;
	}
	return result;
}

/* this function gets a ="xxxxxxx" string from a
 * line. It removes the =" and the last " and returns
 * the string in a static buffer. */
char *get_parameter_string(char *data, int *pos)
{
	char *start_ptr, *end_ptr;
	static char buf[4024];

	/* we assume a " after the =... don't be to shy, we search for a '"' */
	start_ptr = strchr(data+*pos,'"');
	if(!start_ptr)
		return NULL; /* error */

	end_ptr = strchr(++start_ptr,'"');
	if(!end_ptr)
		return NULL; /* error */

	strncpy(buf, start_ptr, end_ptr-start_ptr);
	buf[end_ptr-start_ptr]=0;

	/* ahh... ptr arithmetic... eat that, high level language fans ;) */
	*pos += ++end_ptr-(data+*pos);

	return buf;
}
