MAKEFLAGS += --no-builtin-rules # Disable implicit rules execution.
GCC = gcc
BUILD = ./build
TARGET = DirSyncD
INCLUDE = $(addprefix -I,$(shell find include/ -type d -print))
SOURCE = $(shell find source/ -type f -iregex ".*\.c")
OBJECTS = $(SOURCE:%.c=$(BUILD)/%.o)

all: $(TARGET)

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(GCC) $(INCLUDE) -c -o $@ $<
# INCLUDE with gcc's -I option allows to not specify
# full .h file paths in #include directives in .c files.

$(TARGET): $(BUILD)/$(TARGET)

$(BUILD)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(GCC) -o $(BUILD)/$(TARGET) $^

clean:
	@rm -rvf $(BUILD)
