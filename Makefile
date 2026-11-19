EE_BIN = bin/BALDI.ELF
EE_OBJS = src/main.o src/audio.o

EE_INCS = -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/ports/include
EE_LDFLAGS = -L$(PS2SDK)/ee/lib -L$(PS2SDK)/ports/lib
EE_LIBS = -lgskit -ldmakit -lpad -laudsrv -lm -lc -lkernel

all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)
	mkdir -p bin
	$(EE_CC) $(EE_LDFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LIBS)
	ee-strip --strip-all $(EE_BIN)

iso: $(EE_BIN)
	mkdir -p iso_build
	cp $(EE_BIN) iso_build/BALDI.ELF
	cp iso_files/SYSTEM.CNF iso_build/SYSTEM.CNF
	mkisofs -l -o bin/BALDI_PS2.ISO -V "BALDI_PS2" iso_build/

clean:
	rm -f $(EE_OBJS) $(EE_BIN) bin/BALDI_PS2.ISO
	rm -rf iso_build

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
