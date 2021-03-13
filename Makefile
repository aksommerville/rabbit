all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

CC:=gcc -c -MMD -O2 -Isrc -Imid -Werror -Wimplicit
LD:=gcc
LDPOST:=-lX11 -lGLX -lGL -lm -lpulse -lpulse-simple -lpthread
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
OFILES_CTEST:=$(filter mid/test/common/%,$(OFILES_ALL))
OFILES_UTEST:=$(filter mid/test/unit/%,$(OFILES_ALL))
OFILES_ITEST:=$(filter mid/test/int/%,$(OFILES_ALL))

LIB_STATIC:=out/librabbit.a
EXE_DEMO:=out/demo
EXE_ITEST:=out/itest
EXES_UTEST:=$(patsubst mid/test/unit/%.o,out/utest/%,$(OFILES_UTEST))
all:$(LIB_STATIC) $(EXE_DEMO) $(EXE_ITEST) $(EXES_UTEST)

$(LIB_STATIC):$(OFILES_LIB);$(PRECMD) $(AR) $@ $^
$(EXE_DEMO):$(OFILES_LIB) $(OFILES_DEMO);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)
$(EXE_ITEST):$(OFILES_LIB) $(OFILES_ITEST) $(OFILES_CTEST);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)
out/utest/%:mid/test/unit/%.o $(OFILES_CTEST);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

demo:$(EXE_DEMO);$(EXE_DEMO)
demo-%:$(EXE_DEMO);$(EXE_DEMO) $*
test:$(EXE_ITEST) $(EXES_UTEST);RB_TEST_FILTER="" etc/tool/runtests.sh $^
test-%:$(EXE_ITEST) $(EXES_UTEST);RB_TEST_FILTER="$*" etc/tool/runtests.sh $^
clean:;rm -rf mid out
