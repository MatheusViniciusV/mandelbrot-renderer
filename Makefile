.PHONY: all clean help

APPNAME := $(notdir $(CURDIR))

USER_CFLAGS := -mavx2 -fopenmp -mfma

USER_LIBS   := -fopenmp -lm

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Iincludes $(USER_CFLAGS)

ifeq ($(OS),Windows_NT)
    PLATFORM := win32
    TARGET   := $(APPNAME).exe
    SRCS     := main.c platforms/win32.c
    LIBS     := -lmingw32 -lgdi32 -luser32 -lkernel32 $(USER_LIBS)
    MSG      := "Building for Windows (Win32)..."
else
    PLATFORM := linux
    TARGET   := $(APPNAME)
    SRCS     := main.c platforms/linux_x11.c
    LIBS     := -lX11 $(USER_LIBS)
    MSG      := "Building for Linux (X11)..."
endif

all: $(TARGET)

$(TARGET): $(SRCS)
	@echo $(MSG)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	@echo "Cleaning up..."
	-rm -f $(TARGET) $(TARGET).exe

help:
	@echo "Usage:"
	@echo "  make        - Auto-detects OS and builds '$(TARGET)'"
	@echo "  make clean  - Removes the executable"