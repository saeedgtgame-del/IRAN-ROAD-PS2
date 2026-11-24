EE_BIN = IRANROAD.ELF
EE_OBJS = src/main.o
EE_LIBS = -lgskit -ldmakit -lpad -lkernel -lm
EE_INCS += -I$(PS2SDK)/ports/include
EE_LDFLAGS += -L$(PS2SDK)/ports/lib

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
