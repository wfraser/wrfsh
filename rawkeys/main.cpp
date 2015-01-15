#include <stdio.h>
#include <unistd.h>
#include <termio.h>

int main()
{
    termios savedTermios, tp;

    tcgetattr(STDIN_FILENO, &tp);
    savedTermios = tp;
    cfmakeraw(&tp);
    tp.c_oflag |= ONLCR|OPOST;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tp);

    for (;;)
    {
        char c;
        ssize_t num_read = read(STDIN_FILENO, &c, 1);
        if (num_read <= 0 || c == 4)
        {
            break;
        }

        printf("0x%02x ", c);
        if (c > ' ' && c < 127)
        {
            // Printable character.
            printf("%c", c);
        }
        else
        {
            // Non-printable character.
            switch (c)
            {
            case '\x1b':
                printf("ESC");
                break;
            case '\x20':
                printf("SPACE");
                break;
            case '\x7F':
                printf("DEL");
                break;
            }
        }
        printf("\n");
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &savedTermios);
    return 0;
}
