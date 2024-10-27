//[c]Based system layer
//[c]
//[c]All code is lower case, only comments use capitalization.
//[c]Alignment is done via elastic tabstops feature, which is only available in a few editors like Clade.

//[c]Entry point
int start(void);
//[c]Exit function
void exit(int status);

#define _CRT_DECLARE_NONSTDC_NAMES 0
#define NOMINMAX
#include <windows.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(linker, "/subsystem:console")
#pragma comment(linker, "/entry:enter")

extern void _mm_pause(void);

void exit(int status)
{
	ExitProcess(status);
}

#pragma function(memset)
void* memset(void* dest, int ch, size_t count)
{
	unsigned char* ptr = (unsigned char*)dest;
	for (size_t i = 0; i < count; i++) {
		ptr[i] = (unsigned char)ch;
	}
	return (dest);
}

#pragma function(memcpy)
void* memcpy(void* dest, const void* src, size_t count)
{
	unsigned char* d = (unsigned char*)dest;
	const unsigned char* s = (const unsigned char*)src;
	for (size_t i = 0; i < count; i++) {
		d[i] = s[i];
	}
	return (dest);
}

#pragma function(memcmp)
int memcmp(const void* lhs, const void* rhs, size_t count)
{
	const unsigned char* p1 = (const unsigned char*)lhs;
	const unsigned char* p2 = (const unsigned char*)rhs;
	for (size_t i = 0; i < count; ++i) {
		if (p1[i] != p2[i]) {
			return (p1[i] - p2[i]);
		}
	}
	return (0);
}

//[c]Macros
#define null	((void*)0)
#define false	0
#define true	!false
#define type_of	__typeof__
#define align_of	alignof
#define offset_of	offsetof
#define size_of	sizeof
#define count_of(a)	(size_of(a) / size_of((a)[0]))
#define cast(t, m)	((t)(m))
#define bit_cast(t, m)	*cast(t*, &(m))
#define trap	__debugbreak
#define cpu_relax	_mm_pause

#define abs(x) \
	((x) < 0 ? -(x) : (x))
#define min(x, y) \
	((x) < (y) ? (x) : (y))
#define max(x, y) \
	((x) > (y) ? (x) : (y))
#define clamp_top(x, y) \
	min(x, y)
#define clamp_bot(x, y) \
	max(x, y)
#define clamp(x, low, high) \
	clamp_top(clamp_bot(x, low), high)
#define swap(a, b) \
	do{type_of(a) t__ = a; a = b; b = t__;}while(0)
#define size_of_batch(t, n) \
	(size_of(t) * (n))
#define array(type, name) \
	type* name, i64 name##_count
#define array_expand(a) \
	a, count_of(a)
#define string_to_array(s) \
	s, count_of(s) - 1
#define vcall(v, c, ...) \
	(v)->c(v, ##__VA_ARGS__)
#define debug_assert(cond) \
	do{if(!(cond)){trap();}}while(0)

//[c]Types
typedef char	i8;
typedef short	i16;
typedef int	i32;
typedef long long	i64;
typedef unsigned char	n8;
typedef unsigned short	n16;
typedef unsigned int	n32;
typedef unsigned long long	n64;
typedef n8	byte;
typedef n64	bool;
typedef n32	uchar;
typedef float	f32;
typedef double	f64;

//[c]Limits
#define i8_min	0x80
#define i16_min	0x8000
#define i32_min	0x80000000
#define i64_min	0x8000000000000000ull
#define i8_max	0x7F
#define i16_max	0x7FFF
#define i32_max	0x7FFFFFFF
#define i64_max	0x7FFFFFFFFFFFFFFFull
#define n8_min	0x00
#define n16_min	0x0000
#define n32_min	0x00000000
#define n64_min	0x0000000000000000ull
#define n8_max	0xFF
#define n16_max	0xFFFF
#define n32_max	0xFFFFFFFF
#define n64_max	0xFFFFFFFFFFFFFFFFull

//[c]Memory
#define mem_byte	1ll
#define mem_kilobyte	(1024 * mem_byte)
#define mem_megabyte	(1024 * mem_kilobyte)
#define mem_gigabyte	(1024 * mem_megabyte)
#define mem_page_size	(4 * mem_kilobyte)
#define mem_big_page_size	(64 * mem_kilobyte)

#define mem_copy(dst, src, size) \
	memcpy(cast(void*, dst), cast(const void*, src), cast(size_t, size))
#define mem_set(addr, size, value) \
	memset(cast(void*, addr), cast(int, value), cast(size_t, size))
#define mem_zero(addr, size) \
	mem_set(addr, size, 0)
#define is_power_of_two(x) \
	(((x) > 0) && (((x) & ((x) - 1)) <= 0))
#define mem_align_forward(addr, alignment) \
	((is_power_of_two(alignment)) ? (((addr) + ((alignment) - 1)) & ~((alignment) - 1)) : 0)

//[c]Unicode and strings
//[of]:i64	uchar_size_naive(uchar uc)
i64 uchar_size_naive(uchar uc)
{
	static const char sizes[32] = {
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		0, 0, 0, 0, 0, 0, 0, 0,
		2, 2, 2, 2, 3, 3, 4, 0
	};
	unsigned char c = *cast(unsigned char*, &uc);
	i64 out = sizes[c >> 3];
	out = uc ? out : 0;
	return (out);
}
//[cf]
//[of]:i64	uchar_size(uchar uc)
i64 uchar_size(uchar uc)
{
	static const n32 mins[] = {4194304, 0, 128, 2048, 65536};
	static const i32 shifte[] = {0, 6, 4, 2, 0};
	static const i32 masks[]  = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
	static const i32 shiftc[] = {0, 18, 12, 6, 0};
	char* bytes = cast(char*, &uc);
	i64 size = uchar_size_naive(uc);
	n32 c32 = (bytes[0] & masks[size]) << 18;
	c32 |= (bytes[1] & 0x3f) << 12;
	c32 |= (bytes[2] & 0x3f) << 6;
	c32 |= (bytes[3] & 0x3f) << 0;
	c32 >>= shiftc[size];
	i32 err = (c32 < mins[size]) << 6; // non-canonical encoding
	err |= ((c32 >> 11) == 0x1b) << 7;  // surrogate half
	err |= (c32 > 0x10FFFF) << 8;  // out of range
	err |= (bytes[1] & 0xc0) >> 2;
	err |= (bytes[2] & 0xc0) >> 4;
	err |= (bytes[3]) >> 6;
	err ^= 0x2a; // incorrect top two bits of each tail byte
	err >>= shifte[size];
	size = err ? 0 : size;

//[c]//	TODO: work this one out, much less obtuse
//[c]	i64 size = uchar_size_naive(uc);
//[c]	bool valid = false;
//[c]	valid = (uc <= 0x7f) ? true : valid;
//[c]	bool is_2byte = ((uc & 0xc0e0) == 0x80c0);
//[c]	bool is_3byte = ((uc & 0xc0c0f0) == 0x8080e0);
//[c]	bool is_4byte = ((uc & 0xc0c0c0f8) == 0x808080f0);
//[c]	valid = ((0x80c2 <= uc) & (uc <= 0xbfdf)) ? is_2byte : valid;
//[c]//	surrogate half
//[c]	valid = ((0xeda080 <= uc) & (uc <= 0xedbfbf)) ? false : valid;
//[c]	valid = ((0x80a0e0 <= uc) & (uc <= 0xbfbfef)) ? is_3byte : valid;
//[c]	valid = ((0x808090f0 <= uc) & (uc <= 0xbfbf8ff4)) ? is_4byte : valid;
//[c]	size = valid ? size : 0;

	return (size);
}
//[cf]
//[of]:uchar	string_decode_uchar(const char* s)
uchar string_decode_uchar(const char* s)
{
	uchar uc;
	char* bytes = cast(char*, &uc);
	i64 size = uchar_size_naive(*s);
	size = *s ? size : 0;
	char c = *s;
	c = (size < 1) ? 0 : c;
	bytes[0] = c;
	s += *s ? 1 : 0;
	c = *s;
	c = (size < 2) ? 0 : c;
	bytes[1] = c;
	s += *s ? 1 : 0;
	c = *s;
	c = (size < 3) ? 0 : c;
	bytes[2] = c;
	s += *s ? 1 : 0;
	c = *s;
	c = (size < 4) ? 0 : c;
	bytes[3] = c;
	return (uc);
}
//[cf]
//[of]:bool	string_is(const char* s, const char* x)
bool string_is(const char* s, const char* x)
{
	bool match = true;
	while (true) {
		match = (*s != *x) ? false : match;
		if ((*s == 0) | (*x == 0)) {
			break;
		}
		s++;
		x++;
	}
	return (match);
}
//[cf]
//[of]:i64	string_count(const char* s)
i64 string_count(const char* s)
{
	i64 count = 0;
	while (s[count] != 0) {
		count++;
	}
	return (count);
}
//[cf]
//[of]:i64	string_copy(array(char, buf), const char* s)
i64 string_copy(array(char, buf), const char* s)
{
	buf_count--;
	i64 pos = 0;
	while (*s != 0) {
		i64 char_size = uchar_size(string_decode_uchar(s));
		char_size = clamp_bot(char_size, 1);
		i64 space_left = buf_count - pos;
		if (space_left < char_size) {
			break;
		}
		mem_copy(buf + pos, s, char_size);
		pos += char_size;
		s += char_size;
	}
	buf[pos] = 0;
	return (pos);
}
//[cf]
//[of]:i64	string_append(array(char, buf), const char* s)
i64 string_append(array(char, buf), const char* s)
{
	buf_count--;
	i64 b_count = string_count(buf);
	i64 pos = b_count;
	while (*s != 0) {
		i64 char_size = uchar_size(string_decode_uchar(s));
		char_size = clamp_bot(char_size, 1);
		i64 space_left = buf_count - pos;
		if (space_left < char_size) {
			break;
		}
		mem_copy(buf + pos, s, char_size);
		pos += char_size;
		s += char_size;
	}
	buf[pos] = 0;
	return (pos - b_count);
}
//[cf]
//[c]TODO: string_from_float
//[of]:[internal] char*	string_from_integer(char (*buf)[66], n64 x, n64 base, bool is_signed, n64 leading_zeroes)
char* string_from_integer(char (*buf)[66], n64 x, n64 base, bool is_signed, n64 leading_zeroes)
{
	static const char* digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	debug_assert(base <= 36);
	bool is_neg = (is_signed && (cast(i64, x) < 0));
	x = is_neg ? -(cast(i64, x)) : x;
	char* numbers = *buf;
	i64 pos = 65;
	do {
		pos--;
		n64 remainder = x % base;
		numbers[pos] = digits[remainder];
		x /= base;
	} while (x > 0);
	i64 zero_pos = 65 - leading_zeroes;
	i64 zero_size = max(pos - zero_pos, 0);
	mem_set(&numbers[zero_pos], zero_size, '0');
	pos -= zero_size;
	pos--;
	numbers[pos] = '-';
	pos += is_neg ? 0 : 1;
	numbers[65] = 0;
	return (&numbers[pos]);
}
//[cf]
//[of]:[internal] bool	string_formatter_run(struct string_formatter* f, const char* format)
//[c]String formatting using based on views specified as {view}, {{this} escapes the opening brace
//[c]Views:
//[c]b8/16/32/64+ - binary view with optional leading zeroes
//[c]x8/16/32/64+ - hexadecimal view with optional leading zeroes
//[c]n8/16/32/64 - natural numbers
//[c]i8/16/32/64 - integer numbers
//[c]TODO: f32/64.r - floating-point numbers with optional rounding
//[c]c - 1-byte character
//[c]u - 4-byte UTF-8 encoded character (uchar)
//[c]s - UTF-8 encoded string
//[c]
//[c]Examples:
//[c]"hello {{world}}"
//[c]"{i32} = 0x{x32} = 0b{b32+}", 37, 37, 37
//[c]"{s} {u}{u}{c}", "hello", string_decode_uchar("世"), string_decode_uchar("界"), '!'
//[c]TODO
//[c]"{f32.2}", 0.9876
enum string_formatter_type {
	string_formatter_type_bin = 1,
	string_formatter_type_hex,
	string_formatter_type_nat,
	string_formatter_type_int,
	string_formatter_type_char,
	string_formatter_type_uchar,
	string_formatter_type_str,
};
struct string_formatter {
	i64	format_pos;
	bool	args_invalid;
	bool	parsing_view;
	const char*	view;
	char	value_buf[66];

	const char*	value;
};
bool string_formatter_run(struct string_formatter* f, const char* format, va_list* args)
{
	const char* chunk = format + f->format_pos;
	char c = chunk[0];
	uchar uc = string_decode_uchar(chunk);
	i64 uc_size = uchar_size(uc);
	f->format_pos += clamp_bot(uchar_size(uc), 1);
	if (!f->parsing_view) {
		switch (c) {
			case 0: {
				return (false);
			} break;
			case '{': {
				f->parsing_view = true;
				f->view = chunk;
				f->value = null;
				return (true);
			} break;
			default: {
				mem_copy(f->value_buf, &uc, uc_size);
				f->value_buf[uc_size] = 0;
				f->value = f->value_buf;
				return (true);
			} break;
		}
	}
	if (f->view[1] == '{') {
		f->parsing_view = false;
		mem_copy(f->value_buf, "{", 2);
		f->value = f->value_buf;
		return (true);
	}
	if (c == 0) {
		f->format_pos--;
		f->parsing_view = false;
		mem_copy(f->value_buf, "?", 2);
		f->value = f->value_buf;
		return (true);
	}
	if (c != '}') {
		f->value = null;
		return (true);
	}
	f->view++;
	enum string_formatter_type	type_flag = 0;
	type_flag = (f->view[0] == 'b') ? string_formatter_type_bin : type_flag;
	type_flag = (f->view[0] == 'x') ? string_formatter_type_hex : type_flag;
	type_flag = (f->view[0] == 'n') ? string_formatter_type_nat : type_flag;
	type_flag = (f->view[0] == 'i') ? string_formatter_type_int : type_flag;
	type_flag = (f->view[0] == 'c') ? string_formatter_type_char : type_flag;
	type_flag = (f->view[0] == 'u') ? string_formatter_type_uchar : type_flag;
	type_flag = (f->view[0] == 's') ? string_formatter_type_str : type_flag;
	f->view += (type_flag != 0) ? 1 : 0;
	n64 type_size = 0;
	type_size = (f->view[0] == '8') ? 8 : type_size;
	type_size = ((f->view[0] == '1') & (f->view[1] == '6')) ? 16 : type_size;
	type_size = ((f->view[0] == '3') & (f->view[1] == '2')) ? 32 : type_size;
	type_size = ((f->view[0] == '6') & (f->view[1] == '4')) ? 64 : type_size;
	f->view += (type_size > 0) ? 1 : 0;
	f->view += (type_size > 8) ? 1 : 0;
	n64 zero_padding = 0;
	zero_padding = (f->view[0] == '+') ? type_size : zero_padding;
	f->view += (zero_padding != 0) ? 1 : 0;
	zero_padding = (type_flag == string_formatter_type_hex) ? (zero_padding / 4) : zero_padding;
	type_flag = (f->view[0] != '}') ? 0 : type_flag;
	type_flag = ((type_flag == string_formatter_type_nat) && (zero_padding != 0)) ? 0 : type_flag;
	type_flag = ((type_flag == string_formatter_type_int) && (zero_padding != 0)) ? 0 : type_flag;
	type_flag = ((type_flag == string_formatter_type_char) && (type_size != 0)) ? 0 : type_flag;
	type_flag = ((type_flag == string_formatter_type_uchar) && (type_size != 0)) ? 0 : type_flag;
	type_flag = ((type_flag == string_formatter_type_str) && (type_size != 0)) ? 0 : type_flag;
	type_flag = f->args_invalid ? 0 : type_flag;
	f->parsing_view = false;
//[c]	Validate the types and extract args
	switch (type_flag) {
		case string_formatter_type_bin:
		case string_formatter_type_hex:
		case string_formatter_type_nat: {
			n64 arg;
			if (type_size == 64) {
				arg = va_arg(*args, n64);
			} else {
				arg = va_arg(*args, n32);
			}
			n64 base = 10;
			base = (type_flag == string_formatter_type_bin) ? 2 : base;
			base = (type_flag == string_formatter_type_hex) ? 16 : base;
			arg &= (type_size == 8) ? 0x00000000000000ff : 0xffffffffffffffff;
			arg &= (type_size == 16) ? 0x000000000000ffff : 0xffffffffffffffff;
			arg &= (type_size == 32) ? 0x00000000ffffffff : 0xffffffffffffffff;
			f->value = string_from_integer(&f->value_buf, arg, base, false, zero_padding);
		} break;
		case string_formatter_type_int: {
			i64 arg;
			if (type_size == 64) {
				arg = va_arg(*args, i64);
			} else {
				arg = va_arg(*args, i32);
			}
			arg &= (type_size == 8) ? 0x00000000000000ff : 0xffffffffffffffff;
			arg &= (type_size == 16) ? 0x000000000000ffff : 0xffffffffffffffff;
			arg &= (type_size == 32) ? 0x00000000ffffffff : 0xffffffffffffffff;
			f->value = string_from_integer(&f->value_buf, arg, 10, true, 0);
		} break;
		case string_formatter_type_char: {
			char arg = cast(char, va_arg(*args, n32));
			f->value_buf[0] = arg;
			f->value_buf[1] = 0;
			f->value = f->value_buf;
		} break;
		case string_formatter_type_uchar: {
			uchar arg = va_arg(*args, uchar);
			i64 arg_size = uchar_size(arg);
			mem_copy(f->value_buf, &arg, arg_size);
			f->value_buf[arg_size] = 0;
			f->value = f->value_buf;
		} break;
		case string_formatter_type_str: {
			f->value = va_arg(*args, const char*);
		} break;
		default: {
			f->args_invalid = true;
			mem_copy(f->value_buf, "?", 2);
			f->value = f->value_buf;
		} break;
	}
	return (true);
}
//[cf]
//[of]:[internal] void	string_format_args(array(char, buf), const char* format, va_list* args)
void string_format_args(array(char, buf), const char* format, va_list* args)
{
	buf[0] = 0;
	struct string_formatter f = (struct string_formatter){0};
	while (string_formatter_run(&f, format, args)) {
		if (f.value == null) {
			continue;
		}
		i64 n = string_append(buf, buf_count, f.value);
		if (n < string_count(f.value)) {
			break;
		}
	}
}
//[cf]
//[of]:void	string_format(array(char, buf), const char* format, ...)
void string_format(array(char, buf), const char* format, ...)
{
	va_list args;
	va_start(args, format);
	string_format_args(buf, buf_count, format, &args);
	va_end(args);
}
//[cf]

//[c]Basic console API and printing
//[of]:void	sys_console_read(array(char, buf))
void sys_console_read(array(char, buf))
{
	buf_count--;
	i64 pos = 0;
	bool reading = true;
	bool buffer_filled = false;
	while (reading) {
		WCHAR wchars[1024];
		DWORD n = 0;
		debug_assert(ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), &wchars, count_of(wchars), &n, null)); // console reading bug
		char chars[count_of(wchars) * 3];
		int str_size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wchars, n, chars, count_of(chars), null, null);
		reading = (wchars[n - 1] == L'\n') ? false : reading;
		debug_assert(str_size > 0); // unicode conversion bug
		const char* str = chars;
		while (str_size > 0) {
			i64 char_size = uchar_size(string_decode_uchar(str));
			debug_assert(char_size > 0); // unicode decoding bug
			i64 space_left = buf_count - pos;
			buffer_filled = (space_left < char_size) ? true : buffer_filled;
			i64 copy_size = buffer_filled ? 0 : char_size;
			mem_copy(buf + pos, str, copy_size);
			pos += copy_size;
			str += char_size;
			str_size -= char_size;
		}
	}
	pos -= ((pos > 0) & (buf[pos - 1] == '\n')) ? 1 : 0;
	pos -= ((pos > 0) & (buf[pos - 1] == '\r')) ? 1 : 0;
	buf[pos] = 0;
}
//[cf]
//[of]:[internal] void	_sys_console_write(HANDLE handle, const char* str)
//[c]NOTE: writes the string in chunks of valid characters, writes invalid bytes as fffd
void _sys_console_write(HANDLE handle, const char* str)
{
	char buf[mem_page_size];
	i64 buf_pos = 0;
	WCHAR wbuf[count_of(buf)];
	while (*str != 0) {
		i64 space_left = count_of(buf) - buf_pos;
		uchar uc = string_decode_uchar(str);
		i64 char_size = uchar_size(uc);
		str += clamp_bot(char_size, 1);
		if (char_size == 0) {
			char_size = 3;
			mem_copy(&uc, "\ufffd", 4);
		}
		if (space_left < char_size) {
			// oh nooo not enough space whatever will i dooo!
			// just flush whatever is there, actually.
			int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buf, buf_pos, wbuf, count_of(wbuf));
			debug_assert(n > 0); // unicode conversion bug
			WriteConsoleW(handle, wbuf, n, null, null);
			buf_pos = 0;
		}
		mem_copy(&buf[buf_pos], &uc, char_size);
		buf_pos += char_size;
	}
	if (buf_pos > 0) {
		int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buf, buf_pos, wbuf, count_of(wbuf));
		debug_assert(n > 0); // unicode conversion bug
		WriteConsoleW(handle, wbuf, n, null, null);
	}
}
//[cf]
//[of]:void	sys_console_write(const char* str)
void sys_console_write(const char* str)
{
	_sys_console_write(GetStdHandle(STD_OUTPUT_HANDLE), str);
}
//[cf]
//[of]:void	sys_error_write(const char* str)
void sys_error_write(const char* str)
{
	_sys_console_write(GetStdHandle(STD_ERROR_HANDLE), str);
}
//[cf]

//[of]:[internal] void	print_args(bool newline, const char* format, va_list* args)
void print_args(bool newline, const char* format, va_list* args)
{
	char flush_buf[mem_page_size] = {0};
	struct string_formatter f = (struct string_formatter){0};
	while (string_formatter_run(&f, format, args)) {
		if (f.value == null) {
			continue;
		}
		while (true) {
			i64 n = string_append(flush_buf, count_of(flush_buf), f.value);
			if (n == string_count(f.value)) {
				break;
			}
			sys_console_write(flush_buf);
			flush_buf[0] = 0;
			f.value += n;
		}
	}
	if (newline) {
		if (!string_append(flush_buf, count_of(flush_buf), "\n")) {
			sys_console_write(flush_buf);
			flush_buf[0] = '\n';
			flush_buf[1] = 0;
		}
	}
	if (flush_buf[0] != 0) {
		sys_console_write(flush_buf);
	}
}
//[cf]
//[of]:void	print(const char* format, ...)
void print(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	print_args(false, format, &args);
	va_end(args);
}
//[cf]
//[of]:void	println(const char* format, ...)
void println(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	print_args(true, format, &args);
	va_end(args);
}
//[cf]

//[c]Panic and assert
void	(*panic)(const char* msg);
//[of]:void	default_panic(const char* msg)
void default_panic(const char* msg)
{
	sys_error_write(msg);
	trap();
}
//[cf]
#define hope(c, f, ...)	_hope(c, __func__, f, ##__VA_ARGS__)
//[of]:void	_hope(bool cond, const char* proc_name, const char* format, ...)
void _hope(bool cond, const char* proc_name, const char* format, ...)
{
	if (cond) {
		return;
	}
	char buf[mem_page_size];
	va_list args;
	va_start(args, format);
	string_format_args(array_expand(buf), format, &args);
	va_end(args);
	panic(buf);
}
//[cf]
#define hopeless(f, ...)	hope(false, f, ##__VA_ARGS__)

//[c]Command line arguments
#define MAX_CMDLINE_LEN 32767
char args[MAX_CMDLINE_LEN * 3];
i64 args_count;
//[of]:void parse_command_line_args(void)
void parse_command_line_args(void)
{
	LPWSTR wargs = GetCommandLineW();
	i64 argc = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, cast(LPCWCH, wargs), -1, args, count_of(args), null, null);
	bool escaping = false;
	bool in_string = false;
	for (i64 i = 0; i < argc; i++) {
		if (escaping) {
			escaping = false;
			continue;
		}
		char c = args[i];
		switch (c) {
			case '"': {
				if (escaping) {
					escaping = false;
					continue;
				}
				in_string = !in_string;
				continue;
			} break;
			case '\\': {
				if (in_string & !escaping) {
					escaping = true;
					continue;
				}
			} break;
			case ' ': {
				if (in_string) {
					continue;
				}
				args[i] = 0;
				args_count++;
			} break;
		}
	}
	args_count++;
}
//[cf]

//[c]Indicates if the program was run from the console
bool is_cli;

void enter(void)
{
	panic = default_panic;
	DWORD proc_id;
	is_cli = (GetConsoleProcessList(&proc_id, 1) >= 2);
	parse_command_line_args();
	int status = start();
	exit(status);
}
