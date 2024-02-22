#pragma once

#include "pch.h"
#include <algorithm> // For std::copy_n
#include <utility> // For std::move and std::swap

template <class T>
class CircularBuffer {
    T* data;
    unsigned read_pos;
    unsigned write_pos;
    unsigned in_use;
    const unsigned capa;

public:
    CircularBuffer(unsigned capa) :
        data(static_cast<T*>(operator new(capa * sizeof(T)))),
        read_pos(0),
        write_pos(0),
        in_use(0),
        capa(capa)
    {}

    CircularBuffer(const CircularBuffer& other) :
        data(static_cast<T*>(operator new(other.capa * sizeof(T)))),
        read_pos(other.read_pos),
        write_pos(other.write_pos),
        in_use(other.in_use),
        capa(other.capa) {
        for (unsigned i = 0; i < other.in_use; ++i) {
            new(&data[(read_pos + i) % capa]) T(other.data[(other.read_pos + i) % other.capa]);
        }
    }

    CircularBuffer& operator=(CircularBuffer other) {
        swap(*this, other);
        return *this;
    }

    CircularBuffer(CircularBuffer&& other) noexcept :
        CircularBuffer(other.capa) {
        swap(*this, other);
    }

    CircularBuffer& operator=(CircularBuffer&& other) noexcept {
        swap(*this, other);
        return *this;
    }

    // Utility function to safely swap two CircularBuffer instances
    friend void swap(CircularBuffer& first, CircularBuffer& second) noexcept {
        using std::swap;
        swap(first.data, second.data);
        swap(first.read_pos, second.read_pos);
        swap(first.write_pos, second.write_pos);
        swap(first.in_use, second.in_use);
        // Note: capa is const and must be the same for swap to be valid, so no swap needed.
    }

    void push(const T& t) {
        if (in_use == capa)
            pop();

        new(&data[write_pos++]) T(t);
        write_pos %= capa;
        ++in_use;
    }

    T front() const {
        return data[read_pos];
    }

    void pop() {
        if (in_use > 0) {
            data[read_pos++].~T();
            read_pos %= capa;
            --in_use;
        }
    }

    unsigned capacity() const {
        return capa;
    }

    unsigned size() const {
        return in_use;
    }

    void clear() {
        while (in_use != 0) {
            pop();
        }
    }

    ~CircularBuffer() {
        clear();
        operator delete(data);
    }
};
