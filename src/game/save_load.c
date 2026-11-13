/* Pen Pen port, S16 (b01): reading the save file from the memory card (game/save_load.c).
 *
 * The title's state 3 (game/title.c) loads the save: the backup library reads the file "PENWRD_00000" from the
 * game's drive into a buffer; the game's save data (0x380 bytes) follows the file's header in it and is copied to
 * where the game keeps it. Errors show a message on the memory card's screen (func_8C019B30); a file that is not
 * there is made (func_8C018F7C, the save). */
#include "hw.h"
#include "sdk.h"
#include "../core/pad.h"

#define g_save_drive    GUEST_VAR(int32_t, 0x8C06D35Cu) /* the backup library's drive the game saves to */
#define SAVE_FILE_NAME  0x8C06F368u /* "PENWRD_00000" */
#define SAVE_FILE_IMAGE 0x8C1B8290u /* the file as the card holds it (header, icons, then the game's data) */
#define SAVE_FILE_DATA  0x8C1B8910u /* the game's data inside the file image */
#define SAVE_DATA       0x8C1B7E50u /* the game's save data */
#define SAVE_DATA_SIZE  0x380

/* the backup library's results (BUD_ERR_*) */
enum {
  BU_ERR_BUSY = -1,           /* buStat: still working */
  BU_ERR_NO_DISK = -255,
  BU_ERR_UNFORMAT = -253,
  BU_ERR_FILE_NOT_FOUND = -251,
  BU_ERR_FILE_ERROR = -249,   /* -249 and -191 show the same message */
  BU_ERR_OTHER = -191,
};
/* the card messages of func_8C019B30 the load shows */
enum { SAVE_MSG_NO_CARD = 0, SAVE_MSG_UNFORMATTED = 1, SAVE_MSG_DAMAGED = 2 };

/* the backup library (SDK) */
static int32_t bu_IsReady(int32_t drive) { return (int32_t)CALL1(0x8C0A00F2u, drive); }
static int32_t buLoadFile(int32_t drive, gaddr name, gaddr buf, uint32_t first, uint32_t count) {
  return (int32_t)CALL5(0x8C0A091Cu, drive, name, buf, first, count);
}
static int32_t buStat(int32_t drive) { return (int32_t)CALL1(0x8C0A06E4u, drive); }
static int32_t buGetLastError(int32_t drive) { return (int32_t)CALL1(0x8C0A06FEu, drive); }
static void nj_MemCopy4(gaddr dst, gaddr src, uint32_t n) { CALL3(0x8C0A1264u, dst, src, n); }

/* still translated */
/* func_8C019B30: shows message n on the memory card's screen */
static void save_card_message(int32_t n) { CALL1(0x8C019B30u, n); }
/* func_8C018F7C: writes the save file (makes it when it is not there); 1 = done */
static int32_t save_write(void) { return (int32_t)CALL0(0x8C018F7Cu); }

/* after a message: 1 (go on) when Start was just pressed on port A, else 0 */
static int32_t save_message_dismissed(void) { return (g_pad_pressed[0] & PAD_START) != 0 ? 1 : 0; }

/* was func_8C018E7C: loads the save. 1 when the data was read (or the save was made instead, or a message is shown
 * and Start was pressed), 0 while the card is not ready to be read or a message waits. The load is waited for. */
int32_t save_load(void) {
  if (bu_IsReady(g_save_drive) == 0) {
    save_card_message(SAVE_MSG_NO_CARD);
    return save_message_dismissed();
  }
  if (buLoadFile(g_save_drive, SAVE_FILE_NAME, SAVE_FILE_IMAGE, 0, 0) != 0)
    return 0;
  while (buStat(g_save_drive) == BU_ERR_BUSY) {
  }
  int32_t error = buGetLastError(g_save_drive);
  if (error == 0) {
    nj_MemCopy4(SAVE_DATA, SAVE_FILE_DATA, SAVE_DATA_SIZE);
    return 1;
  }
  if (error == BU_ERR_NO_DISK)
    save_card_message(SAVE_MSG_NO_CARD);
  else if (error == BU_ERR_UNFORMAT)
    save_card_message(SAVE_MSG_UNFORMATTED);
  else if (error == BU_ERR_FILE_NOT_FOUND)
    return save_write(); /* a tail call */
  else if (error == BU_ERR_FILE_ERROR)
    save_card_message(SAVE_MSG_DAMAGED);
  else if (error == BU_ERR_OTHER)
    save_card_message(SAVE_MSG_DAMAGED);
  return save_message_dismissed();
}
HW_EXPORT(8c018e7c, RET(save_load()))
