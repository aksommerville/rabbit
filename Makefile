all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit
LD:=gcc
LDPOST:=-lz -lX11 -lGLX -lGL -lm -lpulse -lpulse-simple -lpthread
AR:=ar rc

GENERATED_FILES:=$(addprefix mid/, \
  test/int/rb_itest_toc.h \
)

CFILES:=$(shell find src -name '*.c') $(filter %.c,$(GENERATED_FILES))
OFILES_ALL:=$(addsuffix .o,$(patsubst src/%.c,mid/%,$(CFILES)))
-include $(OFILES_ALL:.o=.d)

mid/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<

mid/test/int/rb_itest_toc.h:etc/tool/genitesttoc.sh $(filter src/test/int/%,$(CFILES));$(PRECMD) $^ $@
mid/test/int/rb_itest_main.o:mid/test/int/rb_itest_toc.h

OFILES_LIB:=$(filter mid/lib/%,$(OFILES_ALL))
OFILES_DEMO:=$(filter mid/demo/%,$(OFILES_ALL))
OFILES_CLI:=$(filter mid/cli/%,$(OFILES_ALL))
OFILES_CTEST:=$(filter mid/test/common/%,$(OFILES_ALL))
OFILES_UTEST:=$(filter mid/test/unit/%,$(OFILES_ALL))
OFILES_ITEST:=$(filter mid/test/int/%,$(OFILES_ALL))

LIB_STATIC:=out/librabbit.a
EXE_DEMO:=out/demo
EXE_CLI:=out/rabbit
EXE_ITEST:=out/itest
EXES_UTEST:=$(patsubst mid/test/unit/%.o,out/utest/%,$(OFILES_UTEST))
all:$(LIB_STATIC) $(EXE_DEMO) $(EXE_CLI) $(EXE_ITEST) $(EXES_UTEST)

$(LIB_STATIC):$(OFILES_LIB);$(PRECMD) $(AR) $@ $(OFILES_LIB)
$(EXE_DEMO):$(OFILES_LIB) $(OFILES_DEMO);$(PRECMD) $(LD) -o $@ $(OFILES_LIB) $(OFILES_DEMO) $(LDPOST)
$(EXE_CLI):$(OFILES_LIB) $(OFILES_CLI);$(PRECMD) $(LD) -o $@ $(OFILES_LIB) $(OFILES_CLI) $(LDPOST)
$(EXE_ITEST):$(OFILES_LIB) $(OFILES_ITEST) $(OFILES_CTEST);$(PRECMD) $(LD) -o $@ $(OFILES_LIB) $(OFILES_ITEST) $(OFILES_CTEST) $(LDPOST)
out/utest/%:mid/test/unit/%.o $(OFILES_CTEST);$(PRECMD) $(LD) -o $@ $< $(OFILES_CTEST) $(LDPOST)

edit:$(EXE_CLI);$(EXE_CLI)
edit-%:$(EXE_CLI);$(EXE_CLI) $*
demo:$(EXE_DEMO);$(EXE_DEMO)
demo-%:$(EXE_DEMO);$(EXE_DEMO) $*
test:$(EXE_ITEST) $(EXES_UTEST);RB_TEST_FILTER="" etc/tool/runtests.sh $(EXE_ITEST) $(EXES_UTEST)
test-%:$(EXE_ITEST) $(EXES_UTEST);RB_TEST_FILTER="$*" etc/tool/runtests.sh $(EXE_ITEST) $(EXES_UTEST)
clean:;rm -rf mid out

#--------------------------------------------------
# Data rules. These are a template for client apps to copy.

DATASRCDIR:=src/data
DATAMIDDIR:=mid/data
DATADST:=out/data
DATAPLAN:=$(DATAMIDDIR)/plan
DATASRCFILES:=$(shell find $(DATASRCDIR) -type f)
all:$(DATADST)
$(EXE_DEMO):$(DATADST)
$(EXE_ITEST):$(DATADST)

ifneq ($(MAKECMDGOALS),clean)
  include $(DATAPLAN)
endif
$(DATAPLAN):$(EXE_CLI) $(DATASRCFILES);$(PRECMD) $(EXE_CLI) plan --dst=$(DATAPLAN) --data=$(DATASRCDIR)
