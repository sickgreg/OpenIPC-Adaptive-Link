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
BUILD_N = $(CC) $(SRCS_N) -I $(SDK)/include -I$(TOOLCHAIN)/usr/include -L$(DRV) $(CFLAGS) $(LIB) -levent_core -Os -s -o $(OUTPUT)/$(TARGET_N)
BUILD_P = $(CC) $(SRCS_P) -I $(SDK)/include -I$(TOOLCHAIN)/usr/include -L$(DRV) $(CFLAGS) $(LIB) -levent_core -Os -s -o $(OUTPUT)/$(TARGET_P)
BUILD_Q = $(CC) $(SRCS_Q) -I $(SDK)/include -I$(TOOLCHAIN)/usr/include -L$(DRV) $(CFLAGS) $(LIB) -levent_core -Os -s -o $(OUTPUT)/$(TARGET_Q)

clean:
	rm -f *.o $(TARGET_N) $(TARGET_P) $(TARGET_Q)

goke:
	$(eval SDK = ./sdk/gk7205v300)
	$(eval CFLAGS += -D__GOKE__)
	$(eval LIB = -shared -ldl -ldnvqe -lgk_api -lhi_mpi -lsecurec -lupvqe -lvoice_engine -ldnvqe)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

hisi:
	$(eval SDK = ./sdk/hi3516ev300)
	$(eval CFLAGS += -D__GOKE__)
	$(eval LIB = -shared -ldnvqe -lmpi -lsecurec -lupvqe -lVoiceEngine)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

star6b0:
	$(eval SDK = ./sdk/infinity6)
	$(eval CFLAGS += -D__SIGMASTAR__ -D__INFINITY6__ -D__INFINITY6B0__)
	$(eval LIB = -lcam_os_wrapper -lm -lmi_rgn -lmi_sys)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

star6e:
	$(eval SDK = ./sdk/infinity6)
	$(eval CFLAGS += -D__SIGMASTAR__ -D__INFINITY6__ -D__INFINITY6E__)
	$(eval LIB = -lcam_os_wrapper -lm -lmi_rgn -lmi_sys -lmi_venc)
	$(BUILD_N)
	$(BUILD_P)
	$(BUILD_Q)

