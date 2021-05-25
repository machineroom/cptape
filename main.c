#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int main (int argc, char **argv) {
    char *in_file_name = argv[1];
    char *out_dev_name = argv[2];
    printf ("Opening %s as input\n", in_file_name);
    FILE *in_file = fopen (in_file_name, "rb");
    printf ("Opening %s as output\n", out_dev_name);
    int out_file = open (out_dev_name, 0, O_RDWR);
    if (in_file == NULL || out_file == -1) {
        printf ("*E* %s\n", strerror(errno));
        exit(-1);
    } else {
        // build the special EOF marker. Ref http://bitsavers.org/bits/Apollo/Apollo_JRJ/readme-jrj.txt
        uint8_t marker[128];
        uint8_t *mp = marker;
        for (int i=0; i < 128; i++) {
            *mp++ = 0xDE;
            *mp++ = 0xAF;
            *mp++ = 0xFA;
            *mp++ = 0xED;
        }
        uint8_t block[512];
        uint32_t block_count = 0u;
        printf ("OK, rewind the tape\n");
        ioctl (out_file, MTREW);
        while (1) {
            if (block_count % 1000 == 0) {
                printf ("Read block %d\n", block_count);
            }
            block_count++;
            size_t num_read = fread (block, sizeof(block), 1, in_file);
            if (num_read == 1) {
                if (memcmp (block, marker, sizeof(marker)) == 0) {
                    printf ("Hit EOF\n");
                }
            } else {
                printf ("All done");
                break;
            }
        }
    }
}
