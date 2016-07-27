#ifndef __SHMRINGBUFFER_HH__
#define __SHMRINGBUFFER_HH__

#include <string>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>      /* For O_CREAT, O_RDWR */
#include <sys/mman.h>   /* shared memory and mmap() */
#include <sys/stat.h>   /* S_IRWXU */

using std::string;

//
// Shared-memory based Ring buffer.
//
// T must be POD type
#define EVENT_BUFFER_SHM "/shm_ring_buffer"
template <typename T>
class ShmRingBuffer {
public:
    ShmRingBuffer(size_t cap = 100, bool master = false, const char * path = EVENT_BUFFER_SHM):
                    _hdr(NULL),_lock(NULL),_v(NULL),_shm_path(path),_shm_size(0),_master(master) {
        init(cap, master, path);
    }
    ~ShmRingBuffer() {
        if (_v)
            munmap((void *)_v, _shm_size);
        _v = NULL;
        //if (_master)
        //    shm_unlink(_shm_path);
    }

    size_t capacity() const;
    size_t begin() const;
    size_t end() const;

    void clear();   // clear buffer
    void push_back(const T&); // insert new event
    T dump_front();
    string unparse() const; // dump contents in the buffer to a string

private:
    // Mutex, Condition and ReadWriteLock must be POD type to use shared memory
    class Mutex {
    public:
        pthread_mutex_t _mutex;
        pthread_mutexattr_t _attr;

        void init(bool pshared = false) {
            pthread_mutexattr_init(&_attr);
            if (pshared)
                pthread_mutexattr_setpshared(&_attr, PTHREAD_PROCESS_SHARED);
            else
                pthread_mutexattr_setpshared(&_attr, PTHREAD_PROCESS_PRIVATE);
            pthread_mutex_init(&_mutex, &_attr);
        }

        int lock() {
            return pthread_mutex_lock(&_mutex);
        }
        int trylock() {
            return pthread_mutex_trylock(&_mutex);
        }
        int unlock() {
            return pthread_mutex_unlock(&_mutex);
        }
    };
    
    class Condition {
    public:
        pthread_cond_t _cond;
        pthread_condattr_t _attr;

        void init(bool pshared = false) {
            pthread_condattr_init(&_attr);
            if (pshared)
                pthread_condattr_setpshared(&_attr, PTHREAD_PROCESS_SHARED);
            else
                pthread_condattr_setpshared(&_attr, PTHREAD_PROCESS_PRIVATE);
    
            pthread_cond_init(&_cond, &_attr);
        }

        int wait(Mutex &m) {
            return pthread_cond_wait(&_cond, &m._mutex);
        }
        int timedwait(const struct timespec &ts, Mutex &m) {
            return pthread_cond_timedwait(&_cond, &m._mutex, &ts);
        }
        int signal() {
            return pthread_cond_signal(&_cond);
        }
        int broadcast() {
            return pthread_cond_broadcast(&_cond);
        }
    };
    
    // Multiple-writer, multiple-reader lock (write-preferring)
    class ReadWriteLock {
    public:
        void init(bool pshared = false) {
            _nread = _nread_waiters = 0;
            _nwrite = _nwrite_waiters = 0;
            _mtx.init(pshared);
            _rcond.init(pshared);
            _wcond.init(pshared);
        }
    
        void read_lock() {
            _mtx.lock();
            if (_nwrite || _nwrite_waiters) {
                _nread_waiters++;
                do _rcond.wait(_mtx);
                while (_nwrite || _nwrite_waiters);
                _nread_waiters--;
            }
            _nread++;
            _mtx.unlock();
        }
    
        void read_unlock() {
            _mtx.lock();
            _nread--;
            if (_nwrite_waiters)
                _wcond.broadcast();
            _mtx.unlock();
        }
    
        void write_lock() {
            _mtx.lock();
            if (_nread || _nwrite) {
                _nwrite_waiters++;
                do _wcond.wait(_mtx);
                while (_nread || _nwrite);
                _nwrite_waiters--;
            }
            _nwrite++;
            _mtx.unlock();
        }
    
        void write_unlock() {
            _mtx.lock();
            _nwrite--;
            if (_nwrite_waiters)
                _wcond.broadcast();
            else if (_nread_waiters)
                _rcond.broadcast();
            _mtx.unlock();
        }

    private:
        Mutex _mtx;
        Condition _rcond;
        Condition _wcond;
        uint32_t _nread, _nread_waiters;
        uint32_t _nwrite, _nwrite_waiters;
    };

    typedef struct _ShmHeader {
        size_t _capacity; // max number of logs
        int _begin;    // start index of the circular buffer
        int _end;      // end index of the circular buffer
    } ShmHeader;

    ShmHeader * _hdr;
    ReadWriteLock * _lock;
    T * _v;  // pointer to the head of event buffer
    string _shm_path;
    size_t _shm_size; // size(bytes) of shared memory
    bool _master;

    // template <class In> ...
    ShmRingBuffer(const ShmRingBuffer<T> &);
    ShmRingBuffer<T>& operator=(const ShmRingBuffer<T>&);

    bool init(size_t cap, bool master, const char * path);
};

template <typename T> inline size_t
ShmRingBuffer<T>::capacity() const
{
    assert(_hdr != NULL);

    size_t cap = 0;
    _lock->read_lock();
    cap = _hdr->_capacity;
    _lock->read_unlock();
    return cap;
}

template <typename T> inline size_t
ShmRingBuffer<T>::begin() const
{
    assert(_hdr != NULL);

    size_t idx = 0;
    _lock->read_lock();
    idx = _hdr->_begin;
    _lock->read_unlock();
    return idx;
}

template <typename T> inline size_t
ShmRingBuffer<T>::end() const
{
    assert(_hdr != NULL);

    size_t idx = 0;
    _lock->read_lock();
    idx = _hdr->_end;
    _lock->read_unlock();
    return idx;
}

template <typename T> inline bool
ShmRingBuffer<T>::init(size_t cap, bool master, const char * path)
{
    assert(path != NULL);
    int shm_fd = shm_open(path, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG); // TODO: O_TRUNC?
    if (shm_fd < 0) {
        perror("shm_open failed");
        return false;
    }

    _shm_size = sizeof(ShmHeader) + sizeof(ReadWriteLock) + cap * sizeof(T);
    if (master && (ftruncate(shm_fd, _shm_size) < 0)) {
        perror("ftruncate failed");
        //shm_unlink(path);
        //return false;
    }

    void *pbuf = NULL; /* shared memory adddress */
    pbuf = mmap(NULL, _shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (pbuf == (void *) -1) {
        perror("mmap failed");
        return false;
    }

    _hdr = reinterpret_cast<ShmHeader *>(pbuf);
    assert(_hdr != NULL);
    _lock = reinterpret_cast<ReadWriteLock *>((char*)_hdr + sizeof(ShmHeader));
    assert(_lock != NULL);
    _v = reinterpret_cast<T *>((char*)_lock + sizeof(ReadWriteLock));
    assert(_v != NULL);

    if (master) {
        _hdr->_capacity = cap;
        _hdr->_begin = _hdr->_end = 0;
        _lock->init(true);
    }

    return true;
}

template <typename T> inline void
ShmRingBuffer<T>::clear()
{
    if (!_hdr || !_lock)
        return;

    _lock->write_lock();
    _hdr->_begin = _hdr->_end = 0;
    // TODO: memset the shared memory?
    _lock->write_unlock();
}

template <typename T> inline void 
ShmRingBuffer<T>::push_back(const T& e)
{
    assert(_hdr != NULL);
    assert(_v != NULL);

    _lock->write_lock();
    memcpy(_v + _hdr->_end, &e, sizeof(e));
    _hdr->_end = (_hdr->_end + 1) % _hdr->_capacity; // make sure index is in range [0..._capacity)
    if (_hdr->_end == _hdr->_begin) // buffer is full, advance begin index, too
        _hdr->_begin = (_hdr->_begin + 1) % _hdr->_capacity;
    _lock->write_unlock();
}

template <typename T> inline T
ShmRingBuffer<T>::dump_front()
{
    assert(_hdr != NULL);
    assert(_v != NULL);

    T ret;
    _lock->write_lock();
    if (_hdr->_begin != _hdr->_end) {
        ret = *(_v + _hdr->_begin);
        _hdr->_begin = (_hdr->_begin + 1) % _hdr->_capacity;
    }
    _lock->write_unlock();
    return ret;
}

template <typename T> inline string
ShmRingBuffer<T>::unparse() const {
    assert(_hdr != NULL);
    assert(_v != NULL);

    string ret;
    _lock->read_lock();
    if (_hdr->_begin == _hdr->_end) {
        _lock->read_unlock();
        return string();
    }

    for (int i = _hdr->_begin; i != _hdr->_end; i = (i+1) % _hdr->_capacity) {
        ret += string((_v + i)->unparse()) + "\n";
    }
    _lock->read_unlock();
    return ret;
}


#endif
