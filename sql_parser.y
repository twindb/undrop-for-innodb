%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#define YYDEBUG 1

#include "tables_dict.h"
table_def_t table_definitions[1];
field_def_t pk_fields[MAX_TABLE_FIELDS];
field_def_t uq_fields[MAX_TABLE_FIELDS];
field_def_t all_fields[MAX_TABLE_FIELDS];

// globals
char column_name[1024];
char table_name[1024];
char table_charset[1024];
unsigned int field_number = 0;
unsigned int pk_field_number = 0;
unsigned int uq_field_number = 0;
int has_primary_key = 0;
int has_unique_key = 0;
unsigned int n_values = 0;
int is_length = 0;
int is_unsigned = 0;
int is_decimals = 0;
int is_primary_index = 0;
int is_unique_index = 0;
int met_unique_index = 0;
unsigned int length = 0;
unsigned int decimals = 0;

extern bool debug;
extern bool process_56;
extern int yylex (void);
extern void error(char*);
extern void yyerror(char*);

field_def_t*  find_field(field_def_t *haystack, field_def_t* needle, field_def_t** result){
    *result = NULL;
    unsigned int i = 0;
    while((haystack + i)->name != NULL){
        if((strlen((haystack + i)->name) == strlen(needle->name))
            && strncmp((haystack + i)->name, needle->name, strlen(needle->name)) == 0){
            *result = (haystack + i);
            return *result;
            }
        i++;
        }
    return *result;
    }

void strip_bq(char*p){
    int i = 0;
    int j = 0;
    char result[1024] = "";
    for(i = 0; i < strlen(p); i++){
        if( p[i] != '`'){
            result[j++] = p[i];
            }
        }
    result[j++] = '\0';
    //fprintf(stderr, "Result %s\n", result);
    strncpy(p, result, sizeof(result));
    }
%}

%union {
    char str[1024];
    long long int si;
 }

%token CREATE TABLE TEMPORARY IF_NOT_EXIST SQLNULL DEFAULT NOT
%token COMMENT FULLTEXT SPATIAL CONSTRAINT
%token INDEX KEY PRIMARY UNIQUE FOREIGN_KEY
%token BIT TINYINT SMALLINT MEDIUMINT INT INTEGER BIGINT REAL DOUBLE FLOAT DECIMAL NUMERIC 
%token DATE TIME TIMESTAMP DATETIME YEAR 
%token CHAR VARCHAR TINYTEXT TEXT MEDIUMTEXT LONGTEXT
%token BINARY VARBINARY TINYBLOB BLOB MEDIUMBLOB LONGBLOB 
%token ENUM SET
%token GEOMETRY POINT LINESTRING POLYGON MULTIPOINT MULTILINESTRING MULTIPOLYGON GEOMETRYCOLLECTION
%token UNSIGNED ZEROFILL
%token ASC DESC
%token USING BTREE HASH RTREE
%token MATCH FULL PARTIAL SIMPLE
%token ON_DELETE ON_UPDATE RESTRICT CASCADE NO ACTION
%token ENGINE TYPE AUTO_INCREMENT AVG_ROW_LENGTH CHECKSUM CHARACTER COLLATE CONNECTION DATA DIRECTORY DELAY_KEY_WRITE INSERT_METHOD MAX_ROWS MIN_ROWS PACK_KEYS PASSWORD ROW_FORMAT UNION
%token FIRST LAST
%token DYNAMIC FIXED COMPRESSED REDUNDANT COMPACT
%token <str>string_value 
%token <str>entity_name
%token <si>int_value 
%token <si>bool_value
%token CURRENT_TIMESTAMP
%token CHARSET
%token REFERENCES
%token CAN_BE_NULL INT_MIN_VAL INT_MAX_VAL CHAR_MIN_LEN CHAR_MAX_LEN CHAR_ASCII_ONLY CHAR_DIGITS_ONLY
%token CHAR_REGEX DATE_VALIDATION ENUM_VALUES SET_VALUES ENUM_VALUES_COUNT SET_VALUES_COUNT
%type<str> charset_value
%%
list_sql: sql_command
   | list_sql sql_command;

sql_command: create_table ';' | ';';

create_table: CREATE opt_temporary
	TABLE opt_if_not_exist 
	tbl_name {
        strip_bq(table_name);
        if(debug) printf("Parsing sturcture of %s\n", table_name);
        table_definitions[0].name = strdup(table_name);
        }
    '(' list_create_definition ')' 
	opt_list_table_option {
        // if table_charset it utf8 or utf8mb
        // increste max_size of each VARCHAR or TEXT field
        unsigned int factor = 1;
        if(strcmp(table_charset, "utf8") == 0) factor = 3;
        if(strcmp(table_charset, "utf8mb4") == 0) factor = 4;
        int i = 0;
        for(i = 0; i < field_number; i++){
            // If charset isn't set explicitely apply table-wide factor
            if(all_fields[i].charset == NULL){
                all_fields[i].fixed_length = all_fields[i].fixed_length * factor;
                }
            }
        // generate table_defs based on all_fields
        unsigned int meta_i = 0;
        if(has_primary_key){
            unsigned int i = 0;
            for(i = 0; i < pk_field_number; i++){
                if(debug) printf("Primary key: %s, type: %d\n", pk_fields[i].name, pk_fields[i].type);
                field_def_t* fld;
                find_field(all_fields, pk_fields + i, &fld);
                assert(fld != NULL);
                memcpy(table_definitions[0].fields + meta_i, fld, sizeof(field_def_t));
                meta_i++;
                }
            }
        else{
            if(has_unique_key){
                unsigned int i = 0;
                for(i = 0; i < uq_field_number; i++){
                    if(debug) printf("Unique key: %s\n", uq_fields[i].name);
                    field_def_t* fld;
                    find_field(all_fields, uq_fields + i, &fld);
                    assert(fld != NULL);
                    memcpy(table_definitions[0].fields + meta_i, fld, sizeof(field_def_t));
                    meta_i++;
                    }
                
                }
            else{
                if(debug) printf("Has primary: %d\n", has_primary_key);
                field_def_t row_id;
                row_id.name = strdup("DB_ROW_ID");
                row_id.type = FT_INTERNAL;
                row_id.can_be_null = 0;
                row_id.fixed_length = 6;
                memcpy(table_definitions[0].fields + meta_i, &row_id, sizeof(field_def_t));
                meta_i++;
                }
            }
        // Add MVCC fields
        field_def_t trx_id, roll_ptr, none;
        trx_id.name = strdup("DB_TRX_ID");
        trx_id.type = FT_INTERNAL;
        trx_id.can_be_null = 0;
        trx_id.fixed_length = 6;
        roll_ptr.name = strdup("DB_ROLL_PTR");
        roll_ptr.type = FT_INTERNAL;
        roll_ptr.can_be_null = 0;
        roll_ptr.fixed_length = 7;
        memcpy(table_definitions[0].fields + meta_i++, &trx_id, sizeof(field_def_t));
        memcpy(table_definitions[0].fields + meta_i++, &roll_ptr, sizeof(field_def_t));
        for(i = 0; i < field_number; i++){
            if(debug) printf("field[%u]: \n", i);
            field_def_t* fld;
            find_field(table_definitions[0].fields, all_fields + i, &fld);
            if(fld == NULL){
                if(debug) printf("Other field[%u]: %s\n", i, all_fields[i].name);
                memcpy(table_definitions[0].fields + meta_i++, all_fields + i, sizeof(field_def_t));
                }
            }
        none.type = FT_NONE;
        //memcpy(table_definitions[0].fields + meta_i++, &none, sizeof(field_def_t));
        i = 0;
        if(debug) printf("Table_defs %s\n", table_definitions[0].name);
        while(table_definitions[0].fields[i].name != NULL){
            if(debug) printf("             name: %s\n", table_definitions[0].fields[i].name);
            if(debug) printf("             type: %d\n", table_definitions[0].fields[i].type);
            if(debug) printf("          charset: %s\n", table_definitions[0].fields[i].charset);
            if(debug) printf("       min_length: %d\n", table_definitions[0].fields[i].min_length);
            if(debug) printf("       max_length: %d\n", table_definitions[0].fields[i].max_length);
            if(debug) printf("      can_be_null: %lu\n", table_definitions[0].fields[i].can_be_null);
            if(debug) printf("     fixed_length: %d\n", table_definitions[0].fields[i].fixed_length);
            if(debug) printf("decimal_precision: %d\n", table_definitions[0].fields[i].decimal_precision);
            if(debug) printf("   decimal_digits: %d\n", table_definitions[0].fields[i].decimal_digits);
            if(debug) printf("   time_precision: %d\n", table_definitions[0].fields[i].time_precision);
            if(debug) printf("       has_limits: %lu\n", table_definitions[0].fields[i].has_limits);
            if(debug) printf("\n");
            i++;
            }
        }
	;

opt_temporary:
	| TEMPORARY
	;

list_create_definition: create_definition
	| list_create_definition  ',' create_definition
	;

create_definition: col_name 
        {
            // Reset floags
            is_length = 0;
            is_unsigned = 0;
            is_decimals = 0;
            n_values = 0;
            // Save name
            strip_bq(column_name);
            if(debug) printf("Column %s\n", column_name);
            all_fields[field_number].name = strdup(column_name);
            all_fields[field_number].min_length = 0;
            all_fields[field_number].can_be_null = 1;
            }
        column_definition opt_onupdate_clause 
        { 
            if(debug) printf("all_fields[%d].name  = %s, type: %d, size: %u\n", 
                    field_number, 
                    all_fields[field_number].name,
                    all_fields[field_number].type,
                    all_fields[field_number].fixed_length);
            all_fields[field_number].has_limits = FALSE;
            all_fields[field_number].char_rstrip_spaces = 0;
            }
        opt_default_clause
        opt_filter_list
        { field_number++; }
	| opt_constraint opt_symbol PRIMARY KEY { 
            has_primary_key = 1; 
            is_primary_index = 1;
            } opt_index_type '(' list_index_col_name ')' {
            is_primary_index = 0; } opt_index_type
	| index_keyword opt_index_name opt_index_type '(' list_index_col_name ')' opt_index_type
	| opt_constraint opt_symbol UNIQUE {
            has_unique_key = 1;
            is_unique_index = 1;
            } index_keyword opt_index_name opt_index_type '(' list_index_col_name ')' {
            is_unique_index = 0;
            met_unique_index = 1;
            }opt_index_type
	| adv_index index_keyword opt_index_name '(' list_index_col_name ')' opt_index_type
	| opt_constraint opt_symbol FOREIGN_KEY opt_index_name '(' list_index_col_name ')' reference_definition
	;

column_definition:
	data_type opt_null_clause opt_default_clause opt_auto_increment opt_key_clause opt_comment_clause opt_reference_definition;

opt_filter_list: | filter_list { all_fields[field_number].has_limits = TRUE; };

filter_list: filter_clause | filter_clause filter_list;

filter_clause: CAN_BE_NULL ':' bool_value { all_fields[field_number].limits.can_be_null = $3; }
    | INT_MIN_VAL ':' int_value { all_fields[field_number].limits.int_min_val = $3;
                                  all_fields[field_number].limits.uint_min_val = $3;
                                }
    | INT_MAX_VAL ':' int_value { all_fields[field_number].limits.int_max_val = $3; 
                                  all_fields[field_number].limits.uint_max_val = $3;
                                }
    | CHAR_MIN_LEN ':' int_value { all_fields[field_number].limits.char_min_len = $3; }
    | CHAR_MAX_LEN ':' int_value { all_fields[field_number].limits.char_max_len = $3; }
    | CHAR_ASCII_ONLY ':' bool_value { all_fields[field_number].limits.char_ascii_only = $3; }
    | CHAR_DIGITS_ONLY ':' bool_value { all_fields[field_number].limits.char_digits_only = $3; }
    | CHAR_REGEX ':' string_value
    | DATE_VALIDATION ':' bool_value { all_fields[field_number].limits.date_validation = $3; }
    | ENUM_VALUES ':' '(' list_values ')'
    | SET_VALUES ':' '(' list_values ')'
    | ENUM_VALUES_COUNT ':' int_value 
    | SET_VALUES_COUNT ':' int_value
opt_if_not_exist: | IF_NOT_EXIST;

opt_null_clause: | NOT SQLNULL { all_fields[field_number].can_be_null = 0; }  | SQLNULL;

opt_default_clause: | DEFAULT value | DEFAULT decimal_value | DEFAULT SQLNULL | DEFAULT CURRENT_TIMESTAMP;

opt_auto_increment: | AUTO_INCREMENT; 

opt_key_clause: | UNIQUE opt_key {
        has_unique_key = 1;
        if(!met_unique_index){
            memcpy(uq_fields + uq_field_number, all_fields + field_number, sizeof(field_def_t));
            uq_field_number++;
            }
        } | opt_primary KEY;

opt_key: | KEY;

opt_primary: | PRIMARY { 
        has_primary_key = 1;
        memcpy(pk_fields + pk_field_number, all_fields + field_number, sizeof(field_def_t));
        pk_field_number++;
        };

opt_comment_clause: | COMMENT string_value

opt_reference_definition: | reference_definition;

adv_index: FULLTEXT | SPATIAL;

index_keyword : INDEX | KEY;

opt_symbol: | entity_name | '`' entity_name '`';

opt_index_name: | index_name;

index_name: entity_name | '`' entity_name '`';

opt_index_type: | index_type;

list_index_col_name: col_name {
        if(is_primary_index){
            strip_bq(column_name);
            pk_fields[pk_field_number++].name = strdup(column_name);
            }
        if(is_unique_index && !met_unique_index){
            strip_bq(column_name);
            uq_fields[uq_field_number++].name = strdup(column_name);
            }
        } opt_length opt_asc
	| list_index_col_name ',' col_name {
        if(is_primary_index){
            strip_bq(column_name);
            pk_fields[pk_field_number++].name = strdup(column_name);
            }
        if(is_unique_index && !met_unique_index){
            strip_bq(column_name);
            uq_fields[uq_field_number++].name = strdup(column_name);
            }
        } opt_length opt_asc
	;

col_name: entity_name { 
                memset(column_name, 0, sizeof(column_name)); 
                strncpy(column_name, $1, strlen($1)); } 
        | '`' entity_name { 
                memset(column_name, 0, sizeof(column_name)); 
                strncpy(column_name, $2, strlen($2)); } '`';

opt_list_table_option: | list_table_option;

list_table_option: table_option
	| list_table_option table_option
	;

data_type: BIT { 
        all_fields[field_number].type = FT_BIT; 
        all_fields[field_number].fixed_length = 1;
        } opt_length {
        if(is_length){
            all_fields[field_number].fixed_length = floor((length + 7 ) / 8);
            }
        }
	| TINYINT { 
        all_fields[field_number].type = FT_INT; 
        all_fields[field_number].fixed_length = 1;
        all_fields[field_number].limits.int_min_val = -127; 
        all_fields[field_number].limits.int_max_val = 127; 
        } opt_length opt_unsigned {
        if(is_unsigned){
            all_fields[field_number].type = FT_UINT;
            all_fields[field_number].limits.uint_min_val = 0; 
            all_fields[field_number].limits.uint_max_val = 255; 
            }
        }opt_zero_fill
	| SMALLINT { 
        all_fields[field_number].type = FT_INT;
        all_fields[field_number].fixed_length = 2; 
        all_fields[field_number].limits.int_min_val = -32768; 
        all_fields[field_number].limits.int_max_val = 32767; 
        } opt_length opt_unsigned {
        if(is_unsigned){
            all_fields[field_number].type = FT_UINT;
            all_fields[field_number].limits.uint_min_val = 0; 
            all_fields[field_number].limits.uint_max_val = 65535; 
            }
        } opt_zero_fill
	| MEDIUMINT { 
        all_fields[field_number].type = FT_INT; 
        all_fields[field_number].fixed_length = 3;
        all_fields[field_number].limits.int_min_val = -8388608L; 
        all_fields[field_number].limits.int_max_val = 8388607L; 
        } opt_length opt_unsigned {
        if(is_unsigned){
            all_fields[field_number].type = FT_UINT;
            all_fields[field_number].limits.uint_min_val = 0; 
            all_fields[field_number].limits.uint_max_val = 16777215UL; 
            }
        } opt_zero_fill
	| INT { 
        all_fields[field_number].type = FT_INT; 
        all_fields[field_number].fixed_length = 4;
        all_fields[field_number].limits.int_min_val = -2147483648LL; 
        all_fields[field_number].limits.int_max_val = 2147483647LL; 
        } opt_length opt_unsigned {
        if(is_unsigned){
            all_fields[field_number].type = FT_UINT;
            all_fields[field_number].limits.uint_min_val = 0; 
            all_fields[field_number].limits.uint_max_val = 4294967295ULL; 
            }
        } opt_zero_fill
	| INTEGER { 
        all_fields[field_number].type = FT_INT; 
        all_fields[field_number].fixed_length = 4;
        all_fields[field_number].limits.int_min_val = -2147483648LL; 
        all_fields[field_number].limits.int_max_val = 2147483647LL; 
        } opt_length opt_unsigned {
        if(is_unsigned){
            all_fields[field_number].type = FT_UINT;
            all_fields[field_number].limits.uint_min_val = 0; 
            all_fields[field_number].limits.uint_max_val = 4294967295ULL; 
            }
        } opt_zero_fill
	| BIGINT { 
        all_fields[field_number].type = FT_INT;
        all_fields[field_number].fixed_length = 8;
        all_fields[field_number].limits.int_min_val = -9223372036854775806LL; 
        all_fields[field_number].limits.int_max_val = 9223372036854775807LL; 
        } opt_length opt_unsigned {
        if(is_unsigned){
            all_fields[field_number].type = FT_UINT;
            all_fields[field_number].limits.uint_min_val = 0; 
            all_fields[field_number].limits.uint_max_val = 18446744073709551615ULL; 
            }
        } opt_zero_fill
	| REAL { all_fields[field_number].type = FT_FLOAT; 
        all_fields[field_number].fixed_length = 4;
        } opt_float_length opt_unsigned opt_zero_fill
	| DOUBLE { 
        all_fields[field_number].type = FT_DOUBLE; 
        all_fields[field_number].fixed_length = 8;
        } opt_float_length opt_unsigned opt_zero_fill
	| FLOAT { 
        all_fields[field_number].type = FT_FLOAT; 
        all_fields[field_number].fixed_length = 4;
        } opt_float_length opt_unsigned opt_zero_fill
	| decimal_type { all_fields[field_number].type = FT_DECIMAL ; } opt_length_opt_decimal {
        if(is_length){
            decimals = (is_decimals) ? decimals: 0;
            all_fields[field_number].fixed_length = ceil((float)(length-decimals) * 4 / 9) + ceil((float)decimals*4/9);
            all_fields[field_number].decimal_precision = length;
            all_fields[field_number].decimal_digits = decimals;
            }
        } opt_unsigned opt_zero_fill
	| DATE { 
        all_fields[field_number].type = FT_DATE; 
        all_fields[field_number].fixed_length = 3;
        }
	| TIME { 
        all_fields[field_number].type = FT_TIME; 
        all_fields[field_number].fixed_length = 3;
        } opt_length{
        if(is_length){
            all_fields[field_number].fixed_length = 3 + ceil(length/2.0);
            }
        }
	| TIMESTAMP { 
        all_fields[field_number].type = FT_TIMESTAMP; 
        all_fields[field_number].fixed_length = 4;
        } opt_length{
        if(is_length){
            all_fields[field_number].fixed_length = 4 + ceil(length/2.0);
            }
        }
	| DATETIME { 
        all_fields[field_number].type = FT_DATETIME;
        all_fields[field_number].fixed_length = (process_56) ? 5: 8;
        } opt_length {
        if(is_length){
            all_fields[field_number].fixed_length = 5 + ceil(length/2.0);
            }
        }
	| YEAR { 
        all_fields[field_number].type = FT_YEAR; 
        all_fields[field_number].fixed_length = 1;
        } opt_length

	| CHAR { all_fields[field_number].type = FT_CHAR; } opt_length {
        all_fields[field_number].fixed_length = (is_length) ? length : 1;
        } opt_charset_clause opt_collate_clause

	| VARCHAR { all_fields[field_number].type = FT_CHAR; } length {
        all_fields[field_number].max_length = length;
        } opt_charset_clause opt_collate_clause

	| BINARY { 
        all_fields[field_number].type = FT_BIN; } opt_length {
        if(is_length){
            all_fields[field_number].fixed_length = length;
            }
        }

	| VARBINARY { all_fields[field_number].type = FT_BIN; } length { all_fields[field_number].max_length = length;}

	| TINYBLOB { 
        all_fields[field_number].type = FT_BLOB; 
        all_fields[field_number].max_length = 255;
        }

	| BLOB { 
        all_fields[field_number].type = FT_BLOB; 
        all_fields[field_number].max_length = 65535;
        }
	| MEDIUMBLOB { 
        all_fields[field_number].type = FT_BLOB; 
        all_fields[field_number].max_length = 16777215;
        }
	| LONGBLOB { 
        all_fields[field_number].type = FT_BLOB; 
        all_fields[field_number].max_length = 4294967295;
        }
	| TINYTEXT { 
        all_fields[field_number].type = FT_TEXT; 
        all_fields[field_number].max_length = 255;
        } opt_binary opt_charset_clause opt_collate_clause
	| TEXT { 
        all_fields[field_number].type = FT_TEXT;
        all_fields[field_number].max_length = 65535; 
        } opt_binary opt_charset_clause opt_collate_clause
	| MEDIUMTEXT { 
        all_fields[field_number].type = FT_TEXT; 
        all_fields[field_number].max_length = 16777215;
        } opt_binary opt_charset_clause opt_collate_clause
	| LONGTEXT { 
        all_fields[field_number].type = FT_TEXT; 
        all_fields[field_number].max_length = 4294967295;
        } opt_binary opt_charset_clause opt_collate_clause
	| ENUM { all_fields[field_number].type = FT_ENUM; } '(' list_values ')' {
        all_fields[field_number].fixed_length = (n_values > 255) ? 2 : 1;
        } opt_charset_clause opt_collate_clause
	| SET { all_fields[field_number].type = FT_SET; } '(' list_values ')' {
        int set_size = (n_values + 7 ) / 8;
        all_fields[field_number].fixed_length = (set_size > 4 ) ? 8 : set_size;
        } opt_charset_clause opt_collate_clause
	| GEOMETRY { error("Unsupported type GEOMETRY"); }
	| POINT { error("Unsupported type POINT"); }
	| LINESTRING { error("Unsupported type LINESTRING"); }
	| POLYGON { error("Unsupported type POLYGON"); }
	| MULTIPOINT { error("Unsupported type MULTIPOINT"); }
	| MULTILINESTRING { error("Unsupported type MULTILINESTRING"); }
	| MULTIPOLYGON { error("Unsupported type MULTIPOLYGON"); } 
	| GEOMETRYCOLLECTION { error("Unsupported type GEOMETRYCOLLECTION"); }
	;

decimal_type: DECIMAL|NUMERIC;

opt_length: | length { is_length = 1; } ;

length: '(' int_value { length = $2; } ')';

opt_unsigned: | UNSIGNED { is_unsigned = 1; };

opt_zero_fill: | ZEROFILL;

opt_float_length: | float_length;

float_length: '(' int_value {is_length = 1; length = $2;} ',' int_value { is_decimals = 1; decimals = $5; } ')';

opt_length_opt_decimal: length { is_length = 1; } | float_length;

opt_charset_clause: | charset_clause;

charset_clause: CHARACTER SET charset_value {
        // if field is multibyte multiply its max size
        unsigned int factor = 1;
        if(strcmp($3, "utf8") == 0) factor = 3;
        if(strcmp($3, "utf8mb4") == 0) factor = 4;
        all_fields[field_number].fixed_length = all_fields[field_number].fixed_length * factor;
        };

charset_value: string_value | entity_name;
collate_value: string_value | entity_name;

opt_binary: | BINARY;

opt_collate_clause: | collate_clause;

collate_clause: COLLATE collate_value;

opt_asc: | ASC | DESC;

index_type: USING index_type_value;

index_type_value: BTREE | HASH | RTREE;

reference_definition: REFERENCES tbl_name '(' list_index_col_name ')' opt_match_clause opt_ondelete_clause opt_onupdate_clause;

opt_match_clause: | MATCH FULL | MATCH PARTIAL | MATCH SIMPLE;

opt_ondelete_clause: | ON_DELETE reference_option;

opt_onupdate_clause: | ON_UPDATE reference_option;

reference_option: RESTRICT | CASCADE | SET SQLNULL | NO ACTION | CURRENT_TIMESTAMP;

table_option: ENGINE opt_equal entity_name
	| TYPE opt_equal entity_name
	| AUTO_INCREMENT opt_equal int_value
	| AVG_ROW_LENGTH opt_equal int_value
	| opt_default CHARACTER SET opt_equal entity_name
	| opt_default CHARSET opt_equal entity_name
	| CHECKSUM opt_equal one_zero_value
	| opt_default COLLATE opt_equal entity_name
	| COMMENT opt_equal string_value
	| CONNECTION opt_equal string_value
	| DATA DIRECTORY opt_equal string_value
	| DELAY_KEY_WRITE opt_equal one_zero_value
	| INDEX DIRECTORY opt_equal string_value
	| INSERT_METHOD opt_equal insert_method_value
	| MAX_ROWS opt_equal int_value
	| MIN_ROWS opt_equal int_value
	| PACK_KEYS opt_equal pack_keys_value
	| PASSWORD opt_equal string_value
	| ROW_FORMAT opt_equal row_format_value
	| UNION opt_equal '(' list_tbl_name ')'
	;

opt_equal: | '=';

opt_default: | DEFAULT;

one_zero_value: '0' | '1';

insert_method_value: NO | FIRST | LAST;

pack_keys_value: one_zero_value | DEFAULT;

row_format_value: DEFAULT|DYNAMIC|FIXED|COMPRESSED|REDUNDANT|COMPACT;

list_tbl_name: tbl_name
	| list_tbl_name tbl_name;

tbl_name: entity_name { strncpy(table_name, $1, strlen($1)); } 
        | '`' entity_name { strncpy(table_name, $2, strlen($2)); } '`'; 

list_values: value { n_values++; }
	| list_values ',' value { n_values++; };

value: string_value | int_value | '\'' int_value '\'';

decimal_value: int_value '.' int_value |
                '\'' int_value '.' int_value '\'';

opt_constraint: | CONSTRAINT;

%%
#include "lex.yy.c"

void yyerror(char *errmsg)
{
    fprintf(stderr, "Line %u: %s at '%s'\n", lineno, errmsg, yytext);
    fprintf(stderr, "%2u: %s\n", lineno -1 , linebuf_prev);
    fprintf(stderr, "%2u: %s\n", lineno , linebuf);
}

int load_table(char* f){
#ifdef DEBUG
    yydebug=1;
#endif
    extern FILE *yyin;
    yyin = fopen(f, "r");
    if(yyin == NULL){
        fprintf(stderr, "Could not open file %s\n", f);
        perror("load_table()");
        exit(EXIT_FAILURE);
    }
    // save first line in case there is an error in it
    if(NULL == fgets(linebuf, sizeof(linebuf), yyin)){
        fprintf(stderr, "Could not read from %s\n", f);
        exit(EXIT_FAILURE);
        }
    rewind(yyin);
    return yyparse();    
}

//int main(int argc, char** argv){
//    load_table(argv[1]);
//}
