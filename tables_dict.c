#include <stdio.h>

#include <univ.i>
#include <page0page.h>
#include <rem0rec.h>

int table_definitions_cnt;
int record_extra_bytes;

#include <tables_dict.h>
//#include <table_defs.h>

extern table_def_t table_definitions[1];
extern bool deleted_pages_only;
extern bool debug;
extern bool process_compact, process_redundant;

/*******************************************************************/
void init_table_defs() {
	int i, j;

	if (debug) printf("Initializing table definitions...\n");
	table_definitions_cnt = sizeof(table_definitions) / sizeof(table_def_t);
	if (debug) printf("There are %d tables defined\n", table_definitions_cnt);
    record_extra_bytes = (process_compact ? REC_N_NEW_EXTRA_BYTES : REC_N_OLD_EXTRA_BYTES);
    
	for (i = 0; i < table_definitions_cnt; i++) {
		table_def_t *table = &(table_definitions[i]);
		if (debug) printf("Processing table: %s\n", table->name);
		
		table->n_nullable = 0;
		table->min_rec_header_len = 0;
		table->data_min_size = 0;
		table->data_max_size = 0;
		
		for(j = 0; j < MAX_TABLE_FIELDS; j++) {
		    if (debug) printf("Counting field: %s\n", table->fields[j].name);
			if (table->fields[j].type == FT_NONE) {
				table->fields_count = j;
				break;
			}

			if (table->fields[j].can_be_null) {
				table->n_nullable++;
			} else {
    			table->data_min_size += table->fields[j].min_length + table->fields[j].fixed_length;
				int size = (table->fields[j].fixed_length ? table->fields[j].fixed_length : table->fields[j].max_length);
				table->min_rec_header_len += (size > 255 ? 2 : 1);
			}

			table->data_max_size += table->fields[j].max_length + table->fields[j].fixed_length;
		}
		
		table->min_rec_header_len += (table->n_nullable + 7) / 8;
		
		if (debug) {
			printf(" - total fields: %i\n", table->fields_count);
			printf(" - nullable fields: %i\n", table->n_nullable);
			printf(" - minimum header size: %i\n", table->min_rec_header_len);
			printf(" - minimum rec size: %i\n", table->data_min_size);
			printf(" - maximum rec size: %lli\n", table->data_max_size);
			printf("\n");
		}
	}
}

/*******************************************************************/
int mysql_get_identifier_quote_char(trx_t* trx, const char* name, ulint namelen) {
	return '"';
}
