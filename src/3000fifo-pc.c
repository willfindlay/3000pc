/* 3000fifo-pc.c  Simple producer consumer with a fifo
 * Original Version Copyright (C) 2017  Anil Somayaji
 * Modified Version Copyright (C) 2020  William Findlay
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>. */

/* You really shouldn't be incorporating parts of this in any other code,
   it is meant for teaching, not production */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#define QUEUESIZE 32
#define WORDSIZE 16

const int wordlist_size = 27;
const char *wordlist[] = {
        "Alpha",
        "Bravo",
        "Charlie",
        "Delta",
        "Echo",
        "Foxtrot",
        "Golf",
        "Hotel",
        "India",
        "Juliet",
        "Kilo",
        "Lima",
        "Mike",
        "November",
        "Oscar",
        "Papa",
        "Quebec",
        "Romeo",
        "Sierra",
        "Tango",
        "Uniform",
        "Victor",
        "Whiskey",
        "X-ray",
        "Yankee",
        "Zulu",
        "Dash"
};

void report_error(char *error)
{
        fprintf(stderr, "Error: %s\n", error);
}

void usage_exit(char *progname)
{
        fprintf(stderr,
                "Usage: %s <event count> <prod interval int> <con interval int>\n",
                progname);
        exit(-1);
}

void pick_word(char *word)
{
        int pick;

        pick = random() % wordlist_size;

        strcpy(word, wordlist[pick]);
}

void output_word(int c, char *w)
{
        printf("Word %d: %s\n", c, w);
}

int queue_word(char *word, int pipefd_write)
{
        if (write(pipefd_write, word, WORDSIZE) == -1)
        {
                fprintf(stderr, "Error: Unable to write to pipe: %s\n", strerror(errno));
                return -1;
        }

        return 0;
}

int get_next_word(char *word, int pipefd_read)
{
        if (read(pipefd_read, word, WORDSIZE) == -1)
        {
                fprintf(stderr, "Error: Unable to read from pipe: %s\n", strerror(errno));
                return -1;
        }

        return 0;
}

void producer(int event_count, int pipefd_write, int prod_interval)
{
        char word[WORDSIZE];
        int i;

        for (i=0; i < event_count; i++) {
                pick_word(word);
                queue_word(word, pipefd_write);
                /* Sleep if we hit our interval */
                if (i % prod_interval == 0 && i > 0)
                {
                        fprintf(stderr, "Producer sleeping for 1 second...\n");
                        sleep(1);
                }
        }

        close(pipefd_write);
        fprintf(stderr, "Producer finished.\n");
        exit(0);
}

void consumer(int event_count, int pipefd_read, int con_interval)
{
        char word[WORDSIZE];
        int i;

        for (i=0; i < event_count; i++) {
                get_next_word(word, pipefd_read);
                output_word(i, word);
                /* Sleep if we hit our interval */
                if (i % con_interval == 0 && i > 0)
                {
                        fprintf(stderr, "Consumer sleeping for 1 second...\n");
                        sleep(1);
                }
        }

        close(pipefd_read);
        fprintf(stderr, "Consumer finished.\n");
        exit(0);
}

int main(int argc, char *argv[])
{
        int pid, count, prod_interval, con_interval;
        int pipefd[2];

        srandom(42);

        if (argc < 4) {
                if (argc < 1) {
                        report_error("no command line");
                        usage_exit(argv[0]);
                } else {
                        report_error("Not enough arguments");
                        usage_exit(argv[0]);
                }
        }

        count = atoi(argv[1]);
        prod_interval = atoi(argv[2]);
        con_interval = atoi(argv[3]);

        /* Open a fifo
         * pipefd[0] will be open for reading, and
         * pipefd[1] will be open for writing */
        if (pipe(pipefd))
        {
                fprintf(stderr, "Error: Unable to open pipe: %s\n", strerror(errno));
                exit(-1);
        }

        pid = fork();

        if (pid) {
                /* producer */
                producer(count, pipefd[1], prod_interval);
        } else {
                /* consumer */
                consumer(count, pipefd[0], con_interval);
        }

        /* This line should never be reached */
        return -1;
}
