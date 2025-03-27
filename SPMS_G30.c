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
    bool essentials[7]; // Essentials reserved (Lockers, Umbrellas, Batteries, Cables, Valet parking, Inflation services, normal parking) (1 = Reserved) (e.g. 110010 = Locker, Umbrella, and Valet parking reserved)
    bool fAccepted; // Accepted or not (First Come First Served)
    bool pAccepted; // Accepted or not (Priority)
    bool oAccepted; // Accepted or not (Optimized)
    struct bookingInfo *next;
} bookingInfo;

char essentialsName[6][18] = {"Locker", "Umbrella", "Battery", "Cable", "Valet parking", "Inflation service"};

int invalidCount = 0; // Count of invalid request

char COMMAND[10][100];
bookingInfo *head = NULL;
 
// handle - subroutine/ helper function not called directly by input handler
// get / is - helper function for logic

void insertIntoBookings(bookingInfo **head, bookingInfo *newBooking);
int getEssentialSum(bool *essentials);
void handleSetEssentials(bool *essentials, const char *command, bool includePaired);
int addDurationToTime(int time, int duration);
bool isMorePriorityThan(bookingInfo *bookingA, bookingInfo *bookingB);
bool isAvailableEssential(bookingInfo *targetBooking, int target, int limit, bool checkPR);
bool EvictEssential(bookingInfo *targetBooking);
bool isAvailableFCFS(bookingInfo *targetBooking);
bool isAvailablePR(bookingInfo *targetBooking);
bool isCorrectArgCount(int count);
bookingInfo *handleCreateBooking();
void setCommandFromString(char input[]);
void CreateBookingFromCommand(int fd);
void printFCFS();
void printPR();
void printOPT();
void handlePrintBooking(int member, bool isAccepted, int acceptedType);
void addBatch(const char *filename, int fd); 

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
int getEssentialSum(bool *essentials)
{
    int i;
    int count = 0;
    for (i = 0; i < 6; i++) {
        if (essentials[i]) {
            count++;
        }
    }
    return count;
}

// take string parameters in COMMAND[] and set the 1 and 0s 
void handleSetEssentials(bool *essentials, const char *command, bool includePaired) 
{
    // printf("essential to add: %s\n", command);
    if (strstr(command, "normalpark") != NULL) {
        essentials[6] = true;

    } else if (strstr(command, "locker") != NULL) {
        essentials[0] = true;
        if (includePaired) essentials[1] = true;
    }
    else if (strstr(command, "umbrella") != NULL) {
        essentials[1] = true;
        if (includePaired) essentials[0] = true;
    }
    else if (strstr(command, "battery") != NULL) {
        essentials[2] = true;
        if (includePaired) essentials[3] = true;
    }
    else if (strstr(command, "cable") != NULL) {
        essentials[3] = true;
        if (includePaired) essentials[2] = true;
    }
    else if (strstr(command, "valetpark") != NULL) {
        essentials[4] = true;
        if (includePaired) essentials[5] = true;
    }
    else if (strstr(command, "inflationservice") != NULL) {
        essentials[5] = true;
        if (includePaired) essentials[4] = true;
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

bool isMorePriorityThan(bookingInfo *bookingA, bookingInfo *bookingB) {
    if  (bookingA->priority != bookingB->priority) {
        return bookingA->priority > bookingB->priority;
    }

    // extract start and end times for both bookings
    int startA = bookingA->time % 10000; // Extract HHMM from YYYYMMDDHHMM
    int endA = addDurationToTime(bookingA->time, bookingA->duration) % 10000;
    int startB = bookingB->time % 10000;
    int endB = addDurationToTime(bookingB->time, bookingB->duration) % 10000;

    bool aInPriorityWindow = (startA < 2000 && endA > 800);
    bool bInPriorityWindow = (startB < 2000 && endB > 800);

    // if both in or both out then no difference aka not higher
    if (aInPriorityWindow == bInPriorityWindow) {
        return false;
    } 
    return aInPriorityWindow; 
}

// helper method to check if enough number of batteries etc for one more booking to be in the target time and duration 
bool isAvailableEssential(bookingInfo *targetBooking, int target, int limit, bool checkPR) 
{ 
    bookingInfo *cur = head;
    bookingInfo *temp[100];
    int tempCount = 0;

    int targetEndTime = addDurationToTime(targetBooking->time, targetBooking->duration);
    // scan entire booking list and save (into temp) bookings that are overlapping AND has targeted essential AND more priority
    while (cur != NULL && targetEndTime < cur-> time) {// because linked list is sorted by time, performant
        int curEndTime = addDurationToTime(cur->time, cur->duration); 
        if (cur->essentials[target] && (!checkPR || isMorePriorityThan(cur, targetBooking)) &&
            ((targetBooking->time >= cur->time && targetBooking->time < curEndTime) || 
            (targetEndTime > cur->time && targetEndTime <= curEndTime) ||
            (targetBooking->time <= cur->time && targetEndTime >= curEndTime))) {
            // Add cur to temp list
            temp[tempCount++] = cur; 
        }
        cur = cur->next;
    }

    // i - iterate through each hour in target time period
    // j - iterate through each overlapping temp to see if more than [limit] overlaps (3 for essentials or 10 for parking)
    int i, j;
    for (i = 0; i < targetBooking->duration; i++) {
        int currentHour = addDurationToTime(targetBooking->time, i);
        int count = 0;
        for (j = 0; j < tempCount; j++) {
             
            int tempEndTime = addDurationToTime(temp[j]->time, temp[j]->duration);
            if (currentHour >= temp[j]->time && currentHour < tempEndTime) {
                count++; 
                if (count >= limit) return false; // Overbooked
            }
        }
    }

    return true; // available
}

bool EvictEssential(bookingInfo *targetBooking)  
{
    int i; //iterate  
    // CANCEL BOOKINGS OVERWRITTEN BECAUSE LESS PRIORITY CODE  
    bookingInfo *cur = head;
    bookingInfo *temp[100];
    int tempCount = 0;
    int newEndTime = addDurationToTime(targetBooking->time, targetBooking->duration);

    // 1- Scan the booking list and save overlapping bookings with lower priority
    while (cur != NULL) {
        int curEndTime = addDurationToTime(cur->time, cur->duration);
        if (isMorePriorityThan(targetBooking, cur) && 
            ((targetBooking->time >= cur->time && targetBooking->time < curEndTime) || 
             (newEndTime > cur->time && newEndTime <= curEndTime) || 
             (targetBooking->time <= cur->time && newEndTime >= curEndTime))) {
            for (i = 0; i < 7; i++) {
                if (targetBooking->essentials[i] && cur->essentials[i]) {
                    temp[tempCount++] = cur;
                    break;
                }
            }
        }
        cur = cur->next;
    } 
    // 2- go through the list and cancel the booking with least priority, and repeat until enough no overbooking
} 

bool isAvailableFCFS(bookingInfo *targetBooking) 
{ 
    if (targetBooking->essentials[6] && !isAvaliableEssential(targetBooking, 6, 10, false)) { // 5 for priority cuz no priority for fcfs
        return false; // parking is overbooked
    }

    int i; //iterate through 6 essentials 
    for (i = 0; i < 6; i++) {
        if (targetBooking->essentials[i] && !isAvaliableEssential(targetBooking, i, 3, false)) {
            return false; // Essential item is overbooked
        }
    }

    return true; // Time slot is available
} 
bool isAvailablePR(bookingInfo *targetBooking) 
{
    if (targetBooking->essentials[6] && !isAvaliableEssential(targetBooking, 6, 10, true)) {
        return false; // parking is overbooked
    }

    int i; //iterate through 6 essentials 
    for (i = 0; i < 6; i++) {
        if (targetBooking->essentials[i] && !isAvaliableEssential(targetBooking, i, 3, true)) {
            return false; // Essential item is overbooked
        }
    }
    EvictEssential(targetBooking);
    return true; // Time slot is available
}

bookingInfo *handleCreateBooking() 
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
 
    int i;
    // Convert priority and essentials to integer
    for (i = 0; i < 7; i++) {
        newBooking->essentials[i] = false;
    }
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
    newBooking->fAccepted = isAvailableFCFS(newBooking); 
    newBooking->pAccepted = isAvailablePR(newBooking); 
    newBooking->oAccepted = false;   
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

char *convertEssentialsToString(bool *essentials) {
    char *essentialsString = malloc(8); // 7 char and 1 '\0'

    for (int i = 0; i < 7; i++) {
        essentialsString[i] = essentials[i] ? '1' : '0';
    }
    essentialsString[7] = '\0';

    return essentialsString;
}

char *convertAcceptedToString(bool fAccepted, bool pAccepted, bool oAccepted) {
    char *acceptedString = malloc(4); // 3 char and 1 '\0'

    if (fAccepted) {
        acceptedString[0] = '1';
    }
    else {
        acceptedString[0] = '0';
    }

    if (pAccepted) {
        acceptedString[1] = '1';
    }
    else {
        acceptedString[1] = '0';
    }

    if (oAccepted) {
        acceptedString[2] = '1';
    }
    else {
        acceptedString[2] = '0';
    }

    acceptedString[3] = '\0';

    return acceptedString;
}

void CreateBookingFromCommand(int fd) 
{
    bookingInfo *newBooking = handleCreateBooking();
    if (newBooking != NULL) {
        char buff[100]; // Declare a large enough buffer to write the booking information

        // Convert bool array to string for writing to parent
        char *essentialsString = convertEssentialsToString(newBooking->essentials);
        char *acceptedString = convertAcceptedToString(newBooking->fAccepted, newBooking->pAccepted, newBooking->oAccepted);

        sprintf(buff, "done %d %d %d %d %s %s",
                newBooking->time, newBooking->duration, newBooking->member,
                newBooking->priority, essentialsString, acceptedString); 
        write(fd, buff, sizeof(buff)); // Send booking info to parent

        free(essentialsString);
        free(acceptedString);

        insertIntoBookings(&head, newBooking); // Update the current linked list of child for batch implementation
        
        // free(newBooking); // Free the booking CAUSES BUG WHERE OLD BOOKING FORGOTTEN BY INSERTINTOBOOKINGS
    } else {
        write(fd, "fail", 4); // Tell parent that booking failed
    }
}

// addBatch -test_data_G30.dat 
void addBatch(const char *filename, int fd) 
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

int rejectedCount(int member, int acceptedType)
{
    int count = 0;
    bookingInfo *cur = head;

    bool rejected;
    while (cur != NULL) {
        switch (acceptedType) {
            case 1:
                rejected = !cur->fAccepted;
                break;
            case 2:
                rejected = !cur->pAccepted;
                break;
            case 3:
                rejected = !cur->oAccepted;
                break;
        }
        if (cur->member == member && rejected) {
            count++;
        }
        cur = cur->next;
    }

    return count;
}

void handlePrintBooking(int member, bool isAccepted, int acceptedType) 
{
    bookingInfo *cur = head;
    bool accepted;
    if (isAccepted) {
        printf("\nMember_%c has the following bookings:\n", 'A' + member - 1);
        printf("\nDate\t\tStart\tEnd\tType\t\tDevice");
    }
    else {
        printf("\nMember_%c (there are %d bookings rejected):\n", 'A' + member - 1, rejectedCount(member, acceptedType));
        printf("\nDate\t\tStart\tEnd\tType\t\tEssentials");
    }
    printf("\n===========================================================================\n");
    while (cur != NULL) { 
        switch (acceptedType) {
            case 1:
                accepted = cur->fAccepted;
                break;
            case 2:
                accepted = cur->pAccepted;
                break;
            case 3:
                accepted = cur->oAccepted;
                break;
        }

        if (cur->member == member && accepted == isAccepted) {
            int year = cur->time / 1000000;
            int month = (cur->time / 10000) % 100;
            int day = (cur->time / 100) % 100;
            int startHour = cur->time % 100;
            int endHour = startHour + cur->duration;

            bool noEssential = true;
            int i;
            for (i = 0; i < 6; i++) { // Check the first 6 essentials (excluding the normal parking)
                if (cur->essentials[i]) {
                    noEssential = false;
                    break;
                }
            }
            if (noEssential) {
                printf("%04d-%02d-%02d\t%02d:00\t%02d:00\t%-12s\t%c\n",
                    year, month, day,
                    startHour, endHour,
                    cur->priority == 1 ? "Event" : cur->priority == 2 ? "Reservation" : cur->priority == 3 ? "Parking" : "*",
                    '*');
            }
            else {
                bool firstEssential = true; // First essential for current booking
                printf("%04d-%02d-%02d\t%02d:00\t%02d:00\t%-12s\t",
                    year, month, day,
                    startHour, endHour,
                    cur->priority == 1 ? "Event" : cur->priority == 2 ? "Reservation" : cur->priority == 3 ? "Parking" : "*");
                for (i = 0; i < 6; i++) {
                    if (cur->essentials[i]) {
                        if (firstEssential) {
                            printf("%s\n", essentialsName[i]);
                            firstEssential = false;
                        }
                        else {
                            printf("\t\t\t\t\t\t%s\n", essentialsName[i]);
                        }
                    }
                }
            }
        }
        cur = cur->next;
    }
    printf("\n");
}

void printFCFS() 
{
    int member;
    printf("*** Parking Booking - ACCEPTED / FCFS ***\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 1, 1); // member, isAccept, 1 = fcfs 2 = pr
    }
    printf("\t- End -\n");
    printf("\n===========================================================================\n");
    printf("*** Parking Booking - REJECTED / FCFS ***\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 0, 1);  
    }
    printf("\t- End -\n");
    printf("\n===========================================================================\n");
}

void printPR() 
{
    int member; 
    printf("*** Parking Booking - ACCEPTED / PRIORITY ***\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 1, 2); 
    }
    printf("\t- End -\n");
    printf("\n===========================================================================\n");
    printf("*** Parking Booking - REJECTED / PRIORITY ***\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 0, 2);  
    }
    printf("\t- End -\n");
    printf("\n===========================================================================\n");
}

void printOPT()  
{
    int member; 
    printf("*** Parking Booking - ACCEPTED / OPTIMIZED ***\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 1, 3); 
    }
    printf("\t- End -\n");
    printf("\n===========================================================================\n");
    printf("*** Parking Booking - REJECTED / OPTIMIZED ***\n");
    for (member = 1; member <= 5; member++) { 
        handlePrintBooking(member, 0, 3);  
    }
    printf("\t- End -\n");
    printf("\n===========================================================================\n");
}

int totalReceivedCount()
{
    int count = 0;
    bookingInfo *cur = head;

    while (cur != NULL) {
        count++;
        cur = cur->next;
    }

    return count;
}

int totalAcceptedCount(int acceptedType)
{
    int count = 0;
    bookingInfo *cur = head;

    bool accepted;
    while (cur != NULL) {
        switch (acceptedType) {
            case 1:
                accepted = cur->fAccepted;
                break;
            case 2:
                accepted = cur->pAccepted;
                break;
            case 3:
                accepted = cur->oAccepted;
                break;
        }
        if (accepted) {
            count++;
        }
        cur = cur->next;
    }

    return count;
}

int utilizationCount(int essentialType, int acceptedType)
{
    int count = 0;
    bookingInfo *cur = head;

    bool accepted;
    while (cur != NULL) {
        switch (acceptedType) {
            case 1:
                accepted = cur->fAccepted;
                break;
            case 2:
                accepted = cur->pAccepted;
                break;
            case 3:
                accepted = cur->oAccepted;
                break;
        }
        if (accepted) {
            if (cur->essentials[essentialType]) {
                count++;
            }
        }
        cur = cur->next;
    }

    return count;
}

void printReport()
{
    FILE *fd;
    fd = fopen("SPMS_Report_G30.txt", "w");

    if (fd == NULL) {
        printf("Error occurred when opening the report file\n");
        return;
    }

    int i;
    fprintf(fd, "*** Parking Booking Manager - Summary Report ***\n");
    fprintf(fd, "\nPerformance:\n");
    fprintf(fd, "\nFor FCFS:\n");
    int totalRequest, totalReceived, totalAccepted, totalRejected, utilization;
    totalReceived = totalReceivedCount();
    totalRequest = totalReceived + invalidCount;
    totalAccepted = totalAcceptedCount(1); // 1 for FCFS
    totalRejected = totalReceived - totalAccepted;
    fprintf(fd, "\tTotal Number of Bookings Received: %d (%.1f%%)\n", totalReceived, (float)totalReceived / totalRequest * 100);
    fprintf(fd, "\t\t Number of Bookings Assigned: %d (%.1f%%)\n", totalAccepted, (float)totalAccepted / totalRequest * 100);
    fprintf(fd, "\t\t Number of Bookings Rejected: %d (%.1f%%)\n", totalRejected, (float)totalRejected / totalRequest * 100);
    fprintf(fd, "\n\t Utilization of Time Slot:\n");
    for (i = 0; i < 6; i++) {
        utilization = utilizationCount(i, 1); // 1 for FCFS
        fprintf(fd, "\n\t\t%s - %.1f%%\n", essentialsName[i], (float)utilization / totalAccepted * 100);
    }
    fprintf(fd, "\n\t Invalid request(s) made: %d\n", invalidCount);

    fprintf(fd, "\nFor PRIO:\n");
    totalAccepted = totalAcceptedCount(2); // 2 for Priority
    totalRejected = totalReceived - totalAccepted;
    fprintf(fd, "\tTotal Number of Bookings Received: %d (%.1f%%)\n", totalReceived, (float)totalReceived / totalRequest * 100);
    fprintf(fd, "\t\t Number of Bookings Assigned: %d (%.1f%%)\n", totalAccepted, (float)totalAccepted / totalRequest * 100);
    fprintf(fd, "\t\t Number of Bookings Rejected: %d (%.1f%%)\n", totalRejected, (float)totalRejected / totalRequest * 100);
    fprintf(fd, "\n\t Utilization of Time Slot:\n");
    for (i = 0; i < 6; i++) {
        utilization = utilizationCount(i, 2); // 2 for Priority
        fprintf(fd, "\n\t\t%s - %.1f%%\n", essentialsName[i], (float)utilization / totalAccepted * 100);
    }
    fprintf(fd, "\n\t Invalid request(s) made: %d\n", invalidCount);

    fprintf(fd, "\nFor OPTI:\n");
    totalAccepted = totalAcceptedCount(3); // 3 for Optimized
    totalRejected = totalReceived - totalAccepted;
    fprintf(fd, "\tTotal Number of Bookings Received: %d (%.1f%%)\n", totalReceived, (float)totalReceived / totalRequest * 100);
    fprintf(fd, "\t\t Number of Bookings Assigned: %d (%.1f%%)\n", totalAccepted, (float)totalAccepted / totalRequest * 100);
    fprintf(fd, "\t\t Number of Bookings Rejected: %d (%.1f%%)\n", totalRejected, (float)totalRejected / totalRequest * 100);
    fprintf(fd, "\n\t Utilization of Time Slot:\n");
    for (i = 0; i < 6; i++) {
        utilization = utilizationCount(i, 3); // 3 for Optimized
        fprintf(fd, "\n\t\t%s - %.1f%%\n", essentialsName[i], (float)utilization / totalAccepted * 100);
    }
    fprintf(fd, "\n\t Invalid request(s) made: %d\n", invalidCount);

    fclose(fd);
    printf("\nThe report has been written to the file successfully\n");
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
            }  else if (strcmp(COMMAND[0], "addBatch") == 0) { 
                addBatch(COMMAND[1], fd[1]); 
            } else if (strcmp(COMMAND[0], "printBookings") == 0) { // Tell parent to print bookings
                if (strcmp(COMMAND[1], "-fcfs") == 0) {
                    write(fd[1], "FC", 3);  
                }
                else if (strcmp(COMMAND[1], "-prio") == 0) {
                    write(fd[1], "PR", 3);  
                }
                else if (strcmp(COMMAND[1], "-opti") == 0) {
                    write(fd[1], "OP", 3);  
                } 
                else if (strcmp(COMMAND[1], "-ALL") == 0) {
                    write(fd[1], "AL", 3);  
                } 
            } else if (
            strcmp(COMMAND[0], "addEvent") == 0 ||
            strcmp(COMMAND[0], "addReservation") == 0 ||
            strcmp(COMMAND[0], "addParking") == 0 ||
            strcmp(COMMAND[0], "bookEssentials") == 0 ) {
                CreateBookingFromCommand(fd[1]);
            } else {
                printf("invalid command, must be one of the following:\n");
                printf("endProgram, addBatch, printBookings, addEvent, addParking, addParking, bookEssentials\n");
                write(fd[1], "invalid", 7);  
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
                    char essentialsString[8]; // 8 is because the size of written essentialsString is 8 (including '\0')
                    char acceptedString[4]; // 4 is because the size of written acceptedString is 4 (including '\0')
                    sscanf(buff + 5, "%d %d %d %d %s %s",
                        &newBooking->time, &newBooking->duration, &newBooking->member,
                        &newBooking->priority, essentialsString, acceptedString);
                    newBooking->next = NULL;
                    int i;
                    for (i = 0; i < 7; i++) { // 7 is because we only need the first 7 char of essentialsString (excluding '\0')
                        if (essentialsString[i] == '1') {
                            newBooking->essentials[i] = true;
                        }
                        else {
                            newBooking->essentials[i] = false;
                        }
                    }

                    if (acceptedString[0] == '1') {
                        newBooking->fAccepted = true;
                    }
                    else {
                        newBooking->fAccepted = false;
                    }

                    if (acceptedString[1] == '1') {
                        newBooking->pAccepted = true;
                    }
                    else {
                        newBooking->pAccepted = false;
                    }

                    if (acceptedString[2] == '1') {
                        newBooking->oAccepted = true;
                    }
                    else {
                        newBooking->oAccepted = false;
                    }

                    insertIntoBookings(&head, newBooking);

                    // DEBUG
                    printf("--------------------------\n");
                    printf("New Booking:\n");
                    printf("Time: %d\n", newBooking->time);
                    printf("Duration: %d\n", newBooking->duration);
                    printf("Member: %d\n", newBooking->member);
                    printf("Priority: %d\n", newBooking->priority);
                    printf("Essentials:\n");
                    for (i = 0; i < 6; i++) {
                        if (newBooking->essentials[i]) {
                            printf("%s\n", essentialsName[i]);
                        }
                    }
                    printf("fAccepted: %s\n", newBooking->fAccepted ? "True" : "False");
                    printf("pAccepted: %s\n", newBooking->pAccepted ? "True" : "False"); 
                    // printf("oAccepted: %d\n", newBooking->oAccepted); 
                    
                } else if (strncmp(buff, "FC", 3) == 0) {
                    printFCFS();
                } else if (strncmp(buff, "PR", 3) == 0) {
                    printPR();
                } else if (strncmp(buff, "OP", 3) == 0) {
                    printOPT();
                } else if (strncmp(buff, "AL", 3) == 0) {
                    printFCFS();
                    printPR();
                    printOPT();
                    printReport();
                } else if (strncmp(buff, "fail", 4) == 0) {
                    printf("Operation failed\n");
                    invalidCount++;
                } else if (strncmp(buff, "invalid", 7) == 0) {
                    invalidCount++;
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