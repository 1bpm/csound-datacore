#ifdef USE_PROCMAPS
#include "pmparser.h"
#include <sys/mman.h>
#define PROC_DIR "/proc/"
#endif

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

#ifdef USE_X11
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>





#ifdef USE_PROCMAPS

struct mempsname : csnd::Plugin<1, 1> {
    static constexpr char const *otypes = "S";
    static constexpr char const *itypes = "i";
    int init() {
        int pid = inargs[0];
        char* path = (char*) csound->malloc(sizeof(char) * PATH_MAX);
        sprintf(path, "%s%d/cmdline", PROC_DIR, pid);
        
        FILE* f = fopen(path, "r");
        char* buffer = (char*) csound->malloc(sizeof(char) * 1024);
        size_t size = fread(buffer, sizeof(char), 1024, f);
        if (size > 0) {
            if ('\n' == buffer[size - 1]) {
                buffer[size - 1] = '\0';
            }
        }

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
                    if (stat(strcat(path, "/maps"), &fileInfo) == 0) {
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





class MemParser {
private:
    csnd::Csound* csound;
    int fd_mem;
    int pid;
    bool skip_zero;
    int buffer_write_position;
    
    // func arguments: index and value ; return false to stop iteration, true to continue
    void iterate_memory_step(std::function<bool (long, MYFLT)> func) {
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
            //csound->free(lbuffer); // causes segfault, WHY???????????
            //lbuffer = NULL;
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
        if (fd_mem) {
            close(fd_mem);
        }
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


struct mempssize : csnd::Plugin<1, 2> {
    static constexpr char const *otypes = "i";
    static constexpr char const *itypes = "io";

    int init() {
        MemParser* mp = new MemParser(csound, (int)inargs[0], (inargs[1] > 0));

        long length = mp->get_size();
        outargs[0] = FL(length);
        return OK;
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
// aout memson ipid, koffset, kbuffermultiplier, ibuffersize=441000, iskipzero=0
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
        MYFLT buffer_multiplier = inargs[2];
        if (UNLIKELY(buffer_multiplier > 1)) {
            buffer_multiplier = 1;
        } else if (UNLIKELY(buffer_multiplier < 0)) {
            buffer_multiplier = 0;
        }
        
        for (int i = 0; i < nsmps; i++) {
            outargs(0)[i] = mp->buffer[buffer_read_position];
            if (LIKELY(buffer_read_position < mp->buffer_size*buffer_multiplier)) {
                buffer_read_position++;
            } else {
                buffer_read_position = 0;
            }
        }
        if (inargs[1] != last_offset) {
            last_offset = inargs[1];
            if (UNLIKELY(last_offset > 1)) {
                last_offset = 1;
            } else if (UNLIKELY(last_offset < 0)) {
                last_offset = 0;
            }
            mp->fill_buffer(last_offset);
        }
            
        return OK;    
        
    }
    
};

#endif
// end with procmaps


// ifn rawreadtable Sfile, ichannels, istart, iskip
struct rawreadtable : csnd::Plugin<1, 4> {
    static constexpr char const *otypes = "i";
    static constexpr char const *itypes = "Sjjj";
    
    int init() {
        FILE* infile;
        STRINGDAT &path = inargs.str_data(0);
        FUNC *table;
        int channels = (inargs[1] == 2) ? 2 : 1;            
        int start = (inargs[2] < 0) ? 0 : (int) inargs[2];
        int skip = (inargs[3] < 1) ? 1 : (int) inargs[3];
        
        int buffsize = 16384;
        
        char* buffer = (char*) csound->malloc(sizeof(char) * buffsize);
        size_t buffused;
        int readpos = 0;
        long int writepos = 0;
        
        infile = fopen(path.data, "rb");
        fseek(infile, 0L, SEEK_END);
        int length = ftell(infile) / skip;
        fseek(infile, start, SEEK_SET);
        
        if ((maketable(csound, (int)length, &table, channels)) != OK) {
            return csound->init_error("Cannot create ftable");
        
        }
        buffused = fread(buffer, sizeof(char), buffsize, infile);
        
        while (buffused != 0) {
            while (readpos < buffused) {
                table->ftable[writepos] = ((MYFLT) buffer[readpos] / CHAR_MAX);
                writepos ++;
                readpos += skip;
            }
            readpos = 0;
            buffused = fread(buffer, sizeof(char), buffsize, infile);

        }
        csound->free(buffer);
        outargs[0] = table->fno;
        return OK;
    }
};

// aout rawread Sfile, [iloop = 0]
struct rawread : csnd::Plugin<1, 2> {
    static constexpr char const *otypes = "a";
    static constexpr char const *itypes = "So";
    
    FILE* infile;
    char* buffer;
    int buffsize;
    size_t buffused;
    int readpos;
    bool doloop;
    bool dooutput;
    
    int init() {
        buffsize = 16384;
        readpos = 0;
        doloop = (inargs[1] >= 1) ? true : false;
        buffer = (char*) csound->malloc(sizeof(char) * buffsize);
        STRINGDAT &path = inargs.str_data(0);
        infile = fopen(path.data, "rb");
        if (infile == NULL) {
            return csound->init_error("Cannot open file");
        }
        dooutput = true;
        fillbuffer();
        csound->plugin_deinit(this);
        return OK;
    }
    
    int deinit() {
        if (infile != NULL) {
            fclose(infile);
        }
        return OK;
    }
    
    void fillbuffer() {
        buffused = fread(buffer, sizeof(char), buffsize, infile);
    }
    
    int aperf() {
        if (!dooutput) {
            return OK;
        }
        for (int i = 0; i < nsmps; i++) {
            outargs(0)[i] = ((MYFLT) buffer[readpos] / CHAR_MAX);
            if (readpos + 1 < buffused) {
                readpos ++;
            } else {
                readpos = 0;
                fillbuffer();
                if (buffused == 0) {
                    if (doloop) {
                        fseek(infile, 0, SEEK_SET);
                        fillbuffer();
                    } else {
                        dooutput = false;
                    }
                }
            }
        }
        return OK;
    }
};

// aL, aR rawread Sfile, [iloop = 0]
struct rawreadstereo : csnd::Plugin<2, 2> {
    static constexpr char const *otypes = "aa";
    static constexpr char const *itypes = "So";
    
    FILE* infile;
    char* buffer;
    int buffsize;
    size_t buffused;
    int readpos;
    bool doloop;
    bool dooutput;
    
    int init() {
        buffsize = 16384;
        readpos = 0;
        doloop = (inargs[1] >= 1) ? true : false;
        buffer = (char*) csound->malloc(sizeof(char) * buffsize);
        STRINGDAT &path = inargs.str_data(0);
        infile = fopen(path.data, "rb");
        if (infile == NULL) {
            return csound->init_error("Cannot open file");
        }
        dooutput = true;
        fillbuffer();
        csound->plugin_deinit(this);
        return OK;
    }
    
    int deinit() {
        if (infile != NULL) {
            fclose(infile);
        }
        return OK;
    }
    
    void fillbuffer() {
        buffused = fread(buffer, sizeof(char), buffsize, infile);
    }
    
    int aperf() {
        if (!dooutput) {
            return OK;
        }
        for (int i = 0; i < nsmps; i++) {
            outargs(0)[i] = ((MYFLT) buffer[readpos] / CHAR_MAX); // - 0.5 ;
            outargs(1)[i] = ((MYFLT) buffer[readpos+1] / CHAR_MAX);
            if (readpos + 2 < buffused) {
                readpos = readpos + 2;
            } else {
                readpos = 0;
                fillbuffer();
                if (buffused == 0) {
                    if (doloop) {
                        fseek(infile, 0, SEEK_SET);
                        fillbuffer();
                    } else {
                        dooutput = false;
                    }
                }
            }
        }
        return OK;
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


#include <modload.h>

void csnd::on_load(csnd::Csound *csound) {
    csnd::plugin<rawread>(csound, "rawread", csnd::thread::ia);
    csnd::plugin<rawreadstereo>(csound, "rawread.s", csnd::thread::ia);
    csnd::plugin<rawreadtable>(csound, "rawreadtable", csnd::thread::i);
    
#ifdef USE_PROCMAPS
    csnd::plugin<memson>(csound, "memson", csnd::thread::ia);
    csnd::plugin<memps>(csound, "memps", csnd::thread::i);
    csnd::plugin<mempssize>(csound, "mempssize", csnd::thread::i);
    csnd::plugin<mempsname>(csound, "mempsname", csnd::thread::i);
    csnd::plugin<mem2tab>(csound, "mem2tab", csnd::thread::i);
#endif

#ifdef USE_X11
	csnd::plugin<winson>(csound, "winson", csnd::thread::ia);
#endif
}

