.EXPORT_ALL_VARIABLES:

.SUFFIXES:

.PHONY: clean all

# := is only evaluated once

SHELL 		= /bin/sh

LIB_DIR 	= $(HOME)/lib
BIN_DIR		= $(HOME)/bin

NAME		= CaenReadout

ROOTLIBS     	:= $(shell root-config --glibs)
ROOTINC      	:= -I$(shell root-config --incdir)

COMMON_DIR 	= $(HOME)/CommandLineInterface

INCLUDES        = -I$(COMMON_DIR) -I.

LIBRARIES	= CommandLineInterface CAENDigitizer

CC		= gcc
CXX   = g++
CPPFLAGS	= $(ROOTINC) $(INCLUDES) -fPIC
CXXFLAGS	= -pedantic -Wall -Wno-long-long -g -O3 -std=c++11

LDFLAGS		= -g -fpic

LDLIBS 		= -L$(LIB_DIR) $(ROOTLIBS) $(addprefix -l,$(LIBRARIES))

LOADLIBES = \
				CaenSettings.o \
				CaenDigitizer.o \
				CaenEvent.o \
				$(NAME)Dictionary.o 

# -------------------- implicit rules --------------------
# n.o made from n.c by 		$(CC) -c $(CPPFLAGS) $(CFLAGS)
# n.o made from n.cc by 	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS)
# n made from n.o by 		$(CC) $(LDFLAGS) n.o $(LOADLIBES) $(LDLIBS)

# -------------------- rules --------------------

all:  $(NAME) Histograms $(LIB_DIR)/lib$(NAME).so
	@echo Done

$(LIB_DIR)/lib$(NAME).so: $(LOADLIBES)
	$(CXX) $(LDFLAGS) -shared -Wl,-soname,lib$(NAME).so -o $(LIB_DIR)/lib$(NAME).so $(LOADLIBES) -lc

# -------------------- pattern rules --------------------
# this rule sets the name of the .cc file at the beginning of the line (easier to find)

%.o: %.cc %.hh
	$(CXX) $< -c $(CPPFLAGS) $(CXXFLAGS) -o $@

# -------------------- default rule for executables --------------------

%: %.cc $(LOADLIBES)
	$(CXX) $< $(CXXFLAGS) $(CPPFLAGS) $(LOADLIBES) $(LDLIBS) -o $@

# -------------------- Root stuff --------------------

DEPENDENCIES = \
					CaenSettings.hh \
					CaenEvent.hh \
					RootLinkDef.h

$(NAME)Dictionary.o: $(NAME)Dictionary.cc
	 $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<

$(NAME)Dictionary.cc: $(DEPENDENCIES)
	 rm -f $(NAME)Dictionary.cc $(NAME)Dictionary.h; rootcint -f $@ -c $(CPPFLAGS) $(DEPENDENCIES)

# -------------------- tar ball --------------------

tar:
	@echo "creating zipped tar-ball ... "
	@cd ..; tar -chvzf $(NAME)/$(NAME).tar.gz $(NAME)/Makefile \
	$(NAME)/*.hh $(NAME)/*.cc; cd $(NAME);

# -------------------- clean --------------------

clean:
	rm  -f $(NAME) *.o
