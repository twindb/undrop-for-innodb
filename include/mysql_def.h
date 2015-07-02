#ifndef _MYSQL_DEF_H
#define _MYSQL_DEF_H 1

#define UNIV_PAGE_SIZE (16*1024)

#define UT_HASH_RANDOM_MASK     1463735687
#define UT_HASH_RANDOM_MASK2    1653893711

/* The byte offsets on a file page for various variables */
#define FIL_PAGE_SPACE_OR_CHKSUM 0	/* in < MySQL-4.0.14 space id the
					page belongs to (== 0) but in later
					versions the 'new' checksum of the
					page */
#define FIL_PAGE_OFFSET		4	/* page offset inside space */
#define FIL_PAGE_PREV		8	/* if there is a 'natural' predecessor
					of the page, its offset */
#define FIL_PAGE_NEXT		12	/* if there is a 'natural' successor
					of the page, its offset */
#define FIL_PAGE_LSN		16	/* lsn of the end of the newest
					modification log record to the page */
#define	FIL_PAGE_TYPE		24	/* file page type: FIL_PAGE_INDEX,...,
					2 bytes */
#define FIL_PAGE_FILE_FLUSH_LSN	26	/* this is only defined for the
					first page in a data file: the file
					has been flushed to disk at least up
					to this lsn */
#define FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID  34 /* starting from 4.1.x this
					contains the space id of the page */
#define FIL_PAGE_DATA		38	/* start of the data on the page */

/* File page trailer */
#define FIL_PAGE_END_LSN_OLD_CHKSUM 8	/* the low 4 bytes of this are used
					to store the page checksum, the
					last 4 bytes should be identical
					to the last 4 bytes of FIL_PAGE_LSN */
#define FIL_PAGE_DATA_END	8

/* File page types */
#define FIL_PAGE_INDEX		17855
#define FIL_PAGE_UNDO_LOG	2
#define FIL_PAGE_INODE		3
#define FIL_PAGE_IBUF_FREE_LIST	4
/* File page types introduced in MySQL/InnoDB 5.1.7 */
#define FIL_PAGE_TYPE_ALLOCATED 0   /* Freshly allocated page */
#define FIL_PAGE_IBUF_BITMAP    5   /* Insert buffer bitmap */
#define FIL_PAGE_TYPE_SYS   6   /* System page */
#define FIL_PAGE_TYPE_TRX_SYS   7   /* Transaction system data */
#define FIL_PAGE_TYPE_FSP_HDR   8   /* File space header */
#define FIL_PAGE_TYPE_XDES  9   /* Extent descriptor page */
#define FIL_PAGE_TYPE_BLOB  10  /* Uncompressed BLOB page */

#define	PAGE_HEADER	FSEG_PAGE_DATA	/* index page header starts at this
				offset */
/*-----------------------------*/
#define PAGE_N_DIR_SLOTS 0	/* number of slots in page directory */
#define	PAGE_HEAP_TOP	 2	/* pointer to record heap top */
#define	PAGE_N_HEAP	 4	/* number of records in the heap,
				bit 15=flag: new-style compact page format */
#define	PAGE_FREE	 6	/* pointer to start of page free record list */
#define	PAGE_GARBAGE	 8	/* number of bytes in deleted records */
#define	PAGE_LAST_INSERT 10	/* pointer to the last inserted record, or
				NULL if this info has been reset by a delete,
				for example */
#define	PAGE_DIRECTION	 12	/* last insert direction: PAGE_LEFT, ... */
#define	PAGE_N_DIRECTION 14	/* number of consecutive inserts to the same
				direction */
#define	PAGE_N_RECS	 16	/* number of user records on the page */
#define PAGE_MAX_TRX_ID	 18	/* highest id of a trx which may have modified
				a record on the page; a dulint; defined only
				in secondary indexes; specifically, not in an
				ibuf tree; NOTE: this may be modified only
				when the thread has an x-latch to the page,
				and ALSO an x-latch to btr_search_latch
				if there is a hash index to the page! */
#define PAGE_HEADER_PRIV_END 26	/* end of private data structure of the page
				header which are set in a page create */
/*----*/
#define	PAGE_LEVEL	 26	/* level of the node in an index tree; the
				leaf level is the level 0 */
#define	PAGE_INDEX_ID	 28	/* index id where the page belongs */
#define PAGE_BTR_SEG_LEAF 36	/* file segment header for the leaf pages in
				a B-tree: defined only on the root page of a
				B-tree, but not in the root of an ibuf tree */
#define PAGE_BTR_IBUF_FREE_LIST	PAGE_BTR_SEG_LEAF
#define PAGE_BTR_IBUF_FREE_LIST_NODE PAGE_BTR_SEG_LEAF
				/* in the place of PAGE_BTR_SEG_LEAF and _TOP
				there is a free list base node if the page is
				the root page of an ibuf tree, and at the same
				place is the free list node if the page is in
				a free list */
#define PAGE_BTR_SEG_TOP (36 + FSEG_HEADER_SIZE)
				/* file segment header for the non-leaf pages
				in a B-tree: defined only on the root page of
				a B-tree, but not in the root of an ibuf
				tree */
/*----*/
#define PAGE_DATA	(PAGE_HEADER + 36 + 2 * FSEG_HEADER_SIZE)
				/* start of data on the page */

#define PAGE_OLD_INFIMUM	(PAGE_DATA + 1 + REC_N_OLD_EXTRA_BYTES)
				/* offset of the page infimum record on an
				old-style page */
#define PAGE_OLD_SUPREMUM	(PAGE_DATA + 2 + 2 * REC_N_OLD_EXTRA_BYTES + 8)
				/* offset of the page supremum record on an
				old-style page */
#define PAGE_OLD_SUPREMUM_END (PAGE_OLD_SUPREMUM + 9)
				/* offset of the page supremum record end on
				an old-style page */
#define PAGE_NEW_INFIMUM	(PAGE_DATA + REC_N_NEW_EXTRA_BYTES)
				/* offset of the page infimum record on a
				new-style compact page */
#define PAGE_NEW_SUPREMUM	(PAGE_DATA + 2 * REC_N_NEW_EXTRA_BYTES + 8)
				/* offset of the page supremum record on a
				new-style compact page */
#define PAGE_NEW_SUPREMUM_END (PAGE_NEW_SUPREMUM + 8)
				/* offset of the page supremum record end on
				a new-style compact page */
/*-----------------------------*/

/* Directions of cursor movement */
#define	PAGE_LEFT		1
#define	PAGE_RIGHT		2
#define	PAGE_SAME_REC		3
#define	PAGE_SAME_PAGE		4
#define	PAGE_NO_DIRECTION	5

#define FSEG_PAGE_DATA          FIL_PAGE_DATA
#define FSEG_HEADER_SIZE        10

#define REC_N_NEW_EXTRA_BYTES   5
#define REC_N_OLD_EXTRA_BYTES   6

typedef uint8_t page_t;

// #define mach_read_from_4(b) ( ((uint_fast32_t)((b)[0]) << 24) + ((uint_fast32_t)((b)[1]) << 16) + ((uint_fast32_t)((b)[2]) << 8) + (uint_fast32_t)((b)[3]) )


inline uint32_t mach_read_from_4(page_t *b){
  //  return 0;
  return( ((uint32_t)(b[0]) << 24)
          + ((uint32_t)(b[1]) << 16)
          + ((uint32_t)(b[2]) << 8)
          + (uint32_t)(b[3])
          );
}
uint32_t mach_read_from_2(page_t* b){
    return(((uint32_t)(b[0]) << 8)
        + (uint32_t)(b[1]));
}

uint64_t mach_read_from_8(page_t* b){
    uint32_t high;
    uint32_t low;

    high = mach_read_from_4(b);
    low = mach_read_from_4(b + 4);
    return ((uint64_t)high << 32) + (uint64_t)low;
}

uint32_t ut_fold_ulint_pair(uint32_t n1, uint32_t n2){
    return(((((n1 ^ n2 ^ UT_HASH_RANDOM_MASK2) << 8) + n1)
                        ^ UT_HASH_RANDOM_MASK) + n2);
}
uint32_t ut_fold_binary(page_t* str, uint32_t len){
    uint32_t i;
    uint32_t fold= 0;
    for (i= 0; i < len; i++){
        fold= ut_fold_ulint_pair(fold, (uint32_t)(*str));
        str++;
        }
    return(fold);
}
uint32_t buf_calc_page_old_checksum(page_t* page) {
    uint32_t checksum;
    checksum = ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN);
    checksum= checksum & 0xFFFFFFFF;
    return(checksum);
}
uint32_t buf_calc_page_new_checksum(page_t* page){
    uint32_t checksum;
    /* Since the fields FIL_PAGE_FILE_FLUSH_LSN and ..._ARCH_LOG_NO
    are written outside the buffer pool to the first pages of data
    files, we have to skip them in the page checksum calculation.
    We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
    checksum is stored, and also the last 8 bytes of page because
    there we store the old formula checksum. */

    checksum= ut_fold_binary(page + FIL_PAGE_OFFSET,
                             FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET)
            + ut_fold_binary(page + FIL_PAGE_DATA,
                             UNIV_PAGE_SIZE - FIL_PAGE_DATA
                             - FIL_PAGE_END_LSN_OLD_CHKSUM);
    checksum= checksum & 0xFFFFFFFF;
    return(checksum);
}
#endif
