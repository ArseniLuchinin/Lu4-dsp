#ifndef BAND_PASS_COMPUTE_HPP
#define BAND_PASS_COMPUTE_HPP

#include <IModule.hpp>
#include <VirtualRX.hpp>
#include <CpuFloatSignal.hpp>

class BandPassCompute : public IModule, public VirtualRX {
    public:
    BandPassCompute();
    ~BandPassCompute();
    
    bool init() override;

    /// @brief Расчёт полосового фильтра
    bool run() override;

    /*!
     * @brief Установка параметров
     * @param "sample rate" - частота дискретизации
     * @param "filter order" - порядок фильтра
     * @param "block size" - размер блока
     * @param "low cutoff" - нижняя граница частоты
     * @param "high cutoff" - верхняя граница частоты
    */
    void setParam(const std::string& paramName, const std::any& value) override;
    bool setData(std::shared_ptr<IData> data) override;

    /// @brief Возвращает данные
    /// @return CpuFloatSignal
    std::shared_ptr<IData> getData() override;

private:
    float m_sampleRate;
    int   m_filterOrder;
    int   m_blockSize;
    float m_lowCutoff;
    float m_highCutoff;
    std::shared_ptr<CpuFloatSignal> m_data;
};

#endif // BAND_PASS_COMPUTE_HPP