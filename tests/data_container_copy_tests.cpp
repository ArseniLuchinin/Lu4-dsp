#include <CpuComplexSignal.hpp>
#include <CpuFloatSignal.hpp>
#include <EmptyContainer.hpp>
#include <GpuByteSignal.hpp>
#include <GpuComplexSignal.hpp>
#include <GpuFloatSignal.hpp>
#include <cuda_runtime.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cuComplex.h>
#include <memory>
#include <vector>

namespace {

bool isCudaAvailable() {
  int deviceCount = 0;
  return cudaGetDeviceCount(&deviceCount) == cudaSuccess && deviceCount > 0;
}

} // namespace

// ============================================================================
// EmptyContainer
// ============================================================================

TEST(DataContainerCopyTest, EmptyContainer_Copy_ReturnsEmpty) {
  EmptyContainer empty;
  auto copy = empty.copy();

  ASSERT_NE(copy, nullptr);
  EXPECT_EQ(copy->getDataName(), "Empty container");
  EXPECT_EQ(copy->size(), 0u);
  EXPECT_FALSE(copy->isValid());
}

// ============================================================================
// CpuFloatSignal
// ============================================================================

TEST(DataContainerCopyTest, CpuFloatSignal_Copy_CreatesIndependentCopy) {
  constexpr size_t size = 10;
  float *hostData = new float[size];
  for (size_t i = 0; i < size; ++i) {
    hostData[i] = static_cast<float>(i) * 1.5f;
  }

  auto original = std::make_shared<CpuFloatSignal>(hostData, size);
  auto copy = std::dynamic_pointer_cast<CpuFloatSignal>(original->copy());

  ASSERT_NE(copy, nullptr);
  EXPECT_TRUE(copy->isValid());
  EXPECT_EQ(copy->size(), size);
  EXPECT_EQ(copy->getDataName(), "CPU float signal");

  // Данные совпадают
  for (size_t i = 0; i < size; ++i) {
    EXPECT_FLOAT_EQ(copy->getData()[i], hostData[i]);
  }

  // Изменение копии не влияет на оригинал
  copy->getData()[0] = 999.0f;
  EXPECT_FLOAT_EQ(original->getData()[0], hostData[0]);
}

TEST(DataContainerCopyTest, CpuFloatSignal_CopyInvalid_ReturnsNull) {
  auto invalid = std::make_shared<CpuFloatSignal>();
  auto copy = invalid->copy();

  EXPECT_EQ(copy, nullptr);
}

// ============================================================================
// CpuComplexSignal
// ============================================================================

TEST(DataContainerCopyTest, CpuComplexSignal_Copy_CreatesIndependentCopy) {
  constexpr size_t size = 5;
  cuComplex *hostData = new cuComplex[size];
  for (size_t i = 0; i < size; ++i) {
    hostData[i] =
        make_cuComplex(static_cast<float>(i), static_cast<float>(i) * 2.0f);
  }

  auto original = std::make_shared<CpuComplexSignal>(hostData, size);
  auto copy = std::dynamic_pointer_cast<CpuComplexSignal>(original->copy());

  ASSERT_NE(copy, nullptr);
  EXPECT_TRUE(copy->isValid());
  EXPECT_EQ(copy->size(), size);
  EXPECT_EQ(copy->getDataName(), "CPU complex signal");

  // Данные совпадают
  for (size_t i = 0; i < size; ++i) {
    EXPECT_FLOAT_EQ(copy->getData()[i].x, hostData[i].x);
    EXPECT_FLOAT_EQ(copy->getData()[i].y, hostData[i].y);
  }

  // Изменение копии не влияет на оригинал
  copy->getData()[0] = make_cuComplex(999.0f, 999.0f);
  EXPECT_FLOAT_EQ(original->getData()[0].x, hostData[0].x);
  EXPECT_FLOAT_EQ(original->getData()[0].y, hostData[0].y);
}

TEST(DataContainerCopyTest, CpuComplexSignal_CopyInvalid_ReturnsNull) {
  auto invalid = std::make_shared<CpuComplexSignal>();
  auto copy = invalid->copy();

  EXPECT_EQ(copy, nullptr);
}

// ============================================================================
// GpuFloatSignal
// ============================================================================

TEST(DataContainerCopyTest, GpuFloatSignal_Copy_CreatesIndependentGpuCopy) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable.";
  }

  constexpr size_t size = 20;
  std::vector<float> hostData(size);
  for (size_t i = 0; i < size; ++i) {
    hostData[i] = static_cast<float>(i) * 0.5f;
  }

  auto original = std::make_shared<GpuFloatSignal>(size);
  original->setDataFromHost(hostData.data(), size);

  auto copy = std::dynamic_pointer_cast<GpuFloatSignal>(original->copy());

  ASSERT_NE(copy, nullptr);
  EXPECT_TRUE(copy->isValid());
  EXPECT_EQ(copy->size(), size);

  // Скачиваем и сравниваем данные
  std::vector<float> copyHost(size);
  const auto err = cudaMemcpy(copyHost.data(), copy->getDeviceData(),
                              size * sizeof(float), cudaMemcpyDeviceToHost);
  ASSERT_EQ(err, cudaSuccess);

  for (size_t i = 0; i < size; ++i) {
    EXPECT_FLOAT_EQ(copyHost[i], hostData[i]);
  }

  // Изменение копии не влияет на оригинал
  std::vector<float> modifiedData(size, -1.0f);
  copy->setDataFromHost(modifiedData.data(), size);

  std::vector<float> originalHost(size);
  cudaMemcpy(originalHost.data(), original->getDeviceData(),
             size * sizeof(float), cudaMemcpyDeviceToHost);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_FLOAT_EQ(originalHost[i], hostData[i]);
  }
}

TEST(DataContainerCopyTest, GpuFloatSignal_CopyInvalid_ReturnsNull) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable.";
  }

  auto invalid = std::make_shared<GpuFloatSignal>();
  auto copy = invalid->copy();

  EXPECT_EQ(copy, nullptr);
}

// ============================================================================
// GpuComplexFloatSignal
// ============================================================================

TEST(DataContainerCopyTest,
     GpuComplexFloatSignal_Copy_CreatesIndependentGpuCopy) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable.";
  }

  constexpr size_t size = 10;
  std::vector<cuComplex> hostData(size);
  for (size_t i = 0; i < size; ++i) {
    hostData[i] =
        make_cuComplex(static_cast<float>(i), static_cast<float>(i) * 3.0f);
  }

  auto original = std::make_shared<GpuComplexFloatSignal>(size);
  original->setDataFromHost(hostData.data(), size);

  auto copy =
      std::dynamic_pointer_cast<GpuComplexFloatSignal>(original->copy());

  ASSERT_NE(copy, nullptr);
  EXPECT_TRUE(copy->isValid());
  EXPECT_EQ(copy->size(), size);

  // Скачиваем и сравниваем данные
  std::vector<cuComplex> copyHost(size);
  const auto err = cudaMemcpy(copyHost.data(), copy->getDeviceData(),
                              size * sizeof(cuComplex), cudaMemcpyDeviceToHost);
  ASSERT_EQ(err, cudaSuccess);

  for (size_t i = 0; i < size; ++i) {
    EXPECT_FLOAT_EQ(copyHost[i].x, hostData[i].x);
    EXPECT_FLOAT_EQ(copyHost[i].y, hostData[i].y);
  }

  // Изменение копии не влияет на оригинал
  std::vector<cuComplex> modifiedData(size, make_cuComplex(-1.0f, -1.0f));
  copy->setDataFromHost(modifiedData.data(), size);

  std::vector<cuComplex> originalHost(size);
  cudaMemcpy(originalHost.data(), original->getDeviceData(),
             size * sizeof(cuComplex), cudaMemcpyDeviceToHost);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_FLOAT_EQ(originalHost[i].x, hostData[i].x);
    EXPECT_FLOAT_EQ(originalHost[i].y, hostData[i].y);
  }
}

// ============================================================================
// GpuByteSignal
// ============================================================================

TEST(DataContainerCopyTest, GpuByteSignal_Copy_CreatesIndependentGpuCopy) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable.";
  }

  constexpr size_t size = 32;
  std::vector<uint8_t> hostData(size);
  for (size_t i = 0; i < size; ++i) {
    hostData[i] = static_cast<uint8_t>(i);
  }

  auto original = std::make_shared<GpuByteSignal>(size);
  original->setDataFromHost(hostData.data(), size);

  auto copy = std::dynamic_pointer_cast<GpuByteSignal>(original->copy());

  ASSERT_NE(copy, nullptr);
  EXPECT_TRUE(copy->isValid());
  EXPECT_EQ(copy->size(), size);

  // Скачиваем и сравниваем данные
  std::vector<uint8_t> copyHost(size);
  const auto err = cudaMemcpy(copyHost.data(), copy->getDeviceData(),
                              size * sizeof(uint8_t), cudaMemcpyDeviceToHost);
  ASSERT_EQ(err, cudaSuccess);

  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(copyHost[i], hostData[i]);
  }

  // Изменение копии не влияет на оригинал
  std::vector<uint8_t> modifiedData(size, 0xFF);
  copy->setDataFromHost(modifiedData.data(), size);

  std::vector<uint8_t> originalHost(size);
  cudaMemcpy(originalHost.data(), original->getDeviceData(),
             size * sizeof(uint8_t), cudaMemcpyDeviceToHost);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(originalHost[i], hostData[i]);
  }
}

TEST(DataContainerCopyTest, GpuByteSignal_CopyInvalid_ReturnsNull) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable.";
  }

  auto invalid = std::make_shared<GpuByteSignal>();
  auto copy = invalid->copy();

  EXPECT_EQ(copy, nullptr);
}

// ============================================================================
// Полиморфные тесты через базовый указатель IData
// ============================================================================

TEST(DataContainerCopyTest, PolymorphicCopy_FloatSignal_ReturnsCorrectType) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable.";
  }

  std::shared_ptr<IData> original = std::make_shared<GpuFloatSignal>(10);
  auto copy = original->copy();

  ASSERT_NE(copy, nullptr);
  EXPECT_NE(dynamic_cast<GpuFloatSignal *>(copy.get()), nullptr);
  EXPECT_EQ(copy->getDataName(), original->getDataName());
}

TEST(DataContainerCopyTest, PolymorphicCopy_ComplexSignal_ReturnsCorrectType) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable.";
  }

  std::shared_ptr<IData> original = std::make_shared<GpuComplexFloatSignal>(10);
  auto copy = original->copy();

  ASSERT_NE(copy, nullptr);
  EXPECT_NE(dynamic_cast<GpuComplexFloatSignal *>(copy.get()), nullptr);
  EXPECT_EQ(copy->getDataName(), original->getDataName());
}
