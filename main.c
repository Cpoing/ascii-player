#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

static FILE *child_pipe = NULL;

static void cleanup(int sig)
{
    if (child_pipe) pclose(child_pipe);
    printf("\x1b[?25h");
    exit(0);
}

static inline int write_int(char *buf, int n)
{
    if (n == 0) { buf[0] = '0'; return 1; }
    char tmp[3];
    int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    return i;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video>\n", argv[0]);
        return 1;
    }
    char *input = argv[1];
    const char ASCII_RAMP[] = " .:-=+*#%@";
    const int ASCII_RAMP_LEN = strlen(ASCII_RAMP);

    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    int w = ws.ws_col;
    int h = ws.ws_row;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -re -i '%s' -vf scale=%d:%d "
        "-f rawvideo -pix_fmt rgb24 -r 24 "
        "-loglevel quiet pipe:1", input, w, h);

    child_pipe = popen(cmd, "r");
    if (!child_pipe) { perror("popen"); return 1; }

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    unsigned char *frame = malloc(w * h * 3);
    char *out = malloc(w * h * 20 + h + 64);

    printf("\x1b[?25l");

    unsigned char pr = 0, pg = 0, pb = 0;
    while (fread(frame, 1, w * h * 3, child_pipe) == (size_t)(w * h * 3)) {
        int pos = 0;
        out[pos++] = '\x1b'; out[pos++] = '['; out[pos++] = 'H';

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                unsigned char *px = &frame[(y * w + x) * 3];
                int lum = (int)(0.299*px[0] + 0.587*px[1] + 0.114*px[2]);

                if (px[0] != pr || px[1] != pg || px[2] != pb) {
                    out[pos++] = '\x1b'; out[pos++] = '['; out[pos++] = '3'; out[pos++] = '8';
                    out[pos++] = ';';    out[pos++] = '2'; out[pos++] = ';';
                    pos += write_int(&out[pos], px[0]);
                    out[pos++] = ';';
                    pos += write_int(&out[pos], px[1]);
                    out[pos++] = ';';
                    pos += write_int(&out[pos], px[2]);
                    out[pos++] = 'm';
                    pr = px[0]; pg = px[1]; pb = px[2];
                }

                out[pos++] = ASCII_RAMP[lum * (ASCII_RAMP_LEN - 1) / 255];
            }
            out[pos++] = '\n';
        }

        out[pos++] = '\x1b'; out[pos++] = '['; out[pos++] = '0'; out[pos++] = 'm';
        fwrite(out, 1, pos, stdout);
        fflush(stdout);
    }

    cleanup(0);
    free(out);
    free(frame);
    return 0;
}
