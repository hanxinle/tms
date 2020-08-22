# =======================================================================================
INCLUDE_DIR    += -I. -I./include/ -I../common -I../common/ssl/ -I../obj/include -I../

LIB_DIR        += ../obj/lib/libsrt.a
LIB_DIR        += ../obj/lib/libssl.a
LIB_DIR        += ../obj/lib/libcrypto.a
LIB_DIR        += ../obj/lib/libsrtp2.a
LIB_DIR        += -lpthread -lbz2 -lz -ldl

# =======================================================================================
CC             = gcc
CXX 		   = g++
CFLAGS         = -g -W -Wall -Werror -O2
CXXFLAGS       = -g -std=c++11 -W -Wall -Werror -O2 -Wno-strict-aliasing -Wno-missing-field-initializers -Wno-unused-function -Wno-deprecated-declarations \
				 -Wno-unused-variable -Wno-unused-parameter -Wno-unused-value -Wno-class-memaccess

CXXFLAGS       += -DUSE_PUBLISH
CXXFLAGS       += -D__STDC_CONSTANT_MACROS

# =======================================================================================
SOURCES += $(wildcard ./*.cpp)
SOURCES += $(wildcard ../common/*.cpp)
SOURCES += $(wildcard ../common/ssl/*.cpp)
OBJECTS += $(patsubst %.cpp,%.o, $(patsubst %.c,%.o, $(SOURCES)))

# =======================================================================================
ALL_OBJECTS = $(OBJECTS)

# =======================================================================================
DEP_FILE += $(foreach obj, $(ALL_OBJECTS), $(dir $(obj)).$(basename $(notdir $(obj))).d)

# =======================================================================================
TARGET = tms

# =======================================================================================
all: $(TARGET)

-include $(DEP_FILE)

.%.d: %.cpp
	@set -e; rm -f $@; \
	$(CXX) $(INCLUDE_DIR) $(CXXFLAGS) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.cpp
	$(CXX) $(INCLUDE_DIR) $(CXXFLAGS) -o $@ -c $<

.%.d: %.c
	@set -e; rm -f $@; \
	$(CC) $(INCLUDE_DIR) $(CFLAGS) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.c
	$(CC) $(INCLUDE_DIR) $(CFLAGS) -o $@ -c $<

$(TARGET): $(OBJECTS)
	$(CXX) $(INCLUDE_DIR) $(CXXFLAGS) $(OBJECTS) $(LIB_DIR) -o $@

clean:
	rm -f $(DEP_FILE) $(OBJECTS) $(TARGET) *.o
	rm -rf *.mp4 *.m4s *.flv *.aac
