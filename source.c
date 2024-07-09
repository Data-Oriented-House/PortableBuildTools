#include "base.c"
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAME
#define MINIZ_NO_MALLOC
#define NDEBUG
#include "miniz/miniz.c"
#include "gui/gui.h"
#include <shobjidl_core.h>
#include <shlwapi.h>
#include <wininet.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wininet.lib")
#define utf8_to_utf16(str, str_count, wbuf, wbuf_count) \
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, str_count, wbuf, wbuf_count)
#define utf16_to_utf8(wstr, wstr_count, buf, buf_count) \
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, wstr_count, buf, buf_count, null, null)
#define MAX_ENV_LEN 32767

//[of]:Strings
bool string_has_char(const char* s, char x)
{	
	bool match = false;
	while (*s != 0) {
		match = (*s == x) ? true : match;
	}
	return (match);
}

//[c]Check if the string includes any of the bytes in the buffer
bool string_has_any(const char* s, const array(char, buf))
{
	i64 s_count = string_count(s);
	bool match = false;
	for (i64 i = 0; i < s_count; i++) {
		for (i64 j = 0; j < buf_count; j++) {
			match = (s[i] == buf[j]) ? true : match;
		}
	}
	return (match);
}

//[c]Check if the string includes only the bytes in the buffer, and no other bytes
bool string_has_only(const char* s, const array(char, buf))
{
	i64 s_count = string_count(s);
	bool match = true;
	for (i64 i = 0; i < s_count; i++) {
		bool match_byte = false;
		for (i64 j = 0; j < buf_count; j++) {
			match_byte = (s[i] == buf[j]) ? true : match_byte;
		}
		match = match_byte ? match : false;
	}
	return (match);
}

bool string_starts_with(const char* s, const char* x)
{
	bool match = true;
	while (*x != 0) {
		match = (*s != *x) ? false : match;
		if (*s == 0) {
			break;
		}
		s++;
		x++;
	}
	return (match);
}

bool string_ends_with(const char* s, const char* x) {
	i64 s_count = string_count(s);
	i64 x_count = string_count(x);
	i64 diff = clamp_bot(s_count - x_count, 0);
	return string_starts_with(s + diff, x);
}

bool string_trim_start(char* s, const char* x)
{
	i64 s_count = string_count(s);
	i64 x_count = string_count(x);
	bool trim = string_starts_with(s, x);
	i64 copy_size = s_count - x_count + 1;
	copy_size = trim ? copy_size : 0;
	mem_copy(s, s + x_count, copy_size);
	return (trim);
}

bool string_trim_end(char* s, const char* x)
{
	i64 s_count = string_count(s);
	i64 x_count = string_count(x);
	bool trim = string_ends_with(s, x);
	char byte = s[s_count - x_count];
	s[s_count - x_count] = trim ? 0 : byte;
	return (trim);
}

void string_lower(char* s)
{
	while (*s != 0) {
		char lower = *s + ('a' - 'A');
		*s = ((*s >= 'A') & (*s <= 'Z')) ? lower : *s;
		s++;
	}
}

void string_copy(array(char, buf), const char* s)
{
	i64 s_count = string_count(s);
	i64 copy_size = clamp_top(s_count, buf_count - 1);
	mem_copy(buf, s, copy_size);
	buf[copy_size] = 0;
}

//[c]Numeric conversions
char* string_from_i64(char (*buf)[66], i64 x)
{
	return string_from_integer(buf, x, 10, true, 0);
}

char* string_from_n64(char (*buf)[66], n64 x)
{
	return string_from_integer(buf, x, 10, false, 0);
}

//[c]TODO: minus sign, check if number is outside the representable range, check if non-digits, etcetc. This is barely usable right now.
i64 string_to_i64(const char* s)
{
	i64 base = 1;
	i64 out = 0;
	i64 s_count = string_count(s);
	for (i64 i = s_count - 1; i >= 0; i -= 1) {
		i64 digit = s[i] - '0';
		out += (base * digit);
		base *= 10;
	}
	return (out);
}
//[cf]
//[of]:Files
typedef struct {
	void*	handle;
	i64	size;
	i64	pos;
} file_handle;

typedef enum {
	file_mode_read,
	file_mode_write,
} file_mode;

bool file_exists(const char* path)
{
	WCHAR wpath[MAX_PATH];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	hope(n > 0, "path unicode conversion bug");
	DWORD err = GetFileAttributesW(wpath);
	bool ok = (err != INVALID_FILE_ATTRIBUTES) && !(err & FILE_ATTRIBUTE_DIRECTORY);
	return (ok);
}

void file_delete(const char* path)
{
	WCHAR wpath[MAX_PATH];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	hope(n > 0, "path unicode conversion bug");
	hope(DeleteFileW(wpath), "failed to delete the file: {s}", path);
}

void file_create(const char* path)
{
	WCHAR wpath[MAX_PATH];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	hope(n > 0, "path unicode conversion bug");
	DWORD err = GetFileAttributesW(wpath);
	bool exists = (err != INVALID_FILE_ATTRIBUTES) && !(err & FILE_ATTRIBUTE_DIRECTORY);
	if (exists) {
		hope(DeleteFileW(wpath), "failed to delete the file: {s}", path);
	}
	HANDLE handle = CreateFileW(wpath, 0, 0, null, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, null);
	hope(handle != INVALID_HANDLE_VALUE, "failed to create the file: {s}", path);
	hope(CloseHandle(handle), "failed to close the file: {s}", path);
}

file_handle file_open(const char* path, file_mode mode)
{
	file_handle f = (file_handle){0};
	WCHAR wpath[MAX_PATH];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	hope(n > 0, "path unicode conversion bug");
	DWORD access = 0;
	access = (mode == file_mode_read) ? FILE_GENERIC_READ : access;
	access = (mode == file_mode_write) ? FILE_GENERIC_WRITE : access;
	SECURITY_ATTRIBUTES sa = (SECURITY_ATTRIBUTES){0};
	sa.nLength = size_of(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = true;
	DWORD attrs = FILE_ATTRIBUTE_NORMAL;
	attrs = (mode == file_mode_read) ? FILE_ATTRIBUTE_READONLY : attrs;
	f.handle = CreateFileW(wpath, access, FILE_SHARE_READ, &sa, OPEN_EXISTING, attrs, null);
	hope(f.handle != INVALID_HANDLE_VALUE, "failed to open the file: {s}", path);
	hope(GetFileSizeEx(f.handle, cast(LARGE_INTEGER*, &f.size)), "failed to get the file size: {s}", path);
	f.pos = (mode == file_mode_read) ? 0 : f.size;
	SetFilePointerEx(f.handle, bit_cast(LARGE_INTEGER, f.pos), null, FILE_BEGIN);
	return (f);
}

void file_close(file_handle* f)
{
	CloseHandle(f->handle);
}

i64 file_read(file_handle* f, array(char, buf))
{
	DWORD n = 0;
	ReadFile(f->handle, buf, cast(DWORD, buf_count), &n, null);
	f->pos += n;
	return (n);
}

void file_write(file_handle* f, array(const char, data))
{
	DWORD n = 0;
	WriteFile(f->handle, data, cast(DWORD, data_count), &n, null);
	hope(n == data_count, "file did not write enough: {i64}/{i64}", n, data_count);
	f->pos += n;
}

void file_seek(file_handle* f, i64 pos)
{
	SetFilePointerEx(f->handle, bit_cast(LARGE_INTEGER, pos), cast(LARGE_INTEGER*, &f->pos), FILE_BEGIN);
}

void file_flush(file_handle* f)
{
	FlushFileBuffers(f->handle);
}

void file_copy(const char* dst_path, const char* src_path)
{
	file_handle src = file_open(src_path, file_mode_read);
	file_handle dst = file_open(dst_path, file_mode_write);
	char chunk[mem_page_size];
	while (true) {
		i64 n = file_read(&src, array_expand(chunk));
		if (n == 0) {
			break;
		}
		file_write(&dst, chunk, n);
	}
	file_close(&dst);
	file_close(&src);
}
//[cf]
//[of]:Folders
typedef struct {
	HANDLE	handle;
	WIN32_FIND_DATAW	data;
} folder_handle;

bool folder_exists(const char* path)
{
	WCHAR wpath[MAX_PATH];
	utf8_to_utf16(path, -1, wpath, MAX_PATH);
	DWORD err = GetFileAttributesW(wpath);
	bool ok = (err != INVALID_FILE_ATTRIBUTES) && (err & FILE_ATTRIBUTE_DIRECTORY);
	return (ok);
}

void folder_delete(const char* path)
{
//[c]	NOTE: Resulting string must be double-null-terminated
	WCHAR wpath[MAX_PATH + 1];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	wpath[n] = 0;
	SHFILEOPSTRUCTW op = (SHFILEOPSTRUCTW){0};
	op.wFunc = FO_DELETE;
	op.pFrom = wpath;
	op.fFlags = FOF_NO_UI;
	hope(!SHFileOperationW(&op), "failed to delete the file: {s}", path);
}

void folder_create(const char* path)
{
	WCHAR wpath[MAX_PATH];
	utf8_to_utf16(path, -1, wpath, MAX_PATH);
	DWORD err = GetFileAttributesW(wpath);
	bool exists = (err != INVALID_FILE_ATTRIBUTES) && (err & FILE_ATTRIBUTE_DIRECTORY);
	if (exists) {
		folder_delete(path);
	}
	BOOL ok = CreateDirectoryW(wpath, null);
	hope(ok, "failed to create the directory: {s}", path);
}

folder_handle folder_open(const char* path)
{
	WCHAR wpath[MAX_PATH];
	char path_fixed[MAX_PATH * 3];
	string_format(array_expand(path_fixed), "{s}\\*", path);
	utf8_to_utf16(path_fixed, -1, wpath, MAX_PATH);
	folder_handle f = (folder_handle){0};
//[c]	Skip .
	f.handle = FindFirstFileW(wpath, &f.data);
	hope(f.handle != INVALID_HANDLE_VALUE, "Failed to open folder: {s}", path);
//[c]	Skip ..
	FindNextFileW(f.handle, &f.data);
	return (f);
}

void folder_close(folder_handle* f)
{
	FindClose(f->handle);
	*f = (folder_handle){0};
}

bool folder_next(folder_handle* f, char (*file_name)[MAX_PATH * 3])
{
	bool ok = FindNextFileW(f->handle, &f->data);
	if (ok) {
		utf16_to_utf8(f->data.cFileName, -1, *file_name, count_of(*file_name));
	}
	return (ok);
}
//[cf]
//[of]:Time
typedef i64 time_duration;
typedef i64 time_tick;

#define time_nanosecond	cast(time_duration, 1)
#define time_microsecond	(1000 * time_nanosecond)
#define time_millisecond	(1000 * time_microsecond)
#define time_second	(1000 * time_millisecond)
#define time_minute	(60 * time_second)
#define time_hour	(60 * time_minute)

void time_sleep(time_duration duration)
{
	time_duration d_ms = max(duration / time_millisecond, 1);
	Sleep(cast(DWORD, d_ms));
}

time_tick time_tick_now(void)
{
	static LARGE_INTEGER qpc_frequency = {0};
	if (qpc_frequency.QuadPart == 0) {
		QueryPerformanceFrequency(&qpc_frequency);
	}
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	i64 q = now.QuadPart / qpc_frequency.QuadPart;
	i64 r = now.QuadPart % qpc_frequency.QuadPart;
	i64 num = 1000000000;
	time_tick out = q * num + r * num / qpc_frequency.QuadPart;
	return (out);
}
//[cf]
//[of]:Threads
typedef struct {
	HANDLE	handle;
	DWORD	id;
} thread_handle;

typedef void (*thread_proc)(void);

struct thread_config {
	thread_proc	p;
	bool	signal;
};

typedef enum {
	thread_priority_normal,
	thread_priority_low,
	thread_priority_high,
	thread_priority_count,
} thread_priority;

int _thread_priority_map[thread_priority_count] = {
	[thread_priority_normal] = 0,
	[thread_priority_low] = -2,
	[thread_priority_high] = 2,
};

DWORD _internal_thread_enter(void* lpParameter)
{
	struct thread_config* tc = lpParameter;
	thread_proc p = tc->p;
	tc->signal = true;
	p();
	return (0);
}

thread_handle thread_run(thread_proc p, thread_priority priority)
{
	thread_handle out = (thread_handle){0};
	struct thread_config tc = (struct thread_config){.p = p, .signal = false};
	DWORD id;
	HANDLE handle = CreateThread(null, 0, cast(void*, _internal_thread_enter), &tc, CREATE_SUSPENDED, &id);
	hope(handle != null, "failed to create a thread");
	hope(SetThreadPriority(handle, _thread_priority_map[priority]), "failed to set thread priority");
	hope(ResumeThread(handle) != cast(DWORD, -1), "failed to resume a thread");
	while (!tc.signal) {
		cpu_relax();
	}
	out.handle = handle;
	out.id = id;
	return (out);
}

void thread_wait(thread_handle* t)
{
	WaitForSingleObject(t->handle, INFINITE);
	CloseHandle(t->handle);
	*t = (thread_handle){0};
}

void thread_kill(thread_handle* t)
{
	TerminateThread(t->handle, 0);
	*t = (thread_handle){0};
}

void thread_yield()
{
	SwitchToThread();
}
//[cf]
//[of]:JSON parser
typedef char8 (*json_peek_proc)(void* context, bool advance);
typedef struct {
	json_peek_proc	peek;
	void*	context;
} json_parser;

json_parser json_parser_begin(json_peek_proc peek, void* context)
{
	json_parser p;
	p.peek = peek;
	p.context = context;
	return (p);
}

typedef enum {
	json_none,
	json_word,
	json_number,
	json_string,
	json_object,
	json_array,
} json_value;

json_value json_value_skip(json_parser* p);

json_value json_value_next(json_parser* p)
{
	json_value out = json_none;
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		bool skip = ((symbol == ' ') | (symbol == '\t') | (symbol == '\n') | (symbol == '\r') | (symbol == ','));
		out = (((symbol >= 'a') & (symbol <= 'z')) | ((symbol >= 'A') & (symbol <= 'Z'))) ? json_word : out;
		out = (((symbol >= '0') & (symbol <= '9')) | (symbol == '-') | (symbol == '.')) ? json_number : out;
		out = (symbol == '"') ? json_string : out;
		out = (symbol == '{') ? json_object : out;
		out = (symbol == '[') ? json_array : out;
		if (skip) {
			continue;
		}
		break;
	}
	return (out);
}

void json_word_skip(json_parser* p)
{
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (((symbol >= 'a') & (symbol <= 'z')) | ((symbol >= 'A') & (symbol <= 'Z'))) {
			continue;
		}
		break;
	}
}

i64 json_number_extract_i64(json_parser* p)
{
	char n[66] = {0};
	i64 pos = 0;
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (((symbol >= '0') & (symbol <= '9')) | (symbol == '-') | (symbol == '.')) {
			if (pos < cast(i64, count_of(n))) {
				n[pos] = symbol;
				pos++;
			}
			continue;
		}
		break;
	}
	return (string_to_i64(n));
}

void json_number_skip(json_parser* p)
{
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (((symbol >= '0') & (symbol <= '9')) | (symbol == '-') | (symbol == '.')) {
			continue;
		}
		break;
	}
}

void json_string_extract(json_parser* p, array(char, buf))
{
	buf_count--;
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '"') {
			p->peek(p->context, true);
			break;
		}
	}
	i64 pos = 0;
	bool buf_filled = false;
	bool escaping = false;
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if ((symbol == '\\') & !escaping) {
			escaping = true;
			continue;
		}
		if ((symbol == '"') & !escaping) {
			p->peek(p->context, true);
			break;
		}
		escaping = false;
		i64 char_size = char8_size(c8);
		buf_filled = (pos + char_size > buf_count) ? true : buf_filled;
		i64 copy_size = buf_filled ? 0 : char_size;
		mem_copy(buf + pos, &c8, copy_size);
		pos += copy_size;
	}
	buf[pos] = 0;
}

bool json_string_is(json_parser* p, const char* str)
{
	i64 str_count = string_count(str);
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '"') {
			p->peek(p->context, true);
			break;
		}
	}
	i64 pos = 0;
	bool match = true;
	bool escaping = false;
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '\\') {
			if (!escaping) {
				escaping = true;
				continue;
			}
		}
		if (symbol == '"') {
			if (!escaping) {
				p->peek(p->context, true);
				break;
			}
		}
		escaping = false;
		i64 space_left = str_count - pos;
		i64 char_size = char8_size(c8);
		i64 compare_size = clamp_top(char_size, space_left);
		char* bytes = cast(char*, &c8);
		for (i64 i = 0; i < compare_size; i++) {
			match = (str[pos + i] != bytes[i]) ? false : match;
		}
		pos += char_size;
	}
	return ((pos == str_count) & (match));
}

void json_string_skip(json_parser* p)
{
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '"') {
			p->peek(p->context, true);
			break;
		}
	}
	bool escaping = false;
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '\\') {
			if (!escaping) {
				escaping = true;
				continue;
			}
		}
		if (symbol == '"') {
			if (!escaping) {
				p->peek(p->context, true);
				break;
			}
		}
		escaping = false;
	}
}

bool json_array_next(json_parser* p)
{
	bool match = false;
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if ((symbol == '[') | (symbol == ' ') | (symbol == '\t') | (symbol == '\n') | (symbol == '\r') | (symbol == ',')) {
			continue;
		}
		if (symbol == ']') {
			p->peek(p->context, true);
			break;
		}
		match = true;
		break;
	}
	return (match);
}

void json_array_skip(json_parser* p)
{
	while (json_array_next(p)) {
		json_value_skip(p);
	}
}

bool json_object_next(json_parser* p)
{
	bool match = false;
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if ((symbol == '{') | (symbol == ' ') | (symbol == '\t') | (symbol == '\n') | (symbol == '\r') | (symbol == ',')) {
			continue;
		}
		if (symbol == '}') {
			p->peek(p->context, true);
			break;
		}
		if (symbol == '"') {
			match = true;
		}
		break;
	}
	return (match);
}

void json_object_key_extract(json_parser* p, array(char, buf))
{
	json_string_extract(p, buf, buf_count);
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == ':') {
			p->peek(p->context, true);
			break;
		}
	}
}

void json_object_key_skip(json_parser* p)
{
	json_string_skip(p);
	for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == ':') {
			p->peek(p->context, true);
			break;
		}
	}
}

bool json_object_key_find(json_parser* p, const char* key)
{
	bool match = false;
	while (json_object_next(p) & !match) {
		match = json_string_is(p, key);
		for (char8 c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
			char symbol = *cast(char*, &c8);
			if (symbol == ':') {
				p->peek(p->context, true);
				break;
			}
		}
		if (!match) {
			json_value_skip(p);
		}
	}
	return (match);
}

void json_object_skip(json_parser* p)
{
	while (json_object_next(p)) {
		json_object_key_skip(p);
		json_value_skip(p);
	}
}

json_value json_value_skip(json_parser* p)
{
	json_value v = json_value_next(p);
	if (v == json_word) {
		json_word_skip(p);
	}
	if (v == json_number) {
		json_number_skip(p);
	}
	if (v == json_string) {
		json_string_skip(p);
	}
	if (v == json_object) {
		json_object_skip(p);
	}
	if (v == json_array) {
		json_array_skip(p);
	}
	return (v);
}
//[cf]
//[of]:JSON file parser
typedef struct {
	char	cache[mem_page_size];
	char8	c8;
	i64	cache_size;
	i64	cache_pos;
	file_handle	f;
} json_context;

char8 json_file_peek(void* context, bool advance)
{
	json_context* jc = context;
//[c]	Advance if beginning of the file, or if asked to
	if ((!advance) & (jc->f.pos != 0)) {
		return (jc->c8);
	}
//[c]	If beginning of the file, reset all values
	jc->c8 = (jc->f.pos == 0) ? 0 : jc->c8;
	jc->cache_size = (jc->f.pos == 0) ? 0 : jc->cache_size;
	jc->cache_pos = (jc->f.pos == 0) ? 0 : jc->cache_pos;
//[c]	Chunk end
	if (jc->cache_pos >= jc->cache_size) {
		jc->cache_size = file_read(&jc->f, array_expand(jc->cache));
		jc->cache_pos = 0;
	}
//[c]	File end
	if (jc->cache_size <= 0) {
		jc->cache_pos = 0;
		jc->c8 = 0;
		return (jc->c8);
	}
	const char* str = jc->cache + jc->cache_pos;
	i64 str_size = jc->cache_size - jc->cache_pos;
	jc->c8 = string_decode_char8(str);
	i64 char_size = char8_size(jc->c8);
//[c]	Valid char, or invalid char in the middle of a chunk
	if ((char_size > 0) | (str_size >= 4) | (jc->f.pos == jc->f.size)) {
		jc->cache_pos += clamp_bot(char_size, 1);
		return (jc->c8);
	}
//[c]	Either invalid char or chunk end, start a new chunk just to be sure
	i64 seek_pos = jc->f.pos - (jc->cache_size - jc->cache_pos);
	file_seek(&jc->f, seek_pos);
	jc->cache_size = 0;
	jc->cache_pos = 0;
	return json_file_peek(context, advance);
}

void json_file_context_restore(json_context*	jc, json_context context)
{
	*jc = context;
	file_seek(&jc->f, jc->f.pos);
}
//[cf]
//[of]:miniz helpers
void* miniz_malloc(void* opaque, size_t items, size_t size)
{
	return HeapAlloc(GetProcessHeap(), 0, items * size);
}

void miniz_free(void* opaque, void* address)
{
	HeapFree(GetProcessHeap(), 0, address);
}

void* miniz_realloc(void* opaque, void* address, size_t items, size_t size)
{
	if (!address) {
		return HeapAlloc(GetProcessHeap(), 0, items * size);
	} else if ((items == 0) | (size == 0)) {
		HeapFree(GetProcessHeap(), 0, address);
		return NULL;
	} else {
		return HeapReAlloc(GetProcessHeap(), 0, address, items * size);
	}
}

size_t miniz_file_read(void* context, mz_uint64 file_ofs, void* buf, size_t buf_count)
{
	file_handle* f = context;
	file_seek(f, file_ofs);
	return file_read(f, buf, buf_count);
}

size_t mz_file_write(void* context, mz_uint64 file_ofs, const void* buf, size_t buf_count)
{
	file_handle* f = context;
	file_write(f, buf, buf_count);
	return (buf_count);
}
//[cf]

const char* release_vsmanifest_url = "https://aka.ms/vs/17/release/channel";
const char* preview_vsmanifest_url = "https://aka.ms/vs/17/pre/channel";

char* targets[] = {"x64", "x86", "arm", "arm64",};
char* hosts[] = {"x64", "x86", "arm64"};

bool install_start;
bool is_preview;
n64 env_mode; // 0: none; 1: user; 2: global;
char* msvc_version;
char* sdk_version;
char* target_arch = "x64";
char* host_arch = "x64";
char install_path[MAX_PATH * 3] = "C:\\BuildTools";
bool license_accepted;
bool is_admin;
char temp_path[MAX_PATH * 3];

char release_manifest_url[L_MAX_URL_LENGTH * 3];
char release_license_url[L_MAX_URL_LENGTH * 3];
char preview_manifest_url[L_MAX_URL_LENGTH * 3];
char preview_license_url[L_MAX_URL_LENGTH * 3];

//[c]NOTE: actual number of versions is 12, not 32
char release_msvc_versions[32][16];
i64 release_msvc_versions_count;
//[c]NOTE: actual number of versions is 13, not 32
char preview_msvc_versions[32][16];
i64 preview_msvc_versions_count;

//[c]NOTE: actual number of versions is 5, not 16
char release_sdk_versions[16][8];
i64 release_sdk_versions_count;
//[c]NOTE: actual number of versions is 5, not 16
char preview_sdk_versions[16][8];
i64 preview_sdk_versions_count;

HINTERNET internet_session;

void env_set(HKEY location, LPCWSTR subkey, LPCWSTR key, const char* env)
{
	WCHAR wenv[MAX_ENV_LEN];
	int n = utf8_to_utf16(env, -1, wenv, count_of(wenv));
	RegSetKeyValueW(location, subkey, key, REG_SZ, wenv, n * size_of(WCHAR));
}

void cleanup(void)
{
	if (internet_session) {
		InternetCloseHandle(internet_session);
		internet_session = null;
	}
	CoUninitialize();
}

void console_panic(const char* msg)
{
	cleanup();
	sys_error_write(msg);
	exit(1);
}

void message_box_panic(const char* msg)
{
	cleanup();
	WCHAR wmsg[mem_page_size / size_of(WCHAR)];
	utf8_to_utf16(msg, -1, wmsg, count_of(wmsg));
	MessageBoxW(GetConsoleWindow(), wmsg, null, MB_TOPMOST | MB_ICONERROR);
	exit(1);
}

void refill_gui(HWND dlg)
{
//[c]	NOTE: iterating backwards, assuming versions are sorted backwards in the JSON
	{
		char (*msvc_versions)[16] = is_preview ? preview_msvc_versions : release_msvc_versions;
		i64 msvc_versions_count = is_preview ? preview_msvc_versions_count : release_msvc_versions_count;
		HWND item = GetDlgItem(dlg, ID_COMBO_MSVC);
		SendMessageW(item, CB_RESETCONTENT, 0, 0);
		for (i64 i = msvc_versions_count - 1; i >= 0; i--) {
			WCHAR wver[MAX_ENV_LEN];
			utf8_to_utf16(msvc_versions[i], -1, wver, count_of(wver));
			SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, wver));
		}
		SendMessageW(item, CB_SETCURSEL, 0, 0);
		msvc_version = msvc_versions[msvc_versions_count - 1];
	}
	{
		char (*sdk_versions)[8] = is_preview ? preview_sdk_versions : release_sdk_versions;
		i64 sdk_versions_count = is_preview ? preview_sdk_versions_count : release_sdk_versions_count;
		HWND item = GetDlgItem(dlg, ID_COMBO_SDK);
		SendMessageW(item, CB_RESETCONTENT, 0, 0);
		for (i64 i = sdk_versions_count - 1; i >= 0; i--) {
			WCHAR wver[MAX_ENV_LEN];
			utf8_to_utf16(sdk_versions[i], -1, wver, count_of(wver));
			SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, wver));
		}
		SendMessageW(item, CB_SETCURSEL, 0, 0);
		sdk_version = sdk_versions[sdk_versions_count - 1];
	}
	{
		char* license_url = is_preview ? preview_license_url : release_license_url;
		char link_url[64 + L_MAX_URL_LENGTH * 3] = {0};
		string_format(array_expand(link_url), "I accept the <a href=\"{s}\">License Agreement</a>", license_url);
		WCHAR wlink[64 + L_MAX_URL_LENGTH];
		utf8_to_utf16(link_url, -1, wlink, count_of(wlink));
		SetDlgItemTextW(dlg, ID_SYSLINK_LICENSE, wlink);
	}
}

//[c]Updates install_path, adds shield to install button if needed
void path_update(HWND dlg)
{
	WCHAR wpath[MAX_PATH] = {0};
	GetDlgItemTextW(dlg, ID_EDIT_PATH, wpath, MAX_PATH);
	int n = utf16_to_utf8(wpath, -1, install_path, count_of(install_path));
//[c]	Check if path is accessible by creating temporary path/~ file
	if (folder_exists(install_path)) {
		WCHAR check_path[MAX_PATH] = {0};
		mem_copy(check_path, wpath, n * size_of(WCHAR));
		n--;
		check_path[n] = L'\\';
		n++;
		check_path[n] = L'~';
		n++;
		check_path[n] = 0;
		HANDLE tfile = CreateFileW(check_path, GENERIC_WRITE, 0, null, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, null);
		if (tfile != INVALID_HANDLE_VALUE) {
			CloseHandle(tfile);
			is_admin = false;
			SendMessageW(GetDlgItem(dlg, ID_BUTTON_INSTALL), BCM_SETSHIELD, 0, false);
			return;
		}
	} else {
		BOOL ok = CreateDirectoryW(wpath, null);
		if (ok) {
			folder_delete(install_path);
			is_admin = false;
			SendMessageW(GetDlgItem(dlg, ID_BUTTON_INSTALL), BCM_SETSHIELD, 0, false);
			return;
		}
	}
	is_admin = (GetLastError() == ERROR_ACCESS_DENIED) ? true : false;
	SendMessageW(GetDlgItem(dlg, ID_BUTTON_INSTALL), BCM_SETSHIELD, 0, is_admin);
}

bool is_folder_empty(const char* path)
{
	folder_handle f = folder_open(path);
	char file_name[MAX_PATH * 3];
	bool has_files = folder_next(&f, &file_name);
	folder_close(&f);
	return (!has_files);
}

BOOL WINAPI window_proc(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
		case WM_INITDIALOG: {
			SetForegroundWindow(dlg);
			{
				HICON icon = LoadIconW(GetModuleHandleW(null), cast(LPCWSTR, ID_ICON));
				SendMessageW(dlg, WM_SETICON, ICON_SMALL, cast(LPARAM, icon));
				SendMessageW(dlg, WM_SETICON, ICON_BIG, cast(LPARAM, icon));
			}
			{
				HWND item = GetDlgItem(dlg, ID_COMBO_ENV);
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"Create environment setup scripts"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"Create the scripts & add to user environment"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"Create the scripts & add to global environment"));
				SendMessageW(item, CB_SETCURSEL, 0, 0);
			}
			{
				HWND item = GetDlgItem(dlg, ID_COMBO_TARGET);
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"x64"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"x86"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"arm"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"arm64"));
				SendMessageW(item, CB_SETCURSEL, 0, 0);
			}
			{
				HWND item = GetDlgItem(dlg, ID_COMBO_HOST);
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"x64"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"x86"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"arm64"));
				SendMessageW(item, CB_SETCURSEL, 0, 0);
			}
			{
				HWND item = GetDlgItem(dlg, ID_EDIT_PATH);
				SendMessageW(item, WM_SETTEXT, 0, cast(LPARAM, L"C:\\BuildTools"));
				SHAutoComplete(item, SHACF_FILESYSTEM);
			}
			refill_gui(dlg);
			path_update(dlg);
			EnableWindow(GetDlgItem(dlg, ID_BUTTON_INSTALL), false);
			return (true);
		} break;
		case WM_CLOSE: {
			EndDialog(dlg, 0);
			return (true);
		} break;
		case WM_COMMAND: {
			WORD* wparam_words = cast(WORD*, &wparam);
			switch (wparam_words[0]) {
				case ID_CHECK_PREVIEW: {
					is_preview = (IsDlgButtonChecked(dlg, ID_CHECK_PREVIEW) == BST_CHECKED);
					refill_gui(dlg);
				} break;
				case ID_COMBO_ENV: {
					env_mode = SendMessageW(GetDlgItem(dlg, ID_COMBO_ENV), CB_GETCURSEL, 0, 0);
				} break;
				case ID_COMBO_MSVC: {
					i64 index = SendMessageW(GetDlgItem(dlg, ID_COMBO_MSVC), CB_GETCURSEL, 0, 0);
					msvc_version = is_preview ? preview_msvc_versions[preview_msvc_versions_count - index - 1] : release_msvc_versions[release_msvc_versions_count - index - 1];
				} break;
				case ID_COMBO_SDK: {
					i64 index = SendMessageW(GetDlgItem(dlg, ID_COMBO_SDK), CB_GETCURSEL, 0, 0);
					sdk_version = is_preview ? preview_sdk_versions[preview_sdk_versions_count - index - 1] : release_sdk_versions[release_sdk_versions_count - index - 1];
				} break;
				case ID_COMBO_TARGET: {
					i64 index = SendMessageW(GetDlgItem(dlg, ID_COMBO_TARGET), CB_GETCURSEL, 0, 0);
					target_arch = targets[index];
				} break;
				case ID_COMBO_HOST: {
					i64 index = SendMessageW(GetDlgItem(dlg, ID_COMBO_HOST), CB_GETCURSEL, 0, 0);
					host_arch = hosts[index];
				} break;
				case ID_EDIT_PATH: {
					if (wparam_words[1] == EN_CHANGE) {
						path_update(dlg);
					}
				} break;
				case ID_BUTTON_BROWSE: {
					IFileOpenDialog* fd;
					CoCreateInstance(&CLSID_FileOpenDialog, null, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, cast(void**, &fd));
//[c]					NOTE: Randomly generated GUID
					vcall(fd, lpVtbl->SetClientGuid, &(GUID){0x65CDF695, 0x0A2C, 0x46DB, {0x9F, 0xE8, 0x73, 0xCF, 0xA7, 0xF7, 0x1E, 0x65}});
					DWORD options;
					vcall(fd, lpVtbl->GetOptions, &options);
					vcall(fd, lpVtbl->SetOptions, options | FOS_PICKFOLDERS);
					if (vcall(fd, lpVtbl->Show, null) == 0) {
						IShellItem* psi;
						vcall(fd, lpVtbl->GetResult, &psi);
						{
							LPWSTR wpath;
							vcall(psi, lpVtbl->GetDisplayName, SIGDN_DESKTOPABSOLUTEPARSING, &wpath);
							SetDlgItemTextW(dlg, ID_EDIT_PATH, wpath);
							CoTaskMemFree(wpath);
						}
						vcall(psi, lpVtbl->Release);
					}
					vcall(fd, lpVtbl->Release);
				} break;
				case ID_CHECK_LICENSE: {
					license_accepted = (IsDlgButtonChecked(dlg, ID_CHECK_LICENSE) == BST_CHECKED);
					EnableWindow(GetDlgItem(dlg, ID_BUTTON_INSTALL), license_accepted);
				} break;
				case ID_BUTTON_INSTALL: {
					if (folder_exists(install_path)) {
						if (!is_folder_empty(install_path)) {
							MessageBoxW(dlg, L"Destination folder is not empty", null, MB_TOPMOST | MB_ICONERROR);
							break;
						}
					} else {
//[c]						Creating a folder to check if it possible to create one
						WCHAR wpath[MAX_PATH];
						utf8_to_utf16(install_path, -1, wpath, MAX_PATH);
						if (CreateDirectoryW(wpath, null)) {
							folder_delete(install_path);
						} else if (GetLastError() != ERROR_ACCESS_DENIED) {
							MessageBoxW(dlg, L"Destination folder cannot be made", null, MB_TOPMOST | MB_ICONERROR);
							break;
						}
					}
					install_start = true;
					EndDialog(dlg, 0);
					return (true);
				} break;
			}
		} break;
		case WM_NOTIFY: {
			NMHDR* param = cast(NMHDR*, lparam);
			if ((param->code == NM_CLICK) || (param->code == NM_RETURN)) {
				NMLINK* link = cast(NMLINK*, lparam);
				if (link->hdr.idFrom == ID_SYSLINK_LICENSE) {
					ShellExecuteW(null, L"open", link->item.szUrl, null, null, SW_NORMAL);
				}
			}
		} break;
	}
	return (false);
}

void parse_vsmanifest(const char* path, bool preview)
{
	println("Parsing {s} Visual Studio Manifest...", preview ? "Preview" : "Release");
	char manifest_url[count_of(release_manifest_url)];
	char license_url[count_of(release_license_url)];
	json_context jc;
	jc.f = file_open(path, file_mode_read);
	json_parser p = json_parser_begin(json_file_peek, &jc);
	hope(json_object_key_find(&p, "channelItems"), "channelItems not found");
	while (json_array_next(&p)) {
		json_context obj_state = jc;
		if (!json_object_key_find(&p, "id")) {
			json_object_skip(&p);
			continue;
		}
		char id[64];
		json_string_extract(&p, array_expand(id));
		if (string_is(id, preview ? "Microsoft.VisualStudio.Manifests.VisualStudioPreview" : "Microsoft.VisualStudio.Manifests.VisualStudio")) {
			json_file_context_restore(&jc, obj_state);
			hope(json_object_key_find(&p, "payloads"), "payloads not found");
			json_array_next(&p);
			hope(json_object_key_find(&p, "url"), "manifest url not found");
			json_string_extract(&p, array_expand(manifest_url));
			json_object_skip(&p);
			json_array_next(&p);
		}
		if (string_is(id, "Microsoft.VisualStudio.Product.BuildTools")) {
			json_file_context_restore(&jc, obj_state);
			hope(json_object_key_find(&p, "localizedResources"), "localizedResources not found");
			while (json_array_next(&p)) {
				json_context res_state = jc;
				hope(json_object_key_find(&p, "language"), "license language not found");
				char language[8];
				json_string_extract(&p, array_expand(language));
				if (string_is(language, "en-us")) {
					json_file_context_restore(&jc, res_state);
					hope(json_object_key_find(&p, "license"), "license url not found");
					json_string_extract(&p, array_expand(license_url));
					json_object_skip(&p);
					break;
				}
				json_object_skip(&p);
			}
		}
		json_object_skip(&p);
	}
	file_close(&jc.f);
	mem_copy(preview ? preview_manifest_url : release_manifest_url, manifest_url, size_of(manifest_url));
	mem_copy(preview ? preview_license_url : release_license_url, license_url, size_of(license_url));
}

void parse_manifest(const char* path, bool preview)
{
	println("Parsing {s} Build Tool Manifest...", preview ? "Preview" : "Release");
	char msvc_versions[32][16] = {0};
	i64 msvc_versions_count = 0;
	char sdk_versions[16][8] = {0};
	i64 sdk_versions_count = 0;
	json_context jc;
	jc.f = file_open(path, file_mode_read);
	json_parser p = json_parser_begin(json_file_peek, &jc);
	hope(json_object_key_find(&p, "packages"), "packages not found");
	while (json_array_next(&p)) {
		hope(json_object_key_find(&p, "id"), "id not found");
		char id[64];
		json_string_extract(&p, array_expand(id));
		char id_lower[count_of(id)];
		mem_copy(id_lower, id, size_of(id));
		string_lower(id_lower);
		bool is_msvc_version = true;
		is_msvc_version = string_trim_start(id_lower, "microsoft.visualstudio.component.vc.") ? is_msvc_version : false;
		is_msvc_version = string_trim_end(id_lower, ".x86.x64") ? is_msvc_version : false;
		bool is_sdk_version = false;
		is_sdk_version = string_trim_start(id_lower, "microsoft.visualstudio.component.windows10sdk.") ? true : is_sdk_version;
		is_sdk_version = string_trim_start(id_lower, "microsoft.visualstudio.component.windows11sdk.") ? true : is_sdk_version;
		bool is_digit = string_has_only(id_lower, string_to_array(".0123456789"));
		if (is_msvc_version & is_digit) {
			string_copy(array_expand(msvc_versions[msvc_versions_count]), id_lower);
			msvc_versions_count++;
		}
		if (is_sdk_version & is_digit) {
			string_copy(array_expand(sdk_versions[sdk_versions_count]), id_lower);
			sdk_versions_count++;
		}
		json_object_skip(&p);
	}
	file_close(&jc.f);
	mem_copy(preview ? preview_sdk_versions : release_sdk_versions, sdk_versions, size_of(sdk_versions));
	mem_copy(preview ? preview_msvc_versions : release_msvc_versions, msvc_versions, size_of(msvc_versions));
	release_msvc_versions_count = preview ? release_msvc_versions_count : msvc_versions_count;
	preview_msvc_versions_count = preview ? msvc_versions_count : preview_msvc_versions_count;
	release_sdk_versions_count = preview ? release_sdk_versions_count : sdk_versions_count;
	preview_sdk_versions_count = preview ? sdk_versions_count : preview_sdk_versions_count;
}

bool download_file(const char* file_url, const char* file_path, const char* display_name)
{
	WCHAR wurl[L_MAX_URL_LENGTH];
	utf8_to_utf16(file_url, -1, wurl, count_of(wurl));
	DWORD flags = INTERNET_FLAG_DONT_CACHE;
	if(string_starts_with(file_url, "https://")) {
		flags |= INTERNET_FLAG_SECURE;
	}
	HINTERNET connection = InternetOpenUrlW(internet_session, wurl, null, 0, flags, 0);
	if (!connection) {
		println("open url failed");
		return (false);
	}
	DWORD content_size;
	if (!HttpQueryInfoW(connection, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &content_size, &(DWORD){sizeof(content_size)}, null)) {
		content_size = 0;
	}
	file_create(file_path);
	file_handle f = file_open(file_path, file_mode_write);
	i64 current_size = 0;
	DWORD n = 0;
	char chunk[16 * mem_page_size];
	while (true) {
		if (!InternetReadFile(connection, chunk, count_of(chunk), &n)){
			println("read file failed");
			return (false);
		}
		if (n == 0) {
			break;
		}
		file_write(&f, chunk, n);
		current_size += n;
		if (content_size > 0) {
			i64 p = (current_size * 100) / content_size;
			p = clamp_top(p, 100);
			print("\r[{i64}%] {s}", p, display_name);
		} else {
			print("\r[{i64}kb] {s}", current_size / mem_kilobyte, display_name);
		}
	}
	if (content_size > 0) {
		print("\r[100%] {s}", display_name);
	}
	println("");
	file_close(&f);
	InternetCloseHandle(connection);
	return (true);
}

void extract_payload_info(json_context* jc, json_parser* p, char (*file_name)[MAX_PATH * 3], char (*url)[L_MAX_URL_LENGTH * 3])
{
//[c]	TODO: extract and validate sha256
	json_context payload_state = *jc;
	hope(json_object_key_find(p, "fileName"), "fileName not found");
	json_string_extract(p, array_expand(*file_name));
	json_file_context_restore(jc, payload_state);
	hope(json_object_key_find(p, "url"), "url not found");
	json_string_extract(p, array_expand(*url));
	json_file_context_restore(jc, payload_state);
}

void install(void)
{
//[c]	Create temporary folders
	char msvc_path[MAX_PATH * 3];
	char sdk_path[MAX_PATH * 3];
	string_format(array_expand(msvc_path), "{s}\\msvc", temp_path);
	string_format(array_expand(sdk_path), "{s}\\sdk", temp_path);
	folder_create(msvc_path);
	folder_create(sdk_path);
	{
		char msvc_packages[32][128];
		i64 msvc_packages_count = 0;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.visualcpp.dia.sdk");
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.crt.headers.base", msvc_version);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.crt.source.base", msvc_version);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.asan.headers.base", msvc_version);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.premium.tools.{s}.base.base", msvc_version, host_arch);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.pgo.headers.base", msvc_version);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.tools.host{s}.target{s}.base", msvc_version, host_arch, target_arch);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.tools.host{s}.target{s}.res.base", msvc_version, host_arch, target_arch);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.crt.{s}.desktop.base", msvc_version, target_arch);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.crt.{s}.store.base", msvc_version, target_arch);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.crt.redist.{s}.base", msvc_version, target_arch);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.premium.tools.host{s}.target{s}.base", msvc_version, host_arch, target_arch);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.pgo.{s}.base", msvc_version, target_arch);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.asan.x86.base", msvc_version);
		msvc_packages_count++;
		string_format(array_expand(msvc_packages[msvc_packages_count]), "microsoft.vc.{s}.asan.x64.base", msvc_version);
		msvc_packages_count++;
		char sdk_packages[16][128];
		i64 sdk_packages_count = 0;
		string_format(array_expand(sdk_packages[sdk_packages_count]), "Windows SDK for Windows Store Apps Tools-x86_en-us.msi");
		sdk_packages_count++;
		string_format(array_expand(sdk_packages[sdk_packages_count]), "Windows SDK for Windows Store Apps Headers-x86_en-us.msi");
		sdk_packages_count++;
		string_format(array_expand(sdk_packages[sdk_packages_count]), "Windows SDK for Windows Store Apps Headers OnecoreUap-x86_en-us.msi");
		sdk_packages_count++;
		string_format(array_expand(sdk_packages[sdk_packages_count]), "Windows SDK for Windows Store Apps Libs-x86_en-us.msi");
		sdk_packages_count++;
		string_format(array_expand(sdk_packages[sdk_packages_count]), "Windows SDK OnecoreUap Headers x86-x86_en-us.msi");
		sdk_packages_count++;
		string_format(array_expand(sdk_packages[sdk_packages_count]), "Windows SDK Desktop Headers x86-x86_en-us.msi");
		sdk_packages_count++;
		string_format(array_expand(sdk_packages[sdk_packages_count]), "Universal CRT Headers Libraries and Sources-x86_en-us.msi");
		sdk_packages_count++;
		string_format(array_expand(sdk_packages[sdk_packages_count]), "Windows SDK Desktop Libs {s}-x86_en-us.msi", target_arch);
		sdk_packages_count++;
		char sdk_package[32] = {0};
		json_context jc;
		char manifest_path[MAX_PATH * 3];
		string_format(array_expand(manifest_path), "{s}\\{s}", temp_path, is_preview ? "BuildToolsManifest.Preview.json" : "BuildToolsManifest.Release.json");
		println("Parsing {s} Build Tool Manifest...", is_preview ? "Preview" : "Release");
		jc.f = file_open(manifest_path, file_mode_read);
		json_parser p = json_parser_begin(json_file_peek, &jc);
		hope(json_object_key_find(&p, "packages"), "packages not found");
		json_context packages_state = jc;
		while (json_array_next(&p)) {
			hope(json_object_key_find(&p, "id"), "id not found");
			char id[64];
			json_string_extract(&p, array_expand(id));
			char id_lower[count_of(id)];
			mem_copy(id_lower, id, size_of(id));
			string_lower(id_lower);
			bool is_sdk = false;
			is_sdk = string_starts_with(id_lower, "microsoft.visualstudio.component.windows10sdk.") ? true : is_sdk;
			is_sdk = string_starts_with(id_lower, "microsoft.visualstudio.component.windows11sdk.") ? true : is_sdk;
			if (is_sdk & string_ends_with(id_lower, sdk_version)) {
				hope(json_object_key_find(&p, "dependencies"), "dependencies not found");
				hope(json_object_next(&p), "dependencies is empty");
				json_object_key_extract(&p, array_expand(sdk_package));
				break;
			}
			json_object_skip(&p);
		}
		json_file_context_restore(&jc, packages_state);
		println("Downloading the packages...");
		while (json_array_next(&p)) {
			json_context pkg_state = jc;
			hope(json_object_key_find(&p, "id"), "id not found");
			char id[64];
			json_string_extract(&p, array_expand(id));
			json_file_context_restore(&jc, pkg_state);
			char lang[8] = {0};
			if (json_object_key_find(&p, "language")) {
				json_string_extract(&p, array_expand(lang));
			}
			json_file_context_restore(&jc, pkg_state);
			char id_lower[count_of(id)];
			mem_copy(id_lower, id, size_of(id));
			string_lower(id_lower);
			bool msvc_pkg = false;
			for (i64 i = 0; i < msvc_packages_count; i++) {
				msvc_pkg = string_is(id_lower, msvc_packages[i]) ? true : msvc_pkg;
			}
			msvc_pkg = (string_is(lang, "en-US") | string_is(lang, "")) ? msvc_pkg : false;
			if (msvc_pkg) {
				hope(json_object_key_find(&p, "payloads"), "payloads not found");
				while (json_array_next(&p)) {
					char file_name[MAX_PATH * 3];
					char url[L_MAX_URL_LENGTH * 3];
					extract_payload_info(&jc, &p, &file_name, &url);
					json_object_skip(&p);
					char path[MAX_PATH * 3];
					string_format(array_expand(path), "{s}\\{s}", msvc_path, file_name);
					hope(download_file(url, path, file_name), "Failed to download package {s}", file_name);
				}
			}
			if (string_is(id, sdk_package)) {
				char msi_cabs[64][37];
				i64 msi_cabs_count = 0;
				hope(json_object_key_find(&p, "payloads"), "payloads not found");
				json_context payloads_state = jc;
//[c]				Download SDK payloads
				while (json_array_next(&p)) {
					char file_name[MAX_PATH * 3];
					char url[L_MAX_URL_LENGTH * 3];
					extract_payload_info(&jc, &p, &file_name, &url);
					json_object_skip(&p);
					if (string_trim_start(file_name, "Installers\\")) {
						bool sdk_pkg = false;
						for (i64 i = 0; i < sdk_packages_count; i++) {
							sdk_pkg = string_is(file_name, sdk_packages[i]) ? true : sdk_pkg;
						}
						if (sdk_pkg) {
							char path[MAX_PATH * 3];
							string_format(array_expand(path), "{s}\\{s}", sdk_path, file_name);
							hope(download_file(url, path, file_name), "Failed to download the package: {s}", file_name);
//[c]							Extract .cab file info
							file_handle msi = file_open(path, file_mode_read);
							char chunk[mem_page_size];
							while (true) {
								i64 n = file_read(&msi, array_expand(chunk));
								if (n <= 0) {
									break;
								}
								for (i64 i = 0; i < n; i++) {
									char str[4];
									str[0] = chunk[i];
									str[1] = ((i + 1) < n) ? chunk[i + 1] : 0;
									str[2] = ((i + 2) < n) ? chunk[i + 2] : 0;
									str[3] = ((i + 3) < n) ? chunk[i + 3] : 0;
									bool match = (str[0] == '.') & (str[1] == 'c') & (str[2] == 'a') & (str[3] == 'b');
									if (match) {
										i64 index = msi.pos - count_of(chunk) + i;
										file_handle cabs_reader = file_open(path, file_mode_read);
										file_seek(&cabs_reader, index - 32);
										file_read(&cabs_reader, msi_cabs[msi_cabs_count], 36);
										file_close(&cabs_reader);
										msi_cabs[msi_cabs_count][36] = 0;
										msi_cabs_count++;
									}
								}
								if (msi.pos == msi.size) {
									break;
								}
								file_seek(&msi, msi.pos - 3);
							}
							file_close(&msi);
						}
					}
				}
				json_file_context_restore(&jc, payloads_state);
//[c]				Download .cab packages
				while (json_array_next(&p)) {
					char file_name[MAX_PATH * 3];
					char url[L_MAX_URL_LENGTH * 3];
					extract_payload_info(&jc, &p, &file_name, &url);
					json_object_skip(&p);
					if (string_trim_start(file_name, "Installers\\")) {
						bool cab_pkg = false;
						for (i64 i = 0; i < msi_cabs_count; i++) {
							cab_pkg = string_is(file_name, msi_cabs[i]) ? true : cab_pkg;
						}
						if (cab_pkg) {
							char path[MAX_PATH * 3];
							string_format(array_expand(path), "{s}\\{s}", sdk_path, file_name);
							hope(download_file(url, path, file_name), "Failed to download the package: {s}", file_name);
						}
					}
				}
			}
			json_object_skip(&p);
		}
		file_close(&jc.f);
	}
	println("Installing MSVC...");
	{
		folder_handle msvc_folder = folder_open(msvc_path);
		char file_name[MAX_PATH * 3];
		while (folder_next(&msvc_folder, &file_name)) {
			char file_path[MAX_PATH * 3];
			string_format(array_expand(file_path), "{s}\\{s}", msvc_path, file_name);
			file_handle msvc_file = file_open(file_path, file_mode_read);
			mz_zip_archive zip_file = (mz_zip_archive){0};
			zip_file.m_pAlloc = miniz_malloc;
			zip_file.m_pFree = miniz_free;
			zip_file.m_pRealloc = miniz_realloc;
			zip_file.m_pRead = miniz_file_read;
			zip_file.m_pIO_opaque = &msvc_file;
			hope(mz_zip_reader_init(&zip_file, msvc_file.size, 0), "Failed to unzip file: {s}", file_name);
			i64 n = mz_zip_reader_get_num_files(&zip_file);
			for (i64 i = 0; i < n; i++) {
				char zipped_file_name[MAX_PATH * 3];
				if (mz_zip_reader_is_file_a_directory(&zip_file, i)) {
					continue;
				}
				mz_zip_reader_get_filename(&zip_file, i, zipped_file_name, count_of(zipped_file_name));
				if (!string_trim_start(zipped_file_name, "Contents/")) {
					continue;
				}
				char zipped_file_path[MAX_PATH * 3];
				string_format(array_expand(zipped_file_path), "{s}\\{s}", install_path, zipped_file_name);
//[c]				Go through the path, creating directories if needed, and converting forward slashes into backslashes
				char* s = zipped_file_path;
				while (*s != 0) {
					if (*s == '/') {
						*s = 0;
						if (!folder_exists(zipped_file_path)) {
							folder_create(zipped_file_path);
						}
						*s = '\\';
					}
					s++;
				}
				file_create(zipped_file_path);
				file_handle zf = file_open(zipped_file_path, file_mode_write);
				hope(mz_zip_reader_extract_to_callback(&zip_file, i, mz_file_write, &zf, 0), "Failed to extract file: {s}", zipped_file_path);
				file_close(&zf);
			}
			hope(mz_zip_end(&zip_file), "Failed to close zip file gracefully: {s}", file_name);
			file_close(&msvc_file);
		}
		folder_close(&msvc_folder);
	}
	println("Installing SDK...");
	{
		folder_handle sdk_folder = folder_open(sdk_path);
		char file_name[MAX_PATH * 3];
		while (folder_next(&sdk_folder, &file_name)) {
			if (!string_ends_with(file_name, ".msi")) {
				continue;
			}
			char file_path[MAX_PATH * 3];
			string_format(array_expand(file_path), "{s}\\{s}", sdk_path, file_name);
			char params[MAX_CMDLINE_LEN * 3];
			string_format(array_expand(params), "/a \"{s}\" /quiet TARGETDIR=\"{s}\"", file_path, install_path);
			WCHAR wparams[MAX_CMDLINE_LEN];
			utf8_to_utf16(params, -1, wparams, count_of(wparams));
			SHELLEXECUTEINFOW info = (SHELLEXECUTEINFOW){0};
			info.cbSize = size_of(SHELLEXECUTEINFOW);
			info.fMask = SEE_MASK_NOCLOSEPROCESS;
			info.lpVerb = L"open";
			info.lpFile = L"msiexec.exe";
			info.lpParameters = wparams;
			info.nShow = SW_NORMAL;
			bool ok = ShellExecuteExW(&info);
			hope(ok, "Failed to run msiexec");
			WaitForSingleObject(info.hProcess, INFINITE);
		}
		folder_close(&sdk_folder);
	}
	char msvcv[MAX_PATH * 3];
	{
		string_format(array_expand(msvcv), "{s}\\VC\\Tools\\MSVC", install_path);
		folder_handle msvcv_folder = folder_open(msvcv);
		folder_next(&msvcv_folder, &msvcv);
		folder_close(&msvcv_folder);
	}
	char sdkv[MAX_PATH * 3];
	{
		string_format(array_expand(sdkv), "{s}\\Windows Kits\\10\\bin", install_path);
		folder_handle sdkv_folder = folder_open(sdkv);
		folder_next(&sdkv_folder, &sdkv);
		folder_close(&sdkv_folder);
	}
	println("Installing debug info...");
	char redist_path[MAX_PATH * 3];
	string_format(array_expand(redist_path), "{s}\\VC\\Redist", install_path);
	if (folder_exists(redist_path)) {
		char debug_path[MAX_PATH * 3];
		string_format(array_expand(debug_path), "{s}\\MSVC\\{s}\\debug_nonredist", redist_path, msvcv);
		folder_handle debug_folder = folder_open(debug_path);
		char debug_target[MAX_PATH * 3];
		while (folder_next(&debug_folder, &debug_target)) {
			char debug_target_path[MAX_PATH * 3];
			string_format(array_expand(debug_target_path), "{s}\\{s}", debug_path, debug_target);
			folder_handle debug_target_folder = folder_open(debug_target_path);
			char debug_package[MAX_PATH * 3];
			while (folder_next(&debug_target_folder, &debug_package)) {
				char debug_package_path[MAX_PATH * 3];
				string_format(array_expand(debug_package_path), "{s}\\{s}", debug_target_path, debug_package);
				folder_handle debug_package_folder = folder_open(debug_package_path);
				char file_name[MAX_PATH * 3];
				while (folder_next(&debug_package_folder, &file_name)) {
					char file_path[MAX_PATH * 3];
					string_format(array_expand(file_path), "{s}\\{s}", debug_package_path, file_name);
					char dst_file_path[MAX_PATH * 3];
					string_format(array_expand(dst_file_path), "{s}\\VC\\Tools\\MSVC\\{s}\\bin\\Host{s}\\{s}\\{s}", install_path, msvcv, host_arch, debug_target, file_name);
					file_create(dst_file_path);
					file_copy(dst_file_path, file_path);
				}
				folder_close(&debug_package_folder);
			}
			folder_close(&debug_target_folder);
		}
		folder_close(&debug_folder);
	}
	println("Installing DIA...");
	{
		char msdia[64] = {0};
		if (string_is(host_arch, "x86")) {
			string_copy(array_expand(msdia), "msdia140.dll");
		} else if (string_is(host_arch, "x64")) {
			string_copy(array_expand(msdia), "amd64\\msdia140.dll");
		} else if (string_is(host_arch, "arm")) {
			string_copy(array_expand(msdia), "arm\\msdia140.dll");
		} else if (string_is(host_arch, "arm64")) {
			string_copy(array_expand(msdia), "arm64\\msdia140.dll");
		}
		char msdia_path[MAX_PATH * 3];
		string_format(array_expand(msdia_path), "{s}\\DIA%20SDK\\bin\\{s}", install_path, msdia);
		char dst_path[MAX_PATH * 3];
		string_format(array_expand(dst_path), "{s}\\VC\\Tools\\MSVC\\{s}\\bin\\Host{s}", install_path, msvcv, host_arch);
		folder_handle dst_folder = folder_open(dst_path);
		char dst_target[MAX_PATH * 3];
		while (folder_next(&dst_folder, &dst_target)) {
			char dst_file_path[MAX_PATH * 3];
			string_format(array_expand(dst_file_path), "{s}\\{s}\\msdia140.dll", dst_path, dst_target);
			file_create(dst_file_path);
			file_copy(dst_file_path, msdia_path);
		}
		folder_close(&dst_folder);
	}
	println("Creating a batch setup script...");
	{
		char bat_path[MAX_PATH * 3];
		string_format(array_expand(bat_path), "{s}\\devcmd.bat", install_path, target_arch);
		file_create(bat_path);
		file_handle f = file_open(bat_path, file_mode_write);
		char buf[mem_page_size];
		file_write(&f, string_to_array("@echo off\n"));
		file_write(&f, string_to_array("set BUILD_TOOLS_ROOT=%~dp0\n"));
		file_write(&f, string_to_array("set WindowsSDKDir=%BUILD_TOOLS_ROOT%Windows Kits\\10\n"));
		string_format(array_expand(buf), "set VCToolsInstallDir=%BUILD_TOOLS_ROOT%VC\\Tools\\MSVC\\{s}\n", msvcv);
		file_write(&f, buf, string_count(buf));
		string_format(array_expand(buf), "set WindowsSDKVersion={s}\n", sdkv);
		file_write(&f, buf, string_count(buf));
		string_format(array_expand(buf), "set VSCMD_ARG_TGT_ARCH={s}\n", target_arch);
		file_write(&f, buf, string_count(buf));
		string_format(array_expand(buf), "set VSCMD_ARG_HOST_ARCH={s}\n", host_arch);
		file_write(&f, buf, string_count(buf));
		file_write(&f, string_to_array("set INCLUDE=%VCToolsInstallDir%\\include;"));
		file_write(&f, string_to_array("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\ucrt;"));
		file_write(&f, string_to_array("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\shared;"));
		file_write(&f, string_to_array("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\um;"));
		file_write(&f, string_to_array("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\winrt;"));
		file_write(&f, string_to_array("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\cppwinrt;\n"));
		file_write(&f, string_to_array("set LIB=%VCToolsInstallDir%\\lib\\%VSCMD_ARG_TGT_ARCH%;"));
		file_write(&f, string_to_array("%WindowsSDKDir%\\Lib\\%WindowsSDKVersion%\\ucrt\\%VSCMD_ARG_TGT_ARCH%;"));
		file_write(&f, string_to_array("%WindowsSDKDir%\\Lib\\%WindowsSDKVersion%\\um\\%VSCMD_ARG_TGT_ARCH%\n"));
		file_write(&f, string_to_array("set BUILD_TOOLS_BIN=%VCToolsInstallDir%\\bin\\Host%VSCMD_ARG_HOST_ARCH%\\%VSCMD_ARG_TGT_ARCH%;"));
		file_write(&f, string_to_array("%WindowsSDKDir%\\bin\\%WindowsSDKVersion%\\%VSCMD_ARG_TGT_ARCH%;"));
		file_write(&f, string_to_array("%WindowsSDKDir%\\bin\\%WindowsSDKVersion%\\%VSCMD_ARG_TGT_ARCH%\\ucrt\n"));
		file_write(&f, string_to_array("set PATH=%BUILD_TOOLS_BIN%;%PATH%\n"));
		file_close(&f);
	}
	println("Creating a PowerShell setup script...");
	{
		char bat_path[MAX_PATH * 3];
		string_format(array_expand(bat_path), "{s}\\devcmd.ps1", install_path, target_arch);
		file_create(bat_path);
		file_handle f = file_open(bat_path, file_mode_write);
		char buf[mem_page_size];
		file_write(&f, string_to_array("#Requires -Version 5\n"));
		file_write(&f, string_to_array("param([string]$InstallPath = $PSScriptRoot)\n"));
		file_write(&f, string_to_array("$env:BUILD_TOOLS_ROOT = $InstallPath\n"));
		file_write(&f, string_to_array("$env:WindowsSDKDir = (Join-Path $InstallPath '\\Windows Kits\\10')\n"));
		file_write(&f, string_to_array("$VCToolsVersion = (Get-ChildItem -Directory (Join-Path $InstallPath '\\VC\\Tools\\MSVC' | Sort-Object -Descending LastWriteTime | Select-Object -First 1) -ErrorAction SilentlyContinue).Name\n"));
		file_write(&f, string_to_array("if (!$VCToolsVersion) { throw 'VCToolsVersion cannot be determined.' }\n"));
		file_write(&f, string_to_array("$env:VCToolsInstallDir = Join-Path $InstallPath '\\VC\\Tools\\MSVC' $VCToolsVersion\n"));
		file_write(&f, string_to_array("$env:WindowsSDKVersion = (Get-ChildItem -Directory (Join-Path $env:WindowsSDKDir 'bin') -ErrorAction SilentlyContinue | Sort-Object -Descending LastWriteTime | Select-Object -First 1).Name\n"));
		file_write(&f, string_to_array("if (!$env:WindowsSDKVersion ) { throw 'WindowsSDKVersion cannot be determined.' }\n"));
		string_format(array_expand(buf), "$env:VSCMD_ARG_TGT_ARCH = '{s}'\n", target_arch);
		file_write(&f, buf, string_count(buf));
		string_format(array_expand(buf), "$env:VSCMD_ARG_HOST_ARCH = '{s}'\n", host_arch);
		file_write(&f, buf, string_count(buf));
		file_write(&f, string_to_array("'Portable Build Tools environment started.'\n"));
		file_write(&f, string_to_array("'* BUILD_TOOLS_ROOT   : {0}' -f $env:BUILD_TOOLS_ROOT\n"));
		file_write(&f, string_to_array("'* WindowsSDKDir      : {0}' -f $env:WindowsSDKDir\n"));
		file_write(&f, string_to_array("'* WindowsSDKVersion  : {0}' -f $env:WindowsSDKVersion\n"));
		file_write(&f, string_to_array("'* VCToolsInstallDir  : {0}' -f $env:VCToolsInstallDir\n"));
		file_write(&f, string_to_array("'* VSCMD_ARG_TGT_ARCH : {0}' -f $env:VSCMD_ARG_TGT_ARCH\n"));
		file_write(&f, string_to_array("'* VSCMD_ARG_HOST_ARCH: {0}' -f $env:VSCMD_ARG_HOST_ARCH\n"));
		file_write(&f, string_to_array("$env:INCLUDE =\"$env:VCToolsInstallDir\\include;"));
		file_write(&f, string_to_array("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\ucrt;"));
		file_write(&f, string_to_array("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\shared;"));
		file_write(&f, string_to_array("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\um;"));
		file_write(&f, string_to_array("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\winrt;"));
		file_write(&f, string_to_array("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\cppwinrt\"\n"));
		file_write(&f, string_to_array("$env:LIB = \"$env:VCToolsInstallDir\\lib\\$env:VSCMD_ARG_TGT_ARCH;"));
		file_write(&f, string_to_array("$env:WindowsSDKDir\\Lib\\$env:WindowsSDKVersion\\ucrt\\$env:VSCMD_ARG_TGT_ARCH;"));
		file_write(&f, string_to_array("$env:WindowsSDKDir\\Lib\\$env:WindowsSDKVersion\\um\\$env:VSCMD_ARG_TGT_ARCH\"\n"));
		file_write(&f, string_to_array("$env:BUILD_TOOLS_BIN = \"$env:VCToolsInstallDir\\bin\\Host$env:VSCMD_ARG_HOST_ARCH\\$env:VSCMD_ARG_TGT_ARCH;"));
		file_write(&f, string_to_array("$env:WindowsSDKDir\\bin\\$env:WindowsSDKVersion\\$env:VSCMD_ARG_TGT_ARCH;"));
		file_write(&f, string_to_array("$env:WindowsSDKDir\\bin\\$env:WindowsSDKVersion\\$env:VSCMD_ARG_TGT_ARCH\\ucrt\"\n"));
		file_write(&f, string_to_array("$env:PATH = \"$env:BUILD_TOOLS_BIN;$env:PATH\"\n"));
		file_close(&f);
	}
	println("Cleanup...");
	{
		if (folder_exists(redist_path)) {
			folder_delete(redist_path);
		}
		folder_handle install_folder = folder_open(install_path);
		char file_name[MAX_PATH * 3];
		while (folder_next(&install_folder, &file_name)) {
			char file_path[MAX_PATH * 3];
			string_format(array_expand(file_path), "{s}\\{s}", install_path, file_name);
			if (string_ends_with(file_name, ".msi")) {
				file_delete(file_path);
			}
			if (string_starts_with(file_name, "DIA%20SDK")) {
				folder_delete(file_path);
			}
			if (string_is(file_name, "Common7")) {
				folder_delete(file_path);
			}
		}
		folder_close(&install_folder);
		char folder_to_remove[MAX_PATH * 3];
		string_format(array_expand(folder_to_remove), "{s}\\VC\\Tools\\MSVC\\{s}\\Auxiliary", install_path, msvcv);
		folder_delete(folder_to_remove);
		string_format(array_expand(folder_to_remove), "{s}\\VC\\Tools\\MSVC\\{s}\\bin\\Host{s}\\{s}\\onecore", install_path, msvcv, target_arch, host_arch);
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(array_expand(folder_to_remove), "{s}\\VC\\Tools\\MSVC\\{s}\\lib\\{s}\\store", install_path, msvcv, target_arch);
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(array_expand(folder_to_remove), "{s}\\VC\\Tools\\MSVC\\{s}\\lib\\{s}\\uwp", install_path, msvcv, target_arch);
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(array_expand(folder_to_remove), "{s}\\VC\\Tools\\MSVC\\{s}\\lib\\{s}\\enclave", install_path, msvcv, target_arch);
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(array_expand(folder_to_remove), "{s}\\VC\\Tools\\MSVC\\{s}\\lib\\{s}\\onecore", install_path, msvcv, target_arch);
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(array_expand(folder_to_remove), "{s}\\Windows Kits\\10\\Catalogs", install_path);
		folder_delete(folder_to_remove);
		string_format(array_expand(folder_to_remove), "{s}\\Windows Kits\\10\\DesignTime", install_path);
		folder_delete(folder_to_remove);
		string_format(array_expand(folder_to_remove), "{s}\\Windows Kits\\10\\bin\\{s}\\chpe", install_path, sdkv);
		folder_delete(folder_to_remove);
		string_format(array_expand(folder_to_remove), "{s}\\Windows Kits\\10\\Lib\\{s}\\ucrt_enclave", install_path, sdkv);
		folder_delete(folder_to_remove);	
		{
			string_format(array_expand(folder_to_remove), "{s}\\Windows Kits\\10\\Lib\\{s}\\ucrt", install_path, sdkv);
			folder_handle f = folder_open(folder_to_remove);
			char file_name[MAX_PATH * 3];
			while (folder_next(&f, &file_name)) {
				if (string_is(file_name, target_arch)) {
					continue;
				}
				char file_path[MAX_PATH * 3];
				string_format(array_expand(file_path), "{s}\\{s}", folder_to_remove, file_name);
				folder_delete(file_path);
			}
			folder_close(&f);
		}
		{
			string_format(array_expand(folder_to_remove), "{s}\\Windows Kits\\10\\Lib\\{s}\\um", install_path, sdkv);
			folder_handle f = folder_open(folder_to_remove);
			char file_name[MAX_PATH * 3];
			while (folder_next(&f, &file_name)) {
				if (string_is(file_name, target_arch)) {
					continue;
				}
				char file_path[MAX_PATH * 3];
				string_format(array_expand(file_path), "{s}\\{s}", folder_to_remove, file_name);
				folder_delete(file_path);
			}
			folder_close(&f);
		}
		{
			string_format(array_expand(folder_to_remove), "{s}\\VC\\Tools\\MSVC\\{s}\\bin", install_path, msvcv);
			folder_handle f = folder_open(folder_to_remove);
			char file_name[MAX_PATH * 3];
			while (folder_next(&f, &file_name)) {
				if (string_ends_with(file_name, host_arch)) {
					continue;
				}
				char file_path[MAX_PATH * 3];
				string_format(array_expand(file_path), "{s}\\{s}", folder_to_remove, file_name);
				folder_delete(file_path);
			}
			folder_close(&f);
		}
		{
			string_format(array_expand(folder_to_remove), "{s}\\Windows Kits\\10\\bin\\{s}", install_path, sdkv);
			folder_handle f = folder_open(folder_to_remove);
			char file_name[MAX_PATH * 3];
			while (folder_next(&f, &file_name)) {
				if (string_is(file_name, host_arch)) {
					continue;
				}
				char file_path[MAX_PATH * 3];
				string_format(array_expand(file_path), "{s}\\{s}", folder_to_remove, file_name);
				folder_delete(file_path);
			}
			folder_close(&f);
		}
		char telemetry_path[MAX_PATH * 3];
		string_format(array_expand(telemetry_path), "{s}\\VC\\Tools\\MSVC\\{s}\\bin\\Host{s}\\{s}\\vctip.exe", install_path, msvcv, host_arch, target_arch);
		file_delete(telemetry_path);
		folder_delete(temp_path);
	}
	if (env_mode != 0) {
		println("Setting up the environment...");
		HKEY location = (env_mode == 2) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
		LPCWSTR subkey = (env_mode == 2) ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment" : L"Environment";
		char buf[MAX_ENV_LEN * 3];
		string_format(array_expand(buf), "{s}\\VC\\Tools\\MSVC\\{s}", install_path, msvcv);
		env_set(location, subkey, L"VCToolsInstallDir", buf);
		string_format(array_expand(buf), "{s}\\Windows Kits\\10", install_path);
		env_set(location, subkey, L"WindowsSDKDir", buf);
		env_set(location, subkey, L"WindowsSDKVersion", sdkv);
		env_set(location, subkey, L"VSCMD_ARG_HOST_ARCH", host_arch);
		env_set(location, subkey, L"VSCMD_ARG_TGT_ARCH", target_arch);
		string_format(array_expand(buf),
			"{s}\\VC\\Tools\\MSVC\\{s}\\include;"
			"{s}\\Windows Kits\\10\\Include\\{s}\\ucrt;"
			"{s}\\Windows Kits\\10\\Include\\{s}\\shared;"
			"{s}\\Windows Kits\\10\\Include\\{s}\\um;"
			"{s}\\Windows Kits\\10\\Include\\{s}\\winrt;"
			"{s}\\Windows Kits\\10\\Include\\{s}\\cppwinrt",
			install_path, msvcv,
			install_path, sdkv,
			install_path, sdkv,
			install_path, sdkv,
			install_path, sdkv,
			install_path, sdkv
		);
		env_set(location, subkey, L"INCLUDE", buf);
		string_format(array_expand(buf),
			"{s}\\VC\\Tools\\MSVC\\{s}\\lib\\{s};"
			"{s}\\Windows Kits\\10\\Lib\\{s}\\ucrt\\{s};"
			"{s}\\Windows Kits\\10\\Lib\\{s}\\um\\{s}",
			install_path, msvcv, target_arch,
			install_path, sdkv, target_arch,
			install_path, sdkv, target_arch
		);
		env_set(location, subkey, L"LIB", buf);
		string_format(array_expand(buf),
			"{s}\\VC\\Tools\\MSVC\\{s}\\bin\\Host{s}\\{s};"
			"{s}\\Windows Kits\\10\\bin\\{s}\\{s};"
			"{s}\\Windows Kits\\10\\bin\\{s}\\{s}\\ucrt\n",
			install_path, msvcv, host_arch, target_arch,
			install_path, sdkv, target_arch,
			install_path, sdkv, target_arch
		);
		env_set(location, subkey, L"BUILD_TOOLS_BIN", buf);
		WCHAR wpath[MAX_ENV_LEN];
		char path[count_of(wpath) * 3];
		DWORD wpath_size = size_of(wpath);
		LSTATUS err = RegGetValueW(location, subkey, L"Path", RRF_RT_REG_SZ | RRF_NOEXPAND, null, wpath, &wpath_size);
		err = (err == ERROR_UNSUPPORTED_TYPE) ? 0 : err;
		i64 wpath_count = (err != 0) ? 0 : (wpath_size / size_of(WCHAR));
		utf16_to_utf8(wpath, wpath_count, path, count_of(path));
//[c]		Split path into strings by ;
		i64 path_count = string_count(path);
		for (i64 i = 0; i < path_count; i++) {
			path[i] = (path[i] == ';') ? 0 : path[i];
		}
//[c]		Check if %BUILD_TOOLS_BIN% is among them
		bool has_build_tools_bin = false;
		for (i64 i = 0; i < path_count; i++) {
			char* entry = &path[i];
			has_build_tools_bin = string_is(entry, "%BUILD_TOOLS_BIN%") ? true : has_build_tools_bin;
			i += string_count(entry);
		}
//[c]		Join the strings back
		for (i64 i = 0; i < path_count; i++) {
			path[i] = (path[i] == 0) ? ';' : path[i];
		}
//[c]		Add %BUILD_TOOLS_BIN% if not present
		string_format(array_expand(path), "{s};{s}", path, has_build_tools_bin ? "" : "%BUILD_TOOLS_BIN%");
		string_trim_end(path, ";");
		string_trim_start(path, ";");
		{
			WCHAR wpath[MAX_ENV_LEN];
			int n = utf8_to_utf16(path, -1, wpath, count_of(wpath));
			RegSetKeyValueW(location, subkey, L"Path", REG_EXPAND_SZ, wpath, n * size_of(WCHAR));
		}
	}
	println("Done!");
}

int start(void)
{
	panic = console_panic;
	bool is_subprocess = false;
	bool list_versions = false;
	{
		i64 arg_pos = 0;
		for (i64 i = 0; i < argc; i++) {
			char* arg = &args[arg_pos];
			i64 arg_count = string_count(arg);
			arg_pos += arg_count + 1;
			if (i == 0) {
				continue;
			}
			if (arg_count == 0) {
				continue;
			}
			if (string_is(arg, "!")) {
				is_subprocess = true;
			} else if (string_is(arg, "gui")) {
				is_cli = false;
			} else if (string_is(arg, "list")) {
				list_versions = true;
			} else if (string_is(arg, "accept_license")) {
				license_accepted = true;
			} else if (string_is(arg, "preview")) {
				is_preview = true;
			} else if (string_starts_with(arg, "msvc=")) {
				string_trim_start(arg, "msvc=");
				msvc_version = arg;
			} else if (string_starts_with(arg, "sdk=")) {
				string_trim_start(arg, "sdk=");
				sdk_version = arg;
			} else if (string_starts_with(arg, "target=")) {
				string_trim_start(arg, "target=");
				target_arch = arg;
			} else if (string_starts_with(arg, "host=")) {
				string_trim_start(arg, "host=");
				host_arch = arg;
			} else if (string_starts_with(arg, "env=")) {
				string_trim_start(arg, "env=");
				if (string_is(arg, "none")) {
					env_mode = 0;
				} else if (string_is(arg, "user")) {
					env_mode = 1;
				} else if (string_is(arg, "global")) {
					env_mode = 2;
				} else {
					hopeless("Invalid environment string: {s}", arg);
				}
			} else if (string_starts_with(arg, "path=")) {
				string_trim_start(arg, "path=");
				string_copy(array_expand(install_path), arg);
				string_trim_end(install_path, "\"");
				string_trim_start(install_path, "\"");
			} else {
				print(
				"usage: PortableBuildTools.exe [gui] [list] [accept_license] [preview] [msvc=MSVC version] [sdk=SDK version] [target=x64/x86/arm/arm64] [host=x64/x86/arm64] [env=none/user/global] [path=\"C:\\BuildTools\"]\n",
					"\n",
					"*gui: run PortableBuildTools in GUI mode\n",
					"*list: show all available Build Tools versions\n",
					"*accept_license: auto-accept the license [default: ask]\n",
					"*preview: install from the Preview channel, instead of the Release channel [default: Release channel]\n",
					"*msvc: which MSVC toolchain version to install [default: whatever is latest]\n",
					"*sdk: which Windows SDK version to install [default: whatever is latest]\n",
					"*target: target architecture [default: x64]\n",
					"*host: host architecture [default: x64]\n",
					"*env: if supplied, then the installed path will be added to PATH environment variable, for the current user or for all users [default: none]\n"
					"*path: installation path [default: C:\\BuildTools]\n"
				);
				cleanup();
				return (1);
			}
		}
	}
	if (is_subprocess || !is_cli) {
		panic = message_box_panic;
	}
	CoInitializeEx(null, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	internet_session = InternetOpenW(L"PortableBuildTools/2.0", INTERNET_OPEN_TYPE_PRECONFIG, null, null, 0);
	hope(internet_session != null, "Failed to open internet session");
	{
		WCHAR wtemp_path[MAX_PATH];
		GetTempPathW(count_of(wtemp_path), wtemp_path);
		utf16_to_utf8(wtemp_path, -1, temp_path, count_of(temp_path));
		string_format(array_expand(temp_path), "{s}BuildTools", temp_path);
	}
	if (is_subprocess) {
		folder_create(install_path);
		install();
		WCHAR* msg = L"PortableBuildTools was installed successfully.";
		msg = (env_mode == 1) ? L"Please log out and back in to finish installation." : msg;
		msg = (env_mode == 2) ? L"Please reboot the machine to finish installation." : msg;
		MessageBoxW(GetConsoleWindow(), msg, L"Success!", MB_TOPMOST | MB_ICONINFORMATION);
		cleanup();
		return (0);
	}
//[c]	Download manifest files
	folder_create(temp_path);
	{
		println("Downloading manifest files...");
		char manifest_path[MAX_PATH * 3];
		string_format(array_expand(manifest_path), "{s}\\Manifest.Release.json", temp_path);
		hope(download_file(release_vsmanifest_url, manifest_path, "Manifest.Release.json"), "Failed to download release manifest");
		string_format(array_expand(manifest_path), "{s}\\Manifest.Preview.json", temp_path);
		hope(download_file(preview_vsmanifest_url, manifest_path, "Manifest.Preview.json"), "Failed to download preview manifest");
	}
	{
		println("Parsing manifest files...");
		char manifest_path[MAX_PATH * 3];
		string_format(array_expand(manifest_path), "{s}\\Manifest.Release.json", temp_path);
		parse_vsmanifest(manifest_path, false);
		string_format(array_expand(manifest_path), "{s}\\Manifest.Preview.json", temp_path);
		parse_vsmanifest(manifest_path, true);
	}
	{
		println("Downloading Build Tools manifest files...");
		char manifest_path[MAX_PATH * 3];
		string_format(array_expand(manifest_path), "{s}\\BuildToolsManifest.Release.json", temp_path);
		hope(download_file(release_manifest_url, manifest_path, "BuildToolsManifest.Release.json"), "Failed to download Build Tools release manifest");
		string_format(array_expand(manifest_path), "{s}\\BuildToolsManifest.Preview.json", temp_path);
		hope(download_file(preview_manifest_url, manifest_path, "BuildToolsManifest.Preview.json"), "Failed to download Build Tools preview manifest");
	}
	{
		println("Parsing Build Tools manifest files...");
		char manifest_path[MAX_PATH * 3];
		string_format(array_expand(manifest_path), "{s}\\BuildToolsManifest.Release.json", temp_path);
		parse_manifest(manifest_path, false);
		string_format(array_expand(manifest_path), "{s}\\BuildToolsManifest.Preview.json", temp_path);
		parse_manifest(manifest_path, true);
	}
	println("");
	if (list_versions) {
		println("MSVC Versions (Release): ");
		for (i64 i = release_msvc_versions_count - 1; i >= 0; i--) {
			println("{s}", release_msvc_versions[i]);
		}
		println("SDK Versions (Release): ");
		for (i64 i = release_sdk_versions_count - 1; i >= 0; i--) {
			println("{s}", release_sdk_versions[i]);
		}
		println("MSVC Versions (Preview): ");
		for (i64 i = preview_msvc_versions_count - 1; i >= 0; i--) {
			println("{s}", preview_msvc_versions[i]);
		}
		println("SDK Versions (Preview): ");
		for (i64 i = preview_sdk_versions_count - 1; i >= 0; i--) {
			println("{s}", preview_sdk_versions[i]);
		}
		cleanup();
		return (0);
	}
	if (is_cli) {
		panic = console_panic;
//[c]		Sanity checks
		{
			bool found = false;
			for (i64 i = 0; i < count_of(targets); i++) {
				found = string_is(target_arch, targets[i]) ? true : found;
			}
			hope(found, "Invalid target architecture: {s}", target_arch);
		}
		{
			bool found = false;
			for (i64 i = 0; i < count_of(hosts); i++) {
				found = string_is(host_arch, hosts[i]) ? true : found;
			}
			hope(found, "Invalid host architecture: {s}", host_arch);
		}
		{
			char (*msvc_versions)[16] = is_preview ? preview_msvc_versions : release_msvc_versions;
			i64 msvc_versions_count = is_preview ? preview_msvc_versions_count : release_msvc_versions_count;
			if (msvc_version == null) {
				msvc_version = msvc_versions[msvc_versions_count - 1];
			}
			bool found = false;
			for (i64 i = 0; i < msvc_versions_count; i++) {
				found = string_is(msvc_version, msvc_versions[i]) ? true : found;
			}
			hope(found, "Invalid MSVC version: {s}", msvc_version);
		}
		{
			char (*sdk_versions)[8] = is_preview ? preview_sdk_versions : release_sdk_versions;
			i64 sdk_versions_count = is_preview ? preview_sdk_versions_count : release_sdk_versions_count;
			if (sdk_version == null) {
				sdk_version = sdk_versions[sdk_versions_count - 1];
			}
			bool found = false;
			for (i64 i = 0; i < sdk_versions_count; i++) {
				found = string_is(sdk_version, sdk_versions[i]) ? true : found;
			}
			hope(found, "Invalid SDK version: {s}", sdk_version);
		}
		println("Preview channel: {s}", is_preview ? "true" : "false");
		println("MSVC version: {s}", msvc_version);
		println("SDK version: {s}", sdk_version);
		println("Target arch: {s}", target_arch);
		println("Host arch: {s}", host_arch);
		println("Install path: {s}", install_path);
		if (folder_exists(install_path)) {
			if (!is_folder_empty(install_path)) {
				hopeless("Destination folder is not empty");
			}
		} else {
			WCHAR wpath[MAX_PATH];
			utf8_to_utf16(install_path, -1, wpath, MAX_PATH);
			if (!CreateDirectoryW(wpath, null)) {
				const char* error = (GetLastError() == ERROR_ACCESS_DENIED) ? "No access to the destination folder, try running this program as administrator" : "Destination folder cannot be made";
				hopeless(error);
			}
		}
		if (license_accepted) {
			install_start = true;
			println("");
		} else {
			println("Do you accept the license agreement? [Y/n] {s}", is_preview ? preview_license_url : release_license_url);
			char answer[4];
			sys_console_read(array_expand(answer));
			string_lower(answer);
			install_start = string_is(answer, "") | string_is(answer, "y") | string_is(answer, "yes");
		}
		hope(install_start, "License was not accepted, cannot proceed further.");
		install();
		const char* msg = "PortableBuildTools was installed successfully.";
		msg = (env_mode == 1) ? "Please log out and back in to finish installation." : msg;
		msg = (env_mode == 2) ? "Please reboot the machine to finish installation." : msg;
		println(msg);
	} else {
		{
			FreeConsole();
			INITCOMMONCONTROLSEX info = (INITCOMMONCONTROLSEX){0};
			info.dwSize = sizeof(INITCOMMONCONTROLSEX);
			info.dwICC = ICC_STANDARD_CLASSES | ICC_LINK_CLASS;
			InitCommonControlsEx(&info);
			DialogBoxW(GetModuleHandleW(null), cast(LPCWSTR, ID_DIALOG_WINDOW), null, cast(DLGPROC, window_proc));
			if (!install_start) {
				cleanup();
				return (1);
			}
		}
		{
			char* env_mode_str = "none";
			env_mode_str = (env_mode == 1) ? "user" : env_mode_str;
			env_mode_str = (env_mode == 2) ? "global" : env_mode_str;
			char params[MAX_CMDLINE_LEN * 3];
			string_format(array_expand(params), "! {s} msvc={s} sdk={s} target={s} host={s} env={s} path=\"{s}\"", is_preview ? "preview" : "", msvc_version, sdk_version, target_arch, host_arch, env_mode_str, install_path);
			WCHAR wprogram[MAX_PATH];
			utf8_to_utf16(args, -1, wprogram, count_of(wprogram));
			WCHAR wparams[MAX_CMDLINE_LEN];
			utf8_to_utf16(params, -1, wparams, count_of(wparams));
			SHELLEXECUTEINFOW info = (SHELLEXECUTEINFOW){0};
			info.cbSize = size_of(SHELLEXECUTEINFOW);
			info.fMask = SEE_MASK_NOCLOSEPROCESS;
			info.lpVerb = is_admin ? L"runas" : L"open";
			info.lpFile = wprogram;
			info.lpParameters = wparams;
			info.nShow = SW_NORMAL;
			bool ok = ShellExecuteExW(&info);
			hope(ok, "Failed to run the installer");
		}
	}
	cleanup();
	return (0);
}
