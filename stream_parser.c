#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#define _XOPEN_SOURCE 600
#include <fcntl.h>
//#ifndef _GNU_SOURCE
//#define _GNU_SOURCE // for O_NOATIME
//#define O_NOATIME 01000000
//#endif
#include <time.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <semaphore.h>

#include "mysql_def.h"

// Global flags from getopt
#ifdef STREAM_PARSER_DEBUG
int debug = 1;
#else
int debug = 0;
#endif

uint64_t filter_id;
int use_filter_id = 0;

ssize_t cache_size = 8*1024*1024; // 8M
off64_t ib_size = 0;
uint32_t max_page_id = 0;
char dst_dir[1024] = "";
int worker = 0;
#define  mutext_pool_size (8)
sem_t index_mutex[mutext_pool_size];
sem_t blob_mutex[mutext_pool_size];

void usage(char*);

#ifdef STREAM_PARSER_DEBUG
int DEBUG_LOG(char* format, ...){
    if(debug){
        char msg[1024] = "";
        //sprintf(format, "Worker(%d): %s\n", worker, fmt);
        va_list args;
        va_start(args, format);
        vsprintf(msg, format, args);
        va_end(args);
        return fprintf(stderr, "Worker(%d): %s\n", worker, msg);
        }
    return 0;
}
#endif

void error(char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}
// Prints size in human readable form
char* h_size(unsigned long long int size, char* buf){
    unsigned int power = 0;
    double d_size = size;
    while(d_size >= 1024){
        d_size /=1024;
        power += 3;
        }
    sprintf(buf, "%3.3f", d_size);
    switch(power){
        case 3: sprintf(buf, "%s %s", buf, "kiB"); break;
        case 6: sprintf(buf, "%s %s", buf, "MiB"); break;
        case 9: sprintf(buf, "%s %s", buf, "GiB"); break;
        case 12: sprintf(buf, "%s %s", buf, "TiB"); break;
        default: sprintf(buf, "%s exp(+%u)", buf, power); break;
        }
    return buf;
}

inline int valid_innodb_checksum(page_t* p){
    uint32_t oldcsum, oldcsumfield, csum, csumfield;
    int result = 0;

    // Checking old style checksums
    oldcsum= buf_calc_page_old_checksum(p);
    oldcsumfield= mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);
#ifdef STREAM_PARSER_DEBUG
    DEBUG_LOG("Old checksum: calculated=%lu, stored=%lu", oldcsum, oldcsumfield);
#endif
    if (oldcsumfield != oldcsum){
        result = 0;
        goto valid_innodb_checksum_exit;
        }
    // Checking new style checksums
    csum= buf_calc_page_new_checksum(p);
    csumfield= mach_read_from_4(p + FIL_PAGE_SPACE_OR_CHKSUM);
    if (csumfield != 0 && csum != csumfield){
        result = 0;
        goto valid_innodb_checksum_exit;
        }
    // return success
    result = 1;
valid_innodb_checksum_exit:
    return result;
    }

inline int valid_blob_page(page_t* page){
    uint16_t page_type = mach_read_from_2(page + FIL_PAGE_TYPE);
    if(page_type != FIL_PAGE_TYPE_BLOB){
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("Wrong page type = %u", page_type);
#endif
        return 0;
        }
    page_t* blob_header = page + FIL_PAGE_DATA; 
    uint32_t part_len = mach_read_from_4(blob_header + 0 /*BTR_BLOB_HDR_PART_LEN*/);
    if(part_len > UNIV_PAGE_SIZE){
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("Wrong part len = %u", part_len);
#endif
        return 0;
        }
    uint32_t page_id = mach_read_from_4(page + FIL_PAGE_OFFSET);
    if(page_id > max_page_id || page_id == 0){
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("Wrong page id %u", page_id);
#endif
        return 0;
        }
    uint32_t next_page = mach_read_from_4(blob_header + 4 /*BTR_BLOB_HDR_NEXT_PAGE_NO*/);
    if(next_page != 0xFFFFFFFF && (next_page > max_page_id || next_page == 0)){
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("Wrong next page_id = %u", next_page);
#endif
        return 0;
        }
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("All criterias are good. Checking checksum");
#endif
    return valid_innodb_checksum(page);

}
inline int valid_innodb_page(page_t* page, uint64_t block_size, off64_t* step){
    int version = 0; // 1 - new, 0 - old
    unsigned int page_n_heap, oldcsumfield;
    int inf_offset = 0, sup_offset = 0;
    uint32_t page_id = 0;

    if(step == NULL){
        fprintf(stderr, "%s: %d: step must not be NULL\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
        }
    *step = 1;

#ifdef STREAM_PARSER_DEBUG
    DEBUG_LOG("Fil Header");
    DEBUG_LOG("\tFIL_PAGE_SPACE:                   %08lX", mach_read_from_4(page + FIL_PAGE_SPACE_OR_CHKSUM));
    DEBUG_LOG("\tFIL_PAGE_OFFSET:                  %08lX", mach_read_from_4(page + FIL_PAGE_OFFSET));
    DEBUG_LOG("\tFIL_PAGE_PREV:                    %08lX", mach_read_from_4(page + FIL_PAGE_PREV));
    DEBUG_LOG("\tFIL_PAGE_NEXT:                    %08lX", mach_read_from_4(page + FIL_PAGE_NEXT));
    DEBUG_LOG("\tFIL_PAGE_LSN:                     %08lX", mach_read_from_4(page + FIL_PAGE_LSN));
    DEBUG_LOG("\tFIL_PAGE_TYPE:                        %04lX", mach_read_from_2(page + FIL_PAGE_TYPE));
    DEBUG_LOG("\tFIL_PAGE_FILE_FLUSH_LSN:  %016lX", mach_read_from_8(page + FIL_PAGE_FILE_FLUSH_LSN));
    DEBUG_LOG("\tFIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID: %08lX", mach_read_from_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID));
    DEBUG_LOG("\tFIL_PAGE_END_LSN_OLD_CHKSUM:      %08X", oldcsumfield);
#endif
    oldcsumfield = mach_read_from_4(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);
    if(mach_read_from_4(page) == 0){
        uint32_t i = 0;
        while(page[i] == 0){
#ifdef STREAM_PARSER_DEBUG
            DEBUG_LOG("page[%lu] = %u", i, page[i]);
#endif
            i++;
            if(i > block_size) break;
            }
        // return 0 if any of the first three bytes is non-zero
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("page[%lu] = %u", i, page[i]);
#endif
        *step = ((i - 3) > 0) ? i - 3 : 0;
        return 0;
        }
    page_id = mach_read_from_4(page + FIL_PAGE_OFFSET);
    if(page_id == 0){
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("page_id can not be zero");
#endif
        return 0;
        }
    if(page_id > max_page_id){
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("page_id %lu is bigger than maximum possible %lu", mach_read_from_4(page + FIL_PAGE_OFFSET), max_page_id);
        DEBUG_LOG("Invalid INDEX page");
#endif
        return 0;
        }
    page_n_heap = mach_read_from_4(page + PAGE_HEADER + PAGE_N_HEAP);
    version = ((page_n_heap & 0x80000000) == 0) ? 0 : 1;
#ifdef STREAM_PARSER_DEBUG
    DEBUG_LOG("Page Header");
    DEBUG_LOG("\tPAGE_N_HEAP: %08X", page_n_heap);
    DEBUG_LOG("\tVersion: %s", (version == 1)? "COMPACT" : "REDUNDANT");
#endif
    if(version == 1){
        inf_offset = PAGE_NEW_INFIMUM;
        sup_offset = PAGE_NEW_SUPREMUM;
        }
    else{
        inf_offset = PAGE_OLD_INFIMUM;
        sup_offset = PAGE_OLD_SUPREMUM;
        }
    if(page[inf_offset + 0] != 'i'
            || page[inf_offset + 1] != 'n'
            || page[inf_offset + 2] != 'f' 
            || page[inf_offset + 3] != 'i' 
            || page[inf_offset + 4] != 'm' 
            || page[inf_offset + 5] != 'u' 
            || page[inf_offset + 6] != 'm'){
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("infimum record is not found");
#endif
        goto invalid_innodb_page_exit;
        }
    
    if(page[sup_offset + 0] != 's'
            || page[sup_offset + 1] != 'u'
            || page[sup_offset + 2] != 'p'
            || page[sup_offset + 3] != 'r'
            || page[sup_offset + 4] != 'e'
            || page[sup_offset + 5] != 'm'
            || page[sup_offset + 6] != 'u'
            || page[sup_offset + 7] != 'm'){
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("supremum record is not found");
#endif
        goto invalid_innodb_page_exit;
        }
#ifdef STREAM_PARSER_DEBUG
    DEBUG_LOG("Valid INDEX page");
#endif
    return 1;
invalid_innodb_page_exit:
#ifdef STREAM_PARSER_DEBUG
    DEBUG_LOG("Invalid INDEX page");
#endif
    return 0;
    }

void show_progress(off64_t offset, off64_t length){
    struct tm timeptr;
    time_t remains;
    time_t finish_at;
    uint64_t progress_step = 0.01 * length;
    static off64_t offset_prev = 0;
    static time_t ts_prev = 0;
    time_t now;

    if(offset_prev == 0) offset_prev = offset ;
    if(ts_prev == 0) time(&ts_prev);

    if((offset - offset_prev) < progress_step) return;
    time(&now);
    if(now == ts_prev) return;

    char buf[32];
    char tmp[20];
    unsigned long processing_rate = (offset - offset_prev)/(now - ts_prev);
    // Remaining time = how much more to process / processing speed
    // We will finish in start time (=now()) + remaining time
    remains = (length - offset) / processing_rate;
    finish_at = remains + now;
    memcpy(&timeptr, localtime(&finish_at), sizeof(timeptr));
    strftime(buf, sizeof(buf), "%F %T", &timeptr);
    time_t h = remains/3600;
    time_t m = (remains - h*3600)/60;
    time_t s = remains - h*3600 - m*60;
    //if(worker == 3)fprintf(stderr, "Worker(%d): %.2f%% done. %s ETA(in %02lu:%02lu:%02lu). Processing speed: %s/sec\n",
    fprintf(stderr, "Worker(%d): %.2f%% done. %s ETA(in %02lu:%02lu:%02lu). Processing speed: %s/sec\n",
            worker,
            100.0 * offset / length, 
            buf, h, m, s,
            h_size(processing_rate, tmp)
            );
    ts_prev = now;
    offset_prev = offset;
    return;
}

inline void process_ibpage(page_t* page){
    uint32_t page_id = mach_read_from_4(page + FIL_PAGE_OFFSET);
    uint64_t index_id = mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID);
    uint16_t page_type = mach_read_from_2(page + FIL_PAGE_TYPE);
    if(filter_id != 0 && filter_id != index_id) return;
    int fn;
    char file_name[1024] = "";
    int flags;
    if(page_type == FIL_PAGE_INDEX){
        sprintf(file_name, "%s/FIL_PAGE_INDEX/%016lu.page", dst_dir, index_id);
        flags = O_WRONLY | O_CREAT | O_APPEND;
        }
    else{
        sprintf(file_name, "%s/FIL_PAGE_TYPE_BLOB/%016u.page", dst_dir, page_id);
        flags = O_WRONLY | O_CREAT ;
        flags = O_WRONLY | O_CREAT | O_APPEND;
        }
    int sem = (page_type == FIL_PAGE_INDEX) 
            ? sem_wait(index_mutex + (index_id % mutext_pool_size))
            : sem_wait(blob_mutex + (page_id % mutext_pool_size));
    fn = open(file_name, flags, 0644);
    if (!fn) error("Can't open file to save page!");
    if(-1 == write(fn, page, UNIV_PAGE_SIZE)){
        fprintf(stderr, "Can't write a page on disk: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
        }
    close(fn);
    sem = (page_type == FIL_PAGE_INDEX) 
            ? sem_post(index_mutex + (index_id % mutext_pool_size))
            : sem_post(blob_mutex + (page_id % mutext_pool_size));
    return;
    }
void process_ibfile(int fn, off64_t start_offset, ssize_t length){
    page_t *cache = NULL;
    cache = malloc(cache_size);
    ssize_t disk_read;
    off64_t curr_disk_offset = 0;
    off64_t prev_disk_offset = 0;
    off64_t global_offset = 0;
    
    if (!cache){
        char tmp[20];
        fprintf(stderr, "Can't allocate memory (%s) for disk cache\n", h_size(cache_size, tmp));
        error("Disk cache allocation failed");
        }
    if(cache_size > SSIZE_MAX){
        char tmp[20];
        fprintf(stderr, "Cache can't be bigger than %lu bytes(%s)\n", SSIZE_MAX, h_size(cache_size, tmp));
        error("Disk cache size is too big");
        }
    // Init cache offset pointer
    ssize_t curr_cache_offset = 0;
    // Read pages to the end of file
    curr_disk_offset = lseek64(fn, start_offset, SEEK_SET);
    while ((curr_disk_offset - start_offset) < length){ // Stop reads when we have read length bytes 
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("Reading from offset %lu", curr_disk_offset);
        DEBUG_LOG("cache_size = %lu", cache_size);
        DEBUG_LOG("curr_cache_offset = %lu", curr_cache_offset);
#endif
        disk_read = read(fn, cache + curr_cache_offset, cache_size - curr_cache_offset);
        if(disk_read == -1){
            fprintf(stderr, "Worker(%d): ", worker);
            perror("Failed to read from disk");
            exit(EXIT_FAILURE);
            }
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("Read %u bytes from disk to RAM", disk_read);
#endif
        if(disk_read == 0) break;

        // Processing pages in the cache
        ssize_t bytes_in_cache = curr_cache_offset + disk_read;
        // scanning the cache from the beginning
        curr_cache_offset = 0;
        while(bytes_in_cache - curr_cache_offset >= UNIV_PAGE_SIZE) {
            ssize_t cache_step = 1;
#ifdef STREAM_PARSER_DEBUG
            DEBUG_LOG("Checking page at cache offset %lu. Global offset %lu", curr_cache_offset, global_offset);
#endif
            if (0
#ifdef CentOS5
                    || valid_innodb_checksum(cache + curr_cache_offset)
#endif
                    || valid_blob_page(cache + curr_cache_offset)
                    || valid_innodb_page(cache + curr_cache_offset, bytes_in_cache - curr_cache_offset, &cache_step)){
                process_ibpage(cache + curr_cache_offset);
                cache_step = UNIV_PAGE_SIZE;
                }
#ifdef STREAM_PARSER_DEBUG
            DEBUG_LOG("Moving cache pointer %lu bytes", cache_step);
#endif
            curr_cache_offset += cache_step;
            global_offset += cache_step;
            }
        // Move remaining part of the cache to the beginning
        // Of course if we have anything remaining in the cache
        if(curr_cache_offset < bytes_in_cache){
#ifdef STREAM_PARSER_DEBUG
            DEBUG_LOG("%lu bytes remain in the cache. Moving it to the beginning", bytes_in_cache - curr_cache_offset);
#endif
            page_t *tmp_cache = NULL;
            tmp_cache = malloc(cache_size);
            if (!tmp_cache){
                char tmp[20];
                fprintf(stderr, "Can't allocate memory (%s) for temporary cache\n", h_size(cache_size, tmp));
                error("Disk cache allocation failed");
                }
            memcpy(tmp_cache, cache + curr_cache_offset, bytes_in_cache - curr_cache_offset);
            memcpy(cache, tmp_cache, bytes_in_cache - curr_cache_offset);
            free(tmp_cache);
            curr_cache_offset = bytes_in_cache - curr_cache_offset;
            }
        else{
            // 
            curr_cache_offset = 0;
            }
        // EOF processing cache

#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("curr_disk_offset = %llu, start_offset = %llu", curr_disk_offset, start_offset);
#endif
        show_progress(curr_disk_offset - start_offset, length);
        prev_disk_offset = curr_disk_offset;
        curr_disk_offset = lseek64(fn, 0, SEEK_CUR);
#ifdef STREAM_PARSER_DEBUG
        DEBUG_LOG("Disk offset at the end of read cycle %llu", curr_disk_offset);
#endif
        }
    return;
}

int open_ibfile(char *fname){
    struct stat st;
    int fn;
    char buf[255];

    fprintf(stderr, "Opening file: %s\n", fname);
    fprintf(stderr, "File information:\n\n");
       
    if(stat(fname, &st) != 0) {
        printf("Errno = %d, Error = %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
       }
    fprintf(stderr, "ID of device containing file: %12ju\n", st.st_dev);
    fprintf(stderr, "inode number:                 %12ju\n", st.st_ino);
    fprintf(stderr, "protection:                   %12o ", st.st_mode);
    switch (st.st_mode & S_IFMT) {
        case S_IFBLK:  fprintf(stderr, "(block device)\n");            break;
        case S_IFCHR:  fprintf(stderr, "(character device)\n");        break;
        case S_IFDIR:  fprintf(stderr, "(directory)\n");               break;
        case S_IFIFO:  fprintf(stderr, "(FIFO/pipe)\n");               break;
        case S_IFLNK:  fprintf(stderr, "(symlink)\n");                 break;
        case S_IFREG:  fprintf(stderr, "(regular file)\n");            break;
        case S_IFSOCK: fprintf(stderr, "(socket)\n");                  break;
        default:       fprintf(stderr, "(unknown file type?)\n");                break;
        }
    fprintf(stderr, "number of hard links:         %12zu\n", st.st_nlink);
    fprintf(stderr, "user ID of owner:             %12u\n", st.st_uid);
    fprintf(stderr, "group ID of owner:            %12u\n", st.st_gid);
    fprintf(stderr, "device ID (if special file):  %12ju\n", st.st_rdev);
    fprintf(stderr, "blocksize for filesystem I/O: %12lu\n", st.st_blksize);
    fprintf(stderr, "number of blocks allocated:   %12ju\n", st.st_blocks);
    fprintf(stderr, "time of last access:          %12lu %s", st.st_atime, ctime(&(st.st_atime)));
    fprintf(stderr, "time of last modification:    %12lu %s", st.st_mtime, ctime(&(st.st_mtime)));
    fprintf(stderr, "time of last status change:   %12lu %s", st.st_ctime, ctime(&(st.st_ctime)));
    h_size(st.st_size, buf);
    fprintf(stderr, "total size, in bytes:         %12jd (%s)\n\n", (intmax_t)st.st_size, buf);

    fn = open(fname, O_RDONLY, O_LARGEFILE);
    posix_fadvise(fn, 0, 0, POSIX_FADV_SEQUENTIAL);
    if (fn == -1){
        perror("Can't open file");
        exit(EXIT_FAILURE);
        }
    if(ib_size == 0){ // determine tablespace size if not given
        if(st.st_size != 0){
                ib_size = st.st_size;
                }
        }
    if(ib_size == 0){
        fprintf(stderr, "Can't determine size of %s. Specify it manually with -t option\n", fname);
        exit(EXIT_FAILURE);
        }
    fprintf(stderr, "Size to process:              %12lu (%s)\n", ib_size, h_size(ib_size, buf));
    max_page_id = ib_size/UNIV_PAGE_SIZE;
       
    return fn;
}

void usage(char* cmd) {
    fprintf(stderr,
        "Usage: %s -f <innodb_datafile> [-T N:M] [-s size] [-t size] [-V|-g]\n"
        "  Where:\n"
        "    -h         - Print this help\n"
        "    -V or -g   - Print debug information\n"
        "    -s size    - Amount of memory used for disk cache (allowed examples 1G 10M). Default 100M\n"
        "    -T         - retrieves only pages with index id = NM (N - high word, M - low word of id)\n"
        "    -t size    - Size of InnoDB tablespace to scan. Use it only if the parser can't determine it by himself.\n",
        cmd);
}

uint64_t get_factor(char suffix){
    uint64_t factor = 1;
    switch (suffix) {
        case 'k':
        case 'K':
            factor = 1024;
            break;
        case 'm':
        case 'M':
            factor = 1024 * 1024;
            break;
        case 'g':
        case 'G':
            factor = 1024 * 1024 * 1024;
            break;
        default:
            fprintf(stderr, "Unrecognized size suffix %c\n", suffix);
            factor = 1;
            }
    return factor;
}
/*******************************************************************/
int main(int argc, char **argv) {
    int fn = 0, ch;
    float m;
    char suffix;
    char buf[255];
    char ibfile[1024] = "";

    while ((ch = getopt(argc, argv, "gVhf:T:s:t:d:")) != -1) {
        switch (ch) {
            case 'f':
                strncpy(ibfile, optarg, sizeof(ibfile));
                break;
            case 'd':
                strncpy(dst_dir, optarg, sizeof(dst_dir));
                break;
            case 'V':
            case 'g':
                debug = 1;
                break;
            case 's':
                sscanf(optarg, "%f%c", &m, &suffix);
                cache_size = m * get_factor(suffix);
                if(cache_size < UNIV_PAGE_SIZE){
                    fprintf(stderr, "Disk cache size %lu can't be less than %u\n", cache_size, UNIV_PAGE_SIZE);
                    usage(argv[0]);
                    exit(EXIT_FAILURE);
                    }
                //cache_size = (cache_size / UNIV_PAGE_SIZE ) * UNIV_PAGE_SIZE;
                fprintf(stderr, "Disk cache:                   %12lu (%s)\n\n", cache_size, h_size(cache_size, buf));
                break;
            case 't':
                sscanf(optarg, "%f%c", &m, &suffix);
                ib_size = m * get_factor(suffix);
                break;
            case 'T':
                filter_id = strtoull(optarg, NULL, 10);
                break;
            default:
            case '?':
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            }
        }
    if(strlen(ibfile) == 0){
        fprintf(stderr, "You must specify file with -f option\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
        }
    if(strlen(dst_dir) == 0){
        snprintf(dst_dir, sizeof(dst_dir), "pages-%s", basename(ibfile));
        }
    // Create pages directory
    if(-1 == mkdir(dst_dir, 0755)){
        fprintf(stderr, "Could not create directory %s\n", dst_dir);
        perror("mkdir()");
        exit(EXIT_FAILURE);
        }
    char d[1024];
    // Create directory for index pages
    sprintf(d, "%s/FIL_PAGE_INDEX", dst_dir);
    if(-1 == mkdir(d, 0755)){
        fprintf(stderr, "Could not create directory %s\n", d);
        perror("mkdir()");
        exit(EXIT_FAILURE);
        }
    sprintf(d, "%s/FIL_PAGE_TYPE_BLOB", dst_dir);
    if(-1 == mkdir(d, 0755)){
        fprintf(stderr, "Could not create directory %s\n", d);
        perror("mkdir()");
        exit(EXIT_FAILURE);
        }
    // Init mutextes
    int i = 0;
    for(i = 0; i < mutext_pool_size; i++){
        sem_init(index_mutex + i, 1, 1);
        sem_init(blob_mutex + i, 1, 1);
        }
    int ncpu = sysconf(_SC_NPROCESSORS_CONF);
#ifdef STREAM_PARSER_DEBUG
    DEBUG_LOG("Number of CPUs %d\n", ncpu);
#endif
    int n;
    pid_t *pids = malloc(sizeof(pid_t)*ncpu);
    if (!pids){
        char tmp[20];
        fprintf(stderr, "Can't allocate memory (%s) for pid cache\n", h_size(sizeof(pid_t)*ncpu, tmp));
        error("PID cache allocation failed");
        }
    //ncpu = 1;
    time_t a,b; 
    time(&a);
    for(n = 0; n < ncpu; n++){
        pid_t pid = fork();
        //pid_t pid = 0;
        if(pid == 0){
            
            fn = open_ibfile(ibfile);
            if (fn == 0) {
                fprintf(stderr, "Can not open file %s\n", ibfile);
                usage(argv[0]);
                exit(EXIT_FAILURE);
                }
            // child
            worker = n;
            //if(worker == 0) debug = 1;
#ifdef STREAM_PARSER_DEBUG
            DEBUG_LOG("I'm child(%d): %u.", n, getpid());
            DEBUG_LOG("Processing from %lu bytes starting from %lu", ib_size/ncpu, n*ib_size/ncpu);
#endif
            process_ibfile(fn, n*ib_size/ncpu, ib_size/ncpu);
            exit(EXIT_SUCCESS);
            }
        else{
            pids[n] = pid;
            }
        }
    for(n = 0; n < ncpu; n++){
        int status;
        waitpid(pids[n], &status, 0);
        }
    // destroy semaphores
    for(i = 0; i < mutext_pool_size; i++){
        sem_destroy(index_mutex + i);
        sem_destroy(blob_mutex + i);
        }
    time(&b);
    printf("All workers finished in %lu sec\n", b - a);
    exit(EXIT_SUCCESS);
}
