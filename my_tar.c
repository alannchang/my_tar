#include "my_tar.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#define ERR_NO_ARGUMENTS "my_tar: invalid number of arguments\n"
#define ERR_CANNOT_OPEN "my_tar: can't open it. sorry\n"
#define ERR_INVALID_OPT "my_tar: invalid number of options\n"
#define ERR_INVALID_FILE "my_tar: invalid file to be written\n"
int calculate_checksum(tar_header* header) {
    
    int sum = 0;
    // cast unsigned char to treat memory pointed by header as a sequence of bytes
    const unsigned char* bytes = (const unsigned char*)header;

    // Calculate the sum of all bytes in the header (excluding the checksum field)
    for (unsigned int i = 0; i < sizeof(tar_header); i++) sum += bytes[i];

    // Calculate the octal representation of the sum
    char sum_octal[8];
    snprintf(sum_octal, sizeof(sum_octal), "%06o", sum);

    // Add the octal representation to the checksum field
    strncpy(header->chksum, sum_octal, sizeof(header->chksum) - 2);

    // tar specifications require a null character followed by a space for the last two characters
    header->chksum[sizeof(header->chksum) - 2] = 0;
    header->chksum[sizeof(header->chksum) - 1] = ' ';

    return 0;
}

int write_end_archive(int archive) {

    // set file descriptor to the end of the tar archive
    lseek(archive, 0, SEEK_END);

    // add two blocks of zero bytes to mark the end of the archive
    int two_blocks = BLOCKSIZE * 2;
    char end_archive[two_blocks];
    memset(end_archive, 0, two_blocks);
    write(archive, end_archive, two_blocks);
    
    return 0;
}

int write_header(int archive, char* arg, int* file_size) {

    // initialize stat struct where most of the information for tar header can be found
    struct stat file_stat;

    // load file_stat struct with file info using stat() 
    if (stat(arg, &file_stat) == -1) {
        fprintf(stderr, "my_tar: %s: Cannot stat: ", arg);
        perror(NULL);
        return -1;
    }
    
    // initialize tar header struct
    tar_header header;

    // add file name to header.name
    int name_len = strlen(arg);
    strncpy(header.name, arg, name_len);

    // set rest of header.name to zero bytes
    memset(header.name + name_len, 0, 100 - name_len);

    // add other file info the header struct
    snprintf(header.mode, sizeof(header.mode), "%07o", file_stat.st_mode & 0777);
    snprintf(header.uid, sizeof(header.uid), "%07o", file_stat.st_uid);
    snprintf(header.gid, sizeof(header.gid), "%07o", file_stat.st_gid);
    snprintf(header.size, sizeof(header.size), "%011llo", (unsigned long long)file_stat.st_size);
    snprintf(header.mtime, sizeof(header.mtime), "%011llo", (unsigned long long)file_stat.st_mtime);
    
    // add "filler" used in checksum calculation
    memset(header.chksum, ' ', sizeof(header.chksum));

    // I'm assuming we're onlying working with regular files, directories, and symbolic links
    if (S_ISREG(file_stat.st_mode)) header.typeflag = '0';
    else if (S_ISLNK(file_stat.st_mode)) header.typeflag = '2';
    else if (S_ISDIR(file_stat.st_mode)) header.typeflag = '5';
    
    // code this later
    memset(header.linkname, 0, sizeof(header.linkname));
    
    // UStar indicator "ustar" then NUL
    strncpy(header.magic, "ustar", sizeof(header.magic));
    // two spaces for version
    memset(header.version, ' ', sizeof(header.version));

    // Get user and group names from uid and gid
    struct passwd *pwd = getpwuid(file_stat.st_uid);
    struct group *grp = getgrgid(file_stat.st_gid);
    if (pwd != NULL) {
        strncpy(header.uname, pwd->pw_name, sizeof(header.uname));
    } else {
        memset(header.uname, 0, sizeof(header.uname));
    }
    if (grp != NULL) {
        strncpy(header.gname, grp->gr_name, sizeof(header.gname));
    } else {
        memset(header.gname, 0, sizeof(header.gname));
    }
    
    // zero bytes for the remaining fields
    memset(header.devmajor, 0, sizeof(header.devmajor));
    memset(header.devminor, 0, sizeof(header.devminor));
    memset(header.prefix, 0, sizeof(header.prefix));
    memset(header.pad, 0, sizeof(header.pad));

    // calculate and then populate checksum field now that all the fields have been entered 
    calculate_checksum(&header);

    // finally, write header data to tar archive file
    write(archive, &header, sizeof(header));

    // update file size for use in padding calculation
    *file_size = (int) file_stat.st_size;

    return 0;
}

int write_content(int archive, char* arg) {

    // open file to be added to tar archive
    int input_fd = open(arg, O_RDONLY);  // O_RDONLY allows for reading only
    if (input_fd != -1) {
        
        // write file content in block size increments until it runs out of content
        char buffer[BLOCKSIZE];
        int bytes_read;
        while ((bytes_read = read(input_fd, buffer, sizeof(buffer))) > 0) {
            write(archive, buffer, bytes_read);
        }
        close(input_fd);
    }

    return 0;
}

int write_files(int archive, int argc, char* argv[]) {

    // iterate thru files to be written to tar archive
    for (int i = 3; i < argc; i++) {
        
        // store file size for later use
        int file_size;

        // write tar header
        write_header(archive, argv[i], &file_size);
        
        // add file content after writing header data
        write_content(archive, argv[i]);

        // add padding based off file size
        int padding_size = (BLOCKSIZE - (file_size % BLOCKSIZE)) % BLOCKSIZE;
        char padding[padding_size];
        memset(padding, 0, padding_size);
        write(archive, padding, padding_size);
    }

    return 0;
}

int list_files(int archive) {
    
    // initialize tar header struct
    tar_header header;
    // Read each tar header in archive 
    while(read(archive, &header, sizeof(header)) == BLOCKSIZE) {

        // if first and second character in 'block' is a zero byte, we've most likely reached the end-of-archive
        if (header.name[0] == '\0') break;
        // print file name
        printf("%s\n", header.name);
        // convert file size field (string) from tar header to long integer
        long file_size = strtol(header.size, NULL, 8);

        // calculate number of blocks to skip
        // -1 added to ensure if file size is a multiple of BLOCKSIZE, an extra block is not added
        int blocks_to_skip = (file_size + BLOCKSIZE - 1)/BLOCKSIZE;
        
        // move file descriptor to end of file content
        lseek(archive, blocks_to_skip * BLOCKSIZE, SEEK_CUR);
    }
    
    return 0;
}

int update_files(int archive, int argc, char* argv[]) {

    // array of strings to hold files to be updated/appended to tar archive
    char* update_list[argc + 1];
    int j = 3;

    // iterate thru files to be updated
    for (int i = 3; i < argc; i++) {

        // initialize tar header struct
        tar_header header;
        
        // variables to keep track of modification times
        long file_mtime;
        long most_recent_mtime;

        // Read each tar header in archive 
        while(read(archive, &header, sizeof(header)) == BLOCKSIZE) {

            // if first and second character in 'block' is a zero byte, we've most likely reached the end-of-archive
            if (header.name[0] == '\0') break;

            // if file name matches
            if (strcmp(header.name, argv[i]) == 0) {
                printf("There's a match!\n");
                // initialize stat struct
                struct stat file_stat;

                // load file_stat struct with file info using stat() 
                if (stat(argv[i], &file_stat) == -1) {
                    fprintf(stderr, "my_tar: %s: Cannot stat: ", argv[i]);
                    perror(NULL);
                    return -1;
                }

                // store modification time for file to be updated
                file_mtime = file_stat.st_mtime;                
                
                // convert modification time from tar header from octal string to long
                long entry_mtime = strtol(header.mtime, NULL, 8);

                // store the most recent modification time for every file with the same name
                if (entry_mtime > most_recent_mtime) most_recent_mtime = entry_mtime;   
            }
            
            // convert file size field (string) from tar header to long integer
            long file_size = strtol(header.size, NULL, 8);

            // calculate number of blocks to skip
            // -1 added to ensure if file size is a multiple of BLOCKSIZE, an extra block is not added
            int blocks_to_skip = (file_size + BLOCKSIZE - 1)/BLOCKSIZE;
            
            // move file descriptor to end of file content
            lseek(archive, blocks_to_skip * BLOCKSIZE, SEEK_CUR);
        }

        if (file_mtime > most_recent_mtime) {
            update_list[j] = argv[i];
            printf("%s is getting added!\n", update_list[j]);
            j++;
        }
        
    }

    update_list[j] = NULL;
    for (int k = 3; k < j; k++) printf("%s\n", update_list[k]);
    printf("j = %d\n", j);

    // set file descriptor to right before last two blocks of zero bytes
    if (lseek(archive, -1024, SEEK_END) == -1) perror("lseek failed");
    
    write_files(archive, j, update_list);
    // write end-of-archive (two blocks of zero bytes) to archive
    write_end_archive(archive);
    return 0;
}


int extract_files(int archive) {

    // initialize tar header struct
    tar_header header;
    // Read each tar header in archive 
    while(read(archive, &header, sizeof(header)) == BLOCKSIZE) {

        // if first and second character in 'block' is a zero byte, we've most likely reached the end-of-archive
        if (header.name[0] == '\0' && header.name[1] == '\0') break;

        // if regular file
        if (header.typeflag == '0'){
            
            // convert file size field (string) from tar header to long integer
            long file_size = strtol(header.size, NULL, 8);
            
            // calculate how much padding is added after file content
            int padding = (512 - (file_size % 512)) % 512;

            // allocate memory for file content based off file size
            char *buffer = (char *)malloc(file_size);

            // read file content to buffer
            read(archive, buffer, file_size);
            
            // create new file with name specified in tar header or overwrite existing file with same name
            int dest_fd = open(header.name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            
            // write file content to new or existing file
            write(dest_fd, buffer, file_size);
            
            close(dest_fd);
            free(buffer);

            // move file descriptor to next header
            lseek(archive, padding, SEEK_CUR);
        } 

        // if symbolic link or directory
        else if (header.typeflag == '2') symlink(header.linkname, header.name);
        else if (header.typeflag == '5') mkdir(header.name, 0755);
        
    }

    return 0;
}


int main(int argc, char *argv[]) {

        // if no arguments, throw error
        if (argc < 2){
            write(2, ERR_NO_ARGUMENTS, strlen(ERR_NO_ARGUMENTS));
            return -1;
        }
        
        // otherwise, if arguments present
        else 
        {
            // first, define the different options
            bool c_opt = false; // Create a new archive containing the specified items.
            bool f_opt = false; // used to specify filename of the archive. 
            bool r_opt = false; // Like -c, but new entries are appended to the archive. The -f option is required.
            bool t_opt = false; // List archive contents to stdout.
            bool u_opt = false; // Like -r, but new entries are added only if they have a modification date newer than the corresponding entry in the archive. The -f option is required.
            bool x_opt = false; // Extract to disk from the archive. If a file with the same name appears more than once in the archive, each copy will be extracted, with later copies overwriting (replacing) earlier copies.
            // iterate thru first argument (options)
            int i = 0;
            while (argv[1][i] != '\0')
            {
                // set to true if any options found
                switch(argv[1][i]) 
                {
                    case 'f': f_opt = true; break; 
                    case 'c': c_opt = true; break; 
                    case 'r': r_opt = true; break; 
                    case 't': t_opt = true; break; 
                    case 'u': u_opt = true; break; 
                    case 'x': x_opt = true; break; 
                }
                i++;
            }

            // only one option should be used in arguments excluding -f
            if (c_opt + r_opt + t_opt + u_opt + x_opt != 1) {
                write(2, ERR_INVALID_OPT, strlen(ERR_INVALID_OPT));
                return -1;
            }

            // if -cf, create new tar archive
            if (c_opt && f_opt)
            {
                // O_CREAT - create file if doesn't already exist
                // O_WRONLY - allows for writing only
                // O_TRUNC - overwrite the file if file already exists by truncating file to zero length
                int new_tar = open(argv[2], O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (new_tar == -1) {
                    write(2, ERR_CANNOT_OPEN, strlen(ERR_CANNOT_OPEN));
                    return -1;
                }
                // write files to archive
                if (write_files(new_tar, argc, argv) == -1) return -1;
                // write end-of-archive (two blocks of zero bytes) to archive
                write_end_archive(new_tar);

                close(new_tar);

            // otherwise, open tar archive
            } else {

                // open existing tar archive specified in arguments
                int existing_tar = open(argv[2], O_CREAT | O_RDWR, 0644); // O_RDWR allows for reading and writing
                // -r append to tar archive
                if (r_opt && f_opt) {
                    // set file descriptor to right before last two blocks of zero bytes
                    lseek(existing_tar, -1024, SEEK_END);
                    // add whatever files are in the arguments
                    write_files(existing_tar, argc, argv);
                    // write end-of-archive (two blocks of zero bytes) to archive
                    write_end_archive(existing_tar);
                }
                
                // -t list archive contents to stdout
                if (t_opt && f_opt) list_files(existing_tar);

                // -u update entries if modification time is more recent
                if (u_opt && f_opt) update_files(existing_tar, argc, argv);

                // -x extract archive entries to current directory
                if (x_opt && f_opt) extract_files(existing_tar);

                close(existing_tar);
            }
        }
        
        return 0;
}