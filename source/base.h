// libc-free base layer

#if !defined(appid)
 #define appid "0"
#endif

#define _CRT_DECLARE_NONSTDC_NAMES 0
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <stdbool.h>

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "shell32")

#pragma comment(linker, "/subsystem:console")
#pragma comment(linker, "/entry:_start")
#pragma comment(linker, "/manifestdependency:\"" \
 "type='win32' " \
 "name='Microsoft.Windows.Common-Controls' " \
 "version='6.0.0.0' " \
 "processorArchitecture='*' " \
 "publicKeyToken='6595b64144ccf1df' " \
 "language='*'\"")

_Noreturn void exit(int exit_code)
{
 ExitProcess(exit_code);
}

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

#pragma function(strlen)
size_t strlen(const char* str)
{
 size_t count = 0;
 while (str[count] != 0)
  count++;
 return (count);
}

#pragma function(strcmp)
int strcmp(const char* lhs, const char* rhs)
{
 while (*lhs == *rhs)
 {
  if ((*lhs == 0) | (*rhs == 0))
   break;
  lhs++;
  rhs++;
 }
 int diff = *(unsigned char*)lhs - *(unsigned char*)rhs;
 return (diff);
}

#pragma region types

typedef unsigned char n8;
typedef unsigned short n16;
typedef unsigned int n32;
typedef unsigned long long n64;
typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef n8 b8;
typedef n16 b16;
typedef n32 b32;
typedef n64 b64;
typedef n8 byte;
typedef n32 uchar;
typedef n64 nat;

#pragma endregion

#pragma region constants

const n8 n8_min = 0x00;
const n8 n8_max = 0xFF;
const n16 n16_min = 0x0000;
const n16 n16_max = 0xFFFF;
const n32 n32_min = 0x00000000;
const n32 n32_max = 0xFFFFFFFF;
const n64 n64_min = 0x0000000000000000;
const n64 n64_max = 0xFFFFFFFFFFFFFFFF;
const i8 i8_min = 0x80;
const i8 i8_max = 0x7F;
const i16 i16_min = 0x8000;
const i16 i16_max = 0x7FFF;
const i32 i32_min = 0x80000000;
const i32 i32_max = 0x7FFFFFFF;
const i64 i64_min = 0x8000000000000000;
const i64 i64_max = 0x7FFFFFFFFFFFFFFF;

enum
{
 mem_byte = 1ull,
 mem_kilobyte = 1024 * mem_byte,
 mem_megabyte = 1024 * mem_kilobyte,
 mem_gigabyte = 1024 * mem_megabyte,
 mem_page_size = 4 * mem_kilobyte
};

#pragma endregion

#pragma region macros

#define null \
 ((void*)0)
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
#define swap(a, b) \
 do { \
  __typeof__(a) t__ = a; \
  a = b; \
  b = t__; \
 } while (0)
#define is_power_of_two(x) \
 (((x) > 0) && (((x) & ((x) - 1)) <= 0))
#define mem_align_forward(addr, alignment) \
 ( \
  is_power_of_two(alignment) \
  ? ((addr) + ((alignment) - 1)) & ~((alignment) - 1) \
  : 0 \
 )
#define vcall(v, c, ...) \
 (v)->c(v, ##__VA_ARGS__)
#define vtcall(v, c, ...) \
 vcall(v, lpVtbl->c, ##__VA_ARGS__)

#pragma endregion

#pragma region strings and unicode
// decoding is completely branchless on clang and msvc with optimizations enabled, but not gcc; this is clearly a skill issue by gcc developers

struct string
{
 char* mem;
 nat count;
};

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
 byte* bytes = autocast(&uc);
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
 char c;
 char uc[4];
 nat size = uchar_size_naive(s[0]);
 c = s[0];
 uc[0] = (size < 1) ? 0 : c;
 s += c ? 1 : 0;
 c = s[0];
 uc[1] = (size < 2) ? 0 : c;
 s += c ? 1 : 0;
 c = s[0];
 uc[2] = (size < 3) ? 0 : c;
 s += c ? 1 : 0;
 c = s[0];
 uc[3] = (size < 4) ? 0 : c;
 return (bitcast(uchar, uc));
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
 nat prev_count = strlen(buf);
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

#define string_join_buf(b, ...) \
 _string_join(b, countof(b), ##__VA_ARGS__)
#define string_join(n, ...) \
 _string_join((char[n]){0}, n, ##__VA_ARGS__)
#define _string_join(b, c, ...) \
 __string_join(b, c, ##__VA_ARGS__, null)
char* __string_join(slice(char, buf), ...)
{
 va_list strings;
 va_start(strings, buf_count);
 buf[0] = 0;
 while (true)
 {
  const char* s = va_arg(strings, const char*);
  if (s == null)
   break;
  nat n = _string_append(buf, buf_count, s);
  if (n < strlen(s))
   break;
 }
 return (&buf[0]);
}

bool string_is(const char* s, const char* x)
{
 bool match = (strcmp(s, x) == 0);
 return (match);
}

bool string_starts_with(const char* s, const char* x)
{
 bool match = true;
 while (*x != 0)
 {
  match = (*s != *x) ? false : match;
  if (*s == 0)
   break;
  s++;
  x++;
 }
 return (match);
}

bool string_ends_with(const char* s, const char* x)
{
 nat s_count = strlen(s);
 nat x_count = strlen(x);
 nat diff = (s_count > x_count) ? (s_count - x_count) : 0;
 return string_starts_with(s + diff, x);
}

bool string_trim_left(char* s, const char* x)
{
 nat s_count = strlen(s);
 nat x_count = strlen(x);
 bool trim = string_starts_with(s, x);
 nat copy_size = trim ? (s_count - x_count) + 1 : 0;
 memcpy(s, &s[x_count], copy_size);
 return (trim);
}

bool string_trim_right(char* s, const char* x)
{
 nat s_count = strlen(s);
 nat x_count = strlen(x);
 bool trim = string_ends_with(s, x);
 nat index = trim ? (s_count - x_count) : s_count;
 s[index] = 0;
 return (trim);
}

#pragma endregion

#pragma region string conversion

#define ctos(x) \
 char_to_string(&(char[2]){0}, x)
#define utos(x) \
 uchar_to_string(&(char[5]){0}, x)
#define btos(x, z) \
 integer_to_string(&(char[66]){0}, x, 2, false, z)
#define htos(x, z) \
 integer_to_string(&(char[66]){0}, x, 16, false, z)
#define ntos(x) \
 integer_to_string(&(char[66]){0}, x, 10, false, 0)
#define itos(x) \
 integer_to_string(&(char[66]){0}, x, 10, true, 0)
#define ftos(x, p) \
 float_to_string(&(char[25]){0}, x, p)

char* char_to_string(char (*buf)[2], char c)
{
 char* chars = *buf;
 chars[0] = c;
 chars[1] = 0;
 return (&chars[0]);
}

char* uchar_to_string(char (*buf)[5], uchar uc)
{
 char* chars = *buf;
 nat size = uchar_size(uc);
 memcpy(chars, &uc, size);
 chars[size] = 0;
 return (&chars[0]);
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

char* float_to_string(char (*buf)[25], double x, nat precision)
{
 precision = clamp_top(precision, 6);
 i64 whole = x;
 double frac = delta(x, cast(double, whole));
 for (nat i = 0; i < precision; i++)
  frac *= 10.0;
 frac = (precision == 0) ? 0 : frac;
 string_join_buf(*buf, itos(whole), ".", ntos(frac));
 return (*buf);
}

#pragma endregion

#pragma region console

#define console_read(b) \
 _console_read(b, countof(b))
nat _console_read(slice(char, buf))
{
 HANDLE stdin = GetStdHandle(STD_INPUT_HANDLE);
 nat count = 0;
 WCHAR codepoint[2] = {0};
 while (true)
 {
  WCHAR wchar;
  ReadConsole(stdin, &wchar, 1, &(DWORD){0}, null);
  if (wchar == L'\n')
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
  int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, codepoint, codepoint_size, autocast(&uc), sizeof(uc), null, null);
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

void console_write(const char* s)
{
 HANDLE stdout = GetStdHandle(STD_OUTPUT_HANDLE);
 WCHAR ws[mem_page_size / sizeof(WCHAR)]; // 1 mem_page of data
 nat pos = 0;
 nat flush_start = 0;
 nat codepoints = 0;
 const nat codepoints_max = countof(ws) / 2; // 1 codepoint is 2 wchars max
 while (s[pos] != 0)
 {
  uchar uc = string_decode_uchar(&s[pos]);
  nat uc_size = uchar_size(uc);
  bool invalid = (uc_size == 0);
  if (!invalid)
  {
   pos += uc_size;
   codepoints++;
  }
  if (invalid | (codepoints == (countof(ws) / 2)))
  {
   codepoints = 0;
   nat ws_count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, &s[flush_start], (pos - flush_start), ws, countof(ws));
   WriteConsole(stdout, ws, ws_count, &(DWORD){0}, null);
   flush_start = pos;
   if (invalid)
   {
    WriteConsole(stdout, &(WCHAR){0xFFFD}, 1, &(DWORD){0}, null);
    pos++;
    flush_start++;
   }
  }
 }
 nat ws_count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, &s[flush_start], (pos - flush_start), ws, countof(ws));
 WriteConsole(stdout, ws, ws_count, &(DWORD){0}, null);
}

#define echo(...) \
 _echo(null, ##__VA_ARGS__, null)
#define echoln(...) \
 _echo(null, ##__VA_ARGS__, "\n", null)
void _echo(void* dud, ...)
{
 va_list strings;
 va_start(strings, dud);
 while (true)
 {
  const char* s = va_arg(strings, const char*);
  if (s == null)
   break;
  console_write(s);
 }
}

#pragma endregion

#pragma region files

struct file
{
 void*	handle;
 n64	size;
 n64	pos;
};

bool file_exists(const char* path)
{
 WCHAR wpath[MAX_PATH];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (false);
 DWORD attr = GetFileAttributes(wpath);
 bool exists = (attr != INVALID_FILE_ATTRIBUTES);
 bool is_file = (attr & FILE_ATTRIBUTE_DIRECTORY) ? false : true;
 return (exists & is_file);
}

struct file file_open(const char* path, bool write_access)
{
 struct file file = {0};
 WCHAR wpath[MAX_PATH];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (file);
 DWORD access = write_access ? FILE_GENERIC_WRITE : FILE_GENERIC_READ;
 SECURITY_ATTRIBUTES sa = (SECURITY_ATTRIBUTES){0};
 sa.nLength = sizeof(SECURITY_ATTRIBUTES);
 sa.bInheritHandle = true;
 DWORD attrs = write_access ? FILE_ATTRIBUTE_NORMAL : FILE_ATTRIBUTE_READONLY;
 file.handle = CreateFile(wpath, access, FILE_SHARE_READ, &sa, OPEN_EXISTING, attrs, null);
 if (file.handle == INVALID_HANDLE_VALUE)
 {
  file.handle = null;
  return (file);
 }
 GetFileSizeEx(file.handle, autocast(&file.size));
 file.pos = write_access ? file.size : 0;
 SetFilePointerEx(file.handle, bitcast(LARGE_INTEGER, file.pos), null, FILE_BEGIN);
 return (file);
}

void file_close(struct file* file)
{
 CloseHandle(file->handle);
 memset(file, 0, sizeof(struct file));
}

n32 file_read(struct file* file, byte* buf, n32 buf_count)
{
 DWORD n = 0;
 ReadFile(file->handle, buf, buf_count, &n, null);
 file->pos += n;
 return (n);
}

n32 file_write(struct file* file, const byte* data, n32 data_count)
{
 DWORD n = 0;
 WriteFile(file->handle, data, data_count, &n, null);
 file->pos += n;
 return (n);
}

void file_seek(struct file* file, n64 pos)
{
 SetFilePointerEx(file->handle, bitcast(LARGE_INTEGER, pos), autocast(&file->pos), FILE_BEGIN);
}

void file_flush(struct file* file)
{
 FlushFileBuffers(file->handle);
}

bool file_create(const char* path)
{
 WCHAR wpath[MAX_PATH];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (false);
 HANDLE handle = CreateFile(wpath, 0, 0, null, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, null);
 if (handle != INVALID_HANDLE_VALUE)
 {
  CloseHandle(handle);
  return (true);
 }
 bool ok = (GetLastError() == ERROR_FILE_EXISTS);
 return (ok);
}

bool file_delete(const char* path)
{
 WCHAR wpath[MAX_PATH];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (false);
 if (DeleteFile(wpath))
  return (true);
 bool ok = (GetLastError() == ERROR_FILE_NOT_FOUND);
 return (ok);
}

void file_copy(const char* out_path, const char* in_path)
{
 file_delete(out_path);
 file_create(out_path);
 struct file in = file_open(in_path, false);
 struct file out = file_open(out_path, true);
 char chunk[mem_page_size];
 while (true)
 {
  n32 n = file_read(&in, chunk, countof(chunk));
  if (n == 0)
   break;
  file_write(&out, chunk, n);
 }
 file_close(&out);
 file_close(&in);
}

#pragma endregion

#pragma region folders

struct folder
{
 HANDLE handle;
 WIN32_FIND_DATA data;
 char file_name[MAX_PATH * 3];
};

bool folder_exists(const char* path)
{
 WCHAR wpath[MAX_PATH];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (false);
 DWORD attr = GetFileAttributes(wpath);
 bool exists = (attr != INVALID_FILE_ATTRIBUTES);
 bool is_folder = (attr & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
 return (exists & is_folder);
}

struct folder folder_open(const char* path)
{
 struct folder folder = {0};
 WCHAR wpath[MAX_PATH];
 char path_fixed[MAX_PATH * 3];
 string_join_buf(path_fixed, path, "\\*");
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path_fixed, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (folder);
 // skip .
 folder.handle = FindFirstFile(wpath, &folder.data);
 if (folder.handle == INVALID_HANDLE_VALUE)
 {
  folder.handle = null;
  return (folder);
 }
 // skip ..
 FindNextFile(folder.handle, &folder.data);
 return (folder);
}

void folder_close(struct folder* folder)
{
 FindClose(folder->handle);
 memset(folder, 0, sizeof(struct folder));
}

bool folder_next(struct folder* folder)
{
 if (folder->handle == null)
  return (false);
 if (!FindNextFile(folder->handle, &folder->data))
  return (false);
 WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, folder->data.cFileName, -1, folder->file_name, countof(folder->file_name), null, null);
 return (true);
}

bool folder_create(const char* path)
{
 WCHAR wpath[MAX_PATH];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (false);
 WCHAR* ws = wpath;
 while (true)
 {
  if ((*ws == L'\\') | (*ws == L'/') | (*ws == 0))
  {
   bool was_end = (*ws == 0);
   *ws = 0;
   bool ok = CreateDirectory(wpath, null);
   if (!ok)
    ok = (GetLastError() == ERROR_ALREADY_EXISTS);
   if (!ok)
    return (false);
   if (was_end)
    break;
   *ws = '\\';
  }
  ws++;
 }
 return (true);
}

bool folder_delete(const char* path)
{
 WCHAR wpath[MAX_PATH];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (false);
 struct folder folder = folder_open(path);
 while (folder_next(&folder))
 {
  char file_path[MAX_PATH * 3];
  string_join_buf(file_path, path, "\\", folder.file_name);
  if (folder_exists(file_path))
  {
   folder_delete(file_path);
  }
  else
  {
   file_delete(file_path);
  }
 }
 folder_close(&folder);
 bool ok = RemoveDirectory(wpath);
 if (!ok)
  ok = (GetLastError() == ERROR_FILE_NOT_FOUND);
 return (ok);
}

#pragma endregion

#pragma region time

#define nanosecond 1ull
#define microsecond (1000 * nanosecond)
#define millisecond (1000 * microsecond)
#define second (1000 * millisecond)
#define minute (60 * second)
#define hour (60 * minute)

void sleep(n64 duration)
{
 Sleep(clamp_bot(duration / millisecond, 1));
}

n64 tick(void)
{
 static LARGE_INTEGER f;
 if (f.QuadPart == 0)
  QueryPerformanceFrequency(&f);
 LARGE_INTEGER t;
 QueryPerformanceCounter(&t);
 n64 q = t.QuadPart / f.QuadPart;
 n64 r = t.QuadPart % f.QuadPart;
 n64 out = q * second + r * second / f.QuadPart;
 return (out);
}

#pragma endregion

#pragma region threads

typedef void (*thread_callback)(void*);

struct _thread_data
{
 thread_callback callback;
 void* data;
 HANDLE signal;
};

DWORD WINAPI _thread(void* data)
{
 struct _thread_data thread_data = {0};
 memcpy(&thread_data, data, sizeof(struct _thread_data));
 SetEvent(thread_data.signal);
 thread_data.callback(thread_data.data);
 return (0);
}

void thread_start(thread_callback callback, void* data)
{
 struct _thread_data thread_data = {0};
 thread_data.callback = callback;
 thread_data.data = data;
 thread_data.signal = CreateEvent(null, false, false, null);
 CreateThread(null, 0, _thread, &thread_data, 0, null);
 WaitForSingleObject(thread_data.signal, INFINITE);
 CloseHandle(thread_data.signal);
}

#pragma endregion

#pragma region embed

struct embed
{
 const byte* data;
 n32 size;
};

struct embed embed_load(const char* name)
{
 WCHAR wname[256];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wname, countof(wname) - 1);
 wname[n] = 0;
 HRSRC rc = FindResource(null, wname, autocast(RT_RCDATA));
 struct embed embed = {0};
 embed.data = LockResource(LoadResource(null, rc));
 embed.size = SizeofResource(null, rc);
 return (embed);
}

bool embed_add(const char* path, struct embed embed, const char* name)
{
 WCHAR wpath[MAX_PATH];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (false);
 WCHAR wname[256];
 n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wname, countof(wname) - 1);
 wname[n] = 0;
 HANDLE resource = BeginUpdateResource(wpath, false);
 UpdateResource(resource, RT_RCDATA, wname, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), autocast(embed.data), embed.size);
 return EndUpdateResource(resource, false);
}

bool embed_add_file(const char* path, const char* embed_path, const char* name)
{
 WCHAR wpath[MAX_PATH];
 int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, MAX_PATH);
 if (n <= 0)
  return (false);
 WCHAR wname[256];
 n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wname, countof(wname) - 1);
 wname[n] = 0;
 HANDLE resource = BeginUpdateResource(wpath, false);
 struct file file = file_open(embed_path, false);
 HANDLE map = CreateFileMapping(file.handle, null, PAGE_READONLY, 0, file.size, null);
 void* file_map = MapViewOfFile(map, FILE_MAP_READ, 0, 0, file.size);
 UpdateResource(resource, RT_RCDATA, wname, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), file_map, file.size);
 UnmapViewOfFile(file_map);
 CloseHandle(map);
 file_close(&file);
 return EndUpdateResource(resource, false);
}

#pragma endregion

#pragma region runtime

#define MAX_CMDLINE_LEN_UTF8 (32767 * 3)

nat args_count;
const char* args[MAX_CMDLINE_LEN_UTF8];
bool invoked_from_console;

void start(void);

void _console_parse_arguments(void)
{
 static char args_global[MAX_CMDLINE_LEN_UTF8];
 int args_position = 0;
 int wargs_count = 0;
 LPCWSTR* wargs = CommandLineToArgvW(GetCommandLine(), &wargs_count);
 for (int i = 0; i < wargs_count; i++)
 {
  char arg[MAX_CMDLINE_LEN_UTF8];
  int arg_count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargs[i], -1, arg, _countof(arg), null, null);
  memcpy(&args_global[args_position], arg, arg_count);
  args[args_count++] = &args_global[args_position];
  args_position += arg_count;
 }
 LocalFree(wargs);
}

void _set_dpi_awareness(void)
{
 // windows 10 1703
 HMODULE user32 = LoadLibrary(L"user32.dll");
 typedef BOOL (WINAPI* SetProcessDpiAwarenessContext_t)(DPI_AWARENESS_CONTEXT);
 SetProcessDpiAwarenessContext_t SetProcessDpiAwarenessContext = (void*)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
 if (SetProcessDpiAwarenessContext != null)
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 FreeLibrary(user32);
 // windows 8.1
 HMODULE shcore = LoadLibrary(L"shcore.dll");
 if (shcore != null)
 {
  typedef HRESULT (WINAPI* SetProcessDpiAwareness_t)(int);
  SetProcessDpiAwareness_t SetProcessDpiAwareness = (void*)GetProcAddress(shcore, "SetProcessDpiAwareness");
  if (SetProcessDpiAwareness != null)
   SetProcessDpiAwareness(2);
  FreeLibrary(shcore);
 }
 // windows 7
 SetProcessDPIAware();
}

LRESULT CALLBACK _service_proc(HWND service, UINT message, WPARAM wparam, LPARAM lparam)
{
 if (message == WM_APP)
 {
  SetForegroundWindow(GetConsoleWindow());
  return (0);
 }

 if (message == WM_DESTROY)
 {
  PostQuitMessage(0);
  return (0);
 }

 return DefWindowProc(service, message, wparam, lparam);
}

void _main_thread(HWND service)
{
 SetConsoleOutputCP(437);
 _console_parse_arguments();
 DWORD pid;
 GetWindowThreadProcessId(GetConsoleWindow(), &pid);
 invoked_from_console = (GetCurrentProcessId() != pid);
 start();
 SendMessage(service, WM_CLOSE, 0, 0);
}

void _start(void)
{
 _set_dpi_awareness();
 HINSTANCE instance = GetModuleHandle(null);
 RegisterClass(&(WNDCLASS){.lpfnWndProc = _service_proc, .hInstance = instance, .lpszClassName = TEXT(appid) L":service"});
 HWND service = CreateWindow(TEXT(appid) L":service", null, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, null, instance, null);
 thread_start(_main_thread, service);
 MSG msg = {0};
 while (GetMessage(&msg, null, 0, 0))
 {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
 }
 exit(0);
}

HWND find_app_service(void)
{
 HWND service = null;
 DWORD pid = GetCurrentProcessId();
 while (true)
 {
  service = FindWindowEx(HWND_MESSAGE, service, TEXT(appid) L":service", null);
  if (service == null)
   break;
  DWORD service_pid;
  GetWindowThreadProcessId(service, &service_pid);
  if (pid != service_pid)
   continue;
  break;
 }
 return (service);
}

// allows only one appid instance at a time, otherwise exits the process
// if appid is not defined ("0") - does nothing
void appid_instance_guard(void)
{
 if (string_is(appid, "0"))
  return;
 HWND service = null;
 DWORD pid = GetCurrentProcessId();
 while (true)
 {
  service = FindWindowEx(HWND_MESSAGE, service, TEXT(appid) L":service", null);
  if (service == null)
   break;
  DWORD service_pid;
  GetWindowThreadProcessId(service, &service_pid);
  if (pid == service_pid)
   continue;
  AllowSetForegroundWindow(service_pid);
  PostMessage(service, WM_APP, 0, 0);
  break;
 }
 if (service != null)
  exit(0);
 return;
}

#pragma endregion
