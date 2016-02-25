nc = 2

ifeq ($(adjoint),false)
	adj = 
else
	adj = -DADJOINT
endif

CC       = mpigcc
CPP      = mpicxx
OPTIMIZE = -O2 -pipe -march=native -fopenmp #-Wall -Wextra
INC	= -I./extra_libs/Eigen -I./extra_libs/ -I./source/ -I/home/pis52920/local/usr/include/
CFLAGS   = $(OPTIMIZE) $(INC)
CPPFLAGS = $(OPTIMIZE) $(INC) -DMBE="\"$(PWD)/source/utils/MatrixBaseExtension.h\"" $(adj) -DENABLE_MPI -DNUMCOLORS=$(nc) -DMULTITHREADING -DEIGEN

include Makefile.obj.mk

all: leonardQCD
	$(CPP) $(CPPFLAGS) $(OBJECTS) -o leonardQCD.exe /kerndataC/pis52920/libboost_program_options.so.1.49.0 -L/home/pis52920/local/usr/lib/ -Wl,-rpath,/kerndataC/pis52920/
	@echo "Compiled with NUMCOLORS: " $(nc)

leonardQCD: $(OBJECTS)
	@echo "done!"

include Makefile.list.mk

clean:
	-rm ./*.o
	-rm ./*.exe


