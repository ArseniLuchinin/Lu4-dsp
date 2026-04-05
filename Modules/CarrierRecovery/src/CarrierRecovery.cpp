#include <CarrierRecovery.hpp>

#include <module.hpp>

#include <cuda_runtime.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

extern cudaError_t carrierRecoveryQueryReduceStorageBytes(const cuComplex* in, size_t n, int order, size_t* storageBytes);
extern cudaError_t carrierRecoveryRunReduce(
    const cuComplex* in,
    size_t n,
    int order,
    void* tempStorage,
    size_t tempStorageBytes,
    double2* outSum
);
extern void launchCarrierRecoveryRotateKernel(
    const cuComplex* in,
    cuComplex* out,
    size_t n,
    float c,
    float s,
    int blocks,
    int threads
);

IModule* createModule() {
    return new CarrierRecovery();
}

CarrierRecovery::CarrierRecovery()
    : IModule({"CarrierRecovery", "", ""})
{}

CarrierRecovery::~CarrierRecovery() {
    if (m_devSum) {
        cudaFree(m_devSum);
        m_devSum = nullptr;
    }
    if (m_reduceTempStorage) {
        cudaFree(m_reduceTempStorage);
        m_reduceTempStorage = nullptr;
        m_reduceTempStorageBytes = 0;
    }
}

bool CarrierRecovery::init() {
    if (m_order <= 0) {
        ERROR << "CarrierRecovery::init failed: order must be > 0." << std::endl;
        return false;
    }

    if (!m_devSum) {
        const auto allocErr = cudaMalloc(reinterpret_cast<void**>(&m_devSum), sizeof(double2));
        if (allocErr != cudaSuccess) {
            ERROR << "CarrierRecovery::init failed: unable to allocate reduce output buffer: "
                  << cudaGetErrorString(allocErr) << std::endl;
            return false;
        }
    }

    return true;
}

bool CarrierRecovery::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(data);
    if (!m_inData) {
        ERROR << "CarrierRecovery::setData failed: input must be GpuComplexFloatSignal." << std::endl;
        return false;
    }
    if (!m_inData->isValid()) {
        ERROR << "CarrierRecovery::setData failed: input signal is invalid." << std::endl;
        return false;
    }

    return true;
}

bool CarrierRecovery::run() {
    if (!m_inData) {
        ERROR << "CarrierRecovery::run failed: input data is null." << std::endl;
        return false;
    }

    const size_t n = m_inData->size();

    auto out = std::make_shared<GpuComplexFloatSignal>(std::max<size_t>(size_t(1), n));
    if (!out || !out->isValid()) {
        ERROR << "CarrierRecovery::run failed: output allocation failed." << std::endl;
        return false;
    }
    if (!out->setLogicalSize(n)) {
        ERROR << "CarrierRecovery::run failed: unable to set output logical size." << std::endl;
        return false;
    }

    if (n > 0) {
        size_t requiredStorageBytes = 0;
        const auto queryErr = carrierRecoveryQueryReduceStorageBytes(
            m_inData->getDeviceData(),
            n,
            m_order,
            &requiredStorageBytes
        );
        if (queryErr != cudaSuccess) {
            ERROR << "CarrierRecovery::run failed: unable to query CUB reduce storage: "
                  << cudaGetErrorString(queryErr) << std::endl;
            return false;
        }

        if (requiredStorageBytes > m_reduceTempStorageBytes) {
            if (m_reduceTempStorage) {
                cudaFree(m_reduceTempStorage);
                m_reduceTempStorage = nullptr;
                m_reduceTempStorageBytes = 0;
            }

            const auto allocTempErr = cudaMalloc(reinterpret_cast<void**>(&m_reduceTempStorage), requiredStorageBytes);
            if (allocTempErr != cudaSuccess) {
                ERROR << "CarrierRecovery::run failed: unable to allocate CUB temp storage: "
                      << cudaGetErrorString(allocTempErr) << std::endl;
                return false;
            }
            m_reduceTempStorageBytes = requiredStorageBytes;
        }

        const auto reduceErr = carrierRecoveryRunReduce(
            m_inData->getDeviceData(),
            n,
            m_order,
            m_reduceTempStorage,
            m_reduceTempStorageBytes,
            m_devSum
        );
        if (reduceErr != cudaSuccess) {
            ERROR << "CarrierRecovery::run failed: CUB reduce failed: "
                  << cudaGetErrorString(reduceErr) << std::endl;
            return false;
        }

        double2 hostSum = make_double2(0.0, 0.0);
        const auto copyErr = cudaMemcpy(&hostSum, m_devSum, sizeof(double2), cudaMemcpyDeviceToHost);
        if (copyErr != cudaSuccess) {
            ERROR << "CarrierRecovery::run failed: unable to copy reduce result: "
                  << cudaGetErrorString(copyErr) << std::endl;
            return false;
        }

        float phase = 0.0f;
        if (hostSum.x != 0.0 || hostSum.y != 0.0) {
            phase = std::atan2f(static_cast<float>(hostSum.y), static_cast<float>(hostSum.x)) / static_cast<float>(m_order);

            if (m_order == 4) {
                phase -= static_cast<float>(M_PI_4);
            }

            const float wrapRange = static_cast<float>(M_PI) / static_cast<float>(m_order);
            while (phase >= wrapRange) {
                phase -= 2.0f * wrapRange;
            }
            while (phase < -wrapRange) {
                phase += 2.0f * wrapRange;
            }
        }

        const float c = std::cos(phase);
        const float s = std::sin(phase);

        const int threads = 256;
        const int blocks = static_cast<int>((n + static_cast<size_t>(threads) - 1) / static_cast<size_t>(threads));

        launchCarrierRecoveryRotateKernel(
            m_inData->getDeviceData(),
            out->getDeviceData(),
            n,
            c,
            s,
            blocks,
            threads
        );
        const auto launchErr = cudaGetLastError();
        if (launchErr != cudaSuccess) {
            ERROR << "CarrierRecovery::run failed: kernel launch failed: "
                  << cudaGetErrorString(launchErr) << std::endl;
            return false;
        }

        const auto syncErr = cudaDeviceSynchronize();
        if (syncErr != cudaSuccess) {
            ERROR << "CarrierRecovery::run failed: kernel execution failed: "
                  << cudaGetErrorString(syncErr) << std::endl;
            return false;
        }
    }

    m_outData = out;
    return true;
}

void CarrierRecovery::setParam(const std::string& paramName, const std::any& value) {
    if (paramName == "order") {
        if (value.type() == typeid(int32_t)) {
            m_order = std::any_cast<int32_t>(value);
        } else if (value.type() == typeid(int64_t)) {
            m_order = static_cast<int>(std::any_cast<int64_t>(value));
        } else if (value.type() == typeid(int)) {
            m_order = std::any_cast<int>(value);
        } else {
            ERROR << "CarrierRecovery::setParam failed: invalid order type." << std::endl;
        }
        return;
    }

    ERROR << "CarrierRecovery::setParam unknown parameter: " << paramName << std::endl;
}

std::shared_ptr<IData> CarrierRecovery::getData() {
    return m_outData;
}
