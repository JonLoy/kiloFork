/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig {
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/
void editorClearScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void die(const char *s) {
    editorClearScreen(); 

    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
       die("tcsetattr");
    }

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
       die("tcgetattr");
    }

    tcgetattr(STDIN_FILENO, &E.orig_termios);
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) { 
            die("read"); 
        }
    }
    return c;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

/*** output ***/

void editorDrawRows() {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScreen() {
    editorClearScreen();

    editorDrawRows();

    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c) {
    case CTRL_KEY('q'):
        editorClearScreen();
        exit(0);
        break;
    }
}

/*** init ***/

void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
        die("getWindowSize");
    }
}

int main() {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
