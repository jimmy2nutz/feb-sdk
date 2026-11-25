PYTHON ?= python3
BUILD_TOOL = ../../tools/feb_build.py

APP_TITLE = Slot Machine
APP_AUTHOR = Developer
APP_VER = 1.0.0
TARGET = slots.feb

all: $(TARGET)

$(TARGET): main.c ../../include/feb.h
	$(PYTHON) $(BUILD_TOOL) main.c -o $(TARGET) \
		--title "$(APP_TITLE)" \
		--author "$(APP_AUTHOR)" \
		--ver "$(APP_VER)"

clean:
	rm -f $(TARGET)
