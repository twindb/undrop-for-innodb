#ifndef common_h
#define common_h

#include <univ.i>
#include <page0page.h>
#include <rem0rec.h>
#include <btr0cur.h>

#define MAX_TABLE_FIELDS 500
#define MAX_ENUM_VALUES 65535
#define MAX_SET_VALUES 64

// Field limits type
typedef struct field_limits {
	// In opposite to field.can_be_null, this field sets
	// limit from the data point of view
	ibool can_be_null;

	// min and max values for FT_INT fields
	long long int int_min_val;
	long long int int_max_val;

	// min and max values for FT_UNT fields
	unsigned long long int uint_min_val;
	unsigned long long int uint_max_val;

	// min and max string length
	long long int char_min_len;
	long long int char_max_len;

	// Should data be forced down to some ASCII sub-set or not
	ibool char_ascii_only;
	ibool char_digits_only;

	char* char_regex;

	// Dates validation
	ibool date_validation;

	// Enum values
	char *enum_values[MAX_ENUM_VALUES];
	char *set_values[MAX_SET_VALUES];
	uint enum_values_count;
	uint set_values_count;
} field_limits_t;

// Table definition types
typedef enum field_type {
	FT_NONE,		// dummy type for stop records
	FT_INTERNAL,		// supported
	FT_CHAR,		// supported
	FT_INT,			// supported
	FT_UINT,		// supported
	FT_FLOAT,		// supported
	FT_DOUBLE,		// supported
	FT_DATE,		// supported
	FT_TIME,		// supported
	FT_DATETIME,		// supported
	FT_ENUM,		// supported
	FT_SET,			// supported
	FT_BLOB,		// supported
	FT_TEXT,		// supported
	FT_BIT,			// supported
	FT_DECIMAL,		// supported
	FT_TIMESTAMP,		// supported
	FT_YEAR,		// supported
	FT_BIN                  // supported
} field_type_t;

typedef struct field_def {
	char *name;
	char *charset;
	field_type_t type;
	unsigned int min_length;
	unsigned int max_length;

	ibool can_be_null;
	int fixed_length;

    	// For DECIMAL numbers only
	int decimal_precision;
	int decimal_digits;
	// For temporal types
	int time_precision;

	ibool has_limits;
	ibool char_rstrip_spaces;
	field_limits_t limits;
} field_def_t;

typedef struct table_def {
	char *name;
	field_def_t fields[MAX_TABLE_FIELDS];
	int fields_count;
	int data_min_size;
	long long int data_max_size;
	int n_nullable;
	int min_rec_header_len;
} table_def_t;

#ifndef table_definitions_cnt
    extern table_def_t table_definitions[];
    extern int table_definitions_cnt;
    extern int record_extra_bytes;
#endif

void init_table_defs();

#endif
