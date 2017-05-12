#include <cstdio>
#include <string>
#include "ring_buffer.hh"

using std::string;

void dump_ring_buffer(const RingBuffer<int> &buf, const string &word)
{
    printf("%s:  capacity %lu, size %lu, head %lu, tail %lu, %s, elements:\n", word.c_str(), buf.capacity(), buf.size(), buf.begin(), buf.end(),
            buf.full() ? "full" : "not full");
    for (size_t i = 0; i < buf.size(); ++i)
        printf(" %d", buf[i]);
    printf("\n");
}

int main()
{
    RingBuffer<int> buf(8);
    size_t i = 0;
    for (i = 0; i < 7; ++i)
        buf.push_back(i);
    dump_ring_buffer(buf, "Initial state");

    buf.push_back(i++);
    dump_ring_buffer(buf, "1-elem inserted");
    buf.push_back(i++);
    buf.push_back(i++);
    dump_ring_buffer(buf, "2-elem inserted");

    buf.pop_front();
    buf.pop_front();
    dump_ring_buffer(buf, "2-elem poped");

    buf.push_back(i++);
    buf.push_back(i++);
    dump_ring_buffer(buf, "2-elem inserted");

    buf.resize(10);
    dump_ring_buffer(buf, "Resized to 10");

    buf.push_back(i++);
    dump_ring_buffer(buf, "1-elem inserted");

    buf.resize(6);
    dump_ring_buffer(buf, "Resized to 6");

    buf.push_back(i++);
    dump_ring_buffer(buf, "1-elem inserted");

    return 0;
}
