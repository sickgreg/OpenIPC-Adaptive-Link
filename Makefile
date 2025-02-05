# Get the current date and time in the format YYYYMMDD_HHMMSS
VERSION_STRING := $(shell date +"%Y%m%d_%H%M%S")
CFLAGS ?=
CFLAGS += -Wno-address-of-packed-member -DVERSION_STRING="\"$(VERSION_STRING)\""

TARGET_N :="ALink42n"
TARGET_P :="ALink42p"
TARGET_Q :="ALink42q"

SRCS_N := $(TARGET_N).c
SRCS_P := $(TARGET_P).c
SRCS_Q := $(TARGET_Q).c

OUTPUT ?= $(PWD)
BUILD_N = $(CC) $(SRCS_N) -I$(TOOLCHAIN)/usr/include -L$(DRV) $(CFLAGS) $(LIB) -levent_core -Os -s -o $(OUTPUT)/$(TARGET_N)
BUILD_P = $(CC) $(SRCS_P) -I$(TOOLCHAIN)/usr/include -L$(DRV) $(CFLAGS) $(LIB) -levent_core -Os -s -o $(OUTPUT)/$(TARGET_P)
BUILD_Q = $(CC) $(SRCS_Q) -I$(TOOLCHAIN)/usr/include -L$(DRV) $(CFLAGS) $(LIB) -levent_core -Os -s -o $(OUTPUT)/$(TARGET_Q)

clean:
	rm -f *.o $(TARGET_N) $(TARGET_P) $(TARGET_Q)

goke:
	$(eval CFLAGS += -D__GOKE__)
	$(eval LIB = -ldl -ldnvqe -lgk_api -lhi_mpi -lsecurec -lupvqe -lvoice_engine -ldnvqe)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

hisi:
	$(eval CFLAGS += -D__GOKE__)
	$(eval LIB = -ldnvqe -lmpi -lsecurec -lupvqe -lVoiceEngine)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

hi3536:
	$(eval CFLAGS += -D__GOKE__ -D__HI3536__)
	$(eval LIB = -lm -ldnvqe -lmpi -ljpeg -lupvqe -lVoiceEngine)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

star6b0:
	$(eval CFLAGS += -D__SIGMASTAR__ -D__INFINITY6__ -D__INFINITY6B0__)
	$(eval LIB = -lcam_os_wrapper -lm -lmi_rgn -lmi_sys)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

star6c:
	$(eval CFLAGS += -D__SIGMASTAR__ -D__INFINITY6__ -D__INFINITY6C__)
	$(eval LIB = -lcam_os_wrapper -lmi_rgn -lmi_sys)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

star6e:
	$(eval CFLAGS += -D__SIGMASTAR__ -D__INFINITY6__ -D__INFINITY6E__)
	$(eval LIB = -lcam_os_wrapper -lm -lmi_rgn -lmi_sys -lmi_venc)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

native:
	$(eval CFLAGS += -D_x86)
	$(eval LIB = -lcsfml-graphics -lcsfml-window -lcsfml-system `pkg-config --libs cairo x11` -lm)
	$(eval BUILD = $(CC) $(SRCS) -L $(DRV) $(CFLAGS) $(LIB) -levent_core -O0 -g -o $(OUTPUT))
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)