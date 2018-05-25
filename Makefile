APP = jrtp_h264

INCLUDE = ./include/

LIB = ./lib/libjthread.a ./lib/libjthread.so ./lib/libjthread.so.1.3.3  ./lib/libjrtp.a ./lib/libjrtp.so ./lib/libjrtp.so.3.11.1

LINK_OPTS = -ljrtp -ljthread

OBJ  = jrtp_h264.cpp

out: 
	g++ -g $(OBJ) -o $(APP)  $(LINK_OPTS) -L $(LIB) -I $(INCLUDE)

clean:
	rm -rf *o $(APP)
