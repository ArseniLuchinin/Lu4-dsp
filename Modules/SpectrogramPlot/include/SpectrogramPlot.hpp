#ifndef SPECTROGRAM_PLOT_HPP
#define SPECTROGRAM_PLOT_HPP

#include <IModule.hpp>
#include <GpuFloatSignal.hpp>

#include <any>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class SpectrogramPlot : public IModule {
public:
    SpectrogramPlot();
    ~SpectrogramPlot() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    size_t m_sampleRate = 1;
    size_t m_fftSize = 1024;
    size_t m_windowSize = 0;
    size_t m_freqBins = 0;
    size_t m_hopSize = 0;

    bool m_centeredSpectrum = false;
    double m_freqMin = 0.0;
    double m_freqMax = 0.0;
    bool m_hasFreqRange = false;
    bool m_hasDbRange = false;
    double m_dbMin = 0.0;
    double m_dbMax = 0.0;
    bool m_hasMaskBelowDb = false;
    double m_maskBelowDb = -120.0;

    bool m_isShow = true;
    std::string m_savePath;
    int m_pngCompression = 1;

    std::shared_ptr<IData> m_transitData;
    std::shared_ptr<GpuFloatSignal> m_data;
    std::vector<float> m_hostBuffer;
    std::vector<unsigned char> m_colorLutHost;
    unsigned char* m_gpuColorLut = nullptr;
    unsigned char* m_gpuImageBuffer = nullptr;
    size_t m_gpuImageCapacity = 0;
};

#endif
