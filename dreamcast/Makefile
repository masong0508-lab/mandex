# MANDEX - Sega Dreamcast build.  Needs KallistiOS (https://dreamcast.wiki/Getting_Started_with_Dreamcast_development):
#
#   source /opt/toolchains/dc/kos/environ.sh
#   make            -> mandex.elf   (run in Flycast/redream, or burn with a selfboot tool)
#
# NOTE: not run yet - KOS was not available when this was written.  It follows
# the stock KOS example Makefile layout.  The core builds in its default float
# mode here (SH-4 has an FPU), unlike the DS build.

TARGET = mandex.elf
OBJS   = main.o ../core/mx.o ../core/mx_meta.o ../core/cloth.o

KOS_CFLAGS += -std=gnu99 -O2 -I../core -I../art

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lm

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS)
	$(KOS_STRIP) $(TARGET)
