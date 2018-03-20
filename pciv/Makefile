
include ../Makefile.param

LIBS:=$(REL_LIB)/libpciv.a $(REL_LIB)/libmpi.a $(REL_LIB)/libhdmi.a

INC_FLAGS += -I$(REL_INC) -I../common

# target source
SRC  := $(wildcard *.c)  
OBJ  := $(SRC:%.c=%.o)

TARGET := $(OBJ:%.o=%)

#CFLAGS += -Wall -O2
#CFLAGS += -lpthread -lm 

host:
	$(CC) -o sample_pciv_host  sample_pciv_host.c  pciv_msg.c pciv_trans.c sample_common.c $(COMMON_DIR)/sample_comm_sys.c $(COMMON_DIR)/sample_comm_vo.c $(INC_FLAGS) $(CFLAGS) $(LIBS) $(AUDIO_LIBA) $(JPEGD_LIBA)
slave:
	$(CC) -o sample_pciv_slave sample_pciv_slave.c pciv_msg.c pciv_trans.c loadbmp.c sample_common.c $(COMMON_DIR)/sample_comm_sys.c $(INC_FLAGS) $(CFLAGS) $(LIBS) $(AUDIO_LIBA) $(JPEGD_LIBA)

clean:
	@rm sample_pciv_slave sample_pciv_host *.o -fr
