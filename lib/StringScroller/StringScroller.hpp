#pragma once

#include <array>
#include <stdint.h>
#include <algorithm>
#include <cstring>
#include "../../include/defines.h"

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

    std::array<char, SID_MESSAGE_BUFFER_SIZE> _originalString;
    std::array<char, SID_MESSAGE_BUFFER_SIZE> _scrolledString;
    size_t _maxLength = 0;
    size_t _scrollIndex = 0;
    size_t _originalStringLength = 0;
    size_t _nPaddingAdded = 0;
    PADDING_STATE _paddingState;
};
