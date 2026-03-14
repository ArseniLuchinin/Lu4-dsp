#include <SpectrogramPlot.hpp>

#include <GpuFloatSignal.hpp>
#include <module.hpp>

#include <cuda_runtime.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

IModule* createModule() {
    return new SpectrogramPlot();
}

SpectrogramPlot::SpectrogramPlot()
    : IModule({"SpectrogramPlot", "SpectrogramPlot-module.so", "SpectrogramPlot.json"}) {}

SpectrogramPlot::~SpectrogramPlot() = default;

bool SpectrogramPlot::init() {
    return true;
}

int32_t i = 0;
bool SpectrogramPlot::run() {
    if (!m_data || !m_data->isValid()) {
        ERROR << "SpectrogramPlot::run: input data is null or invalid." << std::endl;
        return false;
    }

    if (m_fftSize == 0) {
        ERROR << "SpectrogramPlot::run: fft size is zero." << std::endl;
        return false;
    }

    if (m_windowSize > 0) {
        m_freqBins = m_windowSize;
    } else {
        m_freqBins = (m_fftSize / 2) + 1;
    }
    if (m_freqBins == 0) {
        ERROR << "SpectrogramPlot::run: frequency bins count is zero." << std::endl;
        return false;
    }

    const size_t totalSize = m_data->size();
    if (totalSize < m_freqBins) {
        ERROR << "SpectrogramPlot::run: input size (" << totalSize
              << ") is smaller than one spectrogram row (" << m_freqBins << ")." << std::endl;
        return false;
    }

    const size_t rows = totalSize / m_freqBins;
    const size_t usedSize = rows * m_freqBins;
    if (rows == 0) {
        ERROR << "SpectrogramPlot::run: no rows to render." << std::endl;
        return false;
    }

    if (usedSize != totalSize) {
        INFO << "SpectrogramPlot::run: input size is not divisible by row size. Ignoring tail of "
             << (totalSize - usedSize) << " elements." << std::endl;
    }

    std::vector<float> hostBuffer(usedSize);
    const auto copyErr = cudaMemcpy(
        hostBuffer.data(),
        m_data->getDeviceData(),
        usedSize * sizeof(float),
        cudaMemcpyDeviceToHost);
    if (copyErr != cudaSuccess) {
        ERROR << "SpectrogramPlot::run: cudaMemcpy failed: " << cudaGetErrorString(copyErr) << std::endl;
        return false;
    }

    const auto [minIt, maxIt] = std::minmax_element(hostBuffer.begin(), hostBuffer.end());
    const float minValue = *minIt;
    const float maxValue = *maxIt;
    const float range = std::max(maxValue - minValue, 1.0e-12f);

    cv::Mat image(static_cast<int>(rows), static_cast<int>(m_freqBins), CV_8UC3);

    for (size_t y = 0; y < rows; ++y) {
        const size_t dstY = rows - 1 - y;
        for (size_t x = 0; x < m_freqBins; ++x) {
            const float raw = hostBuffer[(y * m_freqBins) + x];
            const float normalized = std::clamp((raw - minValue) / range, 0.0f, 1.0f);

            const unsigned char r = static_cast<unsigned char>(std::round(255.0f * normalized));
            const unsigned char g = static_cast<unsigned char>(std::round(255.0f * normalized * normalized));
            const unsigned char b = static_cast<unsigned char>(std::round(255.0f * (1.0f - normalized)));

            image.at<cv::Vec3b>(static_cast<int>(dstY), static_cast<int>(x)) = cv::Vec3b(b, g, r);
        }
    }

    const std::string outPath = m_savePath.empty() ? "spectrogram" + std::to_string(i) + ".png" : m_savePath + std::to_string(i) + ".png";
    if (!cv::imwrite(outPath, image)) {
        ERROR << "SpectrogramPlot::run: failed to save image to " << outPath << std::endl;
        return false;
    }
    INFO << "SpectrogramPlot::run: saved spectrogram to " << outPath << std::endl;

    if (m_isShow) {
        INFO << "SpectrogramPlot::run: show=true is set, but interactive preview is not implemented." << std::endl;
    }

    ++i;
    return true;
}

void SpectrogramPlot::setParam(const std::string& paramName, const std::any& value) {
    if (paramName == "sample rate") {
        m_sampleRate = static_cast<size_t>(std::any_cast<int32_t>(value));
        return;
    }

    if (paramName == "fft size") {
        m_fftSize = static_cast<size_t>(std::any_cast<int32_t>(value));
        return;
    }

    if (paramName == "window size") {
        m_windowSize = static_cast<size_t>(std::any_cast<int32_t>(value));
        return;
    }

    if (paramName == "show") {
        m_isShow = std::any_cast<bool>(value);
        return;
    }

    if (paramName == "save path") {
        m_savePath = std::any_cast<std::string>(value);
        return;
    }
}

bool SpectrogramPlot::setData(std::shared_ptr<IData> data) {
    auto gpuData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    if (!gpuData) {
        ERROR << "SpectrogramPlot::run: input data is not a valid GpuFloatSignal." << std::endl;
        return false;
    }

    m_data = gpuData;

    m_transitData = data;
    return true;
}

std::shared_ptr<IData> SpectrogramPlot::getData() {
    return m_transitData;
}
