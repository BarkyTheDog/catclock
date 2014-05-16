#
# Copyright, 1985, Massachusetts Institute of Technology.
#	xclock - makefile for the X window system clock.
#
#	Written by:	Tony Della Fera, DEC
#			11-Sep-84
SRCS = xclock.c alarm.c
OBJS = xclock.o alarm.o

XLIB      = -lX11
MOTIFLIBS = -lXm -lXt
EXTENSIONLIB = -lXext
SYSLIBS   = -lm
LIBS      = -L/opt/local/lib $(MOTIFLIBS) $(EXTENSIONLIB) $(XLIB) $(SYSLIBS)
#LIBS      = $(MOTIFLIBS) $(EXTENSIONLIB) $(XLIB) $(SYSLIBS)

LOCALINCS = -I.
MOTIFINCS = -I/opt/local/include -I/opt/local/include/X11
#MOTIFINCS = -I/usr/include
INCS      = $(LOCALINCS) $(MOTIFINCS)

#DEFINES     = -DHAS_GNU_EMACS
CDEBUGFLAGS = -g
CFLAGS      = $(DEFINES) $(INCS) $(CDEBUGFLAGS)

DESTINATION = /udir/pjs/bin

PROG  = xclock
DEBUG = debug

DEPEND = makedepend

.SUFFIXES: .o .c

.c.o:
	$(CC) -c $(INCS) $(CFLAGS) $*.c

all: $(PROG)

$(PROG): $(SRCS) Makefile
	$(CC) -o $(PROG) $(CFLAGS) $(SRCS) $(LIBS)
	strip $(PROG)

$(DEBUG): $(OBJS) Makefile
	$(CC) -o $(DEBUG) $(OBJS) $(LIBS)

install: $(PROG)
	install -c -s $(PROG) $(DESTINATION)

lint:
	lint $(INCS) $(SRCS) > lint_errs

clean: 
	rm -f *~ *.bak core *.o \#*\# $(PROG) $(DEBUG) lint_errs

depend:
	$(DEPEND) $(INCS) $(SRCS)


# DO NOT DELETE THIS LINE -- make depend depends on it.
