CPP := g++# for C++ code
CC := gcc# for C code
CFLAGS := -Wall -DTIXML_USE_STL

ifeq ($(PEDANTIC),y)
   CPP += -ansi -pedantic
endif

ifeq ($(WEXTRA),y)
   CPP += -Wextra
endif

# define the output executable file
# OUT is used by clean. MYOUT is used for compiling to.
OUT := bin/PokerNoLimit
OUTD := bin/PokerNoLimitd

ifeq ($(DEBUG),y)
   CFLAGS += -g
   MYOUT := $(OUTD)
   CFGDIR := debug
else
   CFLAGS += -O3
   MYOUT := $(OUT)
   CFGDIR := release
endif

# define library paths in addition to /usr/lib with -L
# define any libraries to link into executable with -l
LFLAGS := -lpthread

# so that I do not have to change the source of pokereval
INCLUDES = -Ipokereval/include

# file that includes dependencies from makedepend
DEPFILE = .depends

# define the object files
SRC1 := $(wildcard PokerLibrary/*.cpp HandIndexingV1/*.cpp PokerNoLimit/*.cpp TinyXML++/*.cpp)
SRC2 := $(wildcard pokereval/lib/*.c)
OBJ := $(SRC1:%.cpp=$(CFGDIR)/%.o)
OBJ += $(SRC2:%.c=$(CFGDIR)/%.o) 

.PHONY:	clean all

all:
	@mkdir -p $(CFGDIR)/PokerLibrary
	@mkdir -p $(CFGDIR)/PokerNoLimit
	@mkdir -p $(CFGDIR)/HandIndexingV1
	@mkdir -p $(CFGDIR)/pokereval/lib
	@mkdir -p $(CFGDIR)/TinyXML++
	@rm -f $(DEPFILE)
	@touch $(DEPFILE)
	@makedepend -Y -p$(CFGDIR)/ -f$(DEPFILE) -- $(CFLAGS) -- $(INCLUDES) $(SRC1) $(SRC2) 2>/dev/null
	@$(MAKE) $(MYOUT) -r --no-print-directory
	@rm -f $(DEPFILE) $(DEPFILE).bak

clean:
	@echo "deleting crap..."
	@find . -name "*.o" -exec rm {} \;
	@rm -f $(OUT) $(OUTD) $(DEPFILE) $(DEPFILE).bak

$(MYOUT): $(OBJ)
	@echo "$(CPP) (object files) $(LFLAGS) -o $@"
	@$(CPP) $(OBJ) $(LFLAGS) -o $@

$(CFGDIR)/%.o : %.cpp
	$(CPP) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(CFGDIR)/%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

sinclude $(DEPFILE)

