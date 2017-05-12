#include <assert.h>
#include <vector>
using std::vector;

/* @class RingBuffer
 * 
 * RingBuffer implements a circular buffer based on vector.
 * Each RingBuffer object could store no more than "capacity" elements.
 * New elements are pushed to the end of the buffer. And The buffer also
 * can pop elements from the head.
 *
 * Note that the head of RingBuffer can start at any place of the underlying
 * vector. Operator [] is overloaded to get the i-th element from the head.
 *
 * Example code:
 * @code
 * RingBuffer<int> buf(16);
 * for (int i=0; i<20; ++i)
 *     buf.push_back(i);
 *
 * // dump the whole buffer
 * for (int i=0; i<buff.size(); ++i)
 *     printf(" %d", buff[i]);
 * @endcode
 *
 * Copyright(c) 2017, Bo Yang
 * Contact: bonny95@gmail.com
 */
template <typename T>
class RingBuffer {
public:
    RingBuffer(size_t cap = 8):_cap(cap), _size(0), _head(0), _tail(0) {}
    ~RingBuffer() {}

    size_t capacity() const {return _cap;}
    size_t size() const {return _size;}
    bool empty() const {return (0 == _size);}
    bool full() const {return (_cap == _size);}

    inline size_t begin() const {return _head;} // Returns the index of the head
    inline size_t end() const {return _tail;} // Returns the index of the tail

    inline T &front();
    inline const T &front() const;
    inline T &back();
    inline const T &back() const;

    inline void push_back(const T&); // insert new element
    inline void pop_front();
    inline void clear();   // clear buffer
    inline void resize(size_t cap);

    inline T &operator[](size_t i);
    inline const T &operator[](size_t i) const;

private:
    vector<T> _v;
    size_t _cap;  // capacity, max possible size of buffer
    size_t _size; // real number of elments in use
    size_t _head;
    size_t _tail;
};

// Returns a reference to the head element in the buffer.
template <typename T>
inline T &RingBuffer<T>::front()
{
     if(this->empty() || _v.empty())
         return _v.front();
     return _v[_head];
}

template <typename T>
inline const T &RingBuffer<T>::front() const
{
     if(this->empty() || _v.empty())
         return _v.front();
     return _v[_head];
}

// Returns a reference to the tail element in the buffer.
template <typename T>
inline T &RingBuffer<T>::back()
{
     if(this->empty() || _v.empty())
         return _v.back();
     return _v[_tail];
}

template <typename T>
inline const T &RingBuffer<T>::back() const
{
     if(this->empty() || _v.empty())
         return _v.front();
     return _v[_tail];
}

// Insert new element to the end
template <typename T>
inline void RingBuffer<T>::push_back(const T& t)
{
    assert(_cap > 0);
    if(_v.empty())
        _v.resize(_cap);

    _v[_tail] = t;
    _tail = (_tail + 1) % _cap; // advance _tail
    if(full()) // buffer full, advance _head accordingly
        _head = (_head + 1) % _cap;
    else    // buffer not full, only increase size
        _size++;
}

// Pop one element from the head.
// No memory really freed in this function, just advance the head by 1.
template <typename T>
inline void RingBuffer<T>::pop_front()
{
    if(!empty()) {
        _head = (_head + 1) % _cap;
        _size--;
    }
}

// Clear the buffer
template <typename T>
inline void RingBuffer<T>::clear()
{
    _v.clear();
    _head = 0;
    _tail = 0;
    _size = 0;
}

// Change the capacity of the buffer
template <typename T>
inline void RingBuffer<T>::resize(size_t cap)
{
    assert(cap > 0);

    vector<T> vec;
    if (empty()) {
        clear();
    } else if(size() <= cap) {
        // buffer enlarged, _size no change
        for (size_t i = 0; i < size(); ++i)
            vec.push_back(operator[](i));
        _tail = size();
    } else {
        // buffer shrinked, truncate the first (cap - _size) elements
        for(size_t i = size()-cap; i <= size(); ++i)
            vec.push_back(operator[](i));
        _tail = 0; // buffer full
        _size = cap;
    }

    _cap = cap;
    _v.resize(cap);
    _v = vec;
    _head = 0;
}

// Returns a reference to the ith element from head.
template <typename T>
inline T &RingBuffer<T>::operator[](size_t i)
{
    if(this->empty() || _v.empty())
        return _v.front();
    return _v[(_head + i) % _cap];
}

// Returns a const reference to the ith element from head.
template <typename T>
inline const T &RingBuffer<T>::operator[](size_t i) const
{
    if(this->empty() || _v.empty())
        return _v.front();
    return _v[(_head + i) % _cap];
}
