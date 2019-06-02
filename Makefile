
SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/*.c)
SOURCES += $(wildcard src/*/*.cpp)
SOURCES += $(wildcard common/*.cpp)
SOURCES += $(wildcard ext/LambertW/*.cc)
SOURCES += $(wildcard widgets/*.cpp)


FLAGS += -I. -I./ext/LambertW 

DISTRIBUTABLES += $(wildcard LICENSE*) res

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
