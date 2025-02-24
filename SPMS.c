#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

char COMMAND[10][100];

// Using linked list to store the booking information
typedef struct bookingInfo {
    int date; // YYYYMMDD
    int time; // HHMM
    float duration; // Duration (t.t hours)
    int member; // Member A to E (1 - 5)
    int priority; // Priority (1: Event, 2: Reservation, 3: Parking, 4: Essentials)
    int essentials; // Essentials reserved (Lockers, Umbrellas, Batteries, Cables, Valet parking, Inflation services) (1 = Reserved) (e.g. 110010 = Locker, Umbrella, and Valet parking reserved)
    bool fAccepted; // Accepted or not (First Come First Served)
    bool pAccepted; // Accepted or not (Priority)
    bool oAccepted; // Accepted or not (Optimized)
    struct bookingInfo *next;
} bookingInfo;

// Variables to handle the count of remaining essentials or parking spaces at corresponding time slot (Thinking)

void getInput() {
    char input[100];
    scanf(" %[^\n]", input); //get
    
    for (int i = 0; i < 10; i++) {
        COMMAND[i][0] = '\0';
    } //wipe
    
    int i = 0; //seperate by space
    char *token = strtok(input, " ");
    while (token != NULL && i < 10) {
        strcpy(COMMAND[i++], token);
        token = strtok(NULL, " ");
    }
}

bool checkArgCount(int count) {
    int i = 0;
    while (COMMAND[i][0] != '\0') i++;
    if (i == count) {
        return true;
    }
    printf("input error: wrong argument count for %s, expected %d, received %d\n", COMMAND[0], count-1, i-1);
    return false;
}

void addParking() {
    printf("addParking\n"); 
}

void addReservation() {
    printf("addReservation\n"); 
}

void bookEssentials() {
    printf("bookEssentials\n"); 
}

void addEvent() {
    printf("addEvent\n"); 
}

void importBatch() {
    printf("importBatch\n"); 
}

void printBookings() {
    printf("printBookings\n"); 
}

int main() {
    printf("~~ WELCOME TO PolyU ~~\n");
    while (true) { 
        
        printf("Please enter booking: "); 
        
        getInput();
        
        int fd[2], pid;

        if (pipe(fd) < 0) {
            printf("Pipe created error\n");
            exit(1);
        }
        pid = fork();
        if (pid < 0) {
            printf("Fork failed\n");
            exit(1);
        }
        else if (pid == 0) {
            close(fd[0]); // Close child in
            // Write
            if (strcmp(COMMAND[0], "endProgram") == 0) {
                write(fd[1], "end", 3); // Tell parent to end the program
            } else if (strcmp(COMMAND[0], "addParking") == 0) { 
                addParking();
            } else if (strcmp(COMMAND[0], "addReservation") == 0) { 
                addReservation();
            } else if (strcmp(COMMAND[0], "bookEssentials") == 0) { 
                bookEssentials();
            } else if (strcmp(COMMAND[0], "addEvent") == 0) { 
                addEvent();
            } else if (strcmp(COMMAND[0], "importBatch") == 0) { 
                importBatch();
            } else if (strcmp(COMMAND[0], "printBookings") == 0) { 
                printBookings();
            } else {
                printf("invalid, entered: %s\n", COMMAND[0]);
            }
            close(fd[1]); // Close child out
            exit(0);
        }
        else {
            close(fd[1]); // Close parent out
            // Read
            char buff[4];
            read(fd[0], buff, 4); // Read whether child needs to end the program
            if (strcmp(buff, "end") == 0) {
                break;
            }
            close(fd[0]); // Close parent in
            wait(NULL);
        }
    }
    
    printf("Bye!\n");
    return 0;
}