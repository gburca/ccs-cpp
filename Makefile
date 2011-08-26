FIG_NAME = ccs-cpp
LIB_NAME = ccs
API_DIR = api
MAIN_DIR = src
TEST_DIR = test
TARBALL = resources.tar.gz

ifdef CCACHE_HOME
  CCACHE = $(CCACHE_HOME)/bin/ccache
else ifneq ($(shell which ccache),)
  CCACHE = $(shell which ccache)
else
  CCACHE =
endif

export FIG_REMOTE_URL ?= ftp://devnas/Builds/Fig/repos
LIB_PATH=$(if $(GCC_HOME),$(GCC_HOME)/lib64:,)lib

PLATCFLAGS =
PLATLDFLAGS =
PLATLIBS =

system := $(shell uname)
ifeq ($(system),Linux)
    PLATCFLAGS = -fpic
    PLATLIBS = -lrt
endif
ifeq ($(system),Darwin)
    DARWIN_ARCH = x86_64 # i386
#    PLATCFLAGS = -fPIC -arch $(DARWIN_ARCH) -I/opt/local/include
    PLATCFLAGS = -fPIC -m64 -I/opt/local/include
    PLATLIBS =
    PLATLDFLAGS = -arch $(DARWIN_ARCH) -L/opt/local/lib
endif

CXX = $(CCACHE) $(if $(GCC_HOME),$(GCC_HOME)/bin/,)g++ 
CFLAGS = -std=gnu++0x -ggdb -O3 -Wall -fdiagnostics-show-option \
  $(PLATCFLAGS) $(INCLUDES)
AR = ar rcu
RANLIB = ranlib
RM = rm -f
LIBS = $(PLATLIBS) -Llib
LIBS_TEST = -lgtest
INCLUDES = -Iapi -Isrc \
  -isystem include
INCLUDES_TEST = 
LIB_A = dist/lib/lib$(LIB_NAME).a
LIB_SO = dist/lib/lib$(LIB_NAME).so
INCLUDE_OUT = dist/include

MAIN_SRCS = $(shell find $(MAIN_DIR) -name '*.cpp')
API_INCS = $(shell find $(API_DIR) -name '*.h')
TEST_SRCS = $(shell find $(TEST_DIR) -name '*.cpp')
MAIN_O = $(patsubst $(MAIN_DIR)/%.cpp,out/main/%.o,$(MAIN_SRCS))
API_HC = $(patsubst $(API_DIR)/%.h,out/api/%.hc,$(API_INCS))
TEST_O = $(patsubst $(TEST_DIR)/%.cpp,out/test/%.o,$(TEST_SRCS))
ALL_O = $(MAIN_O) $(TEST_O)
ALL_T = $(API_HC) $(LIB_A) $(LIB_SO) $(TEST_T)
FIG_MAIN = .fig_done_main
FIG_TEST = .fig_done_test
CP_INCLUDE = .headers_copied
TESTS_PASSED = .tests_passed

TEST_T = out/test/run_tests

default: all

all: echo $(ALL_T) $(TESTS_PASSED) $(CP_INCLUDE) $(TARBALL)

$(FIG_MAIN): package.fig
	fig -m -c build && touch $@

$(FIG_TEST): package.fig
	fig -m -c test && touch $@

$(MAIN_O):out/main/%.o: $(MAIN_DIR)/%.cpp $(FIG_MAIN)
	@mkdir -p $(dir $@)
	$(CXX) -c -o $@ $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT "$@" -MT"$(@:%.o=%.d)" $<

$(TEST_O):out/test/%.o: $(TEST_DIR)/%.cpp $(FIG_TEST)
	@mkdir -p $(dir $@)
	$(CXX) -c -o $@ $(CFLAGS) $(INCLUDES_TEST) -MMD -MP -MF"$(@:%.o=%.d)" -MT "$@" -MT"$(@:%.o=%.d)" $<
	
$(LIB_A): $(MAIN_O)
	@mkdir -p $(dir $@)
	$(AR) $@ $?
	$(RANLIB) $@

$(LIB_SO): $(MAIN_O)
	@mkdir -p $(dir $@)
	$(CXX) -o $@ -rdynamic -shared $(PLATLDFLAGS) $^ $(LIBS)

$(CP_INCLUDE): $(shell find $(API_DIR) -name '*.h')
	-@$(RM) -r $(INCLUDE_OUT)
	@mkdir -p $(INCLUDE_OUT)
	cp -a $(API_DIR)/* $(INCLUDE_OUT)
	touch $(CP_INCLUDE)

$(API_HC):out/api/%.hc: $(API_DIR)/%.h $(FIG_DEP)
	@mkdir -p $(dir $@)
	@echo "Checking self-contained: $<"
	@$(CXX) $(CFLAGS) -o /dev/null -c -w $<
	@touch $@

$(TEST_T): $(TEST_O) $(LIB_A)
	@mkdir -p $(dir $@)
	$(CXX) -o $@ $^ $(PLATLDFLAGS) $(LIBS) $(LIBS_TEST)
	
$(TARBALL): $(ALL_T) $(CP_INCLUDE)
	tar zcfp $@ dist

$(TESTS_PASSED): $(TEST_T)
	LD_LIBRARY_PATH=$(LIB_PATH) $(TEST_T)
	touch $(TESTS_PASSED)
	
publish: $(TARBALL)
	if [ -z "$$PROJECT_VERSION" ]; then exit 1; fi
	fig --publish $(FIG_NAME)/$(PROJECT_VERSION)

clean:
	-$(RM) $(ALL_T) $(ALL_O)
	-$(RM) $(CP_INCLUDE)
	-$(RM) $(TESTS_PASSED)
	-$(RM) $(TARBALL)

spotless: clean
	-$(RM) $(ALL_O:.o=.d)
	-$(RM) $(FIG_MAIN)
	-$(RM) $(FIG_TEST)
	-$(RM) -r dist out lib include

echo:
	@echo "******************************************"
	@echo "CXX = $(CXX)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "API_INCS = $(API_INCS)"
	@echo "MAIN_SRCS = $(MAIN_SRCS)"
	@echo "TEST_SRCS = $(TEST_SRCS)"
	@echo "CXX VERSION = $(shell $(CXX) --version)"
	@echo "******************************************"

-include $(ALL_O:.o=.d)

.PHONY: all default test benchmarks echo clean spotless publish