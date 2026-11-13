/* Pen Pen port, S18 b01: writing the save file to the memory card (game/save_load.c reads it): the backup library
 * makes the file image "PENWRD_00000" from a file description (its comments, icon, the game's 0x380 bytes of save
 * data) and saves it with today's date. Errors show a message on the memory card's screen. */
#include "hw.h"
#include "sdk.h"
#include "save_write.h"
#include "../core/vec.h"
#include "../core/pad.h"
#include "../core/system.h"

#define g_save_drive    GUEST_VAR(int32_t, 0x8C06D35Cu) /* the backup library's drive the game saves to */
#define SAVE_FILE_NAME  0x8C06F368u /* "PENWRD_00000" */
#define SAVE_DATA       0x8C1B7E50u /* the game's save data */
#define SAVE_DATA_SIZE  0x380
#define SAVE_FREE_BLOCKS_MIN 5      /* a new file needs this many free blocks */

/* the description buMakeBackupFileImage makes the file image from (the SDK's file information) */
typedef struct SaveFileInfo {
  char comment_short[0x12];   /* 0x00: from 0x8C06CD35 */
  char comment_long[0x22];    /* 0x12: from 0x8C06CD24 */
  uint8_t game_name[0x10];    /* 0x34: 16 bytes from 0x8C06CD14 */
  gaddr icon_palette;         /* 0x44 */
  gaddr icon_pixels;          /* 0x48 */
  uint16_t icon_count;        /* 0x4C */
  uint16_t icon_speed;        /* 0x4E */
  uint32_t eyecatch_type;     /* 0x50 */
  uint16_t word54;            /* 0x54 */
  uint16_t pad56;
  gaddr save_data;            /* 0x58 */
  uint32_t save_size;         /* 0x5C */
} SaveFileInfo;
HW_OFFSET(SaveFileInfo, comment_long, 0x12);
HW_OFFSET(SaveFileInfo, game_name, 0x34);
HW_OFFSET(SaveFileInfo, icon_count, 0x4C);
HW_OFFSET(SaveFileInfo, word54, 0x54);
HW_OFFSET(SaveFileInfo, save_size, 0x5C);
HW_SIZE(SaveFileInfo, 0x60);

#define SAVE_FILE_INFO   0x8C1B822Cu /* SaveFileInfo */
#define g_save_blocks    GUEST_VAR(int32_t, 0x8C1B828Cu) /* the image's size in blocks (buMakeBackupFileImage) */
#define SAVE_FILE_IMAGE  0x8C1B8290u /* the file as the card holds it */
#define SAVE_COMMENT_SHORT 0x8C06CD35u
#define SAVE_COMMENT_LONG  0x8C06CD24u
#define SAVE_GAME_NAME     0x8C06CD14u
#define SAVE_ICON_PALETTE  0x8C06CD3Cu
#define SAVE_ICON_PIXELS   0x8C06CD5Cu
#define SAVE_ICON_COUNT    3
#define SAVE_ICON_SPEED    0xF
#define BU_SAVE_FLAGS      0x80000000u

/* the backup library's results (BUD_ERR_*) */
enum {
  BU_ERR_BUSY = -1,
  BU_ERR_NO_DISK = -255,
  BU_ERR_UNFORMAT = -253,
  BU_ERR_NO_FREE = -252,
  BU_ERR_FILE_NOT_FOUND = -251,
  BU_ERR_FILE_ERROR = -249,
  BU_ERR_OTHER = -191,
};
/* the card messages (func_8C019B30) the save shows */
enum { SAVE_MSG_NO_CARD = 0, SAVE_MSG_UNFORMATTED = 1, SAVE_MSG_NO_ROOM = 4, SAVE_MSG_FAILED = 5, SAVE_MSG_FULL = 7 };

/* the backup library (SDK; the first two through their thunks, as the original calls them) */
static int32_t bu_IsReady(int32_t drive) { return (int32_t)CALL1(0x8C0A00F2u, drive); }
static int32_t bu_IsFormatted(int32_t drive) { return (int32_t)CALL1(0x8C0A019Cu, drive); }
static int32_t bu_FindFile(int32_t drive, gaddr name) { return (int32_t)CALL2(0x8C0A0684u, drive, name); }
static int32_t bu_GetBlocks(int32_t drive, int32_t which) { return (int32_t)CALL2(0x8C0A028Au, drive, which); }
static int32_t buMakeBackupFileImage(gaddr image, gaddr info) { return (int32_t)CALL2(0x8C0A0AA0u, image, info); }
static int32_t buSaveFile(int32_t drive, gaddr name, gaddr image, int32_t blocks, gaddr time, uint32_t flags) {
  return (int32_t)hw_call_stack(0x8C0A0860u, 6,
                                (const uint32_t[]){(uint32_t)drive, name, image, (uint32_t)blocks, time, flags});
}
static int32_t buStat(int32_t drive) { return (int32_t)CALL1(0x8C0A06E4u, drive); }
static int32_t buGetLastError(int32_t drive) { return (int32_t)CALL1(0x8C0A06FEu, drive); }
/* the C runtime (still translated): memset, memmove */
static void rt_memset(gaddr area, int32_t byte, uint32_t size) { CALL3(0x8C062A98u, area, byte, size); }
static void rt_memmove(gaddr dst, gaddr src, uint32_t size) { CALL3(0x8C0629D8u, dst, src, size); }
/* the C runtime's strcpy func_8C0626BC (its arguments in r0 / r1): the bytes up to the terminating 0 included */
static void rt_strcpy(gaddr dst, gaddr src) {
  uint8_t c;
  do {
    c = *(const uint8_t *)hw_ptr(src++);
    *(uint8_t *)hw_ptr(dst++) = c;
  } while (c != 0);
}
/* func_8C019B30 (game/card_messages.c, not declared in a header): message n on the memory card's screen */
static void card_screen_draw(int32_t n) { CALL1(0x8C019B30u, n); }

/* after a message: 1 (go on) when Start was just pressed on port A, else 0 */
static int32_t save_message_dismissed(void) { return (g_pad_pressed[0] & PAD_START) != 0 ? 1 : 0; }

/* the file description: the comments, the game's name, the icon (3 frames), the save data */
static void save_file_info_build(void) {
  SaveFileInfo *info = (SaveFileInfo *)hw_ptr(SAVE_FILE_INFO);
  rt_memset(SAVE_FILE_INFO, 0, sizeof(SaveFileInfo));
  rt_strcpy(SAVE_FILE_INFO, SAVE_COMMENT_SHORT);
  rt_strcpy(SAVE_FILE_INFO + offsetof(SaveFileInfo, comment_long), SAVE_COMMENT_LONG);
  rt_memmove(SAVE_FILE_INFO + offsetof(SaveFileInfo, game_name), SAVE_GAME_NAME, sizeof info->game_name);
  info->icon_palette = SAVE_ICON_PALETTE;
  info->icon_pixels = SAVE_ICON_PIXELS;
  info->icon_count = SAVE_ICON_COUNT;
  info->icon_speed = SAVE_ICON_SPEED;
  info->eyecatch_type = 0;
  info->word54 = 0;
  info->save_data = SAVE_DATA;
  info->save_size = SAVE_DATA_SIZE;
}

/* the original's frame: the save time (the date's first 8 bytes), then the date */
typedef struct SaveWriteFrame {
  uint8_t time[8];  /* 0x00: year (16 bits), month, day, hour, minute, second, weekday */
  RtcDate date;     /* 0x08 */
} SaveWriteFrame;
HW_OFFSET(SaveWriteFrame, date, 0x08);
#define SAVE_WRITE_FRAME 0x14

/* was func_8C018F7C: writes the save file. No card: message 0. Unformatted: message 1; no disk: message 0 (and
 * then 1 when Start was just pressed, else 0); still busy: 0. Then the description is built; when the file is not
 * there (or the card unformatted) and fewer than 5 blocks are free: 1 if Start was just pressed, else message 4 and
 * 0. The image is made and saved with today's date (a refused save: 0), the save is waited for: 1 when it worked,
 * else the error's message and 1 when Start was just pressed, else 0. */
int32_t save_write(void) {
  if (bu_IsReady(g_save_drive) == 0) {
    card_screen_draw(SAVE_MSG_NO_CARD);
    return save_message_dismissed();
  }
  int32_t state = bu_IsFormatted(g_save_drive);
  if (state != 0) {
    if (state == BU_ERR_UNFORMAT) {
      card_screen_draw(SAVE_MSG_UNFORMATTED);
      return save_message_dismissed();
    }
    if (state == BU_ERR_NO_DISK) {
      card_screen_draw(SAVE_MSG_NO_CARD);
      return save_message_dismissed();
    }
    if (state == BU_ERR_BUSY)
      return 0;
  }

  save_file_info_build();
  int32_t found = bu_FindFile(g_save_drive, SAVE_FILE_NAME);
  if (found != 0) {
    if (found == BU_ERR_FILE_NOT_FOUND || found == BU_ERR_UNFORMAT) {
      if (bu_GetBlocks(g_save_drive, 0) < SAVE_FREE_BLOCKS_MIN) {
        if ((g_pad_pressed[0] & PAD_START) != 0)
          return 1;
        card_screen_draw(SAVE_MSG_NO_ROOM);
        return 0;
      }
    }
  }

  g_save_blocks = buMakeBackupFileImage(SAVE_FILE_IMAGE, SAVE_FILE_INFO);
  gaddr frame = hw_stack_alloc(SAVE_WRITE_FRAME);
  SaveWriteFrame *f = (SaveWriteFrame *)hw_ptr(frame);
  rtc_get_date(member_addr(frame, SaveWriteFrame, date));
  memcpy(&f->time[0], &f->date.year, 2);
  f->time[2] = f->date.month;
  f->time[3] = f->date.day;
  f->time[4] = f->date.hour;
  f->time[5] = f->date.minute;
  f->time[6] = f->date.second;
  f->time[7] = f->date.weekday;
  int32_t refused = buSaveFile(g_save_drive, SAVE_FILE_NAME, SAVE_FILE_IMAGE, g_save_blocks,
                               member_addr(frame, SaveWriteFrame, time), BU_SAVE_FLAGS);
  hw_stack_free(SAVE_WRITE_FRAME);
  if (refused != 0)
    return 0;
  while (buStat(g_save_drive) == BU_ERR_BUSY) {
  }
  int32_t error = buGetLastError(g_save_drive);
  if (error == 0)
    return 1;
  int32_t msg;
  if (error == BU_ERR_NO_DISK)
    msg = SAVE_MSG_NO_CARD;
  else if (error == BU_ERR_UNFORMAT)
    msg = SAVE_MSG_UNFORMATTED;
  else if (error == BU_ERR_FILE_ERROR)
    msg = SAVE_MSG_FAILED;
  else if (error == BU_ERR_OTHER)
    msg = SAVE_MSG_FAILED;
  else if (error == BU_ERR_NO_FREE)
    msg = SAVE_MSG_FULL;
  else
    msg = SAVE_MSG_FAILED;
  card_screen_draw(msg);
  return save_message_dismissed();
}
HW_EXPORT(8c018f7c, RET(save_write()))
