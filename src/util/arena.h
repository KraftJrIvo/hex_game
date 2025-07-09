#include <cstddef>
#include <vector>
#include "zpp_bits.h"

template <size_t CAP, typename T>
class Arena 
{
    friend zpp::bits::access;
	using serialize = zpp::bits::members<2>;

    std::vector<T> _data;
    size_t _firstAvailableIdx;

public:
    
    Arena() :
        _firstAvailableIdx(0)
    {
        _data.resize(CAP);
    }

    T* data() {
        return _data.data();
    }

    size_t size() {
        return CAP * sizeof(T);
    }

    size_t acquire(const T& obj, size_t count = 1) {
        _data[_firstAvailableIdx++] = obj;
        return _firstAvailableIdx;
    }

    T& at(size_t idx) {
        return _data[idx];
    }    

    const T& get(size_t idx) const {
        return _data[idx];
    }

    size_t count() {return _firstAvailableIdx;}
    size_t capacity() {return CAP;}

    void clear() {
        _firstAvailableIdx = 0;
    }

};