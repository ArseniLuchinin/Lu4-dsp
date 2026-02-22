#ifndef IDATA_H
#define IDATA_H

#include <string>

class IData {
public:
    virtual ~IData() = default;
    virtual std::string getDataName() const = 0;
    virtual size_t size() const = 0;
};

#endif // IDATA_H