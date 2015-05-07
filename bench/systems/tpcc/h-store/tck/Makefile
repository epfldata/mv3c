#-----Macros---------------------------------

BOOST = /usr/include/boost

INCLUDES = -I. -I$(BOOST)/include

# PAPI
PAPI_HOME = /usr/local/src/papi-3.0.8.1
IFLAGS += -I$(PAPI_HOME)/src/perfctr-2.6.x/usr.lib
IFLAGS += -I$(PAPI_HOME)/src/perfctr-2.6.x/linux/include
IFLAGS += -I$(PAPI_HOME)
IFLAGS += -I/usr/include
LFLAGS += $(PAPI_HOME)/src/libpapi.a

# set up compiler and options
CXX = g++
#CXXFLAGS = -pg -g $(INCLUDES)
CXXFLAGS = -O3 $(INCLUDES)

#-----Suffix Rules---------------------------
# set up C++ suffixes and relationship between .cc and .o files

.SUFFIXES: .cpp

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<

.cpp :
	$(CXX) $(CXXFLAGS) $< -o $@ -lm $(TLIB)

#-----File Dependencies----------------------

SRC = TupleBuffer.cpp \
      test.cpp \
      HashIndex.cpp \
      UnexpectedException.cpp \
      Exception.cpp \
      Socket.cpp \
      ClientSocket.cpp \
      ServerSocket.cpp \
      StopWatch.cpp \
      util.cpp

MCORE_SRC= TupleBuffer.cpp \
			test-mcore.cpp \
			HashIndex.cpp \
			UnexpectedException.cpp \
			Exception.cpp \
			Socket.cpp \
			ClientSocket.cpp \
			ServerSocket.cpp \
			StopWatch.cpp \
			util.cpp

THREADS_SRC= TupleBuffer.cpp \
			 Master-threads.cpp \
			 HashIndex.cpp \
			 UnexpectedException.cpp \
			 Exception.cpp \
			 Socket.cpp \
			 ClientSocket.cpp \
			 ServerSocket.cpp \
			 StopWatch.cpp \
			 util.cpp

OBJ = $(addsuffix .o, $(basename $(SRC)))
MCORE_OBJ = $(addsuffix .o, $(basename $(MCORE_SRC)))
THREADS_OBJ = $(addsuffix .o, $(basename $(THREADS_SRC)))
SINGLE_CORE_BINARIES = test driver
MCORE_BINARIES = test-mcore driver-mcore
THREADS_BINARIES = master-threads driver-threads

all: $(SINGLE_CORE_BINARIES)
score: $(SINGLE_CORE_BINARIES)
mcore: $(MCORE_BINARIES)
threads: $(THREADS_BINARIES)

test: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) -lm $(TLIB) -L$(BOOST)/lib -lboost_system

test-mcore: $(MCORE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(MCORE_OBJ) -lm -ltbb $(TLIB) -L$(BOOST)/lib -lboost_system

master-threads: $(THREADS_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(THREADS_OBJ) -lm -ltbb -lpthread $(TLIB)

papi: $(OBJ)
	$(CXX) $(CXXFLAGS) $(IFLAGS) -o $@ $(OBJ) ${LFLAGS}  -ltbb -lm $(TLIB)

driver: Driver.o ClientSocket.o Socket.o StopWatch.o
	$(CXX) $(CXXFLAGS) -o $@ Driver.o ClientSocket.o Socket.o StopWatch.o -lm $(TLIB)

driver-mcore: Driver-mcore.o ClientSocket.o Socket.o StopWatch.o
	$(CXX) $(CXXFLAGS) -o $@ Driver-mcore.o ClientSocket.o Socket.o StopWatch.o -lm -ltbb $(TLIB)

driver-threads: Driver-threads.o ClientSocket.o Socket.o StopWatch.o
	$(CXX) $(CXXFLAGS) -o $@ Driver-threads.o ClientSocket.o Socket.o StopWatch.o -lm -ltbb $(TLIB)

#-----Other stuff----------------------------
depend:
	makedepend $(CXXFLAGS) -Y $(SRC)

clean:
	rm -f $(OBJ) $(MCORE_OBJ) $(MCORE_BINARIES) $(THREADS_OBJ) $(THREADS_BINARIES) \
	Driver.o Driver-mcore.o Driver-threads.o driver papi test

exec: all
	@rm test.o
	@echo "--------------------------- W=1"
	@DYLD_LIBRARY_PATH=$(BOOST)/lib ./test W 1
	@echo "--------------------------- W=5"
	@DYLD_LIBRARY_PATH=$(BOOST)/lib ./test W 5
	@echo "--------------------------- W=10"
	@DYLD_LIBRARY_PATH=$(BOOST)/lib ./test W 10

q: all
	@rm test.o
