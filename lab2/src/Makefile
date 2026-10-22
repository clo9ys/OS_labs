CC     := gcc
CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -O2 -g
BUILD  := build

COMMON := $(BUILD)/utils.o

.PHONY: all parent child clean

all: parent child

parent: $(BUILD)/parent
child:  $(BUILD)/child

$(BUILD)/parent: $(BUILD)/parent.o $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/child: $(BUILD)/child.o $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
