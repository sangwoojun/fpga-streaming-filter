TARGET := hw
BUILD_DIR := ../$(TARGET)
KERNEL_DIR := ../c-kernel/
CXXFLAGS += -g -std=c++17 -Wall -O2 -lrt


.PHONY: all swsim

all: $(wildcard *)
	mkdir -p $(BUILD_DIR)
	g++ $(CXXFLAGS) host.cpp StreamInterface.cpp -o $(BUILD_DIR)/app.exe -D HWTARGET -I${XILINX_XRT}/include/ -L$(XILINX_XRT)/lib -lxrt_coreutil -pthread

swsim: $(wildcard *) $(KERNEL_DIR)/kernel.cpp
	mkdir -p $(BUILD_DIR)
	g++ $(CXXFLAGS) host.cpp StreamInterfaceSw.cpp $(KERNEL_DIR)/kernel.cpp -o $(BUILD_DIR)/app.exe -I${XILINX_XRT}/include/ -L$(XILINX_XRT)/lib -lxrt_coreutil -pthread


clean:
	rm -rf $(BUILD_DIR)
