# server: main.cpp  ThreadPool.h http_conn.h http_conn.cpp locker.h  webserver.h webserver.cpp config.h util.h util.cpp
# 	g++ -o server main.cpp ThreadPool.h http_conn.h http_conn.cpp locker.h webserver.h webserver.cpp config.h util.h util.cpp -lpthread

# clean:
# 	rm  -r server

# MAINSOURCE代表含有main入口函数的cpp文件，因为含有测试代码，
# 所以要为多个目标编译，这里把Makefile写的通用了一点，
# 以后加东西Makefile不用做多少改动

MAINSOURCE := main.cpp
# MAINOBJS := $(patsubst %.cpp,%.o,$(MAINSOURCE))
SOURCE  := $(wildcard *.cpp *.h)
override SOURCE := $(filter-out $(MAINSOURCE),$(SOURCE))
OBJS    := $(patsubst %.cpp,%.o,$(SOURCE))

TARGET  := WebServer
CC      := g++
LIBS    := -lpthread
INCLUDE:= -I./usr/local/lib
CFLAGS  := -std=c++11 -g -Wall -O3 -D_PTHREADS
CXXFLAGS:= $(CFLAGS)


.PHONY : objs clean veryclean rebuild all tests debug
all : $(TARGET)
objs : $(OBJS)
rebuild: veryclean all

clean :
	find . -name '*.o' | xargs rm -f
veryclean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET) | xargs rm -f
debug:
	@echo $(SOURCE)

$(TARGET) : $(OBJS) main.o
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)
# $@代表目标，这里是$(TARGET)
