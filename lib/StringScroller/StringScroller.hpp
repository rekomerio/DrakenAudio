#pragma once

#include <array>
#include <stdint.h>
#include <algorithm>
#include <cstring>

class StringScroller
{
public:
    void setString(const char* str, size_t maxLength);
    const char* getScrolledString();
    void scroll();

private:
    enum class PADDING_STATE
    {
        ADD,
        WAIT,
        COOLDOWN,
    };

    char nextToken(size_t index);

    std::array<char, 64> _originalString;
    std::array<char, 64> _scrolledString;
    size_t _maxLength = 0;
    size_t _scrollIndex = 0;
    size_t _originalStringLength = 0;
    size_t _nPaddingAdded = 0;
    PADDING_STATE _paddingState;
};
