#include "StringScroller.hpp"

void StringScroller::setString(const char* str, size_t maxLength)
{
	memset(_originalString.data(), 0, _originalString.size());
	memset(_scrolledString.data(), 0, _scrolledString.size());

	_originalStringLength = std::min(_originalString.size(), strlen(str));

	memcpy(_originalString.data(), str, _originalStringLength);
	memcpy(_scrolledString.data(), str, _originalStringLength);

	_maxLength = std::min(maxLength, _scrolledString.size() - 1);

	_scrolledString[_maxLength] = '\0';

	_nPaddingAdded = 0;
	_scrollIndex = 0;

	_paddingState = PADDING_STATE::WAIT;
}

const char* StringScroller::getScrolledString()
{
	return _scrolledString.data();
}

void StringScroller::scroll()
{
	if (_originalStringLength <= _maxLength)
	{
		// The text is always fully visible so no need to scroll
		return;
	}

	_scrollIndex++;

	for (size_t i = 0; i < _maxLength; i++)
	{
		_scrolledString[i] = nextToken(i);
	}

	_scrolledString[_maxLength] = '\0';

	if (_scrollIndex == _originalStringLength)
	{
		_scrollIndex = 0;
	}
}

char StringScroller::nextToken(size_t index)
{
	// The last character should always be taken from original string or be padding
	if (index == _maxLength - 1 || index >= _originalStringLength)
	{
		switch (_paddingState)
		{
		case PADDING_STATE::ADD:
		{
			_nPaddingAdded++;
			if (_nPaddingAdded >= _maxLength)
			{
				_scrollIndex -= _nPaddingAdded;
				_nPaddingAdded = 0;
				_paddingState = PADDING_STATE::COOLDOWN;
			}

			return ' ';
		}

		case PADDING_STATE::WAIT:
			if (index + _scrollIndex == _originalStringLength - 1)
			{
				_paddingState = PADDING_STATE::ADD;
			}
			break;

		case PADDING_STATE::COOLDOWN:
			_paddingState = PADDING_STATE::WAIT;
			break;
		}

		size_t i = (index + _scrollIndex) % _originalStringLength;
		return _originalString[i];
	}

	return _scrolledString[index + 1];
}
