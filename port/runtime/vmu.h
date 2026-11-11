/* Pen Pen port, S6: the VMU in controller A's slot 1 (A1) as its 128 KiB flash image (vmu.c). */
#ifndef VMU_H
#define VMU_H
#include <stdint.h>

#define VMU_SIZE 131072
#define VMU_BLOCK 512
#define VMU_USER_BLOCKS 200
#define VMU_EXEC_BLOCKS 128 /* the area a VMU game must fit in, from block 0 */

extern uint8_t vmu_img[VMU_SIZE]; /* the host's save image (rtw_save_ptr) */
extern uint32_t vmu_generation;   /* bumped on every write (rtw_save_generation) */

int vmu_blank(void);              /* all zero: a card that was never formatted (a new save) */
int vmu_formatted(void);          /* the root block starts with 16 x 0x55 */
void vmu_format(const uint8_t bcd[8]);
int vmu_free_blocks(void);        /* free user blocks */
int vmu_free_exec(void);          /* free blocks from block 0 up (room for a VMU game) */
const uint8_t *vmu_root(void);
/* files by their 12-byte directory name; time = 8 BCD bytes (century, year, month, day, hour, minute,
 * second, day of week) */
int vmu_find(const char *name);   /* directory entry index, or -1 */
int vmu_file_blocks(int entry);
int vmu_read(int entry, uint8_t *dst, int max_blocks); /* blocks read */
/* write (replace) a data file: 0, -1 not enough free blocks, -2 directory full */
int vmu_write(const char *name, const uint8_t *data, int blocks, const uint8_t bcd[8]);
int vmu_check(void);              /* consistency of FAT and directory: 0 ok, else a negative code */

#endif
