#include <algorithm>
#include <cstddef>
#include <vector>

#ifdef GAME_BASE_DLL
#include "../../../src/util/zpp_bits.h"
#endif

template <size_t CAP, typename T>
class Arena 
{
#ifdef GAME_BASE_DLL
    friend zpp::bits::access;
	using serialize = zpp::bits::members<2>;
#endif

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

    size_t count() const {return _firstAvailableIdx;}
    size_t capacity() {return CAP;}

    void clear() {
        _firstAvailableIdx = 0;
    }

    bool has(const T& obj) {
        bool found = false;
        for (int i = 0; i < count(); ++i) {
            if (_data[i].row == obj.row && _data[i].col == obj.col) {
                found = true;
                break;
            }
        }
        return found;
    }

};