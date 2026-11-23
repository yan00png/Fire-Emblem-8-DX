EE_BIN = fe8_ps2.elf
IRX_NAMES = iomanX fileXio usbd bdm bdmfs_fatfs usbmass_bd libsd audsrv
EE_IRX_OBJS = $(addprefix src/irx/,$(addsuffix _irx.o,$(IRX_NAMES)))
EE_OBJS = src/platform/ps2_platform.o src/platform/ps2_rom.o src/platform/ps2_log.o src/platform/ps2_pad.o src/platform/ps2_crash.o src/platform/ps2_save.o src/platform/ps2_romrun.o src/platform/ps2_audio.o src/platform/ps2_psg.o $(EE_IRX_OBJS) src/menu.o src/main_ps2.o src/backend_gs.o src/gfx_cache.o src/render.o src/platform/gba_mem.o src/platform/bios.o src/platform/dma.o src/platform/lcd.o
EE_INCS += -I$(GSKIT)/include -Isrc
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS += -laudsrv -lgskit -ldmakit -lpad -lfileXio -lpatches -ldebug -leedebug -lc -lkernel -lm
EE_CFLAGS += -DNEWLIB_PORT_AWARE -O2 -std=gnu11 -Wall $(EXTRA_CFLAGS)

# Módulos IOP embutidos: $(PS2SDK)/iop/irx/<nome>.irx -> src/irx/<nome>_irx.c (bin2c) -> objeto
src/irx/%_irx.c: $(PS2SDK)/iop/irx/%.irx
	@mkdir -p src/irx
	$(PS2SDK)/bin/bin2c $< $@ $*_irx

# Build do jogo completo: FE8_BUILD aponta para o diretório gerado por port/build_all.sh (objetos da decomp, dados, banim)
ifdef FE8_BUILD
EE_LINKFILE = port/fe8_ps2.ld
EE_CFLAGS += -DFE8_GAME
EE_OBJS += $(wildcard $(FE8_BUILD)/c/*.o) $(wildcard $(FE8_BUILD)/data/*.o) $(wildcard $(FE8_BUILD)/banim/*.o) $(wildcard $(FE8_BUILD)/banim/motion/*.o) $(FE8_BUILD)/rom_slices.o
endif

all: $(EE_BIN)
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

# Testes no host (sem PS2): valida BIOS/DMA/decodificação de LCD
host-test:
	gcc -O1 -std=gnu11 -Wall -Isrc -o tests/host_test tests/host_test.c src/platform/gba_mem.c src/platform/bios.c src/platform/dma.c src/platform/lcd.c -lm && ./tests/host_test

