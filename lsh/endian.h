#ifndef __LSH_ENDIAN_H__
#define __LSH_ENDIAN_H__

#if __cplusplus > 201103L
extern "C++" {
#endif

// 1 means little-endian
#define LSH_LITTLE_ENDIAN 1
// 2 means big-endian
#define LSH_BIG_ENDIAN 2

#include <byteswap.h>
#include <stdint.h>
#include <type_traits>

namespace lsh {

    template <class T>
    // std::enable_if 用于在编译时根据条件决定是否启用该函数模板
    // 条件为 sizeof(T) == sizeof(uint64_t)，即类型 T 的大小必须等于 uint64_t 的大小
    // 如果条件满足，std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type 会解析为类型 T
    // 否则，该函数模板不会被实例化，从而避免了不合法的调用
    typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type byteswap(T value) {
        return (T)bswap_64((uint64_t)value);
    }

    template <class T>
    typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type byteswap(T value) {
        return (T)bswap_32((uint32_t)value);
    }

    template <class T>
    typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type byteswap(T value) {
        return (T)bswap_16((uint16_t)value);
    }

#if BYTE_ORDER == __BIG_ENDIAN
#define LSH_BYTE_ORDER LSH_BIG_ENDIAN
#else
#define LSH_BYTE_ORDER LSH_LITTLE_ENDIAN
#endif

/*
 * byteswapOnLittleEndian() 函数用于在小端序机器上将 value 转换为大端序
 * byteswapOnBigEndian() 函数用于在大端序机器上将 value 转换为小端序
 * 所以 byteswapOnLittleEndian() 函数在大端序机器上不会改变 value 的值
 * byteswapOnBigEndian() 函数在小端序机器上不会改变 value 的值
 */
#if LSH_BYTE_ORDER == LSH_BIG_ENDIAN
    template <class T>
    T byteswapOnLittleEndian(T value) {
        return value;
    }

    template <class T>
    T byteswapOnBigEndian(T value) {
        return lsh::byteswap(value);
    }
#else
    template <class T>
    T byteswapOnLittleEndian(T value) {
        return lsh::byteswap(value);
    }

    template <class T>
    T byteswapOnBigEndian(T value) {
        return value;
    }

#endif
} // namespace lsh

#if __cplusplus > 201103L
}
#endif
#endif