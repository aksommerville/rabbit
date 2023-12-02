UNAMESNM:=$(shell uname -snm)

ifneq (,$(strip $(filter Linux,$(UNAMESNM))))

ifneq (,$(strip $(filter raspberrypi,$(UNAMESNM))))

ifneq (,$(strip $(filter aarch64,$(UNAMESNM)))) # pi 4: drmgx,alsa

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit -DRB_ARCH=RB_ARCH_linux -I/usr/include/libdrm
LD:=gcc
LDPOST:=-lz -lm -lasound -lpthread -ldrm -lgbm -lEGL -lGLESv2
AR:=ar rc

OPT_ENABLE:=evdev drmgx alsa

else # pi 1: bcm,alsa

RPIINC:=-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
RPILIB:=-L/opt/vc/lib -lbcm_host 

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit -DRB_ARCH=RB_ARCH_raspi $(RPIINC)
LD:=gcc
LDPOST:=-lz -lm -lasound -lpthread $(RPILIB)
AR:=ar rc

OPT_ENABLE:=evdev alsa bcm

endif
else # general linux: glx,drmgx,alsa,pulse

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit -DRB_ARCH=RB_ARCH_linux -I/usr/include/libdrm
LD:=gcc
LDPOST:=-lz -lX11 -lGLX -lGL -lm -lpulse -lpulse-simple -lasound -lpthread -ldrm -lgbm -lEGL
AR:=ar rc

OPT_ENABLE:=evdev glx drmgx pulse alsa

endif
else ifeq ($(UNAMES),Darwin)

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit -Wno-parentheses -Wno-tautological-constant-out-of-range-compare -DRB_ARCH=RB_ARCH_macos
LD:=gcc
LDPOST:=-lz -lm
AR:=ar rc

OPT_ENABLE:=

else
  $(error Unable to guess host configuration)
endif
