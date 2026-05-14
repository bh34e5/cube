SRCS=main.cc
LIBS=GL glfw

CXX=g++
FLAGS=-std=c++11 -MMD -g
DEPS=$(SRCS:.cc=.d)

BUILD=build
TARGET=cube

.PHONY: clean run gen

$(BUILD)/$(TARGET): $(SRCS) gl_functions.hh | $(BUILD)
	$(CXX) $(FLAGS) $< -o $@ $(foreach lib,$(LIBS),-l$(lib))

gl_functions.hh: glFunctions.txt $(BUILD)/prepass
	./$(BUILD)/prepass $< $@

$(BUILD)/prepass: prepass.cc | $(BUILD)
	$(CXX) -std=c++11 $^ -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

run: $(BUILD)/$(TARGET)
	./$(BUILD)/$(TARGET)

gen: gl_functions.hh

-include $(foreach d,DEPS,$(BUILD)/$(d))
