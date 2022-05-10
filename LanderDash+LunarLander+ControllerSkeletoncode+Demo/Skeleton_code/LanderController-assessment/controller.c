/* Sample solution for the LunarLander assignment
 * KV5002
 *
 * Dr Alun Moon
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include <pthread.h>
#include <semaphore.h>

#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "libnet.h"
#include "console.h"

#include <ctype.h>
#include <curses.h>
#include <time.h>

/* semaphores and global variables for communication */

struct command
{
    float thrust;
    float rotn;
} landercommand;
sem_t cmdlock;

struct state
{
    float x, y, O;
    float dx, dy, dO;
} landerstate;
sem_t statelock;

enum condstate
{
    Flying,
    Down,
    Crashed
};
struct condition
{
    float fuel;
    float altitude;
    int contact;
} landercond;
sem_t condlock;

/* -------------------- Keyboard Input --------------------

    Runs in own thread, interprets user input
    Generates command structures for transmission to lander
*/
int last;
void *keyboard(void *data)
{
    landercommand.thrust = 0;
    landercommand.rotn = 0;
    while (true)
    {
        int key;
        while ((key = key_pressed()) == ERR)
        {
            ;
        } /* wait for a key-press */

        last = key;
        sem_wait(&cmdlock); /*Enter critical section*/
        switch (key)
        {
        case KEY_UP:
            landercommand.thrust += 2;
            if (landercommand.thrust > 100)
                landercommand.thrust = 100;
            break;
        case KEY_DOWN:
            landercommand.thrust -= 2;
            if (landercommand.thrust < 0)
                landercommand.thrust = 0;
            break;
        case KEY_RIGHT:
            landercommand.rotn += 0.1;
            if (landercommand.rotn >= 1)
                landercommand.rotn = 1;
            break;
        case KEY_LEFT:
            landercommand.rotn -= 0.1;
            if (landercommand.rotn <= -1.0)
                landercommand.rotn = -1;
            break;
        }
        sem_post(&cmdlock); /*Exit critical section*/
    }
}

/* -------------------- Display Management --------------------

    Updates the display with lander diagnostic information
*/
void *display(void *data)
{
    while (true)
    {
        sem_wait(&condlock);
        switch (landercond.contact)
        {
        case Flying:
            lcd_set_colour(3, 0);
            lcd_write_at(1, 30, "Flying ");
            break;
        case Down:
            lcd_set_colour(2, .0);
            lcd_write_at(1, 30, "Down   ");
            break;
        case Crashed:
            lcd_set_colour(1, 0);
            lcd_write_at(1, 30, "Crashed");
            break;
        }
        lcd_set_colour(7, 0);
        lcd_write_at(0, 0, "fuel %f", landercond.fuel);
        lcd_write_at(1, 0, "alt  %f", landercond.altitude);
        sem_post(&condlock);

        sem_wait(&statelock);
        lcd_write_at(3, 0, "x %-6.1f  x' %-8.6f", landerstate.x, landerstate.dx);
        lcd_write_at(4, 0, "y %-6.1f  y' %-8.6f", landerstate.y, landerstate.dy);
        lcd_write_at(5, 0, "O %-6.3f  O' %-8.6f", landerstate.O, landerstate.dO);
        sem_post(&statelock);

        sem_wait(&cmdlock);
        lcd_write_at(3, 30, "thrust %6.1f", landercommand.thrust);
        lcd_write_at(4, 30, "rotn %6.1f", landercommand.rotn);
        sem_post(&cmdlock);

        switch (last)
        {
        case KEY_UP:
            lcd_write_at(0, 40, "up   ");
            break;
        case KEY_DOWN:
            lcd_write_at(0, 40, "down ");
            break;
        case KEY_LEFT:
            lcd_write_at(0, 40, "left ");
            break;
        case KEY_RIGHT:
            lcd_write_at(0, 40, "right");
            break;
        default:
            lcd_write_at(0, 40, "%c   ", last);
        }

        usleep(500000);
    }
}

// --- Parse condition reply message --
void parsecondition(char *m)
{
    char *line;
    char *rest;
    for (/* split into lines lecture 07-2 slide 21 */
         line = strtok_r(m, "\r\n", &rest);
         line != NULL;
         line = strtok_r(NULL, "\r\n", &rest))
    {
        char *key, *value;
        key = strtok(line, ":");
        value = strtok(NULL, ":");

        sem_wait(&condlock); /* Enter critical section */

        if (strcmp(key, "fuel") == 0)
            sscanf(value, "%f%%", &(landercond.fuel));

        if (strcmp(key, "altitude") == 0)
            sscanf(value, "%f", &(landercond.altitude));

        if (strcmp(key, "contact") == 0)
        {
            if (strcmp(value, "flying") == 0)
                landercond.contact = Flying;

            if (strcmp(value, "down") == 0)
                landercond.contact = Down;

            if (strcmp(value, "crashed") == 0)
                landercond.contact = Crashed;
        }
        sem_post(&condlock); /* Exit critical section */
    }
}

// --- Parse state message ---
void parsestate(char *m)
{
    char *line;
    char *rest;
    for (/* split into lines lecture 07-2 slide 21 */
         line = strtok_r(m, "\r\n", &rest);
         line != NULL; line = strtok_r(NULL, "\r\n", &rest))
    {
        char *key, *value;
        key = strtok(line, ":");
        value = strtok(NULL, ":");

        sem_wait(&statelock); /*Enter critical section */

        if (strcmp(key, "x") == 0)
            sscanf(value, "%f", &(landerstate.x));

        if (strcmp(key, "y") == 0)
            sscanf(value, "%f", &(landerstate.y));

        if (strcmp(key, "O") == 0)
            sscanf(value, "%f", &(landerstate.O));

        if (strcmp(key, "x'") == 0)
            sscanf(value, "%f", &(landerstate.dx));

        if (strcmp(key, "y'") == 0)
            sscanf(value, "%f", &(landerstate.dy));

        if (strcmp(key, "O'") == 0)
            sscanf(value, "%f", &(landerstate.dO));

        sem_post(&statelock); /*Exit critical section */
    }
}

/* -------------------- Lander communication --------------------

    Communicates with the lander model
    Sends commands and queries state
    Parses and decodes messages

    Arguments:
        data -> port number
*/
void *lander(void *data)
{
    size_t msgsize = 1000;
    char msgbuf[msgsize];
    int l;
    struct addrinfo *landr;

    const char conditionq[] = "condition:?\n";
    const char stateq[] = "state:?\n";

    // Get address and open a socket
    if (!getaddr("127.0.1.1", (char *)data, &landr))
        fprintf(stderr, "Can't get lander address\n");
    ;
    l = mksocket();

    while (true)
    {
        int m;
        usleep(50000); /* 20Hz = 0.05s = 50ms = 50000us */
        /* poll for condition */
        sendto(l, conditionq, strlen(conditionq), 0, landr->ai_addr,
               landr->ai_addrlen);

        m = recvfrom(l, msgbuf, msgsize, 0, NULL, NULL);
        msgbuf[m] = '\0';

        /* parse condition */
        parsecondition(msgbuf);

        /* poll for state */
        sendto(l, stateq, strlen(stateq), 0, landr->ai_addr,
               landr->ai_addrlen);

        m = recvfrom(l, msgbuf, msgsize, 0, NULL, NULL);
        msgbuf[m] = '\0';
        parsestate(msgbuf);

        /* format command to send to lander */
        sprintf(msgbuf,
                "command:!\n"
                "main-engine: %f\n"
                "rcs-roll: %f\n",
                landercommand.thrust, landercommand.rotn);
        sendto(l, msgbuf, strlen(msgbuf), 0, landr->ai_addr, landr->ai_addrlen);
        m = recvfrom(l, msgbuf, msgsize, 0, NULL, NULL);
        msgbuf[m] = '\0';

        usleep(100000);
    }
}

/* -------------------- Dashboard communication --------------------

    Formats and sends data messages to the dashboard
*/
void *dashboard(void *data)
{
    size_t bufsize = 1024;
    char buffer[bufsize];
    int d;
    struct addrinfo *daddr;

    // Get address and open a socket
    if (!getaddr("127.0.1.1", (char *)data, &daddr))
        fprintf(stderr, "Canott get dashboard address");
    d = mksocket();

    while (true)
    {
        int buffer_error = sprintf(buffer, "fuel:%f\naltitude:%f\n", landercond.fuel, landercond.altitude);

        if (buffer_error == -1)
            fprintf(stderr, "Error creating buffer array");

        // Send buffer with the message to the dashboard through socket
        sendto(d, buffer, strlen(buffer), 0, daddr->ai_addr, daddr->ai_addrlen);

        usleep(500000);
    }
}

/* -------------------- Data Logging --------------------

    Periodically logs data to a file.
*/
void *datalogging(void *data)
{
    FILE *fileptr;

    // Open the data file
    fileptr = fopen("data.csv", "w");
    if (fileptr == NULL)
    {
        fprintf(stderr, "Data file could not be opened");
        exit(1);
    }

    // Logged variables
    const int message_size = 500;
    const int round_numbers = 4;
    char *delimiter = ", ";

    time_t raw_time;
    struct tm *time_info;
    char *current_time = "";

    char *key_pressed = "";

    char lander_thrust[10];
    char lander_rotation[10];

    char lander_state_x[10];
    char lander_state_y[10];
    char lander_state_O[10];

    char lander_state_dx[10];
    char lander_state_dy[10];
    char lander_state_dO[10];

    while (true)
    {
        // Get current time
        time(&raw_time);
        time_info = localtime(&raw_time);
        current_time = asctime(time_info);
        current_time[strcspn(current_time, "\n")] = 0;

        // Get currently pressed key
        switch (last)
        {
        case KEY_UP:
            key_pressed = "up";
            break;
        case KEY_DOWN:
            key_pressed = "down";
            break;
        case KEY_LEFT:
            key_pressed = "left";
            break;
        case KEY_RIGHT:
            key_pressed = "right";
            break;
        default:
            key_pressed = "none";
        }

        // Get current lander command
        gcvt(landercommand.thrust, round_numbers, lander_thrust);
        gcvt(landercommand.rotn, round_numbers, lander_rotation);

        // Get current lander state
        gcvt(landerstate.x, round_numbers, lander_state_x);
        gcvt(landerstate.y, round_numbers, lander_state_y);
        gcvt(landerstate.O, round_numbers, lander_state_O);

        gcvt(landerstate.dx, round_numbers, lander_state_dx);
        gcvt(landerstate.dy, round_numbers, lander_state_dy);
        gcvt(landerstate.dO, round_numbers, lander_state_dO);

        // Build the string that will be logged
        char log_text[message_size];
        int string_building_error = sprintf(log_text, "{\"%s\":[{\"key\":\"%s\",\"lander\":[{\"thrust \":\"%s\", \"rotation\":\"%s\", \"lander state\":[{\"x\":\"%s\", \"y\":\"%s\", \"O\":\"%s\", \"dx\":\"%s\", \"dy\":\"%s\", \"dO\":\"%s\"}]}]}]}",
                                            current_time,
                                            key_pressed,
                                            lander_thrust,
                                            lander_rotation,
                                            lander_state_x,
                                            lander_state_y,
                                            lander_state_O,
                                            lander_state_dx,
                                            lander_state_dy,
                                            lander_state_dO);
        if (string_building_error == -1)
            fprintf(stderr, "Failed to build data logging string");

        // Write into the data file
        fprintf(fileptr, "%s%s", log_text, delimiter);
        fflush(fileptr); // Use fflush due to buffered IO

        usleep(5000000); // Sleep 5 seconds -> log data each 5 seconds
    }

    // Close the data file
    fclose(fileptr);
}

/* -------------------- MAIN --------------------

Arguments:
    argv[1] -> lander
    argv[2] -> dashboard
*/
int main(int argc, char *argv[])
{
    pthread_t keyboard_thread;     // Keyboard
    pthread_t display_thread;      // Display
    pthread_t lander_thread;       // Lander
    pthread_t dashboard_thread;    // Dashboard
    pthread_t data_logging_thread; // Data logging

    int thread_error;

    // Initialize semaphores
    sem_init(&condlock, 0, 1);
    sem_init(&statelock, 0, 1);
    sem_init(&cmdlock, 0, 1);

    // Initialize the console display
    console_init();

    // --- Create threads ---

    // Display thread
    if ((thread_error = pthread_create(&display_thread, NULL, display, NULL)))
        fprintf(stderr, "Failed creating display thread: %s\n", strerror(thread_error));

    // Keyboard thread
    if ((thread_error = pthread_create(&keyboard_thread, NULL, keyboard, NULL)))
        fprintf(stderr, "Failed creating keyboard thread: %s\n", strerror(thread_error));

    // Lander thread
    if ((thread_error = pthread_create(&lander_thread, NULL, lander, argv[1])))
        fprintf(stderr, "Failed creating lander thread: %s\n", strerror(thread_error));

    // Dashboard thread
    if ((thread_error = pthread_create(&dashboard_thread, NULL, dashboard, argv[2])))
        fprintf(stderr, "Failed creating dashboard thread: %s\n", strerror(thread_error));

    // Data logging thread
    if ((thread_error = pthread_create(&data_logging_thread, NULL, datalogging, NULL)))
        fprintf(stderr, "Failed creating data logging thread: %s\n", strerror(thread_error));

    pthread_join(display_thread, NULL);
}