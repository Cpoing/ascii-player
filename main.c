#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main()
{
  const char ASCII_RAMP[] = " .:-=+*#%@";
  const int ASCII_RAMP_LEN = strlen(ASCII_RAMP);

  struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  int w = ws.ws_col;
  int h = ws.ws_row;
  char cmd[256];
  snprintf(cmd, sizeof(cmd),
      "ffmpeg -i drift.mp4 -vf scale=%d:%d "
      "-f rawvideo -pix_fmt rgb24 -r 24 "
      "-loglevel quiet pipe:1", w, h);

  FILE *pipe = popen(cmd, "r");
  if (!pipe) { perror("popen"); return 1;}
  
  unsigned char *frame = malloc(w * h * 3);

  printf("\x1b[?25l");

  char *out = malloc(w * h + h + 32);
  while (fread(frame, 1, w * h * 3, pipe) == w * h * 3) {
    int pos = 0;
    out[pos++] = '\x1b'; out[pos++] = '['; out[pos++] = 'H';
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        unsigned char *px = &frame[(y * w + x) * 3];
        int lum = (int)(0.299*px[0] + 0.587*px[1] + 0.114*px[2]);
        out[pos++] = ASCII_RAMP[lum * (ASCII_RAMP_LEN - 1) / 255];
      }
      out[pos++] = '\n';
    }
    fwrite(out, 1, pos, stdout);
    fflush(stdout);
  }
  pclose(pipe);
  free(frame);
  return 0;
}
