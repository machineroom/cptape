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
#include <unistd.h>

int main (int argc, char **argv) {
    char *in_file_name = argv[1];
    char *out_dev_name = argv[2];
    printf ("Opening %s as input\n", in_file_name);
    FILE *in_file = fopen (in_file_name, "rb");
    printf ("Opening %s as output\n", out_dev_name);
    int tape_fd = open (out_dev_name, O_RDWR);
    if (in_file == NULL || tape_fd == -1) {
        printf ("*E* %s\n", strerror(errno));
        exit(-1);
    } else {
        // build the special EOF marker. Ref http://bitsavers.org/bits/Apollo/Apollo_JRJ/readme-jrj.txt
        uint8_t cptape_EOF_magic[512];
        uint8_t *mp = cptape_EOF_magic;
        for (int i=0; i < sizeof(cptape_EOF_magic)/4; i++) {
            *mp++ = 0xDE;
            *mp++ = 0xAF;
            *mp++ = 0xFA;
            *mp++ = 0xED;
        }
        uint8_t block[512];
        int blocks_to_buffer = 16;
        uint8_t block_buffer[blocks_to_buffer*sizeof(block)];
        int block_buffer_count=0;
        int block_count = 0;
        printf ("OK, rewind the tape\n");
        {
            struct mtop mt_op;
            mt_op.mt_op = MTREW;
            mt_op.mt_count = 0;
            ioctl (tape_fd, MTIOCTOP, &mt_op);   
        }
        struct mtget mt_status;
        ioctl(tape_fd, MTIOCGET, &mt_status);
        printf ("Drive status:\n");
        printf ("\tmt_type 0x%lX\n", mt_status.mt_type);
        printf ("\tmt_resid 0x%lX\n", mt_status.mt_resid);
        printf ("\tmt_dsreg 0x%lX = block size %ld, density=%ld\n", mt_status.mt_dsreg, mt_status.mt_dsreg%0xFFFFFF, mt_status.mt_dsreg>>24);
        printf ("\tmt_gstat 0x%lX:\n", mt_status.mt_gstat);
        printf ("\t\tGMT_EOF %ld\n", GMT_EOF (mt_status.mt_gstat));
        printf ("\t\tGMT_BOT %ld\n", GMT_BOT (mt_status.mt_gstat));
        printf ("\t\tGMT_EOT %ld\n", GMT_EOT (mt_status.mt_gstat));
        printf ("\t\tGMT_SM %ld\n", GMT_SM (mt_status.mt_gstat));
        printf ("\t\tGMT_EOD %ld\n", GMT_EOD (mt_status.mt_gstat));
        printf ("\t\tGMT_WR_PROT %ld\n", GMT_WR_PROT (mt_status.mt_gstat));
        printf ("\t\tGMT_ONLINE %ld\n", GMT_ONLINE (mt_status.mt_gstat));
        printf ("\t\tGMT_D_6250 %ld\n", GMT_D_6250 (mt_status.mt_gstat));
        printf ("\t\tGMT_D_1600 %ld\n", GMT_D_1600 (mt_status.mt_gstat));
        printf ("\t\tGMT_D_800 %ld\n", GMT_D_800 (mt_status.mt_gstat));
        printf ("\t\tGMT_DR_OPEN %ld\n", GMT_DR_OPEN (mt_status.mt_gstat));
        printf ("\t\tGMT_IM_REP_EN %ld\n", GMT_IM_REP_EN (mt_status.mt_gstat));
        printf ("\tmt_erreg 0x%lX\n", mt_status.mt_erreg);
        while (1) {
            if (block_count % 1000 == 0) {
                printf ("Read block %d\n", block_count);
            }
            size_t num_read = fread (block, sizeof(block), 1, in_file);
            if (num_read == 1) {
                // got a block from the cptape dump
                if (memcmp (block, cptape_EOF_magic, sizeof(cptape_EOF_magic)) == 0) {
                    // write any residual blocks
                    if (block_buffer_count > 0) {
                        printf ("Write residual %d blocks to tape\n", block_buffer_count);
                        size_t to_wrte = block_buffer_count * sizeof(block);
                        ssize_t written = write (tape_fd, block_buffer, to_wrte);
                        if (written != to_wrte) {
                            printf ("*E* failed to write all bytes to tape (%ld != %ld)\n!", written, to_wrte);
                            printf ("*E* %s\n",strerror (errno));
                            break;
                        }
                    }
                    block_buffer_count = 0;
                    // Write an EOF marker to tape
                    printf ("Hit EOF @ block %d\n", block_count);
                    struct mtop mt_op;
                    mt_op.mt_op = MTWEOF;
                    mt_op.mt_count = 1;
                    ioctl (tape_fd, MTIOCTOP, &mt_op);   
                } else {
                    //accumulate some blocks before writing to tape to make writing more efficient
                    // (writing single blocks causes a lot of tape seek)
                    memcpy (&block_buffer[block_buffer_count*sizeof(block)], block, sizeof(block));
                    block_buffer_count++;
                    if (block_buffer_count == blocks_to_buffer) {
                        // write blocks to tape
                        size_t to_wrte = block_buffer_count * sizeof(block);
                        ssize_t written = write (tape_fd, block_buffer, to_wrte);
                        if (written != to_wrte) {
                            printf ("*E* failed to write all bytes to tape (%ld != %ld)\n!", written, to_wrte);
                            printf ("*E* %s\n",strerror (errno));
                            break;
                        }
                        block_buffer_count = 0;
                    }
                }
            } else {
                // write any residual blocks
                if (block_buffer_count > 0) {
                    printf ("Write residual %d blocks to tape\n", block_buffer_count);
                    size_t to_wrte = block_buffer_count * sizeof(block);
                    ssize_t written = write (tape_fd, block_buffer, to_wrte);
                    if (written != to_wrte) {
                        printf ("*E* failed to write all bytes to tape (%ld != %ld)\n!", written, to_wrte);
                        printf ("*E* %s\n",strerror (errno));
                        break;
                    }
                }
                break;
            }
            block_count++;
        }
        printf ("All done\n");
        close (tape_fd);
    }
}
