include MakeVars

C_SRCS= $(shell ls *.c 2> /dev/null)
C_OBJS= $(patsubst %.c,%.o,$(C_SRCS))

all: $(EXE) $(DYNLIB) dirs

dirs: 
	@(for i in $(DIRS); \
	  do make -C $$i; \
	done)

%.o: %.c
	$(CC) $(CFLAGS) -c $< 

$(EXE) : $(C_OBJS)
	$(CC) $(CFLAGS) -I. -o $(EXE) $(C_OBJS) $(LIBS)

$(DYNLIB) : $(C_OBJS)
	$(LD) $(LDFLAGS) -I. -o $(DYNLIB) $(C_OBJS)

clean :
	@(for i in $(DIRS); \
	  do make -C $$i clean; \
	done)
	rm -f *.o $(EXE) $(DYNLIB)
	rm -f *~

