#pragma once

#include <cstddef>
#include <string>

template <typename Key, typename Value>
class ICache {
public:
    virtual ~ICache() = default;

    virtual bool get(const Key& key, Value& out) = 0;
    virtual void put(const Key& key, const Value& value) = 0;

    virtual size_t size() const = 0;
    virtual size_t getHitCount() const = 0;
    virtual size_t capacity() const = 0;

    virtual const char* name() const = 0;
    virtual void printState() const {}

    virtual void resetStats() {}
    virtual void clear() {}
};