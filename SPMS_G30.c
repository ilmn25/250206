#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

char COMMAND[10][100];

int i;

// Using linked list to store the booking information
typedef struct bookingInfo {
    int time; // YYYYMMDDHH
    int duration; // hour
    int member; // Member A to E (1 - 5)
    int priority; // Priority (1: Event, 2: Reservation, 3: Parking, 4: Essentials)
    char essentials[6]; // Essentials reserved (Lockers, Umbrellas, Batteries, Cables, Valet parking, Inflation services) (1 = Reserved) (e.g. 110010 = Locker, Umbrella, and Valet parking reserved)
    int fAccepted; // Accepted or not (First Come First Served)
    int pAccepted; // Accepted or not (Priority)
    int oAccepted; // Accepted or not (Optimized)
    struct bookingInfo *next;
} bookingInfo;

bookingInfo *head = NULL;

// Function to insert a booking into the sorted linked list
void insertIntoSortedList(bookingInfo **head, bookingInfo *newBooking) {
    bookingInfo *cur = *head;
    bookingInfo *pre = NULL;

    while (cur != NULL) {
        if (cur->time > newBooking->time) {
            if (pre == NULL) {
                newBooking->next = *head;
                *head = newBooking;
                break;
            } else {
                newBooking->next = cur;
                pre->next = newBooking;
                break;
            }
        }
        pre = cur;
        cur = cur->next;
    }

    if (cur == NULL) {
        if (pre == NULL) {
            *head = newBooking;
        } else {
            pre->next = newBooking;
        }
        newBooking->next = NULL;
    }
}

// return 3 for 000111, 2 for 010100, etc
int getEssentialCount(char *essentials)
{
    int count = 0;
    for (i = 0; i < 6; i++) {
        if (essentials[i] == '1') {
            count++;
        }
    }
    return count;
}

// helper method for setting essentials
void setEssential(char *essentials, const char *command, bool includePaired) {
    if (strstr(command, "locker") != NULL) {
        essentials[0] = '1';
        if (includePaired) essentials[1] = '1';
    }
    if (strstr(command, "umbrella") != NULL) {
        essentials[1] = '1';
        if (includePaired) essentials[0] = '1';
    }
    if (strstr(command, "battery") != NULL) {
        essentials[2] = '1';
        if (includePaired) essentials[3] = '1';
    }
    if (strstr(command, "cable") != NULL) {
        essentials[3] = '1';
        if (includePaired) essentials[2] = '1';
    }
    if (strstr(command, "valetpark") != NULL) {
        essentials[4] = '1';
        if (includePaired) essentials[5] = '1';
    }
    if (strstr(command, "inflationservice") != NULL) {
        essentials[5] = '1';
        if (includePaired) essentials[4] = '1';
    }
}

// NOT WORKING FOR BATCH YET, BECAUSE THIS CHECK IS CALLED BEFORE PREVIOUS BOOKING GETS LINKED
int isFCFS(int time, int duration) {
    bookingInfo *cur = head;
        printf("b date format\n");
    while (cur != NULL) {
        int curEndTime = cur->time + cur->duration * 100;
        int targetEndTime = time + duration * 100;
        if ((time >= cur->time && time < curEndTime) || 
            (targetEndTime > cur->time && targetEndTime <= curEndTime) ||
            (time <= cur->time && targetEndTime >= curEndTime)) {
        printf("b date format\n");
            return 0; 
        }
        cur = cur->next;
    }
        printf("a date format\n");
    return 1; 
    
}

bookingInfo* createBooking() { 
    bookingInfo *newBooking = (bookingInfo *)malloc(sizeof(bookingInfo));
    
    // Convert the member to integer
    if (strcmp(COMMAND[1], "-member_A") == 0) {
        newBooking->member = 1;
    } else if (strcmp(COMMAND[1], "-member_B") == 0) {
        newBooking->member = 2;
    } else if (strcmp(COMMAND[1], "-member_C") == 0) {
        newBooking->member = 3;
    } else if (strcmp(COMMAND[1], "-member_D") == 0) {
        newBooking->member = 4;
    } else if (strcmp(COMMAND[1], "-member_E") == 0) {
        newBooking->member = 5;
    } else {
        printf("Invalid member\n");
        free(newBooking);
        return NULL;
    }

    // Convert the date and time to integer
    int year, month, day, hour;
    if (sscanf(COMMAND[2], "%d-%d-%d", &year, &month, &day) != 3) {
        printf("Invalid date format\n");
        free(newBooking);
        return NULL;
    }
    if (sscanf(COMMAND[3], "%d:00", &hour) != 1) { // Assuming time is given in HH:00 format
        printf("Invalid time format\n");
        free(newBooking);
        return NULL;
    }
    newBooking->time = year * 1000000 + month * 10000 + day * 100 + hour;

    // Store the duration
    newBooking->duration = atoi(COMMAND[4]);
 
    // Convert priority and essentials to integer
    strcpy(newBooking->essentials, "000000"); 
    if (strcmp(COMMAND[0], "addEvent") == 0) {
        newBooking->priority = 1;

        setEssential(newBooking->essentials, COMMAND[5], false);
        setEssential(newBooking->essentials, COMMAND[6], false);
        setEssential(newBooking->essentials, COMMAND[7], false); 
        if (getEssentialCount(newBooking->essentials) != 3) {
            printf("Invalid essentials format: need exactly 3 essentials to be booked\n");
            free(newBooking);
            return NULL;
        }

    } else if (strcmp(COMMAND[0], "addReservation") == 0) {
        newBooking->priority = 2;

        setEssential(newBooking->essentials, COMMAND[5], true);
        setEssential(newBooking->essentials, COMMAND[6], true);
        if (getEssentialCount(newBooking->essentials) != 2) {
            printf("Invalid essentials format: need exactly 2 essentials which belongs in a pair\n");
            free(newBooking);
            return NULL;
        }

    } else if (strcmp(COMMAND[0], "addParking") == 0) {
        newBooking->priority = 3;

        setEssential(newBooking->essentials, COMMAND[5], true);
        setEssential(newBooking->essentials, COMMAND[6], true);
        if (getEssentialCount(newBooking->essentials) != 2 && getEssentialCount(newBooking->essentials) != 0) {
            printf("Invalid essentials format: need either none or 2 essential which belongs in a pair\n");
            free(newBooking);
            return NULL;
        }

    } else if (strcmp(COMMAND[0], "bookEssentials") == 0) {
        newBooking->priority = 4;

        setEssential(newBooking->essentials, COMMAND[5], false);
        if (getEssentialCount(newBooking->essentials) != 1) {
            printf("Invalid essentials format: need at exactly 1 valid essential only\n");
            free(newBooking);
            return NULL;
        }

    } else {
        printf("Invalid command\n");
        free(newBooking);
        return NULL;
    } 
    newBooking->fAccepted = isFCFS(newBooking->time, newBooking->duration); 
    newBooking->pAccepted = 0; 
    newBooking->oAccepted = 0;   
    newBooking->next = NULL;
    return newBooking;
}
 

// take string and split it up by " " and put in COMMAND[,] array. Used after getinput and in loadbatch
void setCommandFromString(char input[]) {
    for (i = 0; i < 10; i++) {
        COMMAND[i][0] = '\0';
    } //wipe
    
    i = 0; //seperate by space
    char *token = strtok(input, " ");
    while (token != NULL && i < 10) {
        if (i == 5) {
            // Combine all remaining tokens into COMMAND[5] (essentials)
            strcpy(COMMAND[i], token);
            while ((token = strtok(NULL, " ")) != NULL) {
                strcat(COMMAND[i], " ");
                strcat(COMMAND[i], token);
            }
            break;
        } else {
            strcpy(COMMAND[i++], token);
            token = strtok(NULL, " ");
        }
    }
}


void addBookingFromCommand(int fd) {
    bookingInfo *newBooking = createBooking(); // (Thinking) May need further distinguished (Thinking)
    if (newBooking != NULL) {
        char buff[100]; // Declare a large enough buffer to write the booking information
        sprintf(buff, "done %d %d %d %d %s %d %d %d",
                newBooking->time, newBooking->duration, newBooking->member,
                newBooking->priority, newBooking->essentials, newBooking->fAccepted,
                newBooking->pAccepted, newBooking->oAccepted); 
        write(fd, buff, sizeof(buff)); // Send booking info to parent
        free(newBooking); // Free the booking
    } else {
        write(fd, "fail", 4); // Tell parent that booking failed
    }
}
 

void importBatch(const char *filename, int fd) {
    FILE *file = fopen(filename + 1, "r"); // Ignore the first character '-'
    if (file == NULL) {
        printf("Failed to open batch file: %s\n", filename);
        return;
    }

    char line[100]; // Buffer to store each line of the file
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0'; // Convert the newline character to null character

        // Seperate the line into tokens and store them in COMMAND array
        setCommandFromString(line);
        // Process command
        addBookingFromCommand(fd);
    }

    fclose(file);
}
void printBooking(int member, int isAccepted) {
    bookingInfo *cur = head;
    printf("Member_%c has the following bookings:\n", 'A' + member - 1);
    printf("Date       Start End   Type         Device\n");
    printf("===========================================================================\n");
    while (cur != NULL) {
        if (cur->member == member && cur->fAccepted == isAccepted) {
            int year = cur->time / 1000000;
            int month = (cur->time / 10000) % 100;
            int day = (cur->time / 100) % 100;
            int startHour = cur->time % 100;
            int endHour = startHour + cur->duration;

            printf("%04d-%02d-%02d %02d:00 %02d:00 %-12s %s\n",
                    year, month, day,
                    startHour, endHour,
                    cur->priority == 1 ? "Event" : cur->priority == 2 ? "Reservation" : cur->priority == 3 ? "Parking" : "Essentials",
                    cur->essentials);
        }
        cur = cur->next;
    }
    printf("\n");
}

// (Tentative) Code for viewing the booking information (Tentative)
void printBookings() {
    int member;
    printf("*** Parking Booking - ACCEPTED / FCFS ***\n\n");
    for (member = 1; member <= 5; member++) { 
        printBooking(member, 1);
    }
    printf("- End -\n");
    printf("===========================================================================\n");
    printf("*** Parking Booking - REJECTED / FCFS ***\n\n");
    for (member = 1; member <= 5; member++) { 
        printBooking(member, 0);
    }
    printf("- End -\n");
}
int main() {
    printf("~~ WELCOME TO PolyU ~~\n");
    while (true) { 
        
        printf("Please enter booking: "); 
        
        // get input (typed in by user)
        char line[100];
        scanf(" %[^\n]", line);
        setCommandFromString(line);
        
        printf("\n");
         
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
            }  else if (strcmp(COMMAND[0], "importBatch") == 0) { 
                importBatch(COMMAND[1], fd[1]);
                // (Command) importBatch -test_data_G30.dat (Command)
            } else if (strcmp(COMMAND[0], "printBookings") == 0) { 
                write(fd[1], "print", 5); // Tell parent to print bookings
            } else {
                // Directly process command in add bookings, if command is random like "awdawd" will report invalid inside
                addBookingFromCommand(fd[1]);
            }
            close(fd[1]); // Close child out
            exit(0);
        }
        else {
            close(fd[1]); // Close parent out
            // Read
            char buff[100]; // Declare a large enough buffer to read the booking information
            while (read(fd[0], buff, sizeof(buff)) > 0) {
                if (strncmp(buff, "end", 3) == 0) {
                    break;
                } else if (strncmp(buff, "done", 4) == 0) {
                    bookingInfo *newBooking = (bookingInfo *)malloc(sizeof(bookingInfo));
                    sscanf(buff + 5, "%d %d %d %d %6s %d %d %d",
                        &newBooking->time, &newBooking->duration, &newBooking->member,
                        &newBooking->priority, newBooking->essentials, 
                        &newBooking->fAccepted, &newBooking->pAccepted, &newBooking->oAccepted);
                    newBooking->next = NULL;
                    insertIntoSortedList(&head, newBooking);

                    // DEBUG Print the details of the new booking
                    printf("\nNew Booking:\n");
                    printf("Time: %d\n", newBooking->time);
                    printf("Duration: %d\n", newBooking->duration);
                    printf("Member: %d\n", newBooking->member);
                    printf("Priority: %d\n", newBooking->priority);
                    printf("Essentials: %s\n", newBooking->essentials);
                    printf("fAccepted: %d\n", newBooking->fAccepted);
                    printf("pAccepted: %d\n", newBooking->pAccepted);
                    printf("oAccepted: %d\n", newBooking->oAccepted);
                    
                } else if (strncmp(buff, "print", 5) == 0) {
                    printBookings();
                } else if (strncmp(buff, "fail", 4) == 0) {
                    printf("Operation failed\n");
                }
            }
            if (strncmp(buff, "end", 3) == 0) {
                break;
            }
            close(fd[0]); // Close parent in
            wait(NULL);
        }
    }
    
    printf("Bye!\n");
    return 0;
}

// arg count validation in command array
bool checkArgCount(int count) {
    i = 0;
    while (COMMAND[i][0] != '\0') i++;
    if (i == count) {
        return true;
    }
    printf("input error: wrong argument count for %s, expected %d, received %d\n", COMMAND[0], count-1, i-1);
    return false;
}