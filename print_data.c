#include <print_data.h>
#include <decimal.h>

static byte buffer[UNIV_PAGE_SIZE * 2];
extern char blob_dir[256];
extern FILE* f_result;
extern bool process_56;
extern char path_ibdata[256];
extern bool external_in_ibdata;
int fn = 0;

/*******************************************************************/
void print_timestamp(unsigned char* value, field_def_t *field){
	char buf[32];
	struct tm result;

	time_t t = mach_read_from_4(value);
	localtime_r(&t, &result);
	strftime(buf, sizeof(buf), "%F %T", &result);
	int frac = 0;
	switch(field->time_precision){
		case 1: frac = mach_read_from_1(value + 4); break;
		case 2: frac = mach_read_from_2(value + 4); break;
		case 3: frac = mach_read_from_3(value + 4); break;
		}
	if(field->time_precision != 0){
		fprintf(f_result, "\"%s.%d\"", buf, frac);
	}else{
		fprintf(f_result, "\"%s\"", buf);
	}
}

// returns 1 if <5.6 , 2 if >=5.6 and 0 if can't detect
int guess_datetime_format(unsigned char* value){
	ulonglong ldate = make_longlong(mach_read_from_8(value));
	ulonglong r;
	int year, month, day, hour, min, sec;
	//
	r = ldate & ~(1ULL << 63);
	sec = r % 100; r /= 100;
	min = r % 100; r /= 100;
	hour = r % 100; r /= 100;
	day = r % 100; r /= 100;
	month = r % 100; r /= 100;
	year = ldate % 10000;
	if(year > 1990 && year < 2100
			&& month >= 1 && month <= 12
			&& day >= 1 && day <= 31
			&& hour >= 0 && hour <= 23
			&& min >= 0 && min <= 59
			&& sec >= 0 && sec <= 59){
		return 1;
		}
	int sign =0;
	sign = (ldate >> 63);
	if(sign == 0){
		return 0;
		}
	ulonglong yd = ((ldate & 0x7FFFC00000000000LL) >> 46 );
	year = yd / 13;
	month = yd - year*13;
	day = ((ldate &  0x00003E0000000000LL) >> 41 );
	hour = ((ldate & 0x000001F000000000LL) >> 36);
	min = ((ldate & 0x0000000FC0000000LL) >> 30);
	sec = ((ldate & 0x000000003F000000LL) >> 24);
	if(year > 1990 && year < 2100
			&& month >= 1 && month <= 12
			&& day >= 1 && day <= 31
			&& hour >= 0 && hour <= 23
			&& min >= 0 && min <= 59
			&& sec >= 0 && sec <= 59){
		return 2;
		}
	else{
		return 0;
		}
       	//printf("%04d-%02d-%02d %02d:%02d:%02d\n", year, month, day, hour, min, sec);	
	return 0;
	}

inline void print_datetime(unsigned char* value, field_def_t *field) {
	int year, month, day, hour, min, sec;
	int format = guess_datetime_format(value);
	if(format == 1 || format == 0){
		ulonglong ldate = make_longlong(mach_read_from_8(value));
		ldate &= ~(1ULL << 63);
    
		sec = ldate % 100; ldate /= 100;
		min = ldate % 100; ldate /= 100;
		hour = ldate % 100; ldate /= 100;
		day = ldate % 100; ldate /= 100;
		month = ldate % 100; ldate /= 100;
		year = ldate % 10000;
	
		fprintf(f_result, "\"%04u-%02u-%02u %02u:%02u:%02u\"", year, month, day, hour, min, sec);
		}
	if(format == 2){
		ulonglong ldate = make_longlong(mach_read_from_8(value));
		ulonglong yd = ((ldate & 0x7FFFC00000000000LL) >> 46 );
		year = yd / 13;
		month = yd - year*13;
		day = ((ldate &  0x00003E0000000000LL) >> 41 );
		hour = ((ldate & 0x000001F000000000LL) >> 36);
		min = ((ldate & 0x0000000FC0000000LL) >> 30);
		sec = ((ldate & 0x000000003F000000LL) >> 24);
		int frac = (ldate << 5*8) >> (64 - field->time_precision*8);
		//int frac = 0;
		fprintf(f_result, "\"%04u-%02u-%02u %02u:%02u:%02u.%u\"", year, month, day, hour, min, sec, frac);
		}
}

void print_string_raw(char *value, ulint len) {
    ulint i;
    for(i = 0; i < len; i++) {
        if (value[i] == '"') fprintf(f_result, "\\\"");
        else if (value[i] == '\n') fprintf(f_result, "\\n");
        else if (value[i] == '\r') fprintf(f_result, "\\r");
        else fprintf(f_result, "%c", value[i]);
    }
}

void print_hex(char *value, ulint len) {
    ulint i;
    unsigned int k = 0;
    char map[256][3];
    char* blob = malloc(len*2 + 1);
    if(blob == NULL){
    	fprintf(stderr, "ERROR: Unable to allocate %lu bytes memory to print BLOB value\n", len*2 + 1);
	fprintf(stderr, "BLOB field dump:\n");
	for(i = 0; i < len; i++){
		if(isprint(value[i])){
			fprintf(stderr, "%c", value[i]);
			}
		else{
			fprintf(stderr, ".");
			}
		}
	fprintf(stderr, "\n END OF BLOB field dump\n");
	exit(EXIT_FAILURE);
    	}
    for(k = 0; k < 256; k++){
    	sprintf(map[k], "%02X", k);
    	}
    for(i = 0; i < len; i++) {
	strncpy((blob + i*2), map[(unsigned int)value[i] & 0x000000FF], 2);
        }
    blob[len*2+1] = '\0';
    fwrite(blob, len*2, 1, f_result);
    free(blob);
}

/*******************************************************************/
inline void print_date(ulong ldate) {
	int year, month, day;

	ldate &= ~(1UL << 23);
	
	day = ldate % 32; ldate /= 32;
	month = ldate % 16; ldate /= 16;
	year = ldate;
	
	fprintf(f_result, "\"%04u-%02u-%02u\"", year, month, day);
}

/*******************************************************************/
inline void print_time(unsigned char* value, field_def_t *field) {
	int hour, min, sec;
	ulong ltime = mach_read_from_3(value);
	
	int frac = 0;
	switch(field->time_precision){
		case 1: frac = mach_read_from_1(value + 3); break;
		case 2: frac = mach_read_from_2(value + 3); break;
		case 3: frac = mach_read_from_3(value + 3); break;
		}
	if(field->time_precision != 0 || field->fixed_length > 3 ||  process_56 == 1){
		hour = ((ltime &  0x3FF000) >> 12 );
		min = ((ltime &  0xFC0) >> 6);
		sec = ((ltime &  0x3F));
		fprintf(f_result, "\"%02u:%02u:%02u.%d\"", hour, min, sec, frac);
	}else{
		ltime &= ~(1UL << 23);
		sec = ltime % 60; ltime /= 60;
		min = ltime % 60; ltime /= 60;
		hour = ltime % 24;
		fprintf(f_result, "\"%02u:%02u:%02u\"", hour, min, sec);
	}
}


/*******************************************************************/
inline void print_enum(int value, field_def_t *field) {
	fprintf(f_result, "%d", value);
	//fprintf(f_result, "\"%s\"", field->limits.enum_values[value-1]);
}

/*******************************************************************/
inline void print_set(int value, field_def_t *field) {
  /*    int i;
         int comma = 0
	fprintf(f_result, "\"");
	for(i=0; i<field->limits.set_values_count; i++){
		if(((value>>i) % 2) == 1){
			if(comma) printf(",");
			fprintf(f_result, "%s", field->limits.set_values[i]);
			comma=1;
		}
	}
	fprintf(f_result, "\"");
    */
  fprintf(f_result, "%d", value);
}

/*******************************************************************/
inline unsigned long long int get_uint_value(field_def_t *field, byte *value) {
	switch (field->fixed_length) {
		case 1: return mach_read_from_1(value);
		case 2: return mach_read_from_2(value);
		case 3: return mach_read_from_3(value) & 0x3FFFFFUL;
		case 4: return mach_read_from_4(value);
		case 5: return make_ulonglong(mach_read_from_8(value)) >> 24;
		case 6: return make_ulonglong(mach_read_from_8(value)) >> 16;
		case 7: return make_ulonglong(mach_read_from_8(value)) >> 8;
		case 8: return make_ulonglong(mach_read_from_8(value));
	}
	return 0;
}

/*******************************************************************/
inline long long int get_int_value(field_def_t *field, byte *value) {
	char v1;
	short int v2;
	int v3, v4;
	long int v8;
	switch (field->fixed_length) {
		case 1: v1 = (mach_read_from_1(value) & 0x7F) | ((~mach_read_from_1(value)) & 0x80);
			return v1;
		case 2: v2 = (mach_read_from_2(value) & 0x7FFF) | ((~mach_read_from_2(value)) & 0x8000);
			return v2;
		case 3: v3 = mach_read_from_3(value);
			if((v3 >> 23) == 1){
				// Positive
				v3 &= 0x007FFFFF;
			}else{
				// Negative
				v3 |= 0xFFFF8000;
			}
			return v3;
		case 4: v4 = (mach_read_from_4(value) & 0x7FFFFFFF) | ((~mach_read_from_4(value)) & 0x80000000);
			return v4;
		case 8: v8 = (make_ulonglong(mach_read_from_8(value)) & 0x7FFFFFFFFFFFFFFFULL) | ((~make_ulonglong(mach_read_from_8(value))) & 0x8000000000000000ULL);
			return v8;
	}
	return 0;
}

/*******************************************************************/
inline void print_string(char *value, ulint len, field_def_t *field) {
    uint i, num_spaces = 0, out_pos = 0;
    static char out[32768];
    
    out[out_pos++] = '"';
    for(i = 0; i < len; i++) {
        if (value[i] != ' ' && num_spaces > 0) {
            while(num_spaces > 0) {
                out[out_pos++] = ' ';
                num_spaces--;
            }
        }
		if (value[i] == '\\') out[out_pos++] = '\\', out[out_pos++] = '\\';
		else if (value[i] == '"') out[out_pos++] = '\\', out[out_pos++] = '"';
		else if (value[i] == '\n') out[out_pos++] = '\\', out[out_pos++] = 'n';
		else if (value[i] == '\r') out[out_pos++] = '\\', out[out_pos++] = 'r';
		else if (value[i] == '\t') out[out_pos++] = '\\', out[out_pos++] = 't';
		else if (value[i] == '\0') out[out_pos++] = '\\', out[out_pos++] = '0';
		else if (value[i] == '\b') out[out_pos++] = '\\', out[out_pos++] = 'b';
		else if (value[i] == 26) out[out_pos++] = '\\', out[out_pos++] = 'Z';
		else if (value[i] == '%') out[out_pos++] = '\\', out[out_pos++] = '%';
		else if (value[i] == '_') out[out_pos++] = '\\', out[out_pos++] = '_';
    	else {
            if (value[i] == ' ') {
                num_spaces++;
            } else {
                if ((int)value[i] < 32) {
                    //out_pos += sprintf(out + out_pos, "\\x%02x", (uchar)value[i]);
                    out[out_pos++] = value[i];
                } else {
                    out[out_pos++] = value[i];
                }
            }
        }
	}

    if (num_spaces > 0 && !field->char_rstrip_spaces) {
        while(num_spaces > 0) {
            out[out_pos++] = ' ';
            num_spaces--;
        }
    }
    out[out_pos++] = '"';
    out[out_pos++] = 0;
    fputs(out, f_result);
}

size_t strip_space(const char* str_in, char* str_out){
    size_t i = 0;
    size_t j = 0;
    while(str_in[i]!=0){
        if(str_in[i] != ' '){
            str_out[j] = str_in[i];
            j++;
        }
    i++;
    }
    str_out[j] = '\0';
    return j;
}

/*******************************************************************/
inline void print_decimal(byte *value, field_def_t *field) {
    char string_buf[256];
    char string_buf2[256];
    decimal_digit_t dec_buf[256];
    int len = 255;
    
    decimal_t dec;
    dec.buf = dec_buf;
    dec.len = 256;
    
    bin2decimal((char*)value, &dec, field->decimal_precision, field->decimal_digits);
    decimal2string(&dec, string_buf, &len, field->decimal_precision, field->decimal_digits, ' ');
    len = strip_space(string_buf, string_buf2);
    print_string(string_buf2, len, field);
}

/*******************************************************************/
inline void print_field_value(byte *value, ulint len, field_def_t *field) {
  /* time_t t;*/

	switch (field->type) {
		case FT_INTERNAL:
			print_hex((char*)value, len);
    			break;

		case FT_CHAR:
		case FT_TEXT:
			print_string((char*)value, len, field);
			break;

                case FT_BIN:
                        print_hex((char*)value, len);
			break;

		case FT_BLOB:
            		print_hex((char*)value, len);
            		break;

		case FT_BIT:
		case FT_UINT:
            		fprintf(f_result, "%llu", get_uint_value(field, value));
			break;

		case FT_INT:
            		fprintf(f_result, "%lli", get_int_value(field, value));
			break;

		case FT_FLOAT:
			fprintf(f_result, "%f", mach_float_read(value));
			break;

		case FT_DOUBLE:
			fprintf(f_result, "%lf", mach_double_read(value));
			break;

		case FT_DATETIME:
			print_datetime(value, field);
			break;

		case FT_DATE:
			print_date(mach_read_from_3(value));
			break;

		case FT_TIME:
			print_time(value, field);
			break;

		case FT_ENUM:
			if(field->fixed_length == 1){
				print_enum(mach_read_from_1(value), field);
			} else {
				print_enum(mach_read_from_2(value), field);
			}
			break;
		case FT_SET:
			print_set(mach_read_from_1(value), field);
			break;

	        case FT_DECIMAL:
			print_decimal(value, field);
			break;
		case FT_TIMESTAMP:
			print_timestamp(value, field);
			break;
		case FT_YEAR:
			fprintf(f_result, "%lu", mach_read_from_1(value) + 1900);
			break;

		default:
    			fprintf(f_result, "undef(%d)", field->type);
		}
}


/*******************************************************************/
void print_field_value_with_external(byte *value, ulint len, field_def_t *field) {
   ulint   page_no, offset, extern_len, len_sum;
   char tmp[256];
   page_t *page = ut_align(buffer, UNIV_PAGE_SIZE);

   switch (field->type) {
       case FT_TEXT:
       case FT_BLOB:
           page_no = mach_read_from_4(value + len - BTR_EXTERN_FIELD_REF_SIZE + BTR_EXTERN_PAGE_NO);
           offset = mach_read_from_4(value + len - BTR_EXTERN_FIELD_REF_SIZE + BTR_EXTERN_OFFSET);
           extern_len = mach_read_from_4(value + len - BTR_EXTERN_FIELD_REF_SIZE + BTR_EXTERN_LEN + 4);
           len_sum = 0;

           if (field->type == FT_TEXT) {
               fprintf(f_result, "\"");
               print_string_raw((char*)value, len - BTR_EXTERN_FIELD_REF_SIZE);
           } else {
               print_hex((char*)value, len - BTR_EXTERN_FIELD_REF_SIZE);
           }

           for (;;) {
               byte*   blob_header;
               ulint   part_len;

               sprintf(tmp, "%s/%016lu.page", blob_dir, page_no);
                if(external_in_ibdata){
                    if(fn == 0) { 
                        fn = open(path_ibdata, O_RDONLY);
                        if(fn == -1){
                            fprintf(stderr, "-- #####CannotOpen_%s;\n", path_ibdata);
                            perror("-- print_field_value_with_external(): open()");
                            break;
                        }
                    }
                    lseek(fn, page_no * UNIV_PAGE_SIZE, SEEK_SET);
                } else {
                    fn = open(tmp, O_RDONLY);
                    }
               if (fn != -1) {
                   ssize_t nread = read(fn, page, UNIV_PAGE_SIZE);
                   if (nread != UNIV_PAGE_SIZE)
                       break;

                   if (offset > UNIV_PAGE_SIZE)
                       break;

                   blob_header = page + offset;
                   part_len = mach_read_from_4(blob_header + 0 /*BTR_BLOB_HDR_PART_LEN*/);
                   if (part_len > UNIV_PAGE_SIZE)
                       break;
                   len_sum += part_len;
                   page_no = mach_read_from_4(blob_header + 4 /*BTR_BLOB_HDR_NEXT_PAGE_NO*/);
                   offset = FIL_PAGE_DATA;

                   if (field->type == FT_TEXT) {
                       print_string_raw((char*)blob_header + 8 /*BTR_BLOB_HDR_SIZE*/, part_len);
                   } else {
                       print_hex((char*)blob_header + 8 /*BTR_BLOB_HDR_SIZE*/, part_len);
                   }

                   if(!external_in_ibdata) close(fn);

                   if (page_no == FIL_NULL || page_no == 0)
                       break;

                   if (len_sum >= extern_len)
                       break;
               } else {
                    fprintf(stderr, "-- #####CannotOpen_%s;\n", tmp);
                    perror("-- print_field_value_with_external(): open()");
                    break;
               }
           }
           if (field->type == FT_TEXT) {
               fprintf(f_result, "\"");
           }

           break;

       default:
           fprintf(f_result, "error(%d)", field->type);
   }
}

/*******************************************************************/
void
rec_print_new(
/*==========*/
       FILE*           file,   /* in: file where to print */
       rec_t*          rec,    /* in: physical record */
       const ulint*    offsets)/* in: array returned by rec_get_offsets() */
{
       const byte*     data;
       ulint           len;
       ulint           i;

       ut_ad(rec_offs_validate(rec, NULL, offsets));

       ut_ad(rec);

       fprintf(file, "PHYSICAL RECORD: n_fields %lu;"
               " compact format; info bits %lu\n",
               (ulong) rec_offs_n_fields(offsets),
               (ulong) rec_get_info_bits(rec, TRUE));

       for (i = 0; i < rec_offs_n_fields(offsets); i++) {

               data = rec_get_nth_field(rec, offsets, i, &len);

               fprintf(file, " %lu:", (ulong) i);

               if (len != UNIV_SQL_NULL) {
                       if (len <= 30) {

                               ut_print_buf(file, data, len);
                       } else {
                               ut_print_buf(file, data, 30);

                               fputs("...(truncated)", file);
                       }
               } else {
                       fputs(" SQL NULL", file);
               }
               putc(';', file);
       }

       putc('\n', file);
}

