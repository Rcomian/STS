SLUG = SmallTownSound
VERSION = 0.6.0.1

SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/*.c)

DISTRIBUTABLES += $(wildcard LICENSE*) res
BINARIES += src/SpringReverbIR.pcm


RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk

