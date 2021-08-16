#include "pmparser.h"
#include "maketable.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <plugin.h>
#include <functional>
#include <vector>
#include <sys/mman.h>

#ifdef USE_X11
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define PROC_DIR "/proc/"



struct mempsname : csnd::Plugin<1, 1> {
    static constexpr char const *otypes = "S";
    static constexpr char const *itypes = "i";
    int init() {
        int pid = inargs[0];
        char* path = (char*) csound->malloc(sizeof(char) * PATH_MAX);
        sprintf(path, "%s%d/comm", PROC_DIR, pid);
        
        FILE* f = fopen(path, "r");
        char* buffer = (char*) csound->malloc(sizeof(char) * 1024);
        size_t size = fread(buffer, sizeof(char), 1024, f);

        
        fclose(f);
        STRINGDAT &opath = outargs.str_data(0);
        opath.size = strlen(buffer); 
        opath.data = buffer;
        
        csound->free(path);
        return OK;
    }
};


struct memps : csnd::Plugin<1, 0> {
    static constexpr char const *otypes = "i[]";
    static constexpr char const *itypes = "";
    
    int init() {
        
        int* processes = (int*) csound->malloc(sizeof(int) * 1000);
        int processnum = 0;
        if (!getPIDs(processes, &processnum)) {
            return csound->init_error("Cannot read proc directory");
        }
        
        csnd::Vector<MYFLT> &out = outargs.vector_data<MYFLT>(0);
        out.init(csound, processnum);
        
        for (int i = 0; i < processnum; i++) {
            out[i] = (MYFLT) processes[i];
        }
        csound->free(processes);
        return OK;
    }
    
    bool isNumeric(char* input) {
        char* p;
        strtol(input, &p, 10);
        return *p == 0;
    }
    
    bool getPIDs(int* processes, int* processnum) { 
        uid_t thisUser = getuid();
        DIR* dir = opendir(PROC_DIR);

        struct dirent* de = NULL; 
        if (dir == NULL) { 
            //return false; // TODO breaks csound, why??
        } 
        char* path = (char*) csound->malloc(sizeof(char) * PATH_MAX);
        while (NULL != (de = readdir(dir))) {
            if (de->d_type == DT_DIR) {
                if (isNumeric(de->d_name)) {
                    
                    struct stat fileInfo;
                    strcpy(path, PROC_DIR);
                    strcat(path, de->d_name);
                    if (stat(path, &fileInfo) == 0) {
                        if (fileInfo.st_uid == thisUser) {
                            (*processnum)++;
                            *processes = (int)atoi(de->d_name);
                            processes++;
                        }
                    }
                }
            }
        }
        csound->free(path);
        return true;
    }
    
};


#ifdef USE_X11
struct winson : csnd::Plugin<1, 1> {
    static constexpr char const *otypes = "a";
    static constexpr char const *itypes = "i";
    Display* display;
    Window root;
    int width;
    int height;
    MYFLT* buffer;
    int buffer_size;
    int buffer_read_position;
    int buffer_write_position;
    
    int init() {
        display = XOpenDisplay(NULL);
        //root = DefaultRootWindow(display);
        root = 0x2200004;
        XWindowAttributes gwa;
        XGetWindowAttributes(display, root, &gwa);
        
        width = gwa.width;
        height = gwa.height;
        buffer_size = width * height;
        buffer = (MYFLT*) csound->malloc(sizeof(MYFLT) * buffer_size);
        buffer_read_position = 0;
        buffer_write_position = 0;
        refill_buffer();
        return OK;
    }
    
    int refill_buffer() {
        buffer_write_position = 0;
        XImage* image = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
        for (int x = 0; x < width; x++) {
            for (int y=0; y < height; y++) {
                buffer[buffer_write_position] = ((MYFLT) XGetPixel(image,x,y)) / 16777215;
                buffer_write_position++;
            }
        }
        return OK;
    }
    
    int aperf() {
        for (int i = 0; i < nsmps; i++) {
            outargs(0)[i] = buffer[buffer_read_position];
            if (LIKELY(buffer_read_position + 1 < buffer_size)) {
                buffer_read_position++;
            } else {
                refill_buffer();
                buffer_read_position = 0;
            }
        }
            
        return OK;    
        
    }
};
#endif


class MemLocation {
public:
    unsigned long start;
    unsigned long length;
    unsigned long* content;  // for mmap test..
    MemLocation(unsigned long start, unsigned long length) {
        this->start = start;
        this->length = length;
    }
};


class MemParser {
private:
    csnd::Csound* csound;
    int fd_mem;
    int pid;
    bool skip_zero;
    int buffer_write_position;
    std::vector<MemLocation*> locations;
    
    /*
    void iterate_memory(std::function<void (unsigned long, unsigned long)> func) {
        procmaps_iterator* maps = pmparser_parse(csound, pid);
        procmaps_struct* maps_tmp = NULL;
        while ((maps_tmp = pmparser_next(maps)) != NULL) {
            if (maps_tmp->is_r && maps_tmp->is_w) {
                func((unsigned long) maps_tmp->addr_start, (unsigned long) maps_tmp->length);
            }
        }
        pmparser_free(csound, maps);
    }
     */
    
    bool parsed;
    void parse_memory() {
        locations.clear();
        procmaps_iterator* maps = pmparser_parse(csound, pid);
        procmaps_struct* maps_tmp = NULL;
        while ((maps_tmp = pmparser_next(maps)) != NULL) {
            if (maps_tmp->is_r && maps_tmp->is_w) {
                locations.push_back(
                    new MemLocation(
                        (unsigned long) maps_tmp->addr_start, 
                        (unsigned long) maps_tmp->length
                    )
                );
                std::cout << (unsigned long) maps_tmp->addr_start << "-" << (unsigned long) maps_tmp->length << "\n";
            }
        }
        pmparser_free(csound, maps);
        parsed = true;
    }
    
    // this idea ends up with more mem locations than expected, whytf? vector malloc not right with csound???
    void iterate_memory_step(std::function<bool (long, MYFLT)> func) {
        if (!parsed) parse_memory();
        long i;
        MYFLT val; int x = 0;
        unsigned long* lbuffer = NULL;
        std::cout << "sz=" << locations.size() << "\n";
        for (auto &location : locations) {
            lbuffer = (unsigned long*) csound->malloc(sizeof(unsigned long) * location->length);
            lseek(fd_mem, location->start, SEEK_SET);
            read(fd_mem, lbuffer, location->length);
            std::cout << (unsigned long) location->start << "-" << (unsigned long) location->length << "\n";
            for (i = 0; i < location->length; i++) {
                val = ((MYFLT) lbuffer[i]) / ULONG_MAX;
                if (!skip_zero || (skip_zero && val > 0)) {
                    if (!(func(i, val))) goto complete;
                }
                
            }

            if (lbuffer != NULL) {
                csound->free(lbuffer); // causes segfault, WHY???????????
                lbuffer = NULL;
            }

        }
        complete:
        if (lbuffer != NULL) {
            csound->free(lbuffer); // causes segfault, WHY???????????
            lbuffer = NULL;
        }
        
    }
    
    // func arguments: index and value ; return false to stop iteration, true to continue
    void Xiterate_memory_step(std::function<bool (long, MYFLT)> func) {
        MYFLT val;
        unsigned long* lbuffer = NULL;
        procmaps_iterator* maps = pmparser_parse(csound, pid);
        procmaps_struct* maps_tmp = NULL;
        while ((maps_tmp = pmparser_next(maps)) != NULL) {
            if (maps_tmp->is_r && maps_tmp->is_w) {
                unsigned long start = (unsigned long) maps_tmp->addr_start;
                unsigned long length = (unsigned long) maps_tmp->length;
                lbuffer = (unsigned long*) csound->malloc(sizeof(unsigned long) * length);
                lseek(fd_mem, start, SEEK_SET);
                read(fd_mem, lbuffer, length);
                for (long i = 0; i < length; i++) {
                    val = ((MYFLT) lbuffer[i]) / ULONG_MAX;
                    if (!skip_zero || (skip_zero && val > 0)) {
                        if (!(func(i, val))) goto complete;
                    }
                }
                
                if (lbuffer != NULL) {
                    //csound->free(lbuffer); // causes segfault, WHY???????????
                    //lbuffer = NULL;
                }
                
            }
        }
    complete:
        if (lbuffer != NULL) {
            csound->free(lbuffer); // causes segfault, WHY???????????
            lbuffer = NULL;
        }
        pmparser_free(csound, maps);
    }
    
public:
    long total_size;
    MYFLT* buffer;
    int buffer_size;
    
    MemParser(csnd::Csound* csound, int pid, bool skip_zero) {
        this->csound = csound;
        this->skip_zero = skip_zero;
        this->pid = pid;
        parsed = false;
        total_size = 0;
        buffer = NULL;
        buffer_size = 0;
        buffer_write_position = 0;
        char* mem_path = (char*) csound->malloc(sizeof(char) * 50);
        sprintf(mem_path, "/proc/%d/mem", pid);
        fd_mem = open(mem_path, O_RDWR);
        csound->free(mem_path);
    }
    
    ~MemParser() {
        close(fd_mem);
        if (buffer != NULL) {
            csound->free(buffer);
        }
    }
    
    void allocate_buffer(int buffer_size) {
        buffer = (MYFLT*) csound->malloc(sizeof(MYFLT) * buffer_size);
        this->buffer_size = buffer_size;
    }
    
    long get_size() {
        total_size = 0;
        iterate_memory_step(
            [&](long index, MYFLT val) { 
                total_size ++;
                return true;
            }
        );
        return total_size;
    }
    
    
    int fill_buffer(MYFLT offset) {
        if (total_size == 0) {
            total_size = get_size();
        }
        buffer_write_position = 0;
        if (offset > 1) offset = 1;
        long offset_position = total_size * offset;
        /*if (UNLIKELY(offset_position > total_size - (buffer_size + 1))) {
            offset_position = total_size - (buffer_size + 1);
        }*/
        bool active;
        
        iterate_memory_step(
            [&](long index, MYFLT val) { 
                if (index < offset_position) return true;
                //std::cout << index << "-" << offset_position << "\n";
                buffer[buffer_write_position] = val;
                if (buffer_write_position < buffer_size) {
                    buffer_write_position ++;
                } else {
                    buffer_write_position = 0;
                    return false;
                }
                return true;
            }
        );
    }
    
    void fill_table(FUNC* table, long length) {
        long write_index = 0;
        bool active = true;
        
        iterate_memory_step(
            [&](long index, MYFLT val) { 
                table->ftable[write_index] = val;
                write_index ++;
                return true;
            }
        );   
    }
     
};


// ifn mem2tab ipid, iskipzero=0
struct mem2tab : csnd::Plugin<1, 2> {
    static constexpr char const *otypes = "i";
    static constexpr char const *itypes = "io";

    int init() {
        FUNC *table;
        MemParser* mp = new MemParser(csound, (int)inargs[0], (inargs[1] > 0));
        
        long length = mp->get_size();
        if ((maketable(csound, (int)length, &table, 1)) != OK) {
            return csound->init_error("Cannot create ftable");
        }
        mp->fill_table(table, (int)length); 
        outargs[0] = table->fno;
        return OK;
    }
};


// read memory from a given pid as audio
// aout memson ipid, koffset, kbuffer_readratio, ibuffersize=441000, iskipzero=0
struct memson : csnd::Plugin<1, 5> {
    static constexpr char const *otypes = "a";
    static constexpr char const *itypes = "ikkjo";
    MemParser* mp;
    int buffer_read_position;
    MYFLT last_offset;
    
    int init() {
        mp = new MemParser(csound, (int)inargs[0], (inargs[4] > 0));
        mp->allocate_buffer((inargs[2] == -1)? 441000: (int) inargs[3]);
        
        buffer_read_position = 0;
        last_offset = inargs[1];
        mp->fill_buffer(last_offset);
        
        return OK;
    }
    
    int aperf() {
        for (int i = 0; i < nsmps; i++) {
            outargs(0)[i] = mp->buffer[buffer_read_position];
            if (LIKELY(buffer_read_position < mp->buffer_size*inargs[2])) {
                buffer_read_position++;
            } else {
                buffer_read_position = 0;
            }
        }
        if (inargs[1] != last_offset) {
            last_offset = inargs[1];
            mp->fill_buffer(last_offset);
        }
            
        return OK;    
        
    }
    
    
};





typedef struct mmap {
    unsigned long start;
    unsigned long length;
} _mmap;


// read memory from a given pid as audio
// aout memson2 ipid, koffset, kbufsize [, iunsafe]
// where kbufsize is 0 to 1 and koffset is 0 to 1
struct memson2 : csnd::Plugin<1, 4> {
    static constexpr char const *otypes = "a";
    static constexpr char const *itypes = "ikkj";
    int pid;
    int buffer_read_position;
    int buffer_write_position;
    int buffer_size;
    int max_buffer_size;
    unsigned long total_locations;
    MYFLT* buffer;
    MYFLT last_buffer_ratio;
    MYFLT last_offset;
    char* mem_path;
	bool unsafe;
    
    int init() {
        max_buffer_size = 441000;
        buffer_size = (int) (inargs[2] * max_buffer_size);
        last_buffer_ratio = inargs[2];
        last_offset = inargs[1];
		unsafe = (((int)inargs[3]) == 1);
        csound->plugin_deinit(this);
        pid = (int) inargs[0];
        buffer = (MYFLT*) csound->malloc(sizeof(MYFLT) * max_buffer_size);
        mem_path = (char*) csound->malloc(sizeof(char) * 50);
        sprintf(mem_path, "/proc/%d/mem", pid);
      
        buffer_read_position = 0;
        buffer_write_position = 0;
        total_locations = max_locations();
        refill_buffer(inargs[1], true);
        
        return OK;
    }
    
    int deinit() {
        csound->free(mem_path);        
        return OK;
    }
    
    unsigned long max_locations() {
        procmaps_iterator* maps = pmparser_parse(csound, pid);
        int fd_mem = open(mem_path, O_RDWR);
        unsigned long len = 0;
        procmaps_struct* maps_tmp = NULL;
        while ((maps_tmp = pmparser_next(maps)) != NULL) {
            if (maps_tmp->is_r && maps_tmp->is_w) {
                len += maps_tmp->length;
            }
        }
        pmparser_free(csound, maps);
        close(fd_mem);
        return len;
    }
    
    int read_mem(int fd_mem, unsigned long start, unsigned long length) {
        length = length; // 4; // ??????????????????????????????????????????????????????
        // lbuffer as unsigned char
        // https://unix.stackexchange.com/questions/6301/how-do-i-read-from-proc-pid-mem-under-linux
        unsigned long* lbuffer = (unsigned long*) csound->malloc(sizeof(unsigned long) * length);
        lseek(fd_mem, start, SEEK_SET);
        read(fd_mem, lbuffer, length);

        int i;
        for (i = 0; i < length; i++) {
            //std::cout << "a " << buffer_write_position << "x " << buffer_size << "\n";
            MYFLT val = ((MYFLT)lbuffer[i]) / ULONG_MAX;
            buffer[buffer_write_position] = val;
            if (buffer_write_position + 1 < buffer_size) {
                buffer_write_position++;
            } else {
                return 1;
            }
        }
        return 0;
    }
    
    
    int refill_buffer(MYFLT offset, bool first) {
        buffer_write_position = 0;
        if (offset > 1) offset = 1;
        long offset_position = total_locations * offset;
        if (UNLIKELY(offset_position > total_locations - (buffer_size + 1))) {
            offset_position = total_locations - (buffer_size + 1);
        }
        procmaps_iterator* maps = pmparser_parse(csound, pid);
        
        if (maps == NULL) {
            //csound->message("cannot open maps");
            return NOTOK; //csound->perf_error("cannot open maps", this->insdshead);
        }
        
        int fd_mem = open(mem_path, O_RDWR);
        if (fd_mem == -1) {
            //csound->message("cannot open memory");
            return NOTOK; 
        }
        
        long position = 0;
        unsigned long aof = 0;
        procmaps_struct* maps_tmp = NULL;
        int res = 0;
        while ((maps_tmp = pmparser_next(maps)) != NULL) {
            if (maps_tmp->is_r && maps_tmp->is_w) {
                if (position >= offset_position) {
                    aof = position - offset_position; // not quite right, length needs sorting
                    res = read_mem(fd_mem, ((unsigned long) maps_tmp->addr_start) + aof, (unsigned long) maps_tmp->length);
                }
                position += maps_tmp->length;
                if (res == 1) {
                    break;
                }
            }
        }
        pmparser_free(csound, maps);
        close(fd_mem);
        return OK;
    }
    
    int aperf() {
        if (last_offset != inargs[1]  || last_buffer_ratio != inargs[2]) {
            last_offset = inargs[1];
            last_buffer_ratio = inargs[2];
            buffer_size = (int) (inargs[2] * max_buffer_size);
            refill_buffer(inargs[1], false);
            buffer_read_position = 0;
            
        }
        for (int i = 0; i < nsmps; i++) {
            outargs(0)[i] = buffer[buffer_read_position];
            if (LIKELY(buffer_read_position + 1 < buffer_size)) {
                buffer_read_position++;
            } else {
                refill_buffer(inargs[1], false);
                buffer_read_position = 0;
            }
        }
            
        return OK;    
        
    }
    
    
};





// read memory from a given pid as audio
struct memson3 : csnd::Plugin<1, 3> {
    static constexpr char const *otypes = "a";
    static constexpr char const *itypes = "ikk";
    int pid;
    int buffer_read_position;
    int buffer_write_position;
    int buffer_size;
    int max_buffer_size;
    unsigned long total_locations;
    MYFLT* buffer;
    unsigned char* lbuffer;
    MYFLT last_buffer_ratio;
    MYFLT last_offset;
    char* mem_path;
    
    int init() {
        max_buffer_size = 441000;
        buffer_size = (int) (inargs[2] * max_buffer_size);
        last_buffer_ratio = inargs[2];
        last_offset = inargs[1];
        csound->plugin_deinit(this);
        pid = (int) inargs[0];
        buffer = (MYFLT*) csound->malloc(sizeof(MYFLT) * max_buffer_size);
        lbuffer = (unsigned char*) csound->malloc(sizeof(unsigned char) * max_buffer_size);
        mem_path = (char*) csound->malloc(sizeof(char) * 50);
        sprintf(mem_path, "/proc/%d/mem", pid);
      
        buffer_read_position = 0;
        buffer_write_position = 0;
        total_locations = max_locations();
        refill_buffer(inargs[1]);
        
        return OK;
    }
    
    int deinit() {
        csound->free(mem_path);        
        return OK;
    }
    
    unsigned long max_locations() {
        procmaps_iterator* maps = pmparser_parse(csound, pid);
        int fd_mem = open(mem_path, O_RDWR);
        unsigned long len = 0;
        procmaps_struct* maps_tmp = NULL;
        while ((maps_tmp = pmparser_next(maps)) != NULL) {
            if (maps_tmp->is_r && maps_tmp->is_w) {
                len += maps_tmp->length;
            }
        }
        pmparser_free(csound, maps);
        close(fd_mem);
        return len;
    }
    
    int read_mem(int fd_mem, unsigned long start, unsigned long length) {
        length = length; // 4; // ??????????????????????????????????????????????????????
        // lbuffer as unsigned char
        // https://unix.stackexchange.com/questions/6301/how-do-i-read-from-proc-pid-mem-under-linux
        lseek(fd_mem, start, SEEK_SET);
        read(fd_mem, lbuffer, length);

        int i;
        for (i = 0; i < length; i++) {
            //std::cout << "a " << buffer_write_position << "x " << buffer_size << "\n";
            MYFLT val = ((MYFLT)lbuffer[i]) / UCHAR_MAX;
            buffer[buffer_write_position] = val;
            if (buffer_write_position + 1 < buffer_size) {
                buffer_write_position++;
            } else {
                return 1;
            }
        }
        return 0;
    }
    
    
    int refill_buffer(MYFLT offset) {
        buffer_write_position = 0;
        if (offset > 1) offset = 1;
        long offset_position = total_locations * offset;
        if (UNLIKELY(offset_position > total_locations - (buffer_size + 1))) {
            offset_position = total_locations - (buffer_size + 1);
        }
        procmaps_iterator* maps = pmparser_parse(csound, pid);
        
        if (maps == NULL) {
            //csound->message("cannot open maps");
            return NOTOK; //csound->perf_error("cannot open maps", this->insdshead);
        }
        
        int fd_mem = open(mem_path, O_RDWR);
        if (fd_mem == -1) {
            //csound->message("cannot open memory");
            return NOTOK; 
        }
        
        long position = 0;
        unsigned long aof = 0;
        procmaps_struct* maps_tmp = NULL;
        int res = 0;
        while ((maps_tmp = pmparser_next(maps)) != NULL) {
            if (maps_tmp->is_r && maps_tmp->is_w) {
                if (position >= offset_position) {
                    aof = position - offset_position; // not quite right, length needs sorting
                    res = read_mem(fd_mem, ((unsigned long) maps_tmp->addr_start) + aof, (unsigned long) maps_tmp->length);
                }
                position += maps_tmp->length;
                if (res == 1) {
                    break;
                }
            }
        }
        pmparser_free(csound, maps);
        close(fd_mem);
        return OK;
    }
    
    int aperf() {
        if (last_offset != inargs[1]  || last_buffer_ratio != inargs[2]) {
            last_offset = inargs[1];
            last_buffer_ratio = inargs[2];
            buffer_size = (int) (inargs[2] * max_buffer_size);
            refill_buffer(inargs[1]);
            buffer_read_position = 0;
            
        }
        for (int i = 0; i < nsmps; i++) {
            outargs(0)[i] = buffer[buffer_read_position];
            if (LIKELY(buffer_read_position + 1 < buffer_size)) {
                buffer_read_position++;
            } else {
                refill_buffer(inargs[1]);
                buffer_read_position = 0;
            }
        }
            
        return OK;    
        
    }
    
    
};



#include <modload.h>

void csnd::on_load(csnd::Csound *csound) {
    csnd::plugin<memson>(csound, "memson", csnd::thread::ia);
    csnd::plugin<memson2>(csound, "memson2", csnd::thread::ia);
    csnd::plugin<memson3>(csound, "memson3", csnd::thread::ia);
    csnd::plugin<memps>(csound, "memps", csnd::thread::i);
    csnd::plugin<mempsname>(csound, "mempsname", csnd::thread::i);
    csnd::plugin<mem2tab>(csound, "mem2tab", csnd::thread::i);
    

#ifdef USE_X11
	csnd::plugin<winson>(csound, "winson", csnd::thread::ia);
#endif
}

