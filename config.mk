CC = cc
CXX = c++
OBJCOPY = objcopy
AR = ar
MKDIR = mkdir -p
RM = rm -f

CPPFLAGS += -Wall -fPIC -g -I'./include' -pipe -Werror -pedantic
CFLAGS += $(CPPFLAGS) -std=gnu17
CXXFLAGS += $(CPPFLAGS)
LDFLAGS += -lshttp -lsutil -lpthread

