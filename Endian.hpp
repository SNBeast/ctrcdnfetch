#ifndef __ENDIAN_HPP__
#define __ENDIAN_HPP__
#include <cstring>
#include "types.h"

namespace Endian {
	static union {
		u16 foo = 1;
		u8 is_little;
	} check;
	inline u16 Be16(const u16* value) noexcept;
	inline u16 Be16(u16 value) noexcept;
	inline u32 Be32(const u32* value) noexcept;
	inline u32 Be32(u32 value) noexcept;
	inline u64 Be64(const u64* value) noexcept;
	inline u64 Be64(u64 value) noexcept;
	inline u16 Le16(const u16* value) noexcept;
	inline u16 Le16(u16 value) noexcept;
	inline u32 Le32(const u32* value) noexcept;
	inline u32 Le32(u32 value) noexcept;
	inline u64 Le64(const u64* value) noexcept;
	inline u64 Le64(u64 value) noexcept;
	inline u16 Be(const u16* value) noexcept;
	inline u16 Be(u16 value) noexcept;
	inline u32 Be(const u32* value) noexcept;
	inline u32 Be(u32 value) noexcept;
	inline u64 Be(const u64* value) noexcept;
	inline u64 Be(u64 value) noexcept;
	inline u16 Le(const u16* value) noexcept;
	inline u16 Le(u16 value) noexcept;
	inline u32 Le(const u32* value) noexcept;
	inline u32 Le(u32 value) noexcept;
	inline u64 Le(const u64* value) noexcept;
	inline u64 Le(u64 value) noexcept;
}

u16 Endian::Be16(const u16* value) noexcept {
	u16 _value;
	memcpy(&_value, value, sizeof(u16));
	if (check.is_little) {
		#if defined __clang__ || defined __GNUC__
		_value = __builtin_bswap16(_value);
		#else
		u16 tmp = _value;
		for(int i = 0; i < sizeof(u16); i++) ((u8 *)&_value)[i] = ((u8 *)&tmp)[sizeof(u16)-1-i];
		#endif
	}
	return _value;
}

u16 Endian::Be16(u16 value) noexcept {
	if (check.is_little) {
		#if defined __clang__ || defined __GNUC__
		value = __builtin_bswap16(value);
		#else
		u16 tmp = value;
		for(int i = 0; i < sizeof(u16); i++) ((u8 *)&value)[i] = ((u8 *)&tmp)[sizeof(u16)-1-i];
		#endif
	}
	return value;
}

u32 Endian::Be32(const u32* value) noexcept {
	u32 _value;
	memcpy(&_value, value, sizeof(u32));
	if (check.is_little) {
		#if defined __clang__ || defined __GNUC__
		_value = __builtin_bswap32(_value);
		#else
		u32 tmp = _value;
		for(int i = 0; i < sizeof(u32); i++) ((u8 *)&_value)[i] = ((u8 *)&tmp)[sizeof(u32)-1-i];
		#endif
	}
	return _value;
}

u32 Endian::Be32(u32 value) noexcept {
	if (check.is_little) {
		#if defined __clang__ || defined __GNUC__
		value = __builtin_bswap32(value);
		#else
		u32 tmp = value;
		for(int i = 0; i < sizeof(u32); i++) ((u8 *)&value)[i] = ((u8 *)&tmp)[sizeof(u32)-1-i];
		#endif
	}
	return value;
}

u64 Endian::Be64(const u64* value) noexcept {
	u64 _value;
	memcpy(&_value, value, sizeof(u64));
	if (check.is_little) {
		#if defined __clang__ || defined __GNUC__
		_value = __builtin_bswap64(_value);
		#else
		u64 tmp = _value;
		for(int i = 0; i < sizeof(u64); i++) ((u8 *)&_value)[i] = ((u8 *)&tmp)[sizeof(u64)-1-i];
		#endif
	}
	return _value;
}

u64 Endian::Be64(u64 value) noexcept {
	if (check.is_little) {
		#if defined __clang__ || defined __GNUC__
		value = __builtin_bswap64(value);
		#else
		u64 tmp = value;
		for(int i = 0; i < sizeof(u64); i++) ((u8 *)&value)[i] = ((u8 *)&tmp)[sizeof(u64)-1-i];
		#endif
	}
	return value;
}

u16 Endian::Le16(const u16* value) noexcept {
	u16 _value;
	memcpy(&_value, value, sizeof(u16));
	if (!check.is_little) {
		#if defined __clang__ || defined __GNUC__
		_value = __builtin_bswap16(_value);
		#else
		u16 tmp = _value;
		for(int i = 0; i < sizeof(u16); i++) ((u8 *)&_value)[i] = ((u8 *)&tmp)[sizeof(u16)-1-i];
		#endif
	}
	return _value;
}

u16 Endian::Le16(u16 value) noexcept {
	if (!check.is_little) {
		#if defined __clang__ || defined __GNUC__
		value = __builtin_bswap16(value);
		#else
		u16 tmp = value;
		for(int i = 0; i < sizeof(u16); i++) ((u8 *)&value)[i] = ((u8 *)&tmp)[sizeof(u16)-1-i];
		#endif
	}
	return value;
}

u32 Endian::Le32(const u32* value) noexcept {
	u32 _value;
	memcpy(&_value, value, sizeof(u32));
	if (!check.is_little) {
		#if defined __clang__ || defined __GNUC__
		_value = __builtin_bswap32(_value);
		#else
		u32 tmp = _value;
		for(int i = 0; i < sizeof(u32); i++) ((u8 *)&_value)[i] = ((u8 *)&tmp)[sizeof(u32)-1-i];
		#endif
	}
	return _value;
}

u32 Endian::Le32(u32 value) noexcept {
	if (!check.is_little) {
		#if defined __clang__ || defined __GNUC__
		value = __builtin_bswap32(value);
		#else
		u32 tmp = value;
		for(int i = 0; i < sizeof(u32); i++) ((u8 *)&value)[i] = ((u8 *)&tmp)[sizeof(u32)-1-i];
		#endif
	}
	return value;
}

u64 Endian::Le64(const u64* value) noexcept {
	u64 _value;
	memcpy(&_value, value, sizeof(u64));
	if (!check.is_little) {
		#if defined __clang__ || defined __GNUC__
		_value = __builtin_bswap64(_value);
		#else
		u64 tmp = _value;
		for(int i = 0; i < sizeof(u64); i++) ((u8 *)&_value)[i] = ((u8 *)&tmp)[sizeof(u64)-1-i];
		#endif
	}
	return _value;
}

u64 Endian::Le64(u64 value) noexcept {
	if (!check.is_little) {
		#if defined __clang__ || defined __GNUC__
		value = __builtin_bswap64(value);
		#else
		u64 tmp = value;
		for(int i = 0; i < sizeof(u64); i++) ((u8 *)&value)[i] = ((u8 *)&tmp)[sizeof(u64)-1-i];
		#endif
	}
	return value;
}

u16 Endian::Be(const u16* value) noexcept {
	return Be16(value);
}

u16 Endian::Be(u16 value) noexcept {
	return Be16(value);
}

u32 Endian::Be(const u32* value) noexcept {
	return Be32(value);
}

u32 Endian::Be(u32 value) noexcept {
	return Be32(value);
}

u64 Endian::Be(const u64* value) noexcept {
	return Be64(value);
}

u64 Endian::Be(u64 value) noexcept {
	return Be64(value);
}

u16 Endian::Le(const u16* value) noexcept {
	return Le16(value);
}

u16 Endian::Le(u16 value) noexcept {
	return Le16(value);
}

u32 Endian::Le(const u32* value) noexcept {
	return Le32(value);
}

u32 Endian::Le(u32 value) noexcept {
	return Le32(value);
}

u64 Endian::Le(const u64* value) noexcept {
	return Le64(value);
}

u64 Endian::Le(u64 value) noexcept {
	return Le64(value);
}

#endif