UNAMES:=$(shell uname -s)

ifeq ($(UNAMES),Linux)

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit -DRB_ARCH=RB_ARCH_linux
LD:=gcc
LDPOST:=-lz -lX11 -lGLX -lGL -lm -lpulse -lpulse-simple -lpthread
AR:=ar rc

OPT_ENABLE:=evdev glx pulse

else ifeq ($(UNAMES),Darwin)

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit -Wno-parentheses -Wno-tautological-constant-out-of-range-compare -DRB_ARCH=RB_ARCH_macos
LD:=gcc
LDPOST:=-lz -lm
AR:=ar rc

OPT_ENABLE:=

else
  $(error Unable to guess host configuration)
endif
