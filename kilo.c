/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

/*** defines ***/
#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
/*** data ***/

struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

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

int getCursorPosition(int *rows, int *cols) {
    char buffer[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    while (i < sizeof(buffer) - 1) {
        if (read(STDIN_FILENO, &buffer[i], 1) != 1) break;
        if (buffer[i] == 'R') break;
        i++;
    }
    buffer[i] = '\0';

    if (buffer[0] != '\x1b' || buffer[1] != '[') {
        return -1;
    }
    if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2) {
        return -1;
    }

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        return getCursorPosition(rows, cols);
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

/*** append buffer  ***/

struct appendBuffer {
    char *b;
    int length;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct appendBuffer *ab, const char *s, int length) {
    char *new = realloc(ab->b, ab->length + length);

    if (new == NULL) {
        return;
    }
    memcpy(&new[ab->length], s, length);
    ab->b = new;
    ab->length += length;
}

void abFree(struct appendBuffer *ab) {
    free(ab->b);
}

/*** output ***/

void editorDrawRows(struct appendBuffer *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        if (y == E.screenrows / 3) {
            char welcome_msg[80];
            int welcome_msg_len = snprintf(welcome_msg, sizeof(welcome_msg),
                    "Kilo editor -- version %s", KILO_VERSION);
            if (welcome_msg_len > E.screencols) {
                welcome_msg_len = E.screencols;
            }
            int padding = (E.screencols - welcome_msg_len) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding --;
            }
            while (padding--) {
                abAppend(ab, " ", 1);
            }
            abAppend(ab, welcome_msg, welcome_msg_len);
        } else {
          abAppend(ab, "~", 1);
        }

        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows -1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct appendBuffer ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buffer, strlen(buffer));
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.length);
    abFree(&ab);
}

/*** input ***/

void editorMoveCursor(char key) {
    switch (key) {
        case 'h':
             E.cx--;
             break;
        case 'j':
             E.cy++;
             break;
        case 'k':
             E.cy--;
             break;
        case 'l':
             E.cx++;
             break;
    }
}

void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c) {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    case 'h':
    case 'j':
    case 'k':
    case 'l':
        editorMoveCursor(c);
        break;
    }
}

/*** init ***/

void initEditor() {
    E.cx = 0;
    E.cy = 0;

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
