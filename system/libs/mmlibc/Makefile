#Makefile fragment for system library
#Bryan E. Topp <betopp@betopp.com> 2021
	
LIBDIR := mmlibc
LIBVAR := MMLIBC

$(LIBVAR)_SRC := $(L)/$(LIBDIR)/src
$(LIBVAR)_OBJ := $(O)/$(LIBDIR)
$(LIBVAR)_A   := $(A)/$(LIBDIR).a

$(LIBVAR)_CSRC := $(shell find $($(LIBVAR)_SRC) -name *.c)
$(LIBVAR)_COBJ := $(patsubst $($(LIBVAR)_SRC)/%.c, $($(LIBVAR)_OBJ)/%.o, $($(LIBVAR)_CSRC))

sys.tar: $($(LIBVAR)_A)

$($(LIBVAR)_A): $($(LIBVAR)_COBJ)
	$(AR_A)
	
$($(LIBVAR)_OBJ)/%.o : $($(LIBVAR)_SRC)/%.c
	$(CC_OBJ)
