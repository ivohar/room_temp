# Copyright (c) 2024 Ivaylo Haratcherev
# All Rights Reserved
#

#
# @brief	Makefile for room_temp and temp_humid
# @file		Makefile
# @Author	Ivaylo Haratcherev
#########################################################################
# @note		Makefile for room_temp and temp_humid
#
#

CC=gcc
STRIP=strip
APP_INC= 
APP_CC_FLAGS=
APP_LN_FLAGS=-li2c


GENERIC_APP = temp_humid
GENERIC_OBJS = temp_humid.o \
			i2cbusses.o common.o

GENERIC_APP1 = room_temp
GENERIC_OBJS1 = room_temp.o \
			i2cbusses.o common.o

default: all

%.o : %.c 
	$(CC) $(APP_CC_FLAGS) $(APP_INC) -c $< -o $@

$(GENERIC_APP) : $(GENERIC_OBJS)
	$(CC) $(GENERIC_OBJS) $(APP_LN_FLAGS) -o $(GENERIC_APP)
	$(STRIP) -s $(GENERIC_APP) 

$(GENERIC_APP1) : $(GENERIC_OBJS1)
	$(CC) $(GENERIC_OBJS1) $(APP_LN_FLAGS) -o $(GENERIC_APP1)
	$(STRIP) -s $(GENERIC_APP1) 

all: $(GENERIC_APP) $(GENERIC_APP1)	

clean :
	@echo "  CLEAN	."
	find . -name "*.[oa]" -exec rm {} \;
	rm -f *.o $(GENERIC_APP) $(GENERIC_APP1)

