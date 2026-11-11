/* Pen Pen port, S3: rtw_disc_toc / rtw_disc_read for the native build, over a Redump .cue + .bin set
 * (2352-byte raw sectors). The same LBA model as the retrowser host: the single-density tracks from the
 * cue's own times, the high-density area from LBA 45000 (track 3's file), INDEX 01 as the track start. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *disc_dir = "/disc";
typedef struct { int num, data; long idx1; long sectors; long lba_file; char file[512]; } track;
static track tr[99];
static int ntr = -1;
static FILE *fh[99];

static int parse(void) {
  if (ntr >= 0) return ntr;
  ntr = 0;
  DIR *d = opendir(disc_dir);
  if (!d) return -1;
  char cue[512] = "";
  struct dirent *e;
  while ((e = readdir(d))) {
    size_t n = strlen(e->d_name);
    if (n > 4 && !strcasecmp(e->d_name + n - 4, ".cue")) snprintf(cue, sizeof cue, "%s/%s", disc_dir, e->d_name);
  }
  closedir(d);
  FILE *f = fopen(cue, "r");
  if (!f) return -1;
  char line[1024], file[512] = "";
  while (fgets(line, sizeof line, f)) {
    char *q;
    if (!strncmp(line, "FILE", 4) && (q = strchr(line, '"'))) {
      char *r = strchr(q + 1, '"');
      if (r) { *r = 0; snprintf(file, sizeof file, "%s/%s", disc_dir, q + 1); }
    } else if (strstr(line, "TRACK") && ntr < 99) {
      track *t = &tr[ntr++];
      memset(t, 0, sizeof *t);
      sscanf(strstr(line, "TRACK") + 5, "%d", &t->num);
      t->data = strstr(line, "MODE") != NULL;
      snprintf(t->file, sizeof t->file, "%s", file);
    } else if (strstr(line, "INDEX 01") && ntr) {
      int m, s, fr;
      if (sscanf(strstr(line, "INDEX 01") + 8, "%d:%d:%d", &m, &s, &fr) == 3) tr[ntr - 1].idx1 = (m * 60 + s) * 75 + fr;
    }
  }
  fclose(f);
  long lba = 0;
  for (int i = 0; i < ntr; i++) {
    FILE *g = fopen(tr[i].file, "rb");
    if (!g) continue;
    fseek(g, 0, SEEK_END);
    tr[i].sectors = ftell(g) / 2352;
    fclose(g);
    if (tr[i].num == 3) lba = 45000;
    tr[i].lba_file = lba;
    lba += tr[i].sectors;
  }
  return ntr;
}

int rtw_disc_toc(void *entries, int max) {
  if (parse() <= 0) return -1;
  int *o = entries;
  for (int i = 0; i < ntr && i < max; i++) {
    o[4 * i] = tr[i].num;
    o[4 * i + 1] = (int)(tr[i].lba_file + tr[i].idx1);
    o[4 * i + 2] = (int)(tr[i].sectors - tr[i].idx1);
    o[4 * i + 3] = tr[i].data | (tr[i].num >= 3 ? 2 : 0);
  }
  return ntr;
}

int rtw_disc_read(int track_no, int lba, int count, void *dst) {
  if (parse() <= 0) return -1;
  int i = 0;
  while (i < ntr && tr[i].num != track_no) i++;
  if (i == ntr) return -1;
  track *t = &tr[i];
  long first = t->lba_file + t->idx1, last = t->lba_file + t->sectors;
  if (lba < first || lba >= last) return -2;
  if (count > 256) count = 256;
  if (lba + count > last) count = (int)(last - lba);
  if (!fh[i] && !(fh[i] = fopen(t->file, "rb"))) return -3;
  static unsigned char raw[2352];
  unsigned char *d = dst;
  for (int k = 0; k < count; k++) {
    if (fseek(fh[i], (lba + k - t->lba_file) * 2352L, SEEK_SET) || fread(raw, 1, 2352, fh[i]) != 2352) return -3;
    if (t->data) { memcpy(d, raw + 16, 2048); d += 2048; }
    else { memcpy(d, raw, 2352); d += 2352; }
  }
  return count;
}
