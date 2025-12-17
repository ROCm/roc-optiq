#pragma once

#include <type_traits>
#include <span>
#include <optional>
#include <stdint.h>

#ifdef _DEBUG
#include <cstdio>
#include <unordered_map>
#define LOG(FMT, ...) std::fprintf(stderr, FMT, __VA_ARGS__)
#define HIGHLIGHT(FMT, ...) std::fprintf(stderr, "\x1B[93m" FMT "\x1B[0m", __VA_ARGS__)
#define LOGERROR(FMT, ...) std::fprintf(stderr, "\x1B[31m" FMT "\x1B[0m", __VA_ARGS__)
#ifdef _WIN32
#define BREAK_IF(...) if (__VA_ARGS__) __debugbreak()
#endif
#else
#define LOG(FMT, ...)
#define HIGHLIGHT(FMT, ...)
#define LOGERROR(FMT, ...)
#endif

namespace glimmer
{
    template <typename T>
    T clamp(T val, T min, T max)
    {
        return val > max ? max : val < min ? min : val;
    }

    template <typename ItrT, typename T>
    void Fill(ItrT start, ItrT end, T&& el)
    {
        while (start != end) { *start = el; ++start; }
    }

    template <typename T, typename ItrT>
    void Fill(ItrT start, ItrT end)
    {
        while (start != end) { ::new (&(*start)) T{}; ++start; }
    }

#ifdef _DEBUG
    inline int32_t TotalMallocs = 0;
    inline int32_t TotalReallocs = 0;
    inline int32_t AllocatedBytes = 0;
    inline std::unordered_map<void*, size_t> Allocations;

    inline void* AllocateImpl(size_t amount)
    {
        TotalMallocs++;
        AllocatedBytes += amount;
        auto ptr = std::malloc(amount);
        if (Allocations.find(ptr) != Allocations.end())
            LOGERROR("Possibly overwriting memory @ %p\n", ptr);
        Allocations[ptr] = amount;
        return ptr;
    }

    inline void* ReallocateImpl(void* ptr, size_t amount)
    {
        auto result = std::realloc(ptr, amount);
        auto it = Allocations.find(ptr);
        if (it == Allocations.end()) {
            AllocatedBytes += amount;
            TotalMallocs++;
        }
        else {
            AllocatedBytes += amount - it->second;
            TotalReallocs++;
            Allocations.erase(ptr);
        }
        Allocations.emplace(result, amount);
        return result;
    }

    inline void DeallocateImpl(void* ptr)
    {
        if (ptr != nullptr)
        {
            --TotalMallocs;
            std::free(ptr);
            AllocatedBytes -= Allocations.at(ptr);
            Allocations.erase(ptr);
        }
        else LOGERROR("Unchecked de-allocation of nullptr...\n");
    }

    inline void* (*AllocateFunc)(size_t amount) = &AllocateImpl;
    inline void* (*ReallocateFunc)(void* ptr, size_t amount) = &ReallocateImpl;
    inline void (*DeallocateFunc)(void* ptr) = &DeallocateImpl;
#else
    inline void* (*AllocateFunc)(size_t amount) = &std::malloc;
    inline void* (*ReallocateFunc)(void* ptr, size_t amount) = &std::realloc;
    inline void (*DeallocateFunc)(void* ptr) = &std::free;
#endif

    template <typename T, typename Sz, Sz blocksz = 128>
    struct Vector
    {
        template <typename Ty, typename S, S v> friend struct DynamicStack;

        static_assert(blocksz > 0, "Block size has to non-zero");
        static_assert(std::is_integral_v<Sz>, "Sz must be integral type");
        static_assert(!std::is_same_v<T, bool>, "T must not be bool, consider using std::bitset");

        using value_type = T;
        using size_type = Sz;
        using difference_type = std::conditional_t<std::is_unsigned_v<Sz>, std::make_signed_t<Sz>, Sz>;
        using Iterator = T*;

        /*struct Iterator
        {
            Sz current = 0;
            Vector& parent;

            using DiffT = std::make_signed_t<Sz>;

            Iterator(Vector& p, Sz curr) : current{ curr }, parent{ p } {}
            Iterator(const Iterator& copy) : current{ copy.current }, parent{ copy.parent } {}

            Iterator& operator++() { current++; return *this; }
            Iterator& operator+=(Sz idx) { current += idx; return *this; }
            Iterator operator++(int) { auto temp = *this; current++; return temp; }
            Iterator operator+(Sz idx) { auto temp = *this; temp.current += idx; return temp; }

            Iterator& operator--() { current--; return *this; }
            Iterator& operator-=(Sz idx) { current -= idx; return *this; }
            Iterator operator--(int) { auto temp = *this; current--; return temp; }
            Iterator operator-(Sz idx) { auto temp = *this; temp.current -= idx; return temp; }

            difference_type operator-(const Iterator& other) const
            { return (difference_type)current - (difference_type)other.current; }

            bool operator!=(const Iterator& other) const
            { return current != other.current || parent._data != other.parent._data; }

            bool operator<(const Iterator& other) const { current < other.current; }

            T& operator*() { return parent._data[current]; }
            T* operator->() { return parent._data + current; }
        };*/

        ~Vector()
        {
            if constexpr (std::is_destructible_v<T> && std::is_scalar_v<T>)
                for (auto idx = 0; idx < _size; ++idx) _data[idx].~T();
            DeallocateFunc(_data);
        }

        explicit Vector(bool init = true)
        {
            if (init)
            {
                _capacity = blocksz;
                _data = (T*)AllocateFunc(sizeof(T) * blocksz);
                _default_init(0, _capacity);
            }
        }

        template <typename IntegralT,
            typename = std::enable_if<!std::is_same_v<IntegralT, bool>, void>::type>
        explicit Vector(IntegralT initialsz)
            : _capacity{ (Sz)initialsz }, _data{ (T*)AllocateFunc(sizeof(T) * (Sz)initialsz) }
        {
            _default_init(0, _capacity);
        }

        explicit Vector(Sz initialsz, const T& el)
            : _capacity{ initialsz }, _size{ initialsz }, _data{ (T*)AllocateFunc(sizeof(T) * initialsz) }
        {
            Fill(_data, _data + _capacity, el);
        }

        void assign(Iterator from, Iterator to)
        {
            auto count = to - from;
            if (_data == nullptr)
            {
                _data = (T*)AllocateFunc(sizeof(T) * count);
            }
            else if (_capacity < count)
            {
                auto ptr = (T*)ReallocateFunc(_data, sizeof(T) * count);
                assert(ptr != nullptr);
                _data = ptr;
            }

            while (from != to)
            {
                emplace_back(*from);
                ++from;
            }

            _size = _capacity = count;
        }

        void resize(Sz count, bool initialize = true)
        {
            if (_data == nullptr)
            {
                _data = (T*)AllocateFunc(sizeof(T) * count);
            }
            else if (_capacity < count)
            {
                auto ptr = (T*)ReallocateFunc(_data, sizeof(T) * count);
                assert(ptr != nullptr);
                _data = ptr;
            }

            if (initialize) _default_init(_size, count);
            _size = _capacity = count;
        }

        void resize(Sz count, const T& el)
        {
            if (_data == nullptr)
            {
                _data = (T*)AllocateFunc(sizeof(T) * count);
            }
            else if (_capacity < count)
            {
                auto ptr = (T*)ReallocateFunc(_data, sizeof(T) * count);
                assert(ptr != nullptr);
                _data = ptr;
            }

            Fill(_data + _size, _data + count, el);
            _size = _capacity = count;
        }

        void fill(const T& el)
        {
            Fill(_data + _size, _data + _capacity, el);
            _size = _capacity;
        }

        void expand(Sz count, bool initialize = true)
        {
            auto targetsz = _size + std::max(count, blocksz);

            if (_capacity < targetsz)
            {
                auto ptr = (T*)ReallocateFunc(_data, sizeof(T) * targetsz);
                assert(ptr != nullptr);
                _data = ptr;
                _capacity = targetsz;
            }

            if (initialize) _default_init(_size, targetsz);
        }

        void expand_and_create(Sz count, bool initialize = true)
        {
            auto targetsz = _size + std::max(count, blocksz);

            if (_capacity < targetsz)
            {
                auto ptr = (T*)ReallocateFunc(_data, sizeof(T) * targetsz);
                assert(ptr != nullptr);
                _data = ptr;
                _capacity = targetsz;
            }

            _size = targetsz;
            if (initialize) _default_init(_size, targetsz);
        }

        template <typename... ArgsT>
        T& emplace_back(ArgsT&&... args)
        {
            _reallocate(true);
            ::new(_data + _size) T{ std::forward<ArgsT>(args)... };
            _size++;
            return _data[_size - 1];
        }

        T& next(bool init)
        {
            _reallocate(init);
            _size++;
            return _data[_size - 1];
        }

        void push_back(const T& el) { _reallocate(true); _data[_size] = el; _size++; }
        void pop_back(bool definit) { if constexpr (std::is_default_constructible_v<T>) if (definit) _data[_size - 1] = T{}; --_size; }
        void clear(bool definit) { if (definit) _default_init(0, _size); _size = 0; }
        void reset(const T& el) { Fill(_data, _data + _size, el); }
        void shrink_to_fit() { _data = (T*)ReallocateFunc(_data, _size * sizeof(T)); _capacity = _size; }

        Iterator begin() { return _data; }
        Iterator end() { return _data + _size; }
        Iterator begin() const { return _data; }
        Iterator end() const { return _data + _size; }

        const T& operator[](Sz idx) const { assert(idx < _size); return _data[idx]; }
        T& operator[](Sz idx) { assert(idx < _size); return _data[idx]; }

        T& front() { assert(_size > 0); return _data[0]; }
        T const& front() const { assert(_size > 0); return _data[0]; }
        T& back() { assert(_size > 0); return _data[_size - 1]; }
        T const& back() const { assert(_size > 0); return _data[_size - 1]; }
        T* data() { return _data; }

        Sz size() const { return _size; }
        Sz capacity() const { return _capacity; }
        bool empty() const { return _size == 0; }
        std::span<T> span() const { return std::span<T>{ _data, _size }; }

    private:

        void _default_init(Sz from, Sz to)
        {
            if constexpr (std::is_default_constructible_v<T> && !std::is_scalar_v<T>)
                while (from != to)
                {
                    new (_data + from) T{};
                    ++from;
                }
            else if constexpr (std::is_arithmetic_v<T>)
                Fill(_data + from, _data + to, T{ 0 });
        }

        void _reallocate(bool initialize)
        {
            T* ptr = nullptr;
            if (_size == _capacity) 
                ptr = (T*)ReallocateFunc(_data, (_capacity + blocksz) * sizeof(T));

            if (ptr != nullptr)
            {
                _data = ptr;
                if (initialize) _default_init(_capacity, _capacity + blocksz);
                _capacity += blocksz;
            }
        }

        T* _data = nullptr;
        Sz _size = 0;
        Sz _capacity = 0;
    };

    template <typename T, int16_t capacity>
    struct StackStorage
    {
        T storage[capacity];
        T* _data = nullptr;

        StackStorage() { _data = storage; }
    };

    template <typename T, int16_t capacity>
    struct HeapStorage
    {
        T* _data = nullptr;

        HeapStorage() { _data = (T*)AllocateFunc(sizeof(T) * capacity); }
        ~HeapStorage() { DeallocateFunc(_data); }
    };

    template <typename T, int16_t capacity>
    struct FixedSizeStack
    {
        std::conditional_t<(sizeof(T)* capacity) <= (1 << 19),
            StackStorage<T, capacity>, HeapStorage<T, capacity>> _storage;
        int16_t _size = 0;
        int16_t _max = 0;

        static_assert(capacity > 0, "capacity has to be a +ve value");
        static_assert(std::is_default_constructible_v<T>, "Element type must be default constructible");

        FixedSizeStack(bool init = true)
        {
            if (init)
            {
                if constexpr (std::is_default_constructible_v<T>)
                    Fill<T>(_storage._data, _storage._data + capacity);
            }
        }

        ~FixedSizeStack()
        {
            if constexpr (std::is_destructible_v<T>) 
                for (int16_t idx = 0; idx < _size; ++idx) _storage._data[idx].~T();
        }

        T& push()
        {
            assert(_size < capacity);
            ++_size;
            _max = std::max(_max, _size);
            return _storage._data[_size - 1];
        }

        void pop(int16_t depth, bool definit)
        {
            while (depth > 0 && _size > 0)
            {
                --_size;
                if constexpr (std::is_default_constructible_v<T>)
                    if (definit)
                        ::new(_storage._data + _size) T{};
                --depth;
            }
        }

        void clear(bool definit) { pop(_size, definit); }

        int size() const { return _size; }
        bool empty() const { return _size == 0; }
        T* begin() const { return _storage._data; }
        T* end() const { return _storage._data + _size; }

        T& operator[](int16_t idx) { return _storage._data[idx]; }
        T const& operator[](int16_t idx) const { return _storage._data[idx]; }

        T& top(int16_t depth = 0) { return _storage._data[_size - (int16_t)1 - depth]; }
        T const& top(int16_t depth = 0) const { return _storage._data[_size - (int16_t)1 - depth]; }

        T& next(int16_t amount) { return _storage._data[_size - (int16_t)1 - amount]; }
        T const& next(int16_t amount) const { return _storage._data[_size - (int16_t)1 - amount]; }
    };

    template <typename T, typename Sz, Sz blocksz = 128>
    struct DynamicStack
    {
        using IteratorT = typename Vector<T, Sz, blocksz>::Iterator;
        static_assert(std::is_default_constructible_v<T>, "Element type must be default constructible");

        DynamicStack(Sz capacity, const T& el)
            : _data{ capacity, el }
        {
        }

        template <typename Ty>
        DynamicStack(Ty param)
            : _data{ param }
        {
        }

        DynamicStack()
            : _data{ blocksz }
        {
        }

        T& push()
        {
            auto nextidx = _data.size() + (Sz)1;
            if (nextidx <= _max) _data._size++;
            T& el = nextidx <= _max ? _data[nextidx - (Sz)1] : _data.emplace_back();
            _max = std::max(_max, _data.size());
            return el;
        }

        void pop(int depth, bool definit)
        {
            while (depth > 0 && _data._size > 0)
            {
                --_data._size;
                if constexpr (std::is_default_constructible_v<T>)
                    if (definit) ::new(_data._data + _data._size) T{};
                --depth;
            }
        }

        void clear(bool definit) { pop(_data._size, definit); }

        Sz size() const { return _data.size(); }
        bool empty() const { return _data.size() == 0; }
        IteratorT begin() { return _data.begin(); }
        IteratorT end() { return _data.end(); }

        T& operator[](int idx) { return _data[idx]; }
        T const& operator[](int idx) const { return _data[idx]; }

        T& top() { return _data.back(); }
        T const& top() const { return _data.back(); }

        T& next(int amount) { return _data[_data.size() - 1 - amount]; }
        T const& next(int amount) const { return _data[_data.size() - 1 - amount]; }

    private:

        Vector<T, Sz, blocksz> _data;
        Sz _max = 0;
    };

    template <typename T>
    struct UndoRedoStack
    {
        Vector<T, int16_t> stack{ 16 };
        int16_t pos = 0;
        int16_t total = 0;

        template <typename... ArgsT>
        T& push(ArgsT&&... args)
        {
            if (pos == stack.size())
            {
                pos++;
                total = pos;
                return stack.emplace_back(std::forward<ArgsT>(args)...);
            }
            else
            {
                for (auto idx = pos; idx < stack.size(); ++idx)
                    stack[idx].~T();
                ::new (&(stack[pos])) T{ std::forward<ArgsT>(args)... };
                pos++;
                total = pos;
                return stack[pos - 1];
            }
        }

        T& top() { return stack.back(); }

        std::optional<T> undo()
        {
            if (pos == 0) return std::nullopt;
            else
            {
                pos--; total--;
                return stack[pos];
            }
        }

        std::optional<T> redo()
        {
            if (pos == total) return std::nullopt;
            else
            {
                pos++;
                return stack[pos];
            }
        }

        bool empty() const { return total == 0; }
    };

    template <typename T>
    struct Span
    {
        T* source = nullptr;
        int sz = 0;

        template <typename ItrT>
        Span(ItrT start, ItrT end)
            : source{ &(*start) }, sz{ (int)(end - start) }
        {
        }

        template <size_t size>
        Span(T(&array)[size])
            : source{ array }, sz{ size }
        {
        }

        template <typename SzT, SzT v>
        Span(Vector<T, SzT, v>& vec)
            : source{ vec.data() }, sz{ vec.size() }
        {
        }

        Span(T* from, int size) : source{ from }, sz{ size } {}

        Span() {}

        T* begin() { return source; }
        T* end() { return source + sz; }

        T& front() { return *source; }
        T& back() { return *(source + sz - 1); }

        T& operator[](int idx) { return source[idx]; }

        int size() const { return sz; }
        T* data() { return source; }
    };

    template <typename ItrT>
    Span(ItrT start, ItrT end) -> Span<std::decay_t<decltype(*start)>>;
}
