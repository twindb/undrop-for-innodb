/* Copyright (C) 2000-2005 MySQL AB & Innobase Oy

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */

/*
  InnoDB offline file checksum utility.  85% of the code in this file
  was taken wholesale from the InnoDB codebase.

  The final 15% was originally written by Mark Smith of Danga
  Interactive, Inc. <junior@danga.com>

  Published with a permission.
*/

#include <stdint.h>
#include "innochecksum.h"

static void usage(const char *progname)
{
  printf("InnoDB offline file checksum utility.\n");
  printf("usage: %s [-h] [-c] [-s <start page>] [-e <end page>] [-p <page>] [-v] [-d] <filename>\n", progname);
  printf("\t-h\tprint this help message\n");
  printf("\t-c\tprint the count of pages in the file\n");
  printf("\t-s n\tstart on this page number (0 based)\n");
  printf("\t-e n\tend at this page number (0 based)\n");
  printf("\t-p n\tcheck only this page (0 based)\n");
  printf("\t-v\tverbose (prints progress every 5 seconds)\n");
  printf("\t-d\tdebug mode (prints checksums for each page)\n");
  printf("\t-f\tfix checksum if it is wrong\n");
  printf("\t-l <new lsn>\tWrite given LSN if it mismatches\n");
}

int main(int argc, char **argv)
{
  FILE *f;                     /* our input file */
  uchar *p;                     /* storage of pages read */
  int bytes;                   /* bytes read count */
  ulint ct;                    /* current page number (0 based) */
  int now;                     /* current time */
  int lastt;                   /* last time */
  uint32_t oldcsum, oldcsumfield, csum, csumfield, logseq, logseqfield, fixed_chksum, fixed_lsn, buf_lsn; /* checksum storage */
  struct stat st;              /* for stat, if you couldn't guess */
  unsigned long long int size; /* size of file (has to be 64 bits) */
  ulint pages;                 /* number of pages in file */
  ulint start_page= 0, end_page= 0, use_end_page= 0; /* for starting and ending at certain pages */
  off_t offset= 0;
  int just_count= 0;          /* if true, just print page count */
  int fix_checksum= 0;          /* if true, fix checksum if mismatches */
  int fix_lsn= 0;              /* if true, fix LSN if mismatches */
  int verbose= 0;
  int debug= 0;
  int c;
  int fd;
  fpos_t pos;

  /* remove arguments */
  while ((c= getopt(argc, argv, "cvds:e:p:fl:h")) != -1)
  {
    switch (c)
    {
    case 'v':
      verbose= 1;
      break;
    case 'c':
      just_count= 1;
      break;
    case 'f':
      fix_checksum= 1;
      break;
    case 'l':
      fix_lsn= 1;
      sscanf(optarg, "0x%X", &fixed_lsn);
      break;
    case 's':
      start_page= atoi(optarg);
      break;
    case 'e':
      end_page= atoi(optarg);
      use_end_page= 1;
      break;
    case 'p':
      start_page= atoi(optarg);
      end_page= atoi(optarg);
      use_end_page= 1;
      break;
    case 'd':
      debug= 1;
      break;
    case ':':
      fprintf(stderr, "option -%c requires an argument\n", optopt);
      return 1;
      break;
    case '?':
      fprintf(stderr, "unrecognized option: -%c\n", optopt);
      return 1;
      break;
    case 'h':
    default:
      usage(argv[0]);
      return 1;
      break;
    }
  }

  /* debug implies verbose... */
  if (debug) verbose= 1;

  /* make sure we have the right arguments */
  if (optind >= argc)
  {
    usage(argv[0]);
    return 1;
  }

  /* stat the file to get size and page count */
  if (stat(argv[optind], &st))
  {
    perror("error statting file");
    return 1;
  }
  size= st.st_size;
  pages= size / UNIV_PAGE_SIZE;
  if (just_count)
  {
    printf("%lu\n", pages);
    return 0;
  }
  else if (verbose)
  {
    printf("file %s = %llu bytes (%lu pages)...\n", argv[optind], size, pages);
    printf("checking pages in range %lu to %lu\n", start_page, use_end_page ? end_page : (pages - 1));
  }

  if(fix_checksum || fix_lsn){
  	/* open the file for reading and writing*/
  	f= fopen(argv[optind], "r+");
  	}
  else{
  	/* open the file for reading */
  	f= fopen(argv[optind], "r");
  	}
  if (!f)
  {
    perror("error opening file");
    return 1;
  }

  /* seek to the necessary position */
  if (start_page)
  {
    fd= fileno(f);
    if (!fd)
    {
      perror("unable to obtain file descriptor number");
      return 1;
    }

    offset= (off_t)start_page * (off_t)UNIV_PAGE_SIZE;

    if (lseek(fd, offset, SEEK_SET) != offset)
    {
      perror("unable to seek to necessary offset");
      return 1;
    }
  }

  /* allocate buffer for reading (so we don't realloc every time) */
  p= (uchar *)malloc(UNIV_PAGE_SIZE);

  /* main checksumming loop */
  ct= start_page;
  lastt= 0;
  while (!feof(f))
  {
    int modified = 0;

    fgetpos(f, &pos);
    bytes= fread(p, 1, UNIV_PAGE_SIZE, f);
    if (!bytes && feof(f)) return 0;
    if (bytes != UNIV_PAGE_SIZE)
    {
      fprintf(stderr, "bytes read (%d) doesn't match universal page size (%d)\n", bytes, UNIV_PAGE_SIZE);
      return 1;
    }

    /* check the "stored log sequence numbers" */
    logseq= mach_read_from_4(p + FIL_PAGE_LSN + 4);
    logseqfield= mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM + 4);
    if (debug)
      printf("page %lu: log sequence number: first = 0x%08X; second = 0x%08X\n", ct, logseq, logseqfield);
    if (logseq != logseqfield)
    {
      fprintf(stderr, "page %lu invalid (fails log sequence number check)\n", ct);
      printf("page %lu: log sequence number: first = 0x%08X; second = 0x%08X\n", ct, logseq, logseqfield);
      if(fix_lsn){
        fprintf(stderr, "New LSN will be 0x%X\n", fixed_lsn);
	buf_lsn = mach_read_from_4((uchar*)&fixed_lsn);
	memcpy(p + FIL_PAGE_LSN + 4, &buf_lsn, sizeof(buf_lsn));
	memcpy(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM + 4, &buf_lsn, sizeof(buf_lsn));
        modified = 1;
      	}
      else{
      	return 1;
      	}
    }

    /* now check the new method */
    csum= buf_calc_page_new_checksum(p);
    csumfield= mach_read_from_4(p + FIL_PAGE_SPACE_OR_CHKSUM);
    if (debug)
      printf("page %lu: new style: calculated = 0x%08X; recorded = 0x%08X\n", ct, csum, csumfield);
    if (csumfield != 0 && csum != csumfield)
    {
      fprintf(stderr, "page %lu invalid (fails new style checksum)\n", ct);
      fprintf(stderr, "page %lu: new style: calculated = 0x%08X; recorded = 0x%08X\n", ct, csum, csumfield);
      if(fix_checksum){
      	fprintf(stderr, "fixing new checksum of page %lu\n", ct);
	// rewrite recorded checksum with calculated one
	fixed_chksum = mach_read_from_4((uchar*)&csum);
	memcpy(p + FIL_PAGE_SPACE_OR_CHKSUM, &fixed_chksum, sizeof(fixed_chksum));
        modified = 1;
      	}
      else{
      	return 1;
      	}
    }

    /* check old method of checksumming */
    oldcsum= buf_calc_page_old_checksum(p);
    oldcsumfield= mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);
    if (debug)
      printf("page %lu: old style: calculated = 0x%08X; recorded = 0x%08X\n", ct, oldcsum, oldcsumfield);
    if (oldcsumfield != oldcsum)
    {
      fprintf(stderr, "page %lu invalid (fails old style checksum)\n", ct);
      fprintf(stderr, "page %lu: old style: calculated = 0x%08X; recorded = 0x%08X\n", ct, oldcsum, oldcsumfield);
      if(fix_checksum){
      	fprintf(stderr, "fixing old checksum of page %lu\n", ct);
	// rewrite recorded checksum with calculated one
	fixed_chksum = mach_read_from_4((uchar*)&oldcsum);
	memcpy(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM, &fixed_chksum, sizeof(fixed_chksum));
        modified = 1;
      	}
      else{
      	return 1;
      	}
    }

    if (modified) {
      // move to the beginning of the page
      if(0 != fsetpos(f, &pos)) {
        perror("couldn't set a position to the start of the page");
        return 1;
      }
      if(1 != fwrite(p, UNIV_PAGE_SIZE, 1, f)) {
        perror("couldn't update the page");
        return 1;
      }
    }

    /* end if this was the last page we were supposed to check */
    if (use_end_page && (ct >= end_page))
      return 0;

    /* do counter increase and progress printing */
    ct++;
    if (verbose)
    {
      if (ct % 64 == 0)
      {
        now= time(0);
        if (!lastt) lastt= now;
        if (now - lastt >= 1)
        {
          printf("page %lu okay: %.3f%% done\n", (ct - 1), (float) ct / pages * 100);
          lastt= now;
        }
      }
    }
  }
  return 0;
}

