# Generated automatically from Makefile.in by configure.
# *****************************************************************************
# Configuration part. Edit those variables to fit your own system setup.
# *****************************************************************************

# Default libraries required. Edit those to match $(blddir)/Modules/Makefile
# if needed.
LIBS		= -lnsl -ldl -lpthread -lutil

# Supply the Python version used. In most cases, this will be something like 2.x
# If python is installed in very unusal positions, this must be changed by hand
PYTHONVER      	= 2.6

# Supply the python /include and /lib positions
LIBROOT 	= /home/alex/usr/32/lib
INCLUDEROOT 	= /home/alex/usr/32/include

EXTRA_CFLAGS    = 

# When configure fails set these to the required pathes
#LIBROOT        = /usr/lib
#INCLUDEROOT    = /usr/include

# *****************************************************************************
# End of user configuration part. Don't change anything after this line,
# unless things really go wrong with the default setup.
# *****************************************************************************

RM        = /bin/rm
CC        = gcc

MAKEDOC = doc_utils/make_documentation.pl

SRCS = plugin_python.c atrinik_map.c atrinik_object.c

OBJECTS   = $(SRCS:.c=.o)
INCLUDE   = -I./include -I../../include -I$(INCLUDEROOT)/python$(PYTHONVER)

CFLAGS    = -g -O3 -fPIC $(EXTRA_CFLAGS)
DEPEND = /usr/bin/makedepend

LIBPYTHON = $(LIBROOT)/python$(PYTHONVER)/config/libpython$(PYTHONVER).a

LDFLAGS=	-export-dynamic -rdynamic -Xlinker -Bsymbolic 
SYSLIBS=	-lm
ALLLIBS=	$(LIBPYTHON) $(LIBS) $(SYSLIBS)

all : plugin_python.so.0.1 doc

clean :
	$(RM) -f *.o core plugin_python.so.0.1

distclean : clean
	$(RM) -f *.o core plugin_python.so.0.1 Makefile

plugin_python.so.0.1 : $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,plugin_python.so.0 $(LDFLAGS) $(OBJECTS) ../../common/libcross.a $(ALLLIBS) -o plugin_python.so.0.1

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

doc:	
#doc/Map_Methods.txt doc/Object_Methods.txt doc/DaimoninFunctions.txt
#doc/Map_Methods.txt: DaimoninMap.c
#	$(MAKEDOC) $< > $@

#doc/Object_Methods.txt: DaimoninObject.c
#	$(MAKEDOC) $< > $@

#doc/DaimoninFunctions.txt: plugin_python.c
#	$(MAKEDOC) $< > $@

depend:
	$(DEPEND) $(DEPENDFLAGS) -- $(CFLAGS) $(INCLUDE) --

proto:

# DO NOT DELETE THIS LINE -- make depend depends on it.
