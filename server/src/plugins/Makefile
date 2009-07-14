#************************************************************************
#            Atrinik, a Multiplayer Online Role Playing Game            *
#                                                                       *
#                     Copyright (C) 2009 Alex Tokar                     *
#                                                                       * 
# Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
# and Crossfire (Multiplayer game for X-windows).                       *
#                                                                       *
# This program is free software; you can redistribute it and/or modify  *
# it under the terms of the GNU General Public License as published by  *
# the Free Software Foundation; either version 3 of the License, or     *
# (at your option) any later version.                                   *
#                                                                       *
# This program is distributed in the hope that it will be useful,       *
# but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# GNU General Public License for more details.                          *
#                                                                       *
# You should have received a copy of the GNU General Public License     *
# along with this program; if not, write to the Free Software           *
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
#                                                                       *
# The author can be reached at admin@atrinik.org                        *
#***********************************************************************/

PLUGINS = \
	plugin_python \
	plugin_arena

all::
	@for dir in $(PLUGINS); \
	do \
	    echo "Making all in $$dir..."; \
		(cd $$dir; $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' 'EXTRA_CFLAGS=$(EXTRA_CFLAGS)' 'EXTRA_INCLUDES=$(EXTRA_INCLUDES)' 'EXTRA_LIBS=$(EXTRA_LIBS)' 'CC=$(CC)' all || exit 5;) \
	done;

depend::
	@for dir in $(PLUGINS); \
	do \
	    echo "Making depend in $$dir..."; \
	    (cd $$dir; $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' 'EXTRA_CFLAGS=$(EXTRA_CFLAGS)' 'EXTRA_INCLUDES=$(EXTRA_INCLUDES)' 'EXTRA_LIBS=$(EXTRA_LIBS)' depend;) \
	done;

clean::
	@for dir in $(PLUGINS); \
	do \
	    echo "Making clean in $$dir..."; \
	    (cd $$dir; $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' 'EXTRA_CFLAGS=$(EXTRA_CFLAGS)' 'EXTRA_INCLUDES=$(EXTRA_INCLUDES)' 'EXTRA_LIBS=$(EXTRA_LIBS)' clean;) \
	done;

distclean:: clean
	@for dir in $(PLUGINS); \
	do \
	    echo "Making distclean in $$dir..."; \
	    (cd $$dir; $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' 'EXTRA_CFLAGS=$(EXTRA_CFLAGS)' 'EXTRA_INCLUDES=$(EXTRA_INCLUDES)' 'EXTRA_LIBS=$(EXTRA_LIBS)' distclean;) \
	done;
	
proto:
	@for dir in $(PLUGINS); \
	do \
	    echo "Making proto in $$dir..."; \
	    (cd $$dir; $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' 'EXTRA_CFLAGS=$(EXTRA_CFLAGS)' 'EXTRA_INCLUDES=$(EXTRA_INCLUDES)' 'EXTRA_LIBS=$(EXTRA_LIBS)' proto;) \
	done;
