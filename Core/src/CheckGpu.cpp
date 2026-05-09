#include <cuda_runtime.h>
#include <iostream>
#include <string>

bool checkGPU() {
  int deviceCount = 0;
  cudaError_t error = cudaGetDeviceCount(&deviceCount);

  // Проверяем наличие устройств CUDA
  if (error != cudaSuccess) {
    std::cerr << "CUDA error: " << cudaGetErrorString(error) << std::endl;
    return false;
  }

  if (deviceCount == 0) {
    std::cerr << "No CUDA-capable devices found" << std::endl;
    return false;
  }

  // Получаем информацию о текущем устройстве
  int deviceId = 0;
  error = cudaGetDevice(&deviceId);
  if (error != cudaSuccess) {
    std::cerr << "Failed to get current device: " << cudaGetErrorString(error)
              << std::endl;
    return false;
  }

  cudaDeviceProp deviceProp;
  error = cudaGetDeviceProperties(&deviceProp, deviceId);
  if (error != cudaSuccess) {
    std::cerr << "Failed to get device properties: "
              << cudaGetErrorString(error) << std::endl;
    return false;
  }

  // Получаем информацию о доступной памяти
  size_t freeMemory, totalMemory;
  error = cudaMemGetInfo(&freeMemory, &totalMemory);
  if (error != cudaSuccess) {
    std::cerr << "Failed to get memory info: " << cudaGetErrorString(error)
              << std::endl;
    return false;
  }

  // Выводим информацию о GPU
  std::cout << "\n=== GPU Information ===" << std::endl;
  std::cout << "Device count: " << deviceCount << std::endl;
  std::cout << "Current device ID: " << deviceId << std::endl;
  std::cout << "Device name: " << deviceProp.name << std::endl;
  std::cout << "Compute capability: " << deviceProp.major << "."
            << deviceProp.minor << std::endl;
  std::cout << "Total global memory: " << totalMemory / (1024.0 * 1024.0)
            << " MB" << std::endl;
  std::cout << "Free global memory: " << freeMemory / (1024.0 * 1024.0) << " MB"
            << std::endl;
  std::cout << "Memory in use: "
            << (totalMemory - freeMemory) / (1024.0 * 1024.0) << " MB"
            << std::endl;
  std::cout << "Multiprocessors count: " << deviceProp.multiProcessorCount
            << std::endl;
  std::cout << "Max threads per block: " << deviceProp.maxThreadsPerBlock
            << std::endl;
  std::cout << "Warp size: " << deviceProp.warpSize << std::endl;
  std::cout << "========================\n" << std::endl;

  return true;
}
