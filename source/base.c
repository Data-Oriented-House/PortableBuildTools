// Based system layer
//
// This is a copmletely CRT-less base layer, for the most minimal executable size, as well as total dependency control.
// I don't want my program to require the user have VCRedist of every possible version installed. And libc is bad anyway.

#define _CRT_DECLARE_NONSTDC_NAMES 0
#define NOMINMAX
#include <windows.h>

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "shell32")
#pragma comment(linker, "/subsystem:console")
#pragma comment(linker, "/entry:enter")

#pragma function(memset)
void* memset(void* dest, int ch, size_t count)
{
 __stosb(dest, ch, count);
 return (dest);
}

#pragma function(memcpy)
void* memcpy(void* dest, const void* src, size_t count)
{
 __movsb(dest, src, count);
 return (dest);
}

#pragma function(memcmp)
int memcmp(const void* lhs, const void* rhs, size_t count)
{
 const unsigned char* p1 = (const unsigned char*)lhs;
 const unsigned char* p2 = (const unsigned char*)rhs;
 for (size_t i = 0; i < count; ++i)
 {
  int d = p1[i] - p2[i];
  if (d != 0)
   return (d);
 }
 return (0);
}

// Types

typedef unsigned char n8;
typedef unsigned short n16;
typedef unsigned int n32;
typedef unsigned long long n64;
typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef n8 byte;
typedef n32 uchar;
typedef n64 nat;
typedef nat bool;
typedef float f32;
typedef double f64;
#define null ((void*)0)
#define false 0
#define true !false

// Limits

#define n8_min 0x00
#define n8_max 0xFF
#define i8_min 0x80
#define i8_max 0x7F
#define n16_min 0x0000
#define n16_max 0xFFFF
#define i16_min 0x8000
#define i16_max 0x7FFF
#define n32_min 0x00000000
#define n32_max 0xFFFFFFFF
#define i32_min 0x80000000
#define i32_max 0x7FFFFFFF
#define n64_min 0x0000000000000000ull
#define n64_max 0xFFFFFFFFFFFFFFFFull
#define i64_min 0x8000000000000000ull
#define i64_max 0x7FFFFFFFFFFFFFFFull

// Macros

#define trap \
 __debugbreak
#define cast(t, m) \
 ((t)(m))
#define autocast(m) \
 ((void*)(m))
#define bitcast(t, m) \
 *cast(t*, &(m))
#define countof(a) \
 (sizeof(a) / sizeof((a)[0]))
#define min(x, y) \
 ((x) < (y) ? (x) : (y))
#define max(x, y) \
 ((x) > (y) ? (x) : (y))
#define delta(x, y) \
 (max(x, y) - min(x, y))
#define abs(x) \
 ((x) < 0 ? -(x) : (x))
#define clamp_top(x, y) \
 min(x, y)
#define clamp_bot(x, y) \
 max(x, y)
#define clamp(x, low, high) \
 clamp_top(clamp_bot(x, low), high)
#define slice(type, name) \
 type* name, nat name##_count
#define string_slice(s) \
 s, countof(s) - 1
#define vcall(v, c, ...) \
 (v)->c(v, ##__VA_ARGS__)
#define vtcall(v, c, ...) \
 vcall(v, lpVtbl->c, ##__VA_ARGS__)
#define swap(a, b) \
 { \
  __typeof__(a) t__ = a; \
  a = b; \
  b = t__; \
 }
#define once(s) \
 { \
  static bool do_once = false; \
  if (!do_once) \
  { \
   do_once = true; \
   s; \
  } \
 }

// Memory helpers

#define mem_byte 1ull
#define mem_kilobyte (1024 * mem_byte)
#define mem_megabyte (1024 * mem_kilobyte)
#define mem_gigabyte (1024 * mem_megabyte)
#define mem_page_size (4 * mem_kilobyte)

#define is_power_of_two(x) \
 (((x) > 0) && (((x) & ((x) - 1)) <= 0))
#define mem_align_forward(addr, alignment) \
 ( \
  is_power_of_two(alignment) \
  ? ((addr) + ((alignment) - 1)) & ~((alignment) - 1) \
  : 0 \
 )

// Time

#define time_nanosecond 1ull
#define time_microsecond (1000 * time_nanosecond)
#define time_millisecond (1000 * time_microsecond)
#define time_second (1000 * time_millisecond)
#define time_minute (60 * time_second)
#define time_hour (60 * time_minute)

void time_sleep(nat duration)
{
 Sleep(clamp_bot(duration / time_millisecond, 1));
}

nat time_tick(void)
{
 static LARGE_INTEGER f;
 if (f.QuadPart == 0)
  QueryPerformanceFrequency(&f);
 LARGE_INTEGER t;
 QueryPerformanceCounter(&t);
 nat q = t.QuadPart / f.QuadPart;
 nat r = t.QuadPart % f.QuadPart;
 nat out = q * time_second + r * time_second / f.QuadPart;
 return (out);
}

// Unicode and strings
//
// Decoding is completely branchless at -O2 on Clang and MSVC, but not GCC
// This is clearly a skill issue by GCC developers

nat uchar_size_naive(uchar uc)
{
 byte b = bitcast(byte, uc);
 nat size = 0;
 size = ((b & 0b11111000) == 0b11110000) ? 4 : size;
 size = ((b & 0b11110000) == 0b11100000) ? 3 : size;
 size = ((b & 0b11100000) == 0b11000000) ? 2 : size;
 size = ((b & 0b10000000) == 0b00000000) ? 1 : size;
 size = (b == 0) ? 0 : size;
 return (size);
}

nat uchar_size(uchar uc)
{
 byte* bytes = cast(byte*, &uc);
 nat size = uchar_size_naive(uc);
 size = ((size == 2) & ((bytes[1] & 0b11000000) != 0b10000000)) ? 0 : size;
 size = ((size == 3) & ((bytes[1] & 0b11000000) != 0b10000000)) ? 0 : size;
 size = ((size == 3) & ((bytes[2] & 0b11000000) != 0b10000000)) ? 0 : size;
 size = ((size == 4) & ((bytes[1] & 0b11000000) != 0b10000000)) ? 0 : size;
 size = ((size == 4) & ((bytes[2] & 0b11000000) != 0b10000000)) ? 0 : size;
 size = ((size == 4) & ((bytes[3] & 0b11000000) != 0b10000000)) ? 0 : size;
 return (size);
}

uchar string_decode_uchar(const char* s)
{
 uchar uc;
 char c;
 char* bytes = cast(char*, &uc);
 nat size = uchar_size_naive(*s);
 c = s[0];
 c = (size < 1) ? 0 : c;
 bytes[0] = c;
 s += c ? 1 : 0;
 c = s[0];
 c = (size < 2) ? 0 : c;
 bytes[1] = c;
 s += c ? 1 : 0;
 c = s[0];
 c = (size < 3) ? 0 : c;
 bytes[2] = c;
 s += c ? 1 : 0;
 c = s[0];
 c = (size < 4) ? 0 : c;
 bytes[3] = c;
 return (uc);
}

bool string_is(const char* s, const char* x)
{
 bool match = true;
 while (true)
 {
  match = (*s != *x) ? false : match;
  if ((*s == 0) | (*x == 0))
   break;
  s++;
  x++;
 }
 return (match);
}

nat string_count(const char* s)
{
 nat count = 0;
 while (s[count] != 0)
  count++;
 return (count);
}

#define string_copy(b, s) \
 _string_copy(b, countof(b), s)
nat _string_copy(slice(char, buf), const char* s)
{
 nat count = 0;
 while (*s != 0)
 {
  nat size = uchar_size(string_decode_uchar(s));
  size = clamp_bot(size, 1);
  if (buf_count - count < size + 1)
   break;
  memcpy(&buf[count], s, size);
  s += size;
  count += size;
 }
 buf[count] = 0;
 return (count);
}

#define string_append(b, s) \
 _string_append(b, countof(b), s)
nat _string_append(slice(char, buf), const char* s)
{
 nat prev_count = string_count(buf);
 nat count = prev_count;
 while (*s != 0)
 {
  nat size = uchar_size(string_decode_uchar(s));
  size = clamp_bot(size, 1);
  if (buf_count - count < size + 1)
   break;
  memcpy(&buf[count], s, size);
  s += size;
  count += size;
 }
 buf[count] = 0;
 return (count - prev_count);
}

#define string_format(b, ...) \
 _string_format(b, countof(b), __VA_ARGS__)
#define _string_format(b, c, ...) \
 __string_format(b, c, (const char*[]){__VA_ARGS__}, sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*))
char* __string_format(slice(char, buf), slice(const char*, strings))
{
 buf[0] = 0;
 for (nat i = 0; i < strings_count; i++)
 {
  const char* s = strings[i];
  nat n = _string_append(buf, buf_count, s);
  if (n < string_count(s))
   break;
 }
 return (&buf[0]);
}

// String conversion

char* char_to_string(char (*buf)[2], char c)
{
 char* bytes = *buf;
 bytes[0] = c;
 bytes[1] = 0;
 return (&bytes[0]);
}

char* uchar_to_string(char (*buf)[5], uchar uc)
{
 char* bytes = *buf;
 nat size = uchar_size(uc);
 memcpy(bytes, &uc, size);
 bytes[size] = 0;
 return (&bytes[0]);
}

char* integer_to_string(char (*buf)[66], n64 x, nat base, bool is_signed, nat leading_zeroes)
{
 static const char* digits = "0123456789abcdefghijklmnopqrstuvwxyz";
 base = clamp_top(base, 36);
 leading_zeroes = clamp(leading_zeroes, 1, 64);
 bool is_neg = (is_signed & (cast(i64, x) < 0));
 x = is_neg ? -(cast(i64, x)) : x;
 char* numbers = *buf;
 nat pos = 65;
 do
 {
  pos--;
  n64 remainder = x % base;
  numbers[pos] = digits[remainder];
  x /= base;
 } while (x > 0);
 nat zero_pos = 65 - leading_zeroes;
 nat zero_size = (pos > zero_pos) ? (pos - zero_pos) : 0;
 memset(&numbers[zero_pos], '0', zero_size);
 pos -= zero_size;
 pos--;
 numbers[pos] = '-';
 pos += is_neg ? 0 : 1;
 numbers[65] = 0;
 return (&numbers[pos]);
}

// TODO: float_to_string

#define ctos(x) \
 char_to_string(&(char[2]){0}, x)
#define utos(x) \
 uchar_to_string(&(char[5]){0}, x)
#define btos(x, b) \
 integer_to_string(&(char[66]){0}, x, 2, false, b)
#define htos(x) \
 integer_to_string(&(char[66]){0}, x, 16, false, 0)
#define ntos(x) \
 integer_to_string(&(char[66]){0}, x, 10, false, 0)
#define itos(x) \
 integer_to_string(&(char[66]){0}, x, 10, true, 0)

// Console

#define MAX_CMDLINE_LEN_UTF8 (32767 * 3)

bool is_cli;
nat args_count;
const char* args[MAX_CMDLINE_LEN_UTF8];

void _console_parse_arguments(void)
{
 static char args_global[MAX_CMDLINE_LEN_UTF8];
 nat args_position = 0;
 int wargs_count = 0;
 LPWSTR* wargs = CommandLineToArgvW(GetCommandLineW(), &wargs_count);
 for (int i = 0; i < wargs_count; i++)
 {
  char arg[MAX_CMDLINE_LEN_UTF8];
  int arg_count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargs[i], -1, arg, countof(arg), null, null);
  memcpy(&args_global[args_position], arg, arg_count);
  args[args_count++] = &args_global[args_position];
  args_position += arg_count;
 }
 LocalFree(wargs);
}

void console_toggle(bool show)
{
 if (is_cli)
  return;
 HWND console = GetConsoleWindow();
 ShowWindow(console, show ? SW_SHOW : SW_HIDE);
}

#define console_read(b) \
 _console_read(b, countof(b))
nat _console_read(slice(char, buf))
{
 nat count = 0;
 WCHAR codepoint[2] = {0};
 while (true) {
  WCHAR wchar;
  ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), &wchar, 1, &(DWORD){0}, null);
  if (wchar == L'\n') // end
   break;
  if ((wchar >= 0xd800) & (wchar <= 0xdbff))
  {
   codepoint[0] = wchar;
   continue;
  }
  bool surrogate = (codepoint[0] != 0);
  nat codepoint_size = surrogate ? 2 : 1;
  codepoint[0] = surrogate ? codepoint[0] : wchar;
  codepoint[1] = surrogate ? wchar : codepoint[1];
  uchar uc = 0;
  int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, codepoint, codepoint_size, cast(LPSTR, &uc), sizeof(uc), null, null);
  if (buf_count - count < size + 1)
   break;
  memcpy(&buf[count], &uc, size);
  count += size;
  codepoint[0] = 0;
 }
 count -= ((count > 0) & (buf[count - 1] == '\r')) ? 1 : 0;
 buf[count] = 0;
 return (count);
}

void console_write(const char* str)
{
 WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), str, string_count(str), null, null);
}

void console_write_error(const char* str)
{
 WriteFile(GetStdHandle(STD_ERROR_HANDLE), str, string_count(str), null, null);
}

#define echo(...) \
 _echo((const char*[]){__VA_ARGS__}, sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*))
#define echoln(...) \
 _echo((const char*[]){__VA_ARGS__, "\n"}, sizeof((const char*[]){__VA_ARGS__, "\n"}) / sizeof(const char*))
void _echo(slice(const char*, strings))
{
 char flush_buf[mem_page_size] = {0};
 for (nat i = 0; i < strings_count; i++)
 {
  const char* s = strings[i];
  while (true)
  {
   nat n = string_append(flush_buf, s);
   if (n == string_count(s))
    break;
   console_write(flush_buf);
   flush_buf[0] = 0;
   s += n;
  }
 }
 if (flush_buf[0] != 0)
  console_write(flush_buf);
}

// Assertion

#define hopeless(...) \
 hope(false, ##__VA_ARGS__)
#define hope(c, ...) \
 _hope(c, __func__, (const char*[]){__VA_ARGS__}, sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*))
void _hope(bool cond, const char* proc_name, slice(const char*, strings))
{
 if (cond)
  return;
 console_toggle(true);
 char flush_buf[mem_page_size] = {0};
 for (nat i = 0; i < strings_count; i++)
 {
  const char* s = strings[i];
  while (true)
  {
   nat n = string_append(flush_buf, s);
   if (n == string_count(s))
    break;
   console_write_error(flush_buf);
   flush_buf[0] = 0;
   s += n;
  }
 }
 if (flush_buf[0] != 0)
  console_write_error(flush_buf);
 trap();
}

// Embed

struct embed
{
 const void* data;
 nat size;
};

struct embed embed_load(char* name)
{
 WCHAR wname[256] = {0};
 MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wname, countof(wname) - 1);
 HRSRC resource = FindResourceW(null, wname, cast(LPCWSTR, RT_RCDATA));
 struct embed embed = {0};
 embed.data = LoadResource(null, resource);
 embed.size = SizeofResource(null, resource);
 return (embed);
}

// Entry point

void start(void);

void exit(int status)
{
 ExitProcess(status);
}

void enter(void)
{
 is_cli = (GetConsoleProcessList(&(DWORD){}, 1) >= 2);
 _console_parse_arguments();
 SetConsoleOutputCP(65001);
 start();
 exit(0);
}
