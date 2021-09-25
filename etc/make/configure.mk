UNAMES:=$(shell uname -s)

ifeq ($(UNAMES),Linux)

UNAMEN:=$(shell uname -n)
ifeq ($(UNAMEN),raspberrypi)

RPIINC:=-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
RPILIB:=-L/opt/vc/lib -lbcm_host 
#-lGLESv2 -lEGL 

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit -DRB_ARCH=RB_ARCH_raspi $(RPIINC)
LD:=gcc
LDPOST:=-lz -lm -lasound -lpthread $(RPILIB)
AR:=ar rc

OPT_ENABLE:=evdev alsa bcm

else

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit -DRB_ARCH=RB_ARCH_linux
LD:=gcc
LDPOST:=-lz -lX11 -lGLX -lGL -lm -lpulse -lpulse-simple -lpthread
AR:=ar rc

OPT_ENABLE:=evdev glx pulse

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
