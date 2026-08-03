#ifndef FW_BITSET_HPP
#define FW_BITSET_HPP

class BitSet
{
private:
    std::vector<uint64_t> _bits;
    size_t                _size;

public:
    explicit BitSet(size_t size);

    void                 set(size_t index);
    [[nodiscard]] bool   get(size_t index) const;
    bool                 claim(size_t index);
    void                 clear(size_t index);
    [[nodiscard]] size_t size() const;
    void                 reset();
    void                 flip();
};

#endif // FW_BITSET_HPP
