# Copyright (c) 2024 Ivaylo Haratcherev
# All Rights Reserved
#

#
# @brief	Makefile for room_temp 
# @file		Makefile
# @Author	Ivaylo Haratcherev
#########################################################################
# @note		Makefile for room_temp 
#
#

CC=gcc
STRIP=strip
APP_INC= 
APP_CC_FLAGS=
APP_LN_FLAGS=-li2c


GENERIC_APP = room_temp
GENERIC_OBJS = room_temp.o 

%.o : %.c 
	$(CC) $(APP_CC_FLAGS) $(APP_INC) -c $< -o $@

$(GENERIC_APP) : $(GENERIC_OBJS)
	$(CC) $(GENERIC_OBJS) $(APP_LN_FLAGS) -o $(GENERIC_APP)
	$(STRIP) -s $(GENERIC_APP) 

all: $(GENERIC_APP)

clean :
	@echo "  CLEAN	."
	find . -name "*.[oa]" -exec rm {} \;
	rm -f *.o $(GENERIC_APP)

