CXX = g++
CPPFLAGS = -Iinclude
CXXFLAGS = -std=c++17 -m64 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -finput-charset=UTF-8 -fexec-charset=UTF-8
LDFLAGS = -m64 -static -static-libgcc -static-libstdc++ -s

TARGET = ArcNova.exe
SOURCES = src/main.cpp src/core/terminal.cpp src/core/shell.cpp src/core/parser.cpp src/commands/fs_commands.cpp src/commands/sys_commands.cpp src/core/executor.cpp src/core/input.cpp src/core/config.cpp
HEADERS = include/core/config.hpp include/ui/colors.hpp include/ui/git_prompt.hpp include/ui/banner.hpp include/core/terminal.hpp include/core/shell.hpp include/core/parser.hpp include/commands/fs_commands.hpp include/commands/sys_commands.hpp include/core/executor.hpp include/core/input.hpp
LDLIBS = -ladvapi32

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) -o $@ $(LDLIBS)

ifeq ($(OS),Windows_NT)
clean:
	cmd /C "if exist $(TARGET) del /Q $(TARGET)"
else
clean:
	rm -f $(TARGET)
endif
