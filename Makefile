# Get the current date and time in the format YYYYMMDD_HHMMSS
VERSION_STRING := $(shell date +"%Y%m%d_%H%M%S")
CFLAGS ?=
CFLAGS += -Wno-address-of-packed-member -DVERSION_STRING="\"$(VERSION_STRING)\""

TARGET_DRONE :=alink_drone
SRCS_DRONE := $(TARGET_DRONE).c

OUTPUT ?= $(PWD)
BUILD_DRONE = $(CC) $(SRCS_DRONE) -I$(TOOLCHAIN)/usr/include -L$(DRV) $(CFLAGS) $(LIB) -levent_core -Os -s -o $(OUTPUT)/$(TARGET_DRONE)


clean:
	rm -f *.o $(TARGET_DRONE)

goke:
	$(eval CFLAGS += -D__GOKE__)
	$(eval LIB = -lm)
	$(BUILD_DRONE)

hi3516:
	$(eval CFLAGS += -D__HI3516__)
	$(eval LIB = -lm)
	$(BUILD_DRONE)

star6b0:
	$(eval CFLAGS += -D__SIGMASTAR__ -D__INFINITY6__ -D__INFINITY6B0__)
	$(eval LIB = -lm)
	$(BUILD_DRONE)

star6c:
	$(eval CFLAGS += -D__SIGMASTAR__ -D__INFINITY6__ -D__INFINITY6C__)
	$(eval LIB = -lm)
	$(BUILD_DRONE)

star6e:
	$(eval CFLAGS += -D__SIGMASTAR__ -D__INFINITY6__ -D__INFINITY6E__)
	$(eval LIB = -lm)
	$(BUILD_DRONE)

native:
	$(eval CFLAGS += -D_x86)
	$(eval LIB = -lm)
	$(eval BUILD = $(CC) $(SRCS) -L $(DRV) $(CFLAGS) $(LIB) -levent_core -O0 -g -o $(OUTPUT))
	$(BUILD_DRONE)

