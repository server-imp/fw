#include "bitset.hpp"

BitSet::BitSet(const size_t size)
    : _bits((size + 63) >> 6), _size(size) {}

void BitSet::set(const size_t index)
{
    _bits[index >> 6] |= 1ULL << (index & 63);
}

bool BitSet::get(const size_t index) const
{
    return _bits[index >> 6] & (1ULL << (index & 63));
}

bool BitSet::claim(const size_t index)
{
    const size_t   id   = index >> 6;
    const uint64_t mask = 1ULL << (index & 63);

    uint64_t& word = _bits[id];

    if (word & mask)
        return false;

    word |= mask;
    return true;
}

void BitSet::clear(const size_t index)
{
    _bits[index >> 6] &= ~(1ULL << (index & 63));
}

size_t BitSet::size() const
{
    return _size;
}

void BitSet::reset()
{
    std::fill(_bits.begin(), _bits.end(), 0);
}

void BitSet::flip()
{
    for (auto& word : _bits)
        word = ~word;

    if (const size_t extra = _bits.size() * 64 - _size)
        _bits.back() &= UINT64_MAX >> extra;
}
