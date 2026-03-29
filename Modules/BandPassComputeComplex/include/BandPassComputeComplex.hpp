#ifndef BAND_PASS_COMPUTE_COMPLEX_HPP
#define BAND_PASS_COMPUTE_COMPLEX_HPP

#include <IModule.hpp>
#include <IData.hpp>

#include <string>

class BandPassComputeComplex : public IModule {
public:
    BandPassComputeComplex();
    ~BandPassComputeComplex() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;
    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    enum class Sideband {
        Negative,
        Positive,
        Both
    };

    bool parseSideband(const std::string& value);

    double m_sampleRate = 0.0;
    int m_filterOrder = 0;
    double m_lowCutoff = 0.0;
    double m_highCutoff = 0.0;
    Sideband m_sideband = Sideband::Negative;
    bool m_sidebandExplicit = false;

    std::shared_ptr<IData> m_data;
    bool m_isComputed = false;
};

#endif // BAND_PASS_COMPUTE_COMPLEX_HPP
