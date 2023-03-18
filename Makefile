MAKEFLAGS += --no-builtin-rules # Disable implicit rules execution.
BUILD = ./build

all: DirSyncD

DirSyncD: $(BUILD)/DirSyncD
$(BUILD)/DirSyncD: $(BUILD)/DirSyncD.o
	gcc $^ -o $@
$(BUILD)/DirSyncD.o: DirSyncD.c
	@mkdir -p $(BUILD)
	gcc $^ -c -o $@

clean:
	@rm -rvf $(BUILD)
