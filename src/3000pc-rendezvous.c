/* 3000pc-rendezvous.c  More complex producer-consumer using mmap shared memory and pthread_cond_wait
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>

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

typedef struct entry {
        char word[WORDSIZE];
        sem_t lock;
} entry;

typedef struct shared {
        pthread_mutex_t nonfull_mutex;
        pthread_mutex_t nonempty_mutex;
        pthread_cond_t  queue_nonempty;
        pthread_cond_t  queue_nonfull;
        entry queue[QUEUESIZE];
        int last_produced;
        int last_consumed;
        pid_t prod_pid;
        pid_t con_pid;
        int prod_count;
        int con_count;
} shared;


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
        unsigned int pick;

        /* Open /dev/urandom for reading */
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0)
        {
                fprintf(stderr, "Error: Unable to open /dev/urandom for reading: %s\n", strerror(errno));
                pick = 0;
        }
        else if (read(fd, (void *)&pick, sizeof(pick)) == -1)
        {
                fprintf(stderr, "Error: Unable to read from /dev/urandom: %s\n",strerror(errno));
                pick = 0;
        }

        pick = pick % wordlist_size;

        strcpy(word, wordlist[pick]);

        close(fd);
}

void wait_for_producer(shared *s)
{
        fprintf(stderr, "Waiting for producer...\n");
        pthread_mutex_lock(&s->nonempty_mutex);
        pthread_cond_wait(&s->queue_nonempty, &s->nonempty_mutex);
        pthread_mutex_unlock(&s->nonempty_mutex);
}

void wait_for_consumer(shared *s)
{
        fprintf(stderr, "Waiting for consumer...\n");
        pthread_mutex_lock(&s->nonfull_mutex);
        pthread_cond_wait(&s->queue_nonfull, &s->nonfull_mutex);
        pthread_mutex_unlock(&s->nonfull_mutex);
}

void output_word(int c, char *w)
{
        printf("Word %d: %s\n", c, w);
}

int queue_word(char *word, shared *s)
{
        entry *e;
        int current;

        current = (s->last_produced + 1) % QUEUESIZE;
        e = &s->queue[current];
        sem_wait(&e->lock);

        while (e->word[0] != '\0')
        {
                /* consumer hasn't consumed this entry yet */
                sem_post(&e->lock);
                wait_for_consumer(s);
                sem_wait(&e->lock);
                current = (s->last_produced + 1) % QUEUESIZE;
                e = &s->queue[current];
        }

        strncpy(e->word, word, WORDSIZE);
        s->last_produced = current;
        s->prod_count++;

        /* Notify that queue is nonempty */
        pthread_mutex_lock(&s->nonempty_mutex);
        pthread_cond_signal(&s->queue_nonempty);
        pthread_mutex_unlock(&s->nonempty_mutex);

        sem_post(&e->lock);
        return 0;
}

int get_next_word(char *word, shared *s)
{
        entry *e;
        int current;

        current = (s->last_consumed + 1) % QUEUESIZE;
        e = &s->queue[current];
        sem_wait(&e->lock);

        if (e->word[0] == '\0')
        {
                /* producer hasn't filled in this entry yet */
                sem_post(&e->lock);
                wait_for_producer(s);
                sem_wait(&e->lock);
                current = (s->last_consumed + 1) % QUEUESIZE;
                e = &s->queue[current];
        }

        strncpy(word, e->word, WORDSIZE);
        e->word[0] = '\0';
        s->last_consumed = current;
        s->con_count++;

        /* Notify that queue is nonfull */
        pthread_mutex_lock(&s->nonfull_mutex);
        pthread_cond_signal(&s->queue_nonfull);
        pthread_mutex_unlock(&s->nonfull_mutex);

        sem_post(&e->lock);
        return 0;
}

void producer(shared *s, int event_count, int prod_interval)
{
        char word[WORDSIZE];
        int i;

        for (i=0; i < event_count; i++)
        {
                pick_word(word);
                queue_word(word, s);

                /* Don't sleep if interval <= 0 */
                if (prod_interval <= 0)
                        continue;
                /* Sleep if we hit our interval */
                if (i % prod_interval == 0)
                {
                        fprintf(stderr, "Producer sleeping for 1 second...\n");
                        sleep(1);
                }
        }

        fprintf(stderr, "Producer finished.\n");
        exit(0);
}

void consumer(shared *s, int event_count, int con_interval)
{
        char word[WORDSIZE];
        int i;

        for (i=0; i < event_count; i++)
        {
                get_next_word(word, s);
                output_word(s->con_count, word);

                /* Don't sleep if interval <= 0 */
                if (con_interval <= 0)
                        continue;
                /* Sleep if we hit our interval */
                if (i % con_interval == 0)
                {
                        fprintf(stderr, "Consumer sleeping for 1 second...\n");
                        sleep(1);
                }
        }

        fprintf(stderr, "Consumer finished.\n");
        exit(0);
}

void init_shared(shared *s)
{
        int i;

        /* We need to explicitly mark the mutex as shared or risk undefined behavior */
        pthread_mutexattr_t mattr = {};
        pthread_mutexattr_setpshared(&mattr, 1);
        pthread_mutex_init(&s->nonfull_mutex, &mattr);
        pthread_mutex_init(&s->nonempty_mutex, &mattr);

        /* We need to explicitly mark the conditions as shared or risk undefined behavior */
        pthread_condattr_t cattr = {};
        pthread_condattr_setpshared(&cattr, 1);
        pthread_cond_init(&s->queue_nonempty, &cattr);
        pthread_cond_init(&s->queue_nonfull, &cattr);

        s->last_consumed = -1;
        s->last_produced = -1;

        s->prod_pid = -1;
        s->con_pid  = -1;

        s->prod_count = 0;
        s->con_count  = 0;

        for (i=0; i<QUEUESIZE; i++)
        {
                s->queue[i].word[0] = '\0';
                /* semaphore is shared between processes,
                   and initial value is 1 (unlocked) */
                sem_init(&s->queue[i].lock, 1, 1);
        }
}

int main(int argc, char *argv[])
{
        int pid, count, prod_interval, con_interval;

        shared *s;

        if (argc < 4)
        {
                if (argc < 1)
                {
                        report_error("no command line");
                        usage_exit(argv[0]);
                }
                else
                {
                        report_error("Not enough arguments");
                        usage_exit(argv[0]);
                }
        }

        count = atoi(argv[1]);
        prod_interval = atoi(argv[2]);
        con_interval = atoi(argv[3]);

        s = (shared *) mmap(NULL, sizeof(shared),
                             PROT_READ|PROT_WRITE,
                             MAP_SHARED|MAP_ANONYMOUS, -1, 0);

        if (s == MAP_FAILED)
        {
                fprintf(stderr, "Error: Unable to mmap: %s\n", strerror(errno));
                exit(-1);
        }

        init_shared(s);

        pid = fork();

        if (pid == 0)
        {
                /* Producer */
                s->prod_pid = getpid();
                producer(s, count, prod_interval);
        } else
        {
                /* Consumer */
                s->con_pid = getpid();
                consumer(s, count, con_interval);
        }

        /* This line should never be reached */
        return -1;
}
