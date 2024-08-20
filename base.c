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
typedef n32	char8;
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
//[of]:i64	char8_size_naive(char8 c8)
i64 char8_size_naive(char8 c8)
{
	static const char sizes[32] = {
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		0, 0, 0, 0, 0, 0, 0, 0,
		2, 2, 2, 2, 3, 3, 4, 0
	};
	unsigned char c = *cast(unsigned char*, &c8);
	return (sizes[c >> 3]);
}
//[cf]
//[of]:i64	char8_size(char8 c8)
i64 char8_size(char8 c8)
{
	i64 size = char8_size_naive(c8);
	bool valid = false;
	valid = (c8 <= 0x7f) ? true : valid;
	bool is_2byte = ((c8 & 0xc0e0) == 0x80c0);
	bool is_3byte = ((c8 & 0xc0c0f0) == 0x8080e0);
	bool is_4byte = ((c8 & 0xc0c0c0f8) == 0x808080f0);
	valid = ((0x80c2 <= c8) & (c8 <= 0xbfdf)) ? is_2byte : valid;
//[c]	surrogate half
	valid = ((0xeda080 <= c8) & (c8 <= 0xedbfbf)) ? false : valid;
	valid = ((0x80a0e0 <= c8) & (c8 <= 0xbfbfef)) ? is_3byte : valid;
	valid = ((0x808090f0 <= c8) & (c8 <= 0xbfbf8ff4)) ? is_4byte : valid;
	size = valid ? size : 0;
	return (size);
}
//[cf]
//[of]:char8	string_decode_char8(const char* s)
char8 string_decode_char8(const char* s)
{
	char8 c8;
	char* bytes = cast(char*, &c8);
	i64 size = char8_size_naive(*s);
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
	return (c8);
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
//[c]TODO: string_from_float
//[of]:[internal] char*	string_from_integer(char (*buf)[66], n64 x, n64 base, bool is_signed, n64 leading_zeroes)
char* string_from_integer(char (*buf)[66], n64 x, n64 base, bool is_signed, n64 leading_zeroes)
{
	static const char* digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	debug_assert(base <= 36);
	bool is_neg = (is_signed && (cast(i64, x) < 0));
	x = is_neg ? -(cast(i64, x)) : x;
	i64 pos = 65;
	(*buf)[pos] = 0;
	do {
		pos--;
		n64 remainder = x % base;
		(*buf)[pos] = digits[remainder];
		x /= base;
	} while (x > 0);
	i64 zero_pos = 65 - leading_zeroes;
	i64 zero_size = max(pos - zero_pos, 0);
	mem_set(&buf[zero_pos], zero_size, '0');
	pos -= zero_size;
	pos--;
	(*buf)[pos] = '-';
	pos += is_neg ? 0 : 1;
	return (*buf + pos);
}
//[cf]

//[c]Basic console API
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
			i64 char_size = char8_size(string_decode_char8(str));
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
void _sys_console_write(HANDLE handle, const char* str)
{
	WCHAR wstr[1024];
	i64 max_codepoints = count_of(wstr) / 2;
	while (*str != 0) {
		const char* chunk = str;
		i64 chunk_size = 0;
		i64 codepoints = 0;
		while ((*str != 0) & (codepoints <= max_codepoints)) {
			i64 char_size = char8_size(string_decode_char8(str));
			debug_assert(char_size > 0); // unicode decoding bug
			chunk_size += char_size;
			str += char_size;
			codepoints++;
		}
		int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, chunk, chunk_size, wstr, count_of(wstr));
		debug_assert(n > 0); // unicode conversion bug
		WriteConsoleW(handle, wstr, n, null, null);
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

//[c]Write context used for formatting and printing functions
struct write_context {
	char*	buf;
	i64	buf_count;
	i64	pos;
};

//[of]:[internal] void	string_format_args(struct write_context* wcontext, string_format_write_proc w, const char* format, va_list args)
//[c]String formatting using an argument list into views specified as {view}, {{this}} escapes the braces
//[c]Views:
//[c]b8/16/32/64+ - binary view with optional leading zeroes
//[c]x8/16/32/64+ - hexadecimal view with optional leading zeroes
//[c]n8/16/32/64 - natural numbers
//[c]i8/16/32/64 - integer numbers
//[c]TODO: f32/64.r - floating-point numbers with optional rounding
//[c]c - 1-byte character
//[c]u - 4-byte UTF-8 encoded character (char8)
//[c]s - UTF-8 encoded string
//[c]
//[c]Examples:
//[c]"hello {{world}}"
//[c]"{i32} = 0x{x32} = 0b{b32+}", 37, 37, 37
//[c]"{s} {u}{u}{c}", "hello", string_decode_char8("世"), string_decode_char8("界"), '!'
//[c]TODO
//[c]"{f32.2}", 0.9876
typedef void (*string_format_write_proc)(struct write_context*, const char*);
enum string_format_type {
	string_format_type_none	= 0,
	string_format_type_bin	= 1 << 0,
	string_format_type_hex	= 1 << 1,
	string_format_type_nat	= 1 << 2,
	string_format_type_int	= 1 << 3,
	string_format_type_char	= 1 << 4,
	string_format_type_uchar	= 1 << 5,
	string_format_type_str	= 1 << 6,
};
void string_format_args(struct write_context* wcontext, string_format_write_proc w, const char* format, va_list args)
{
	bool args_invalid = false;
	bool parsing_view = false;
	const char* view = null;
	while (*format != 0) {
		const char* chunk = format;
		char8 c8 = string_decode_char8(format);
		format += clamp_bot(char8_size(c8), 1);
		char symbol = chunk[0];
		if (!parsing_view) {
			if (symbol != '{') {
				char* bytes = cast(char*, &c8);
				char buf[5] = {bytes[0], bytes[1], bytes[2], bytes[3], 0};
				w(wcontext, buf);
				continue;
			}
			parsing_view = true;
			view = chunk;
			continue;
		}
		if (string_is(view, "{") && (symbol == '{')) {
			w(wcontext, "{");
			parsing_view = false;
			continue;
		}
		if (symbol != '}') {
			continue;
		}
		if (args_invalid) {
			w(wcontext, "?");
			parsing_view = false;
			continue;
		}
		view++;
//[c]		TODO: floating-point numbers, split at the dot to parse rounding
		enum string_format_type type_flag = string_format_type_none;
		type_flag |= (view[0] == 'b') ? string_format_type_bin : 0;
		type_flag |= (view[0] == 'x') ? string_format_type_hex : 0;
		type_flag |= (view[0] == 'n') ? string_format_type_nat : 0;
		type_flag |= (view[0] == 'i') ? string_format_type_int : 0;
		type_flag |= (view[0] == 'c') ? string_format_type_char : 0;
		type_flag |= (view[0] == 'u') ? string_format_type_uchar : 0;
		type_flag |= (view[0] == 's') ? string_format_type_str : 0;
		view += (type_flag != 0) ? 1 : 0;
		n64 type_size = 0;
		type_size = (view[0] == '8') ? 8 : type_size;
		type_size = ((view[0] == '1') & (view[1] == '6')) ? 16 : type_size;
		type_size = ((view[0] == '3') & (view[1] == '2')) ? 32 : type_size;
		type_size = ((view[0] == '6') & (view[1] == '4')) ? 64 : type_size;
		view += (type_size > 0) ? 1 : 0;
		view += (type_size > 8) ? 1 : 0;
		n64 zero_padding = 0;
		zero_padding = (view[0] == '+') ? type_size : zero_padding;
		view += (zero_padding != 0) ? 1 : 0;
		zero_padding = (type_flag & string_format_type_hex) ? (zero_padding / 4) : zero_padding;
		zero_padding = (type_flag & (string_format_type_bin | string_format_type_hex)) ? zero_padding : 0;
		n64 base = 10;
		base = (type_flag & string_format_type_bin) ? 2 : base;
		base = (type_flag & string_format_type_hex) ? 16 : base;
		enum string_format_type nat_mask = (string_format_type_bin | string_format_type_hex | string_format_type_nat);
		if ((type_flag & nat_mask) && (type_size == 64)) {
			n64 arg = va_arg(args, n64);
			char buf[66];
			w(wcontext, string_from_integer(&buf, arg, base, false, zero_padding));
		} else if ((type_flag & nat_mask) && (type_size != 0)) {
			n32 arg = va_arg(args, n32);
			arg &= (type_size == 8) ? 0x000000ff : 0xffffffff;
			arg &= (type_size == 16) ? 0x0000ffff : 0xffffffff;
			char buf[66];
			w(wcontext, string_from_integer(&buf, arg, base, false, zero_padding));
		} else if ((type_flag & string_format_type_int) && (type_size == 64)) {
			i64 arg = va_arg(args, i64);
			char buf[66];
			w(wcontext, string_from_integer(&buf, arg, 10, true, 0));
		} else if ((type_flag & string_format_type_int) && (type_size != 0)) {
			i32 arg = va_arg(args, i32);
			arg &= (type_size == 8) ? 0x000000ff : 0xffffffff;
			arg &= (type_size == 16) ? 0x0000ffff : 0xffffffff;
			char buf[66];
			w(wcontext, string_from_integer(&buf, arg, 10, true, 0));
		} else if ((type_flag & string_format_type_char) && (type_size == 0)) {
			char arg = cast(char, va_arg(args, n32));
			char buf[2] = {arg, 0};
			w(wcontext, buf);
		} else if ((type_flag & string_format_type_uchar) && (type_size == 0)) {
			char8 arg = va_arg(args, char8);
			char* bytes = cast(char*, &arg);
			char buf[5] = {bytes[0], bytes[1], bytes[2], bytes[3], 0};
			w(wcontext, buf);
		} else if ((type_flag & string_format_type_str) && (type_size == 0)) {
			const char* arg = va_arg(args, const char*);
			w(wcontext, arg);
		} else {
			w(wcontext, "?");
			args_invalid = true;
		}
		parsing_view = false;
	}
	if (parsing_view) {
		w(wcontext, "?");
	}
}
//[cf]
//[of]:[internal] void	string_format_write(const char* s)
void string_format_write(struct write_context* wcontext, const char* s)
{
	while (*s != 0) {
		i64 char_size = char8_size(string_decode_char8(s));
		char_size = clamp_bot(char_size, 1);
		i64 space_left = wcontext->buf_count - wcontext->pos;
		wcontext->pos = (space_left < char_size) ? -1 : wcontext->pos;
		i64 copy_size = (wcontext->pos == -1) ? 0 : char_size;
		mem_copy(wcontext->buf + clamp_bot(wcontext->pos, 0), s, copy_size);
		wcontext->pos += copy_size;
		s += char_size;
	}
}
//[cf]
//[of]:void	string_format(array(char, buf), const char* format, ...)
void string_format(array(char, buf), const char* format, ...)
{
	struct write_context wcontext = (struct write_context){0};
	wcontext.buf = buf;
	wcontext.buf_count = buf_count - 1;
	wcontext.pos = 0;
	va_list args;
	va_start(args, format);
	string_format_args(&wcontext, string_format_write, format, args);
	buf[wcontext.pos] = 0;
	va_end(args);
}
//[cf]

//[of]:[internal] void	print_flush()
//[c]Flush the buffer in chunks of valid characters, write invalid bytes as fffd
void print_flush(struct write_context* wcontext)
{
	char* writable_buf = wcontext->buf;
	writable_buf[wcontext->pos] = 0;
	wcontext->pos = 0;
	const char* flush_buf = null;
	while (*writable_buf != 0) {
		i64 char_size = char8_size(string_decode_char8(writable_buf));
		flush_buf = (flush_buf == null) ? writable_buf : flush_buf;
		*writable_buf = (char_size > 0) ? *writable_buf : 0;
		writable_buf += (char_size > 0) ? char_size : 1;
		if (char_size <= 0) {
			sys_console_write(flush_buf);
			sys_console_write(u8"\ufffd");
//[c]			For debugging
			//sys_console_write("-------------------------------------------");
			flush_buf = null;
		}
	}
	sys_console_write(flush_buf);
}
//[cf]
//[of]:[internal] void	print_write(const char* s)
void print_write(struct write_context* wcontext, const char* s)
{
	while (*s != 0) {
		i64 char_size = char8_size(string_decode_char8(s));
		char_size = clamp_bot(char_size, 1);
		i64 space_left = wcontext->buf_count - wcontext->pos;
		if (space_left < char_size) {
			print_flush(wcontext);
			space_left = wcontext->buf_count - wcontext->pos;
			debug_assert(space_left >= char_size); // buffer is too small
		}
		mem_copy(wcontext->buf + wcontext->pos, s, char_size);
		wcontext->pos += char_size;
		s += char_size;
	}
}
//[cf]
//[of]:void	print(const char* format, ...)
void print(const char* format, ...)
{
	struct write_context wcontext = (struct write_context){0};
	char buf[mem_page_size];
	wcontext.buf = buf;
	wcontext.buf_count = count_of(buf) - 1;
	wcontext.pos = 0;
	va_list args;
	va_start(args, format);
	string_format_args(&wcontext, print_write, format, args);
	va_end(args);
	print_flush(&wcontext);
}
//[cf]
//[of]:void	println(const char* format, ...)
void println(const char* format, ...)
{
	struct write_context wcontext = (struct write_context){0};
	char buf[mem_page_size];
	wcontext.buf = buf;
	wcontext.buf_count = count_of(buf) - 1;
	wcontext.pos = 0;
	va_list args;
	va_start(args, format);
	string_format_args(&wcontext, print_write, format, args);
	va_end(args);
	print_write(&wcontext, "\n");
	print_flush(&wcontext);
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
	struct write_context wcontext = (struct write_context){0};
	char buf[mem_page_size];
	wcontext.buf = buf;
	wcontext.buf_count = count_of(buf) - 1;
	wcontext.pos = 0;
	va_list args;
	va_start(args, format);
	string_format_args(&wcontext, string_format_write, format, args);
	buf[wcontext.pos] = 0;
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
	i64 args_count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, cast(LPCWCH, wargs), -1, args, count_of(args), null, null);
	bool escaping = false;
	bool in_string = false;
	for (i64 i = 0; i < args_count; i++) {
		if (escaping) {
			escaping = false;
			continue;
		}
		char symbol = args[i];
		switch (symbol) {
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
