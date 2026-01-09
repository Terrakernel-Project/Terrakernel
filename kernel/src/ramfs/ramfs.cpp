#include "ramfs.hpp"
#include <mem/mem.hpp>
#include <cstring>
#include <cstdio>

namespace ramfs {

static const int MAX_FDS = 256;
static const int MAX_INODES = 4096;

struct Inode {
    uint64_t ino;
    uint32_t mode;
    uint32_t nlink;
    uint64_t size;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    Inode* parent;
    Inode* first_child;
    Inode* next_sibling;
    char name[NAME_MAX + 1];
    bool in_use;
    void* data;
    uint8_t zero;
};

struct FileDescriptor {
    Inode* inode;
    uint64_t offset;
    int flags;
    bool in_use;
};

static Inode* root_inode = nullptr;
static Inode inode_table[MAX_INODES];
static FileDescriptor fd_table[MAX_FDS];
static uint64_t next_ino = 1;
static char current_dir[PATH_MAX];

static Inode* allocate_inode() {
    for (int i = 0; i < MAX_INODES; i++) {
        if (!inode_table[i].in_use) {
            mem::memset(&inode_table[i], 0, sizeof(Inode));
            inode_table[i].in_use = true;
            inode_table[i].ino = next_ino++;
            inode_table[i].nlink = 1;
            inode_table[i].zero = 0;
            return &inode_table[i];
        }
    }
    return nullptr;
}

static void free_inode(Inode* inode) {
    if (!inode) return;
    if (inode->data && (inode->mode & S_IFMT) == S_IFREG) {
        mem::heap::free(inode->data);
    }
    inode->in_use = false;
}

static int allocate_fd() {
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            mem::memset(&fd_table[i], 0, sizeof(FileDescriptor));
            fd_table[i].in_use = true;
            return i;
        }
    }
    return -1;
}

static void free_fd(int fd) {
    if (fd >= 0 && fd < MAX_FDS) {
        fd_table[fd].in_use = false;
    }
}

static void normalize_path(const char* path, char* result) {
    char temp[PATH_MAX];
    char* segments[128];
    int segment_count = 0;
    
    if (path[0] != '/') {
        strncpy(temp, current_dir, PATH_MAX);
        size_t len = strlen(temp);
        if (len > 0 && temp[len - 1] != '/') {
            temp[len] = '/';
            temp[len + 1] = '\0';
            len++;
        }
        strncpy(temp + len, path, PATH_MAX - len);
    } else {
        strncpy(temp, path, PATH_MAX);
    }
    
    char* token = temp;
    char* next = temp;
    while (*next) {
        if (*next == '/') {
            *next = '\0';
            if (token != next && strlen(token) > 0) {
                if (strcmp(token, ".") == 0) {
                } else if (strcmp(token, "..") == 0) {
                    if (segment_count > 0) segment_count--;
                } else {
                    segments[segment_count++] = token;
                }
            }
            token = next + 1;
        }
        next++;
    }
    if (token != next && strlen(token) > 0) {
        if (strcmp(token, ".") != 0) {
            if (strcmp(token, "..") == 0) {
                if (segment_count > 0) segment_count--;
            } else {
                segments[segment_count++] = token;
            }
        }
    }
    
    if (segment_count == 0) {
        result[0] = '/';
        result[1] = '\0';
    } else {
        size_t pos = 0;
        for (int i = 0; i < segment_count; i++) {
            result[pos++] = '/';
            const char* seg = segments[i];
            while (*seg) {
                result[pos++] = *seg++;
            }
        }
        result[pos] = '\0';
    }
}

static Inode* find_inode(const char* pathname) {
    char normalized[PATH_MAX];
    normalize_path(pathname, normalized);
    
    if (strcmp(normalized, "/") == 0) {
        return root_inode;
    }
    
    Inode* current = root_inode;
    char* path = normalized + 1;
    char name_buf[NAME_MAX + 1];
    
    while (*path) {
        char* end = path;
        while (*end && *end != '/') end++;
        
        size_t name_len = end - path;
        if (name_len > NAME_MAX) return nullptr;
        
        mem::memcpy(name_buf, path, name_len);
        name_buf[name_len] = '\0';
        
        Inode* child = current->first_child;
        bool found = false;
        while (child) {
            if (strcmp(child->name, name_buf) == 0) {
                current = child;
                found = true;
                break;
            }
            child = child->next_sibling;
        }
        
        if (!found) return nullptr;
        
        if (*end == '/') path = end + 1;
        else path = end;
    }
    
    return current;
}

static Inode* find_parent_and_name(const char* pathname, char* name_out) {
    char normalized[PATH_MAX];
    normalize_path(pathname, normalized);
    
    if (strcmp(normalized, "/") == 0) {
        return nullptr;
    }
    
    char* last_slash = nullptr;
    char* ptr = normalized;
    while (*ptr) {
        if (*ptr == '/') last_slash = ptr;
        ptr++;
    }
    
    if (last_slash == normalized) {
        strncpy(name_out, normalized + 1, NAME_MAX + 1);
        return root_inode;
    }
    
    *last_slash = '\0';
    strncpy(name_out, last_slash + 1, NAME_MAX + 1);
    
    return find_inode(normalized);
}

static void add_child(Inode* parent, Inode* child) {
    child->parent = parent;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
}

static void remove_child(Inode* parent, Inode* child) {
    if (!parent || !child) return;
    
    Inode** current = &parent->first_child;
    while (*current) {
        if (*current == child) {
            *current = child->next_sibling;
            child->next_sibling = nullptr;
            child->parent = nullptr;
            return;
        }
        current = &(*current)->next_sibling;
    }
}

void initialise() {
    mem::memset(inode_table, 0, sizeof(inode_table));
    mem::memset(fd_table, 0, sizeof(fd_table));
    
    root_inode = allocate_inode();
    root_inode->mode = S_IFDIR | 0755;
    root_inode->name[0] = '/';
    root_inode->name[1] = '\0';
    
    strncpy(current_dir, "/", PATH_MAX);
}

int open(const char* pathname, int flags, uint32_t mode) {
    Inode* inode = find_inode(pathname);
    
    if (!inode) {
        if (!(flags & O_CREAT)) {
            return -1;
        }
        
        char name[NAME_MAX + 1];
        Inode* parent = find_parent_and_name(pathname, name);
        if (!parent || (parent->mode & S_IFMT) != S_IFDIR) {
            return -1;
        }
        
        inode = allocate_inode();
        if (!inode) return -1;
        
        inode->mode = S_IFREG | (mode & 0777);
        strncpy(inode->name, name, NAME_MAX + 1);
        add_child(parent, inode);
    } else {
        if ((flags & O_CREAT) && (flags & O_EXCL)) {
            return -1;
        }
        
        if ((inode->mode & S_IFMT) == S_IFDIR && !(flags & O_DIRECTORY)) {
            return -1;
        }
        
        if (flags & O_TRUNC && (inode->mode & S_IFMT) == S_IFREG) {
            if (inode->data) {
                mem::heap::free(inode->data);
                inode->data = nullptr;
            }
            inode->size = 0;
        }
    }
    
    int fd = allocate_fd();
    if (fd < 0) {
        if (inode->nlink == 0) free_inode(inode);
        return -1;
    }
    
    fd_table[fd].inode = inode;
    fd_table[fd].offset = (flags & O_APPEND) ? inode->size : 0;
    fd_table[fd].flags = flags;
    
    return fd;
}

int close(int fd) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -1;
    }
    
    free_fd(fd);
    return 0;
}

int64_t read(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -1;
    }
    
    FileDescriptor* file = &fd_table[fd];
    Inode* inode = file->inode;

    if ((file->flags & O_WRONLY) == O_WRONLY) {
        return -1;
    }
    
    if ((inode->mode & S_IFMT) != S_IFREG) {
        return -1;
    }
    
    if (file->offset >= inode->size) {
        return 0;
    }
    
    size_t to_read = count;
    if (file->offset + to_read > inode->size) {
        to_read = inode->size - file->offset;
    }
    
    if (inode->data) {
        mem::memcpy(buf, (char*)inode->data + file->offset, to_read);
    }
    
    file->offset += to_read;
    return to_read;
}

int64_t write(int fd, const void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -1;
    }
    
    FileDescriptor* file = &fd_table[fd];
    Inode* inode = file->inode;
    
    if ((file->flags & O_WRONLY) != O_WRONLY && (file->flags & O_RDWR) != O_RDWR) {
        return -1;
    }
    
    if ((inode->mode & S_IFMT) != S_IFREG) {
        return -1;
    }
    
    if (file->flags & O_APPEND) {
        file->offset = inode->size;
    }
    
    uint64_t new_size = file->offset + count;
    if (new_size > inode->size) {
        void* new_data = mem::heap::realloc(inode->data, new_size);
        if (!new_data) {
            return -1;
        }
        inode->data = new_data;
        if (new_size > inode->size) {
            mem::memset((char*)inode->data + inode->size, 0, new_size - inode->size);
        }
        inode->size = new_size;
    }
    
    mem::memcpy((char*)inode->data + file->offset, buf, count);
    file->offset += count;

    return count;
}

int64_t lseek(int fd, int64_t offset, int whence) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -1;
    }
    
    FileDescriptor* file = &fd_table[fd];
    Inode* inode = file->inode;
    
    int64_t new_offset;
    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = file->offset + offset;
            break;
        case SEEK_END:
            new_offset = inode->size + offset;
            break;
        default:
            return -1;
    }
    
    if (new_offset < 0) {
        return -1;
    }
    
    file->offset = new_offset;
    return new_offset;
}

int stat(const char* pathname, struct stat* statbuf) {
    Inode* inode = find_inode(pathname);
    if (!inode) {
        return -1;
    }
    
    mem::memset(statbuf, 0, sizeof(struct stat));
    statbuf->st_ino = inode->ino;
    statbuf->st_mode = inode->mode;
    statbuf->st_nlink = inode->nlink;
    statbuf->st_size = inode->size;
    statbuf->st_atime = inode->atime;
    statbuf->st_mtime = inode->mtime;
    statbuf->st_ctime = inode->ctime;
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = (inode->size + 511) / 512;
    
    return 0;
}

int fstat(int fd, struct stat* statbuf) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -1;
    }
    
    Inode* inode = fd_table[fd].inode;
    mem::memset(statbuf, 0, sizeof(struct stat));
    statbuf->st_ino = inode->ino;
    statbuf->st_mode = inode->mode;
    statbuf->st_nlink = inode->nlink;
    statbuf->st_size = inode->size;
    statbuf->st_atime = inode->atime;
    statbuf->st_mtime = inode->mtime;
    statbuf->st_ctime = inode->ctime;
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = (inode->size + 511) / 512;
    
    return 0;
}

int mkdir(const char* pathname, uint32_t mode) {
    char name[NAME_MAX + 1];
    Inode* parent = find_parent_and_name(pathname, name);
    
    if (!parent || (parent->mode & S_IFMT) != S_IFDIR) {
        return -1;
    }
    
    Inode* child = parent->first_child;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return -1;
        }
        child = child->next_sibling;
    }
    
    Inode* new_dir = allocate_inode();
    if (!new_dir) return -1;
    
    new_dir->mode = S_IFDIR | (mode & 0777);
    strncpy(new_dir->name, name, NAME_MAX + 1);
    add_child(parent, new_dir);
    
    return 0;
}

int rmdir(const char* pathname) {
    Inode* inode = find_inode(pathname);
    if (!inode || (inode->mode & S_IFMT) != S_IFDIR) {
        return -1;
    }
    
    if (inode == root_inode) {
        return -1;
    }
    
    if (inode->first_child) {
        return -1;
    }
    
    remove_child(inode->parent, inode);
    free_inode(inode);
    
    return 0;
}

int unlink(const char* pathname) {
    Inode* inode = find_inode(pathname);
    if (!inode || (inode->mode & S_IFMT) != S_IFREG) {
        return -1;
    }
    
    remove_child(inode->parent, inode);
    inode->nlink--;
    
    if (inode->nlink == 0) {
        free_inode(inode);
    }
    
    return 0;
}

int link(const char* oldpath, const char* newpath) {
    Inode* old_inode = find_inode(oldpath);
    if (!old_inode || (old_inode->mode & S_IFMT) != S_IFREG) {
        return -1;
    }
    
    char name[NAME_MAX + 1];
    Inode* parent = find_parent_and_name(newpath, name);
    
    if (!parent || (parent->mode & S_IFMT) != S_IFDIR) {
        return -1;
    }
    
    Inode* new_inode = allocate_inode();
    if (!new_inode) return -1;
    
    new_inode->mode = old_inode->mode;
    new_inode->size = old_inode->size;
    new_inode->data = old_inode->data;
    strncpy(new_inode->name, name, NAME_MAX + 1);
    add_child(parent, new_inode);
    
    old_inode->nlink++;
    
    return 0;
}

int rename(const char* oldpath, const char* newpath) {
    Inode* old_inode = find_inode(oldpath);
    if (!old_inode) {
        return -1;
    }
    
    char new_name[NAME_MAX + 1];
    Inode* new_parent = find_parent_and_name(newpath, new_name);
    
    if (!new_parent || (new_parent->mode & S_IFMT) != S_IFDIR) {
        return -1;
    }
    
    Inode* existing = find_inode(newpath);
    if (existing) {
        if ((existing->mode & S_IFMT) == S_IFDIR) {
            if (existing->first_child) return -1;
        }
        remove_child(existing->parent, existing);
        free_inode(existing);
    }
    
    remove_child(old_inode->parent, old_inode);
    strncpy(old_inode->name, new_name, NAME_MAX + 1);
    add_child(new_parent, old_inode);
    
    return 0;
}

DIR* opendir(const char* name) {
    Inode* inode = find_inode(name);
    if (!inode || (inode->mode & S_IFMT) != S_IFDIR) {
        return nullptr;
    }
    
    DIR* dir = (DIR*)mem::heap::malloc(sizeof(DIR));
    if (!dir) return nullptr;
    
    dir->internal = inode->first_child;
    dir->pos = 0;
    
    return dir;
}

struct dirent* readdir(DIR* dirp) {
    if (!dirp) return nullptr;
    
    Inode* current = (Inode*)dirp->internal;
    if (!current) return nullptr;
    
    static struct dirent entry;
    mem::memset(&entry, 0, sizeof(entry));
    
    entry.d_ino = current->ino;
    entry.d_off = dirp->pos++;
    entry.d_reclen = sizeof(entry);
    entry.d_type = ((current->mode & S_IFMT) == S_IFDIR) ? DT_DIR : DT_REG;
    strncpy(entry.d_name, current->name, NAME_MAX + 1);
    
    dirp->internal = current->next_sibling;
    
    return &entry;
}

int closedir(DIR* dirp) {
    if (!dirp) return -1;
    mem::heap::free(dirp);
    return 0;
}

int chdir(const char* path) {
    Inode* inode = find_inode(path);
    if (!inode || (inode->mode & S_IFMT) != S_IFDIR) {
        return -1;
    }
    
    normalize_path(path, current_dir);
    return 0;
}

char* getcwd(char* buf, size_t size) {
    if (!buf || size == 0) return nullptr;
    
    size_t len = strlen(current_dir);
    if (len >= size) return nullptr;
    
    strncpy(buf, current_dir, size);
    return buf;
}

int truncate(const char* path, uint64_t length) {
    Inode* inode = find_inode(path);
    if (!inode || (inode->mode & S_IFMT) != S_IFREG) {
        return -1;
    }
    
    if (length == 0) {
        if (inode->data) {
            mem::heap::free(inode->data);
            inode->data = nullptr;
        }
        inode->size = 0;
    } else if (length != inode->size) {
        void* new_data = mem::heap::realloc(inode->data, length);
        if (!new_data) return -1;
        
        if (length > inode->size) {
            mem::memset((char*)new_data + inode->size, 0, length - inode->size);
        }
        
        inode->data = new_data;
        inode->size = length;
    }
    
    return 0;
}

int ftruncate(int fd, uint64_t length) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -1;
    }
    
    Inode* inode = fd_table[fd].inode;
    if ((inode->mode & S_IFMT) != S_IFREG) {
        return -1;
    }
    
    if (length == 0) {
        if (inode->data) {
            mem::heap::free(inode->data);
            inode->data = nullptr;
        }
        inode->size = 0;
    } else if (length != inode->size) {
        void* new_data = mem::heap::realloc(inode->data, length);
        if (!new_data) return -1;
        
        if (length > inode->size) {
            mem::memset((char*)new_data + inode->size, 0, length - inode->size);
        }
        
        inode->data = new_data;
        inode->size = length;
    }
    
    return 0;
}

int access(const char* pathname, int mode) {
    Inode* inode = find_inode(pathname);
    if (!inode) {
        return -1;
    }
    return 0;
}

int chmod(const char* pathname, uint32_t mode) {
    Inode* inode = find_inode(pathname);
    if (!inode) {
        return -1;
    }
    
    inode->mode = (inode->mode & S_IFMT) | (mode & 0777);
    return 0;
}

int dup(int oldfd) {
    if (oldfd < 0 || oldfd >= MAX_FDS || !fd_table[oldfd].in_use) {
        return -1;
    }
    
    int newfd = allocate_fd();
    if (newfd < 0) return -1;
    
    fd_table[newfd].inode = fd_table[oldfd].inode;
    fd_table[newfd].offset = fd_table[oldfd].offset;
    fd_table[newfd].flags = fd_table[oldfd].flags;
    
    return newfd;
}

int dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= MAX_FDS || !fd_table[oldfd].in_use) {
        return -1;
    }
    
    if (newfd < 0 || newfd >= MAX_FDS) {
        return -1;
    }
    
    if (oldfd == newfd) {
        return newfd;
    }
    
    if (fd_table[newfd].in_use) {
        close(newfd);
    }
    
    fd_table[newfd].in_use = true;
    fd_table[newfd].inode = fd_table[oldfd].inode;
    fd_table[newfd].offset = fd_table[oldfd].offset;
    fd_table[newfd].flags = fd_table[oldfd].flags;
    
    return newfd;
}

struct USTARHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};

static uint64_t parse_octal(const char* str, size_t len) {
    uint64_t result = 0;
    for (size_t i = 0; i < len && str[i] >= '0' && str[i] <= '7'; i++) {
        result = result * 8 + (str[i] - '0');
    }
    return result;
}

static bool verify_ustar_checksum(const USTARHeader* header) {
    uint64_t stored_checksum = parse_octal(header->checksum, 8);
    uint64_t calculated_checksum = 0;
    
    const unsigned char* bytes = (const unsigned char*)header;
    for (size_t i = 0; i < 512; i++) {
        if (i >= 148 && i < 156) {
            calculated_checksum += ' ';
        } else {
            calculated_checksum += bytes[i];
        }
    }
    
    return stored_checksum == calculated_checksum;
}

static void create_parent_directories(const char* path) {
    char temp_path[PATH_MAX];
    strncpy(temp_path, path, PATH_MAX);
    
    char* ptr = temp_path;
    if (*ptr == '/') ptr++;
    
    while (*ptr) {
        if (*ptr == '/') {
            *ptr = '\0';
            
            Inode* existing = find_inode(temp_path);
            if (!existing) {
                mkdir(temp_path, 0755);
            }
            
            *ptr = '/';
        }
        ptr++;
    }
}

static int load_ustar_archive(void* base, size_t size, const char* path_prefix) {
    unsigned char* data = (unsigned char*)base;
    size_t offset = 0;
    int files_loaded = 0;
    
    size_t prefix_len = strlen(path_prefix);
    bool needs_slash = prefix_len > 0 && path_prefix[prefix_len - 1] != '/';
    
    while (offset + 512 <= size) {
        USTARHeader* header = (USTARHeader*)(data + offset);
        
        if (header->name[0] == '\0') {
            break;
        }
        
        if (strcmp(header->magic, "ustar") != 0 && 
            mem::memcmp(header->magic, "ustar", 5) != 0) {
            break;
        }
        
        if (!verify_ustar_checksum(header)) {
            offset += 512;
            continue;
        }
        
        char full_name[PATH_MAX];
        full_name[0] = '\0';
        
        if (header->prefix[0] != '\0') {
            size_t i = 0;
            while (i < 155 && header->prefix[i]) {
                full_name[i] = header->prefix[i];
                i++;
            }
            if (i > 0 && full_name[i - 1] != '/') {
                full_name[i++] = '/';
            }
            full_name[i] = '\0';
        }
        
        size_t full_name_len = strlen(full_name);
        size_t name_len = 0;
        while (name_len < 100 && header->name[name_len]) {
            full_name[full_name_len++] = header->name[name_len++];
        }
        full_name[full_name_len] = '\0';
        
        char final_path[PATH_MAX];
        size_t final_pos = 0;
        
        if (prefix_len > 0) {
            strncpy(final_path, path_prefix, PATH_MAX);
            final_pos = prefix_len;
            
            if (needs_slash && full_name[0] != '/') {
                final_path[final_pos++] = '/';
            }
        }
        
        if (full_name[0] == '/') {
            strncpy(final_path + final_pos, full_name + 1, PATH_MAX - final_pos);
        } else {
            strncpy(final_path + final_pos, full_name, PATH_MAX - final_pos);
        }
        
        uint64_t file_size = parse_octal(header->size, 12);
        uint32_t mode = parse_octal(header->mode, 8);
        
        if (header->typeflag == '5' || 
            (full_name_len > 0 && full_name[full_name_len - 1] == '/')) {
            offset += 512;
            
            if (final_path[0] != '\0') {
                create_parent_directories(final_path);
                
                Inode* existing = find_inode(final_path);
                if (!existing) {
                    mkdir(final_path, mode ? mode : 0755);
                }
                files_loaded++;
            }
            
            offset += 512;
        } else if (header->typeflag == '0' || header->typeflag == '\0') {
            offset += 512;
            
            create_parent_directories(final_path);
            
            int fd = open(final_path, O_CREAT | O_WRONLY | O_TRUNC, mode ? mode : 0644);
            if (fd >= 0) {
                if (file_size > 0 && offset + file_size <= size) {
                    write(fd, data + offset, file_size);
                }
                lseek(fd, 0, SEEK_SET);
                close(fd);
                files_loaded++;
            }
            
            uint64_t aligned_size = (file_size + 511) & ~511ULL;
            offset += aligned_size;
        } else if (header->typeflag == '1') {
            offset += 512;
            
            create_parent_directories(final_path);
            
            char link_target[PATH_MAX];
            size_t link_len = 0;
            while (link_len < 100 && header->linkname[link_len]) {
                link_target[link_len] = header->linkname[link_len];
                link_len++;
            }
            link_target[link_len] = '\0';
            
            char target_path[PATH_MAX];
            size_t target_pos = 0;
            
            if (prefix_len > 0) {
                strncpy(target_path, path_prefix, PATH_MAX);
                target_pos = prefix_len;
                
                if (needs_slash && link_target[0] != '/') {
                    target_path[target_pos++] = '/';
                }
            }
            
            strncpy(target_path + target_pos, link_target, PATH_MAX - target_pos);
            
            link(target_path, final_path);
            files_loaded++;
        } else if (header->typeflag == '2') {
            offset += (file_size + 511) & ~511ULL;
        } else {
            uint64_t aligned_size = (file_size + 511) & ~511ULL;
            offset += aligned_size;
        }
    }
    
    return files_loaded;
}

int load_archive(const char* type, void* base, size_t size, const char* path_prefix) {
    if (!base || size == 0) {
        return -1;
    }
    
    if (!path_prefix) {
        path_prefix = "/";
    }
    
    if (strcmp(path_prefix, "/") != 0) {
        Inode* prefix_inode = find_inode(path_prefix);
        if (!prefix_inode) {
            char temp_path[PATH_MAX];
            strncpy(temp_path, path_prefix, PATH_MAX);
            create_parent_directories(temp_path);
            
            mkdir(path_prefix, 0755);
        }
    }
    
    if (strcmp(type, "USTAR") == 0 || strcmp(type, "ustar") == 0 ||
        strcmp(type, "TAR") == 0 || strcmp(type, "tar") == 0) {
        return load_ustar_archive(base, size, path_prefix);
    }
    
    return -1;
}

}
