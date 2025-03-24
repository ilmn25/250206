// SPMS Group 30
// 23118174d WONG Ka Hei 
// 23118556d KWAN Kai Man 
// 23100195d CAO Wei 

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Using linked list to store the booking information
typedef struct bookingInfo {
    int time; // YYYYMMDDHH
    int duration; // hours, 1 = 1 hour
    int member; // Member A to E (1 - 5)
    int priority; // Priority (1: Event, 2: Reservation, 3: Parking, 4: Essentials)
    char essentials[7]; // Essentials reserved (Lockers, Umbrellas, Batteries, Cables, Valet parking, Inflation services, normal parking) (1 = Reserved) (e.g. 110010 = Locker, Umbrella, and Valet parking reserved)
    int fAccepted; // Accepted or not (First Come First Served)
    int pAccepted; // Accepted or not (Priority)
    int oAccepted; // Accepted or not (Optimized)
    struct bookingInfo *next;
} bookingInfo;

char COMMAND[10][100];
bookingInfo *head = NULL;
 
// handle - subroutine/ helper function not called directly by input handler
// get / is - helper function for logic

void insertIntoBookings(bookingInfo **head, bookingInfo *newBooking);
int getEssentialSum(char *essentials);
void handleSetEssentials(char *essentials, const char *command, bool includePaired);
int addDurationToTime(int time, int duration);
int isAvaliableEssential(int time, int duration, int target, int limit, int priority);
int isAvalibleFCFS(int time, int duration, char essentials[7]);
int isAvaliblePR(int time, int duration, char essentials[7], int priority);
bool isCorrectArgCount(int count);
bookingInfo* handleCreateBooking();
void setCommandFromString(char input[]);
void CreateBookingFromCommand(int fd);
void printBookings();
void handlePrintBooking(int member, int isAccepted, int acceptedType);
void importBatch(const char *filename, int fd); 

// Function to insert a booking into the sorted linked list
void insertIntoBookings(bookingInfo **head, bookingInfo *newBooking) 
{
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
int getEssentialSum(char *essentials)
{
    int i;
    int count = 0;
    for (i = 0; i < 6; i++) {
        if (essentials[i] == '1') {
            count++;
        }
    }
    return count;
}

// take string parameters in COMMAND[] and set the 1 and 0s 
void handleSetEssentials(char *essentials, const char *command, bool includePaired) 
{
    // printf("essential to add: %s\n", command);
    if (strstr(command, "normalpark") != NULL) {
        essentials[6] = '1';

    } else if (strstr(command, "locker") != NULL) {
        essentials[0] = '1';
        if (includePaired) essentials[1] = '1';
    }
    else if (strstr(command, "umbrella") != NULL) {
        essentials[1] = '1';
        if (includePaired) essentials[0] = '1';
    }
    else if (strstr(command, "battery") != NULL) {
        essentials[2] = '1';
        if (includePaired) essentials[3] = '1';
    }
    else if (strstr(command, "cable") != NULL) {
        essentials[3] = '1';
        if (includePaired) essentials[2] = '1';
    }
    else if (strstr(command, "valetpark") != NULL) {
        essentials[4] = '1';
        if (includePaired) essentials[5] = '1';
    }
    else if (strstr(command, "inflationservice") != NULL) {
        essentials[5] = '1';
        if (includePaired) essentials[4] = '1';
    }
}

// helper method, yyyymmddhh + hh (output acts like unique id for each hour)
// example: 2025010100 + 25 becomes 2025010201
int addDurationToTime(int time, int duration) 
{
    int hour = time % 100;
    int day = (time / 100) % 100;
    int month = (time / 10000) % 100;
    int year = time / 1000000;

    hour += duration;
    while (hour >= 24) {
        hour -= 24;
        day++;
    }

    int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        daysInMonth[2] = 29; // Leap year
    }

    while (day > daysInMonth[month]) {
        day -= daysInMonth[month];
        month++;
        if (month > 12) {
            month = 1;
            year++;
        }
    }

    return year * 1000000 + month * 10000 + day * 100 + hour;
}

// helper method to check if enough number of batteries etc for one more booking to be in the target time and duration 
int isAvaliableEssential(int time, int duration, int target, int limit, int priority) 
{ 
    bookingInfo *cur = head;
    bookingInfo *temp[100];
    int tempCount = 0;

    // scan entire booking list and save (into temp) bookings that are overlapping AND has targeted essential AND more priority
    while (cur != NULL) {
        int curEndTime = addDurationToTime(cur->time, cur->duration);
        int targetEndTime = addDurationToTime(time, duration);
        if (cur->essentials[target] == '1' && cur->priority <= priority &&
            ((time >= cur->time && time < curEndTime) || 
            (targetEndTime > cur->time && targetEndTime <= curEndTime) ||
            (time <= cur->time && targetEndTime >= curEndTime))) {
            // Add cur to temp list
            temp[tempCount++] = cur; 
        }
        cur = cur->next;
    }

    // i - iterate through each hour in target time period
    // j - iterate through each overlapping temp to see if more than [limit] overlaps (3 for essentials or 10 for parking)
    int i, j;
    for (i = 0; i < duration; i++) {
        int currentHour = addDurationToTime(time, i);
        int count = 0;
        for (j = 0; j < tempCount; j++) {
             
            int tempEndTime = addDurationToTime(temp[j]->time, temp[j]->duration);
            if (currentHour >= temp[j]->time && currentHour < tempEndTime) {
                count++; 
                if (count >= limit) return 0; // Overbooked
            }
        }
    }

    return 1; // available
}

int isAvalibleFCFS(int time, int duration, char essentials[7]) 
{ 
    if (essentials[6] == '1' && !isAvaliableEssential(time, duration, 6, 10, 5)) { // 5 for priority cuz no priority for fcfs
        return 0; // parking is overbooked
    }

    int i; //iterate through 6 essentials 
    for (i = 0; i < 6; i++) {
        if (essentials[i] == '1' && !isAvaliableEssential(time, duration, i, 3, 5)) {
            return 0; // Essential item is overbooked
        }
    }

    return 1; // Time slot is available
} 
int isAvaliblePR(int time, int duration, char essentials[7], int priority) 
{
    if (essentials[6] == '1' && !isAvaliableEssential(time, duration, 6, 10, priority)) {
        return 0; // parking is overbooked
    }

    int i; //iterate through 6 essentials 
    for (i = 0; i < 6; i++) {
        if (essentials[i] == '1' && !isAvaliableEssential(time, duration, i, 3, priority)) {
            return 0; // Essential item is overbooked
        }
    }

    // Step 1: Scan the booking list and save overlapping bookings with lower priority
    bookingInfo *cur = head;
    bookingInfo *temp[100];
    int tempCount = 0;
    int newEndTime = addDurationToTime(time, duration);

    while (cur != NULL) {
        int curEndTime = addDurationToTime(cur->time, cur->duration);
        if (cur->priority > priority && 
            ((time >= cur->time && time < curEndTime) || 
             (newEndTime > cur->time && newEndTime <= curEndTime) || 
             (time <= cur->time && newEndTime >= curEndTime))) {
            for (i = 0; i < 7; i++) {
                if (essentials[i] == '1' && cur->essentials[i] == '1') {
                    temp[tempCount++] = cur;
                    break;
                }
            }
        }
        cur = cur->next;
    }

    // Step 2: Set pAccepted flag to 0 for overlapping bookings until no more than 3 overlaps remain (10 for parking)
    for (i = 0; i < duration; i++) {
        int currentHour = addDurationToTime(time, i);
        int count[7] = {0}; // Array to count overlaps for each essential
        for (int j = 0; j < tempCount; j++) {
            if (temp[j] == NULL) continue;
            int tempEndTime = addDurationToTime(temp[j]->time, temp[j]->duration);
            if (currentHour >= temp[j]->time && currentHour < tempEndTime) {
                for (int k = 0; k < 7; k++) {
                    if (essentials[k] == '1' && temp[j]->essentials[k] == '1') {
                        count[k]++;
                        int limit = (k == 6) ? 10 : 3; // Set limit based on essential type
                        if (count[k] >= limit) {
                            // Set pAccepted flag to 0
                            temp[j]->pAccepted = 0;
                            temp[j] = NULL; // Mark as processed
                            break;
                        }
                    }
                }
            }
        }
    }

    return 1; // Time slot is available
}

bookingInfo* handleCreateBooking() 
{ 
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
    strcpy(newBooking->essentials, "0000000"); 
    if (strcmp(COMMAND[0], "addEvent") == 0) {
        newBooking->priority = 1;
        handleSetEssentials(newBooking->essentials, "normalpark", false);

        handleSetEssentials(newBooking->essentials, COMMAND[5], false);
        handleSetEssentials(newBooking->essentials, COMMAND[6], false);
        handleSetEssentials(newBooking->essentials, COMMAND[7], false); 
        // printf("Essentials: %s\n", newBooking->essentials);
        if (getEssentialSum(newBooking->essentials) != 3) {
            printf("Invalid essentials format: need exactly 3 essentials to be booked\n");
            free(newBooking);
            return NULL;
        }

    } else if (strcmp(COMMAND[0], "addReservation") == 0) {
        newBooking->priority = 2;
        handleSetEssentials(newBooking->essentials, "normalpark", false);

        handleSetEssentials(newBooking->essentials, COMMAND[5], true);
        handleSetEssentials(newBooking->essentials, COMMAND[6], true);
        if (getEssentialSum(newBooking->essentials) != 2) {
            printf("Invalid essentials format: need exactly 2 essentials which must belong in a pair\n");
            free(newBooking);
            return NULL;
        }

    } else if (strcmp(COMMAND[0], "addParking") == 0) {
        newBooking->priority = 3;
        handleSetEssentials(newBooking->essentials, "normalpark", false);

        handleSetEssentials(newBooking->essentials, COMMAND[5], true);
        handleSetEssentials(newBooking->essentials, COMMAND[6], true);
        if (getEssentialSum(newBooking->essentials) != 2 && getEssentialSum(newBooking->essentials) != 0) {
            printf("Invalid essentials format: need either none or 2 essential which belongs in a pair\n");
            free(newBooking);
            return NULL;
        }

    } else if (strcmp(COMMAND[0], "bookEssentials") == 0) {
        newBooking->priority = 4;

        handleSetEssentials(newBooking->essentials, COMMAND[5], false);
        if (getEssentialSum(newBooking->essentials) != 1) {
            printf("Invalid essentials format: need at exactly 1 valid essential only\n");
            free(newBooking);
            return NULL;
        }

    } else {
        printf("Invalid command\n");
        free(newBooking);
        return NULL;
    } 
    newBooking->fAccepted = isAvalibleFCFS(newBooking->time, newBooking->duration, newBooking->essentials); 
    newBooking->pAccepted = isAvaliblePR(newBooking->time, newBooking->duration, newBooking->essentials, newBooking->priority); 
    newBooking->oAccepted = 0;   
    newBooking->next = NULL;
    return newBooking;
}
 

// take string and split it up by " " and put in COMMAND[,] array. Used after getinput and in loadbatch
void setCommandFromString(char input[]) 
{
    int i;
    for (i = 0; i < 10; i++) {
        COMMAND[i][0] = '\0';
    } //wipe
    
    i = 0; //separate by space
    char *token = strtok(input, " ");
    while (token != NULL && i < 10) {
        strcpy(COMMAND[i++], token);
        token = strtok(NULL, " ");
    }
}

void CreateBookingFromCommand(int fd) 
{
    bookingInfo *newBooking = handleCreateBooking();
    if (newBooking != NULL) {
        char buff[100]; // Declare a large enough buffer to write the booking information
        sprintf(buff, "done %d %d %d %d %s %d %d %d",
                newBooking->time, newBooking->duration, newBooking->member,
                newBooking->priority, newBooking->essentials, newBooking->fAccepted,
                newBooking->pAccepted, newBooking->oAccepted); 
        write(fd, buff, sizeof(buff)); // Send booking info to parent

        insertIntoBookings(&head, newBooking); // Update the current linked list of child for batch implementation
        
        // free(newBooking); // Free the booking
    } else {
        write(fd, "fail", 4); // Tell parent that booking failed
    }
}

// importBatch -test_data_G30.dat 
void importBatch(const char *filename, int fd) 
{
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
        CreateBookingFromCommand(fd);
        // printf("= %s %s %s %s\n", COMMAND[0], COMMAND[1], COMMAND[2], COMMAND[3]);
    }

    fclose(file);
}

void handlePrintBooking(int member, int isAccepted, int acceptedType) 
{
    bookingInfo *cur = head;
    printf("===============================================\n");
    printf("Member_%c has the following bookings:\n", 'A' + member - 1);
    printf("Date       Start End   Type         Needs\n");
    printf("-------------------------------------------\n");
    while (cur != NULL) {
        int accepted = (acceptedType == 1) ? cur->fAccepted : cur->pAccepted;
        if (cur->member == member && accepted == isAccepted) {
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

void printBookings() 
{
    int member;
    printf("*** Parking Booking - ACCEPTED / FCFS ***\n\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 1, 1); // fAccepted
    }
    printf("- End -\n");
    printf("===========================================================================\n");
    printf("*** Parking Booking - REJECTED / FCFS ***\n\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 0, 1); // fAccepted
    }
    printf("- End -\n");

    printf("===========================================================================\n");
    printf("*** Parking Booking - ACCEPTED / PRIORITY ***\n\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 1, 2); // pAccepted
    }
    printf("- End -\n");
    printf("===========================================================================\n");
    printf("*** Parking Booking - REJECTED / PRIORITY ***\n\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 0, 2); // pAccepted
    }
    printf("- End -\n");
}

int main() {
    printf("~~ WELCOME TO PolyU ~~\n");
    while (true) { 
        printf("====================================\n");
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
            } else if (strcmp(COMMAND[0], "printBookings") == 0) { 
                write(fd[1], "print", 5); // Tell parent to print bookings
            } else if (
            strcmp(COMMAND[0], "addEvent") == 0 ||
            strcmp(COMMAND[0], "addReservation") == 0 ||
            strcmp(COMMAND[0], "addParking") == 0 ||
            strcmp(COMMAND[0], "bookEssentials") == 0 ) {
                CreateBookingFromCommand(fd[1]);
            } else {
                printf("invalid command, must be one of the following:\n");
                printf("endProgram, importBatch, printBookings, addEvent, addParking, addParking, bookEssentials");
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
                    sscanf(buff + 5, "%d %d %d %d %7s %d %d %d",
                        &newBooking->time, &newBooking->duration, &newBooking->member,
                        &newBooking->priority, newBooking->essentials, 
                        &newBooking->fAccepted, &newBooking->pAccepted, &newBooking->oAccepted);
                    newBooking->next = NULL;
                    insertIntoBookings(&head, newBooking);

                    // DEBUG
                    printf("--------------------------\n");
                    printf("New Booking:\n");
                    printf("Time: %d\n", newBooking->time);
                    printf("Duration: %d\n", newBooking->duration);
                    printf("Member: %d\n", newBooking->member);
                    printf("Priority: %d\n", newBooking->priority);
                    printf("Essentials: %s\n", newBooking->essentials);
                    printf("fAccepted: %d\n", newBooking->fAccepted);
                    printf("pAccepted: %d\n", newBooking->pAccepted); 
                    // printf("oAccepted: %d\n", newBooking->oAccepted); 
                    
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
bool isCorrectArgCount(int count) {
    int i = 0;
    while (COMMAND[i][0] != '\0') i++;
    if (i == count) {
        return true;
    }
    printf("input error: wrong argument count for %s, expected %d, received %d\n", COMMAND[0], count-1, i-1);
    return false;
}