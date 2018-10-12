
TOP_PATH = $(shell pwd)
SRC_PATH = $(TOP_PATH)/src/
EXAMPLE_PATH = $(TOP_PATH)/example/

src := $(wildcard $(SRC_PATH)*.cc)
src_header := $(wildcard $(SRC_PATH)*.h)
src_obj := $(patsubst %.cc, %.o, $(src))
src_depend := $(patsubst %.cc, %.d, $(src))

example := $(wildcard $(EXAMPLE_PATH)*.cc)
example_message := $(wildcard $(EXAMPLE_PATH)message/*.cc)
target := $(patsubst %.cc, %, $(example))

CXX=g++
CXXFLAGS= -std=c++11 \
					-pthread \
					-Wall \
					-Wextra \
					-Werror \
					-Wno-unused-parameter \
					-Wold-style-cast \
					-Woverloaded-virtual \
					-Wpointer-arith \
					-Wwrite-strings \
					-march=native \
					-Wno-format-security \
					-Wshadow \
					-Wconversion \
					# -g

all: $(target) $(src_obj)

# $@ => file name of the target
# $^ => names of all dependency
# $< => name of the first dependency
$(target): $(src) $(src_header) $(example) $(src_obj)
	$(CXX) -std=c++11 -pthread -Wno-format-security -I $(SRC_PATH) $@.cc $(src_obj) $(example_message) -o $@ `pkg-config --cflags --libs protobuf`

# auto generate head files dependency
%.d: %.cc
	@set -e; rm -f $@; \
	$(CC) -MM $(CXXFLAGS) $< > $@.$$$$; \
	sed 's/\([a-zA-Z0-9]\+\).o:/$(shell echo $(SRC_PATH) | sed 's/\//\\\//g')\1.o $(shell echo $(SRC_PATH) | sed 's/\//\\\//g')\1.d:/g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(src_depend)

.PHONY: clean
clean:
	rm -f $(SRC_PATH)*.o \
				$(SRC_PATH)*.d \
				$(SRC_PATH)*.d.* \
				$(EXAMPLE_PATH)*.o \
				$(target)

