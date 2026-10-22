CC      := gcc
CFLAGS  := -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -O2 -g -pthread
LDFLAGS := -pthread
BUILD   := build

.PHONY: all clean

all: $(BUILD)/minmax

$(BUILD)/minmax: $(BUILD)/main.o $(BUILD)/minmax.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
