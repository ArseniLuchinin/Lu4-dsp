#ifndef EMPTY_CONTAINER_HPP
#define EMPTY_CONTAINER_HPP

#include <IData.hpp>

class EmptyContainer : public IData {
public:
    inline EmptyContainer() : IData("Empty container") {}

    inline size_t size() const override {
        return 0;
    }

    inline size_t availableSize() const override {
        return 0;
    }

    inline bool reserve(const size_t /*size*/) override {
        return false;
    }

    inline bool isValid() const override {
        return false;
    }

    std::shared_ptr<IData> copy() const override {
        return std::make_shared<EmptyContainer>();
    }
};

#endif // EMPTY_CONTAINER_HPP
