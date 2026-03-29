#include <SpectrogramPlot.hpp>
#include <VariablesResolve.hpp>

#include <GpuFloatSignal.hpp>
#include <module.hpp>

#include <cuda_runtime.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace {

bool resolveFreqBins(size_t fftSize,
                     size_t windowSize,
                     size_t totalSize,
                     size_t* freqBins,
                     bool* isRealSpectrum)
{
    if (freqBins == nullptr || isRealSpectrum == nullptr) {
        return false;
    }

    const size_t realBins = (fftSize / 2) + 1;
    const size_t complexBins = fftSize;
    if (realBins == 0 || complexBins == 0) {
        return false;
    }

    if (windowSize > 0) {
        if (windowSize != realBins && windowSize != complexBins) {
            return false;
        }
        *freqBins = windowSize;
        *isRealSpectrum = (windowSize == realBins);
        return true;
    }

    const bool divisibleReal = (totalSize >= realBins) && ((totalSize % realBins) == 0);
    const bool divisibleComplex = (totalSize >= complexBins) && ((totalSize % complexBins) == 0);

    if (divisibleReal) {
        *freqBins = realBins;
        *isRealSpectrum = true;
        return true;
    }
    if (divisibleComplex) {
        *freqBins = complexBins;
        *isRealSpectrum = false;
        return true;
    }

    return false;
}

const std::array<cv::Vec3b, 256>& plasmaLut()
{
    static const std::array<cv::Vec3b, 256> lut = [] {
        std::array<cv::Vec3b, 256> values{};
        cv::Mat gray(1, 256, CV_8UC1);
        for (int i = 0; i < 256; ++i) {
            gray.at<unsigned char>(0, i) = static_cast<unsigned char>(i);
        }

        cv::Mat colored;
        cv::applyColorMap(gray, colored, cv::COLORMAP_PLASMA);
        for (int i = 0; i < 256; ++i) {
            values[static_cast<size_t>(i)] = colored.at<cv::Vec3b>(0, i);
        }
        return values;
    }();
    return lut;
}

cv::Vec3b colorizeLevel(float normalized)
{
    const float t = std::clamp(normalized, 0.0f, 1.0f);
    const float gammaCorrected = std::pow(t, 0.8f);
    const int idx = static_cast<int>(std::round(gammaCorrected * 255.0f));
    return plasmaLut()[static_cast<size_t>(std::clamp(idx, 0, 255))];
}

cv::Vec3b maskedBackground(float normalized)
{
    const float t = std::clamp(normalized, 0.0f, 1.0f);
    const float base = std::pow(t, 1.6f);
    const unsigned char b = static_cast<unsigned char>(std::round(18.0f + 24.0f * base));
    const unsigned char g = static_cast<unsigned char>(std::round(6.0f + 10.0f * base));
    const unsigned char r = static_cast<unsigned char>(std::round(16.0f + 28.0f * base));
    return cv::Vec3b(b, g, r);
}

} // namespace

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

    const size_t totalSize = m_data->size();
    bool isRealSpectrum = false;
    if (!resolveFreqBins(m_fftSize, m_windowSize, totalSize, &m_freqBins, &isRealSpectrum)) {
        ERROR << "SpectrogramPlot::run: failed to determine valid spectrogram width. "
              << "Expected window size " << ((m_fftSize / 2) + 1)
              << " (real) or " << m_fftSize << " (complex)." << std::endl;
        return false;
    }

    if (totalSize < m_freqBins) {
        ERROR << "SpectrogramPlot::run: input size (" << totalSize
              << ") is smaller than one spectrogram row (" << m_freqBins << ")." << std::endl;
        return false;
    }

    if ((totalSize % m_freqBins) != 0) {
        ERROR << "SpectrogramPlot::run: input size (" << totalSize
              << ") is not divisible by spectrogram row size (" << m_freqBins << ")." << std::endl;
        return false;
    }

    if (m_centeredSpectrum && isRealSpectrum) {
        ERROR << "SpectrogramPlot::run: centered spectrum is supported only for full complex spectrum."
              << std::endl;
        return false;
    }

    if (m_hasDbRange && !(m_dbMax > m_dbMin)) {
        ERROR << "SpectrogramPlot::run: invalid dB range, 'db max' must be greater than 'db min'."
              << std::endl;
        return false;
    }

    const size_t rows = totalSize / m_freqBins;
    if (rows == 0) {
        ERROR << "SpectrogramPlot::run: no rows to render." << std::endl;
        return false;
    }

    m_hostBuffer.resize(totalSize);
    const auto copyErr = cudaMemcpy(
        m_hostBuffer.data(),
        m_data->getDeviceData(),
        totalSize * sizeof(float),
        cudaMemcpyDeviceToHost);
    if (copyErr != cudaSuccess) {
        ERROR << "SpectrogramPlot::run: cudaMemcpy failed: " << cudaGetErrorString(copyErr) << std::endl;
        return false;
    }

    float minValue = 0.0f;
    float maxValue = 0.0f;
    if (m_hasDbRange) {
        minValue = static_cast<float>(m_dbMin);
        maxValue = static_cast<float>(m_dbMax);
    } else {
        const auto [minIt, maxIt] = std::minmax_element(m_hostBuffer.begin(), m_hostBuffer.end());
        minValue = *minIt;
        maxValue = *maxIt;
    }
    const float range = std::max(maxValue - minValue, 1.0e-12f);
    DEBUG << "SpectrogramPlot::run: dB range in frame min=" << minValue
          << ", max=" << maxValue
          << (m_hasMaskBelowDb ? (", mask below=" + std::to_string(m_maskBelowDb)) : "")
          << std::endl;

    cv::Mat image(static_cast<int>(rows), static_cast<int>(m_freqBins), CV_8UC3);

    for (size_t y = 0; y < rows; ++y) {
        const size_t dstY = rows - 1 - y;
        auto* rowPtr = image.ptr<cv::Vec3b>(static_cast<int>(dstY));
        for (size_t x = 0; x < m_freqBins; ++x) {
            const float raw = m_hostBuffer[(y * m_freqBins) + x];
            const float normalized = std::clamp((raw - minValue) / range, 0.0f, 1.0f);
            if (m_hasMaskBelowDb && raw < static_cast<float>(m_maskBelowDb)) {
                rowPtr[x] = maskedBackground(normalized);
                continue;
            }
            rowPtr[x] = colorizeLevel(normalized);
        }
    }

    const int topMargin = 30;
    const int leftMargin = 60;
    const int rightMargin = 10;
    const int bottomMargin = 10;
    const int canvasWidth = leftMargin + static_cast<int>(m_freqBins) + rightMargin;
    const int canvasHeight = topMargin + static_cast<int>(rows) + bottomMargin;

    cv::Mat canvas(canvasHeight, canvasWidth, CV_8UC3, cv::Scalar(250, 247, 242));
    image.copyTo(canvas(cv::Rect(leftMargin, topMargin, image.cols, image.rows)));

    const int font = cv::FONT_HERSHEY_SIMPLEX;
    const double fontScale = 0.4;
    const int thickness = 1;
    const cv::Scalar color(32, 24, 18);

    double freqMin = 0.0;
    double freqMax = 0.0;
    if (m_hasFreqRange) {
        freqMin = m_freqMin;
        freqMax = m_freqMax;
    } else if (isRealSpectrum) {
        freqMin = 0.0;
        freqMax = m_sampleRate / 2.0;
    } else if (m_centeredSpectrum) {
        freqMin = -static_cast<double>(m_sampleRate) / 2.0;
        freqMax = static_cast<double>(m_sampleRate) / 2.0;
    } else {
        freqMin = 0.0;
        freqMax = static_cast<double>(m_sampleRate);
    }
    const int freqTicks = 5;
    for (int t = 0; t <= freqTicks; ++t) {
        const double frac = static_cast<double>(t) / freqTicks;
        const int x = leftMargin + static_cast<int>(std::round(frac * (m_freqBins - 1)));
        const double freq = freqMin + (freqMax - freqMin) * frac;
        const std::string label = std::to_string(static_cast<int>(std::round(freq)));
        cv::putText(canvas, label, cv::Point(x - 10, topMargin - 8), font, fontScale, color, thickness);
        cv::line(canvas, cv::Point(x, topMargin - 5), cv::Point(x, topMargin), color, 1);
    }

    const int timeTicks = 5;
    const size_t hopSize = (m_hopSize > 0) ? m_hopSize : m_fftSize;
    const double secondsPerRow = (m_sampleRate > 0) ? (static_cast<double>(hopSize) / m_sampleRate) : 0.0;
    for (int t = 0; t <= timeTicks; ++t) {
        const double frac = static_cast<double>(t) / timeTicks;
        const int y = topMargin + static_cast<int>(std::round(frac * (rows - 1)));
        const double sec = frac * (rows - 1) * secondsPerRow;
        const std::string label = std::to_string(sec).substr(0, 5);
        cv::putText(canvas, label, cv::Point(5, y + 4), font, fontScale, color, thickness);
        cv::line(canvas, cv::Point(leftMargin - 5, y), cv::Point(leftMargin, y), color, 1);
    }

    const std::string outPath = m_savePath.empty() ? "spectrogram" + std::to_string(i) + ".png" : m_savePath + std::to_string(i) + ".png";
    std::vector<int> writeParams = {
        cv::IMWRITE_PNG_COMPRESSION, std::clamp(m_pngCompression, 0, 9),
        cv::IMWRITE_PNG_STRATEGY, cv::IMWRITE_PNG_STRATEGY_RLE
    };
    if (!cv::imwrite(outPath, canvas, writeParams)) {
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
    const std::any resolved = resolveParamValue(value);
    if (paramName == "sample rate") {
        m_sampleRate = static_cast<size_t>(std::any_cast<int32_t>(resolved));
        return;
    }

    if (paramName == "fft size") {
        m_fftSize = static_cast<size_t>(std::any_cast<int32_t>(resolved));
        return;
    }

    if (paramName == "window size") {
        m_windowSize = static_cast<size_t>(std::any_cast<int32_t>(resolved));
        return;
    }

    if (paramName == "hop size") {
        m_hopSize = static_cast<size_t>(std::any_cast<int32_t>(resolved));
        return;
    }

    if (paramName == "centered spectrum") {
        m_centeredSpectrum = std::any_cast<bool>(resolved);
        return;
    }

    if (paramName == "freq min") {
        m_freqMin = std::any_cast<double>(resolved);
        m_hasFreqRange = true;
        return;
    }

    if (paramName == "freq max") {
        m_freqMax = std::any_cast<double>(resolved);
        m_hasFreqRange = true;
        return;
    }

    if (paramName == "db min") {
        m_dbMin = std::any_cast<double>(resolved);
        m_hasDbRange = true;
        return;
    }

    if (paramName == "db max") {
        m_dbMax = std::any_cast<double>(resolved);
        m_hasDbRange = true;
        return;
    }

    if (paramName == "mask below db") {
        m_maskBelowDb = std::any_cast<double>(resolved);
        m_hasMaskBelowDb = true;
        return;
    }

    if (paramName == "show") {
        m_isShow = std::any_cast<bool>(resolved);
        return;
    }

    if (paramName == "save path") {
        m_savePath = std::any_cast<std::string>(resolved);
        return;
    }

    if (paramName == "png compression") {
        m_pngCompression = std::any_cast<int32_t>(resolved);
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
