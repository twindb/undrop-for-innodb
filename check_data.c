#include <check_data.h>
#include <print_data.h>
#include <string.h>
#include <regex.h>

// Linux has no isnumber function? (Debian 4.0 has no such function)
#ifndef isnumber
inline int isnumber(char c) {
    return (isdigit(c) || c == '.' || c == '-');
}
#endif

#define MAX_CHAR_PREFIX_LENGTH 0xfff

/*******************************************************************/
inline ulonglong make_ulonglong(dulint x) {
	ulonglong lx = x.high;
	lx <<= 32;
	lx += x.low;
	return lx;
}

/*******************************************************************/
inline longlong make_longlong(dulint x) {
	longlong lx = x.high;
	lx <<= 32;
	lx += x.low;
	return lx;
}

/*******************************************************************/
inline ibool check_datetime(ulonglong ldate) {
	int year, month, day, hour, min, sec;

	ldate &= ~(1ULL << 63);

	if (ldate == 0) return TRUE;
	if (debug) printf("DATE=OK ");

	sec = ldate % 100; ldate /= 100;
    if (debug) printf("SEC(%d)", sec);
	if (sec < 0 || sec > 59) return FALSE;
	if (debug) printf("=OK ");

	min = ldate % 100; ldate /= 100;
    if (debug) printf("MIN(%d)", min);
	if (min < 0 || min > 59) return FALSE;
	if (debug) printf("=OK ");

	hour = ldate % 100; ldate /= 100;
    if (debug) printf("HOUR(%d)", hour);
	if (hour < 0 || hour > 23) return FALSE;
	if (debug) printf("=OK ");

	day = ldate % 100; ldate /= 100;
    if (debug) printf("DAY(%d)", day);
	if (day < 0 || day > 31) return FALSE;
	if (debug) printf("=OK ");

	month = ldate % 100; ldate /= 100;
    if (debug) printf("MONTH(%d)", month);
	if (month < 0 || month > 12) return FALSE;
	if (debug) printf("=OK ");

	year = ldate % 10000;
    if (debug) printf("YEAR(%d)", year);
	if (year < 1950 || year > 2050) return FALSE;
	if (debug) printf("=OK ");

	return TRUE;
}

/*******************************************************************/
inline ibool check_char_ascii(char *value, ulint len) {
	char *p = value;
	if (!len) return TRUE;
	do {
		if (!isprint(*p) && *p != '\n' && *p != '\t' && *p != '\r') return FALSE;
	} while (++p < value + len);
	return TRUE;
}

/*******************************************************************/
inline ibool check_char_digits(char *value, ulint len) {
	char *p = value;
	if (!len) return TRUE;
	do {
		if (!isdigit(*p)) return FALSE;
	} while (++p < value + len);
	return TRUE;
}

/*******************************************************************/
// Try and match a given regular expression with a given text.
// Returns TRUE/FALSE upon match/mismatch respectively.
// The function recomputes the regular expression per call. There is
// no caching of regex. This can be optimized later on.
/*******************************************************************/
inline ibool regex_match(const char *string, char *pattern) {
	int status;
	regex_t re;
	if(regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0)
		return 0;
	status = regexec(&re, string, (size_t)0, NULL, 0);
	regfree(&re);
	if(status == 0)
		return TRUE;  // success
	else
		return FALSE; // no match
}

/*******************************************************************/
// Validate regex constraints.
// Given value is not null terminated. When len < 4096, use
// statically allocated buffer; otherwise, allocate with malloc.
// For long texts, this means a lot of overhead.
/*******************************************************************/
inline ibool check_regex_match(char *value, ulint len, char *pattern)
{
	static char prefix[MAX_CHAR_PREFIX_LENGTH + 1];
	char * null_terminated_value = NULL ;
	ibool result = FALSE ;

	if (len <= MAX_CHAR_PREFIX_LENGTH)
	{
		null_terminated_value = prefix;
	}
	else
	{
		null_terminated_value = (char*)malloc(len + 1);
	}
	strncpy(null_terminated_value, value, len-1);
	null_terminated_value[len] = 0;

	if (regex_match(null_terminated_value, pattern))
	{
		result = TRUE ;
	}
	if (null_terminated_value != prefix)
	{
		free(null_terminated_value);
	}
	return result;
}

/*******************************************************************/
ibool check_field_limits(field_def_t *field, byte *value, ulint len) {
	long long int int_value;
	unsigned long long int uint_value;

	switch (field->type) {
		case FT_INT:
			int_value = get_int_value(field, value);
			if (debug) printf("INT(%i)=%lli ", field->fixed_length, int_value);
			if (int_value < field->limits.int_min_val) {
    			if (debug) printf(" is less than %lli ", field->limits.int_min_val);
			    return FALSE;
			}
			if (int_value > field->limits.int_max_val) {
    			if (debug) printf(" is more than %lli ", field->limits.int_max_val);
			    return FALSE;
			}
			break;

		case FT_UINT:
			uint_value = get_uint_value(field, value);
			if (debug) printf("UINT(%i)=%llu ", field->fixed_length, uint_value);
			if (uint_value < field->limits.uint_min_val) {
    			if (debug) printf(" is less than %llu ", field->limits.uint_min_val);
			    return FALSE;
			}
			if (uint_value > field->limits.uint_max_val) {
    			if (debug) printf(" is more than %llu ", field->limits.uint_max_val);
			    return FALSE;
			}
			break;

//		case FT_TEXT:
		case FT_CHAR:
			if (debug) {
				if (len != UNIV_SQL_NULL) {
					if (len <= 30) {
						ut_print_buf(stdout, value, len);
					} else {
						ut_print_buf(stdout, value, 30);
						printf("...(truncated)");
					}
				} else {
					printf("SQL NULL ");
				}
			}
			if (len < field->limits.char_min_len) return FALSE;
			if (len > field->limits.char_max_len) return FALSE;
			if (field->limits.char_ascii_only && !check_char_ascii((char*)value, len)) return FALSE;
			if (field->limits.char_digits_only && !check_char_digits((char*)value, len)) return FALSE;
			if (field->limits.char_regex != NULL) {
				if (!check_regex_match((char *)value, len, field->limits.char_regex))
				{
	    			if (debug) printf(" does not match regex [[%s]] ", (char*)field->limits.char_regex);
				    return FALSE;
				}
			}
			break;

		case FT_DATE:
		case FT_DATETIME:
			if (!check_datetime(make_longlong(mach_read_from_8(value)))) return FALSE;
			break;

		case FT_ENUM:
			int_value = get_int_value(field, value);
			if (debug) printf("ENUM=%lli ", int_value);
			if (int_value < 1 || int_value > field->limits.enum_values_count) return FALSE;
			break;
		default:
			break;
	}

	return TRUE;
}
