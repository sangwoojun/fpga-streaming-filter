#include <iostream>
#include <xrt/xrt_device.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_kernel.h>
#include <cstring>
#include <unistd.h>

#define BUFFER_BYTES 4096 // 4KB buffer

int main() {
    try {
        std::string binaryFile = "./kernel.xclbin";
        int device_index = 0;

        // Open the FPGA device
        std::cout << "Open the device " << device_index << std::endl;
        xrt::device device(device_index);

        // Load the .xclbin file onto the FPGA
        std::cout << "Load the xclbin " << binaryFile << std::endl;
        auto uuid = device.load_xclbin(binaryFile);

        // Create the kernel object
        auto kernel = xrt::kernel(device, uuid, "kernel", xrt::kernel::cu_access_mode::exclusive);

        // Allocate input and output buffers
        xrt::bo inBufDev = xrt::bo(device, BUFFER_BYTES, kernel.group_id(1));
        xrt::bo outBufDev = xrt::bo(device, BUFFER_BYTES, kernel.group_id(2));

        // Map buffers to host memory
        void* inBufHost = inBufDev.map();
        void* outBufHost = outBufDev.map();

        // Initialize input buffer with values from 1 to 4096
        int32_t* in_data = (int32_t*)inBufHost;
        for (int i = 0; i < BUFFER_BYTES / sizeof(int32_t); ++i) {
            in_data[i] = i + 1;
        }

        // Manually set the output buffer to verify mapping
        int32_t* out_data = (int32_t*)outBufHost;

        // Clear the output buffer before kernel execution
        std::fill((uint8_t*)outBufHost, (uint8_t*)outBufHost + BUFFER_BYTES, 0);

        // Synchronize input buffer to device
        inBufDev.sync(XCL_BO_SYNC_BO_TO_DEVICE, BUFFER_BYTES, 0);

        // Prepare and execute the kernel
        xrt::run run_kernel = xrt::run(kernel);
        run_kernel.set_arg(0, BUFFER_BYTES);  // Set the size of the buffer
        run_kernel.set_arg(1, inBufDev);      // Set the input buffer
        run_kernel.set_arg(2, outBufDev);     // Set the output buffer

        // Start the kernel
        std::cout << "Starting kernel execution..." << std::endl;
        run_kernel.start();
        run_kernel.wait(); // Wait for the kernel to finish

        // Synchronize output buffer back to host
        outBufDev.sync(XCL_BO_SYNC_BO_FROM_DEVICE, BUFFER_BYTES, 0);

        // Verify received data
        std::cout << "Verifying received data after kernel execution..." << std::endl;
        for (int i = 0; i < BUFFER_BYTES / sizeof(int32_t); ++i) {
            std::cout << "Data[" << i << "]: " << out_data[i] << std::endl;
        }

        std::cout << "Kernel execution complete." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

