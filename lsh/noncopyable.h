#ifndef __LSH_NONCOPYABLE_H__
#define __LSH_NONCOPYABLE_H__

namespace lsh {
    class Noncopyable {
    public:
        Noncopyable() = default;
        Noncopyable(const Noncopyable &) = delete;
        Noncopyable &operator=(const Noncopyable &) = delete;
    };
}

#endif // __LSH_NONCOPYABLE_H__