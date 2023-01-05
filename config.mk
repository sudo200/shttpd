CC = cc
CXX = c++
OBJCOPY = objcopy
AR = ar
MKDIR = mkdir -p
RM = rm -f

CPPFLAGS += -Wall -fPIC -g -I'./include' -pipe -Werror -pedantic
CFLAGS += $(CPPFLAGS)
CXXFLAGS += $(CPPFLAGS)
LDFLAGS += -lshttp -lsutil -lpthread

