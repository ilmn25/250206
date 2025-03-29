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
    int startTime; // YYYYMMDDHH
    int endTime; // YYYYMMDDHH
    int member; // Member A to E (1 - 5)
    int priority; // Priority (1: Event, 2: Reservation, 3: Parking, 4: Essentials)
    bool essentials[7]; // Essentials reserved (Lockers, Umbrellas, Batteries, Cables, Valet parking, Inflation services, normal parking) (1 = Reserved) (e.g. 110010 = Locker, Umbrella, and Valet parking reserved)
    bool fAccepted; // Accepted or not (First Come First Served)
    bool pAccepted; // Accepted or not (Priority)
    bool oAccepted; // Accepted or not (Optimized)
    int bookingID; // Booking ID for cancellation search
    int *overwrittenIDs; // The booking IDs that are overwritten by this booking
    int num; // The number of overwrittenIDs
    int reStartTime; // YYYYMMDDHH the start time after rescheduling
    int reEndTime; // YYYYMMDDHH the start time after rescheduling
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
bool isMorePriorityThan(bookingInfo *bookingA, bookingInfo *bookingB, bool includeSame);
bool isAvailableEssential(bookingInfo *targetBooking, int target, int limit, int mode);
void EvictEssential(bookingInfo *targetBooking);
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

// Function to free the linked list (Only used when endProgram)
void freeBookings() {
    bookingInfo *temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        if (temp->overwrittenIDs != NULL) {
            free(temp->overwrittenIDs);
        }
        free(temp);
    }
}

// Function to count bookings in the linked list (For calculate the bookingID)
int countBookings()
{
    int count = 0;
    bookingInfo *cur = head;
    while (cur != NULL) {
        count++;
        cur = cur->next;
    }
    return count;
}

// Function to insert a booking into the sorted linked list
void insertIntoBookings(bookingInfo **head, bookingInfo *newBooking) 
{
    bookingInfo *cur = *head;
    bookingInfo *pre = NULL;

    while (cur != NULL) {
        if (cur->startTime > newBooking->startTime) {
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

    newBooking->bookingID = countBookings(); // Update the bookingID after insertion
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

bool isMorePriorityThan(bookingInfo *bookingA, bookingInfo *bookingB, bool includeSame) {
    if  (bookingA->priority != bookingB->priority) {
        return bookingA->priority < bookingB->priority;
    }

    // extract start and end times for both bookings
    int startA = bookingA->startTime % 10000; // Extract HHMM from YYYYMMDDHHMM
    int endA = bookingA->endTime % 10000;
    int startB = bookingB->startTime % 10000;
    int endB = bookingB->endTime % 10000;

    bool aInPriorityWindow = (startA < 2000 && endA > 800);
    bool bInPriorityWindow = (startB < 2000 && endB > 800);

    // if both in or both out then no difference aka not higher
    if (aInPriorityWindow == bInPriorityWindow) {
        return includeSame;
    } 
    return aInPriorityWindow; 
}

// helper method to check if enough number of batteries etc for one more booking to be in the target time and duration 
bool isAvailableEssential(bookingInfo *targetBooking, int target, int limit, int mode) 
{ 
    bookingInfo *cur = head;
    bookingInfo *temp[100];
    int tempCount = 0;

    // scan entire booking list and save (into temp) bookings that are overlapping AND has targeted essential AND more priority
    while (cur != NULL) {// because linked list is sorted by time, performant
        if (cur->essentials[target] && 
            ((mode == 1 && cur->pAccepted && isMorePriorityThan(cur, targetBooking, true)) ||
            (mode == 2  && cur->fAccepted) ||
            (mode == 3  && cur->pAccepted)) && 
            ((targetBooking->startTime >= cur->startTime && targetBooking->startTime < cur->endTime) || 
            (targetBooking->endTime > cur->startTime && targetBooking->endTime <= cur->endTime) ||
            (targetBooking->startTime <= cur->startTime && targetBooking->endTime >= cur->endTime))) {
            // Add cur to temp list
            temp[tempCount++] = cur;  
        }
        cur = cur->next;
    }

    // i - iterate through each hour in target time period
    // j - iterate through each overlapping temp to see if more than [limit] overlaps (3 for essentials or 10 for parking)
    int i, j;
    int currentHour = targetBooking->startTime;
    while (currentHour <= targetBooking->endTime) { //go through each hour until reach endtime 
        int count = 0;
        for (j = 0; j < tempCount; j++) {
             
            if (currentHour >= temp[j]->startTime && currentHour < temp[j]->endTime) {
                count++;  
                if (count >= limit) return false; // Overbooked
            }
        }
        currentHour = addDurationToTime(currentHour, 1);  
    }

    return true; // available
}

// Function which is the same as isAvailableEssential but for optimized since optimized needs to handle reschedule time
bool isAvailableEssentialOPT(bookingInfo *targetBooking, int target, int limit) 
{ 
    bookingInfo *cur = head;
    bookingInfo *temp[100];
    int tempCount = 0;

    // For compare when the current booking is also using reschedule time
    int curSt;
    int curEt;

    // scan entire booking list and save (into temp) bookings that are overlapping AND has targeted essential AND more priority
    while (cur != NULL) {// because linked list is sorted by time, performant
        if (cur->essentials[target] && cur->oAccepted) { // oAccepted has two case: 1 is original accepted by priority, 2 is accepted after reschedule
            // Set the value of curSt and curEt
            if (cur->reStartTime != -1 && cur->reEndTime != -1) {
                curSt = cur->reStartTime;
                curEt = cur->reEndTime;
            }
            else {
                curSt = cur->startTime;
                curEt = cur->endTime;
            }
            if ((targetBooking->reStartTime >= curSt && targetBooking->reStartTime < curEt) || 
            (targetBooking->reEndTime > curSt && targetBooking->reEndTime <= curEt) ||
            (targetBooking->reStartTime <= curSt && targetBooking->reEndTime >= curEt)) {
                // Add cur to temp list
                temp[tempCount++] = cur;  
            }
        }
        cur = cur->next;
    }

    // i - iterate through each hour in target time period
    // j - iterate through each overlapping temp to see if more than [limit] overlaps (3 for essentials or 10 for parking)
    int i, j;
    int currentHour = targetBooking->reStartTime;
    while (currentHour <= targetBooking->reEndTime) { //go through each hour until reach endtime 
        int count = 0;
        for (j = 0; j < tempCount; j++) {
            if (temp[j]->reStartTime != -1 && temp[j]->reEndTime != -1) {
                curSt = temp[j]->reStartTime;
                curEt = temp[j]->reEndTime;
            }
            else {
                curSt = temp[j]->startTime;
                curEt = temp[j]->endTime;
            }
            if (currentHour >= curSt && currentHour < curEt) {
                count++;  
                if (count >= limit) return false; // Overbooked
            }
        }
        currentHour = addDurationToTime(currentHour, 1);  
    }

    return true; // available
}

bool isOverlapEssentials(bookingInfo* bookingA, bookingInfo* bookingB) {
    int i;
    for (i = 0; i < 7; ++i) {
        if (bookingA->essentials[i] && bookingB->essentials[i]) {
            return true;
        }
    }
    return false;
}
void EvictEssential(bookingInfo *targetBooking)  
{
    int i; //iterate  
    // CANCEL BOOKINGS OVERWRITTEN BECAUSE LESS PRIORITY CODE  
    bookingInfo *cur = head;
    bookingInfo *temp[100];
    int tempCount = 0;

    // 1- Scan the booking list and save overlapping bookings with lower priority
    while (cur != NULL) {// because linked list is sorted by time, performant
        if (cur->pAccepted && isOverlapEssentials(targetBooking, cur) && isMorePriorityThan(targetBooking, cur, false) && 
            ((targetBooking->startTime >= cur->startTime && targetBooking->startTime < cur->endTime) || 
             (targetBooking->endTime > cur->startTime && targetBooking->endTime <= cur->endTime) || 
             (targetBooking->startTime <= cur->startTime && targetBooking->endTime >= cur->endTime))) {
            for (i = 0; i < 7; i++) {
                if (targetBooking->essentials[i] && cur->essentials[i]) {
                    temp[tempCount++] = cur;
                    break;
                }
            }
        }
        cur = cur->next;
    } 
            printf("BOOM %d\n", tempCount);

    int j;
    // CHECK IF CORRECTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT
    // 2- Go through the list and cancel the booking with least priority, and repeat until no overbooking
    for (i = 0; i < 7; i++) {
        if (!targetBooking->essentials[i]) continue; //4 and 11 because need to be "just full", not "have spare spot available"
        while ((i != 6 && !isAvailableEssential(targetBooking, i, 3, 3)) || (i == 6 && !isAvailableEssential(targetBooking, 6, 10, 3))) {
            // Get lowest priority in temp
            int target = 0;
            for (j = 0; j < tempCount; j++) {
                if (temp[j] != NULL && (temp[target] == NULL || isMorePriorityThan(temp[target], temp[j], false))) {
                    target = j;
                }
            }

            // Remove it from temp and set its pAccepted flag to false
            temp[target]->pAccepted = false;

            int newSize = targetBooking->num + 1;
            int *newMemory;

            if (targetBooking->overwrittenIDs == NULL) {
                // Memory allocation for the first time
                newMemory = malloc(sizeof(int));
                if (newMemory == NULL) {
                    fprintf(stderr, "Memory allocation failed for overwrittenIDs.\n");
                    return;
                }
            } else {
                // Reallocation for newSize
                newMemory = realloc(targetBooking->overwrittenIDs, newSize * sizeof(int));
                if (newMemory == NULL) {
                    fprintf(stderr, "Memory reallocation failed for overwrittenIDs.\n");
                    free(targetBooking->overwrittenIDs); // Free the original memory allocation
                    return;
                }
            }

            targetBooking->overwrittenIDs = newMemory; // Update the pointer
            targetBooking->overwrittenIDs[targetBooking->num++] = temp[target]->bookingID;
            // printf("NUKED %d\n", temp[target]->priority);
            // IT NEEDS TO SET P ACCEPTED TO FALSE IN MAIN PROCESS NOT HERE!!! 
            // 🔰🚸☣❇☢🚸🚸⚠☣🚸🔰🔰🚸🚸🚸⚠⚠⚠⚠
            // 🔰🚸☣❇☢🚸🚸⚠☣🚸🔰🔰🚸🚸🚸⚠⚠⚠⚠
            temp[target] = NULL;  
        }
    }
}

bool isAvailableFCFS(bookingInfo *targetBooking) 
{ 
    if (targetBooking->essentials[6] && !isAvailableEssential(targetBooking, 6, 10, 2)) {  
        return false; // parking is overbooked
    }

    int i; //iterate through 6 essentials 
    for (i = 0; i < 6; i++) {
        if (targetBooking->essentials[i] && !isAvailableEssential(targetBooking, i, 3, 2)) {
            return false; // Essential item is overbooked
        }
    }

    return true; // Time slot is available
}

bool isAvailablePR(bookingInfo *targetBooking) 
{
    if (targetBooking->essentials[6] && !isAvailableEssential(targetBooking, 6, 10, 1)) {
        return false; // parking is overbooked
    }

    int i; //iterate through 6 essentials 
    for (i = 0; i < 6; i++) {
        if (targetBooking->essentials[i] && !isAvailableEssential(targetBooking, i, 3, 1)) {
            return false; // Essential item is overbooked
        }
    }
    EvictEssential(targetBooking);
    return true; // Time slot is available
}

bool isAvailableOPT(bookingInfo *targetBooking) 
{ 
    if (targetBooking->essentials[6] && !isAvailableEssentialOPT(targetBooking, 6, 10)) {  
        return false; // parking is overbooked
    }

    int i; //iterate through 6 essentials 
    for (i = 0; i < 6; i++) {
        if (targetBooking->essentials[i] && !isAvailableEssentialOPT(targetBooking, i, 3)) {
            return false; // Essential item is overbooked
        }
    }

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
    newBooking->startTime = year * 1000000 + month * 10000 + day * 100 + hour;

    // Store the duration
    newBooking->endTime = addDurationToTime(newBooking->startTime, atoi(COMMAND[4]));
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

    newBooking->num = 0;
    newBooking->overwrittenIDs = NULL; // Initialize the pointer of overwrittenIDs to NULL
    newBooking->fAccepted = isAvailableFCFS(newBooking); 
    newBooking->pAccepted = isAvailablePR(newBooking); 
    newBooking->oAccepted = false; // Initialize the oAccepted to false
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

    int i;
    for (i = 0; i < 7; i++) {
        essentialsString[i] = essentials[i] ? '1' : '0';
    }
    essentialsString[7] = '\0';

    return essentialsString;
}

void CreateBookingFromCommand(int fd) 
{  
    bookingInfo *newBooking = handleCreateBooking();  
    if (newBooking != NULL) { 
        char buff[100]; // Declare a large enough buffer to write the booking information

        // Convert bool array to string for writing to parent
        char *essentialsString = convertEssentialsToString(newBooking->essentials);

        sprintf(buff, "done %d %d %d %d %s %d %d %d %d %d ",
                newBooking->startTime, newBooking->endTime, newBooking->member,
                newBooking->priority, essentialsString, (int)newBooking->fAccepted,
                (int)newBooking->pAccepted, (int)newBooking->oAccepted, newBooking->bookingID, newBooking->num);
        int i;
        // Appending each item in overwrittenIDs to buff
        for (i = 0; i < newBooking->num; i++) {
            sprintf(buff + strlen(buff), "%d ", newBooking->overwrittenIDs[i]);
        }
        write(fd, buff, sizeof(buff)); // Send booking info to parent

        free(essentialsString);

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
    freeBookings(); // Free bookings for child
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

// Function for merge sort
bookingInfo *merge(bookingInfo *left, bookingInfo *right, bool forReschedule)
{
    bookingInfo *result = NULL;

    if (left == NULL) return right;
    if (right == NULL) return left;

    int leftSt;
    int rightSt;

    if (forReschedule) {
        if (left->reStartTime != -1) {
            leftSt = left->reStartTime;
        }
        else {
            leftSt = left->startTime;
        }
        if (right->reStartTime != -1) {
            rightSt = right->reStartTime;
        }
        else {
            rightSt = right->startTime;
        }
    }
    else {
        leftSt = left->startTime;
        rightSt = right->startTime;
    }

    if (leftSt <= rightSt) {
        result = left;
        result->next = merge(left->next, right, forReschedule);
    } else {
        result = right;
        result->next = merge(left, right->next, forReschedule);
    }

    return result;
}

// Function for merge sort to split the linked list in half
void split(bookingInfo *source, bookingInfo **front, bookingInfo **back) {
    bookingInfo *fast;
    bookingInfo *slow;
    slow = source;
    fast = source->next;

    while (fast != NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }

    *front = source;
    *back = slow->next;
    slow->next = NULL;
}

// Merge sort function
void mergeSort(bookingInfo **headRef, bool forReschedule) {
    bookingInfo *h = *headRef;
    bookingInfo *a;
    bookingInfo *b;

    if (h == NULL || h->next == NULL) {
        return;
    }

    split(h, &a, &b);

    mergeSort(&a, forReschedule);
    mergeSort(&b, forReschedule);

    *headRef = merge(a, b, forReschedule);
}

void handlePrintBooking(int member, bool isAccepted, int acceptedType) 
{
    if (acceptedType == 3) {
        mergeSort(&head, true); // Sort the linked list including reStartTime
    }
    else {
        mergeSort(&head, false); // Sort the linked list by startTime only
    }
    
    bookingInfo *cur = head;
    bool accepted;

    int curSt;
    int curEt;

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
                curSt = cur->startTime;
                curEt = cur->endTime;
                break;
            case 2:
                accepted = cur->pAccepted;
                curSt = cur->startTime;
                curEt = cur->endTime;
                break;
            case 3:
                accepted = cur->oAccepted;
                if (cur->reStartTime != -1 && cur->reEndTime != -1) {
                    curSt = cur->reStartTime;
                    curEt = cur->reEndTime;
                }
                else {
                    curSt = cur->startTime;
                    curEt = cur->endTime;
                }
                break;
        }

        if (cur->member == member && accepted == isAccepted) {
            int year = curSt / 1000000;
            int month = (curSt / 10000) % 100;
            int day = (curSt / 100) % 100;
            int startHour = curSt % 100;
            int endHour = curEt % 100;

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
                
                // Print the essentials for this booking
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

// Function for optimized algorithm to reschedule the rejected bookings of priority
void reschedule()
{
    bookingInfo *cur = head;

    // Step 1: Copy the priority result to optimized
    while (cur != NULL) {
        if (cur->pAccepted) {
            cur->oAccepted = true;
            cur->reStartTime = -1; // Initialize the reStartTime to -1
            cur->reEndTime = -1; // Initialize the reEndTime to -1
        }
        cur = cur->next;
    }

    cur = head; // Reset cur to head
    // Step 2: Reschedule the rejected bookings of priority to the nearest available time slot
    while (cur != NULL) {
        if (!cur->oAccepted) {
            cur->reStartTime = cur->startTime; // Initialize the reStartTime to the startTime
            cur->reEndTime = cur->endTime; // Initialize the reEndTime to the endTime
            // Check if the current time slot requested by this rejected booking is available
            while (!isAvailableOPT(cur)) {
                // Add one hour for both reStartTime and reEndTime to check if the next hour is available
                cur->reStartTime = addDurationToTime(cur->reStartTime, 1);
                cur->reEndTime = addDurationToTime(cur->reEndTime, 1);
            }
            cur->oAccepted = true;
        }
        cur = cur->next;
    }
}

void printOPT()  
{
    reschedule(); // Reschedule the rejected bookings of priority
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
    fd = fopen("SPMS_Report_G30.txt", "w"); // Open the report file with write mode

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
    // Print the utilization of each essential type
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
    // Print the utilization of each essential type
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
    // Print the utilization of each essential type
    for (i = 0; i < 6; i++) {
        utilization = utilizationCount(i, 3); // 3 for Optimized
        fprintf(fd, "\n\t\t%s - %.1f%%\n", essentialsName[i], (float)utilization / totalAccepted * 100);
    }
    fprintf(fd, "\n\t Invalid request(s) made: %d\n", invalidCount);

    fclose(fd); // Close the report file
    printf("\nThe report has been written to the file successfully\n");
}

void cancelBookings(bookingInfo *newBooking)
{
    int num;
    bookingInfo *cur = head;
    printf("\nThere is %d booking needs to be cancelled. BookingID: ", newBooking->num);
    for (num = 0; num < newBooking->num; num++) printf("%d ", newBooking->overwrittenIDs[num]);
    while (cur != NULL) {
        for (num = 0; num < newBooking->num; num++) {
            if (cur->bookingID == newBooking->overwrittenIDs[num] && cur->pAccepted) {
                cur->pAccepted = false;
                printf("\nBookingID: %d Cancelled\n", cur->bookingID);
            }
        }
        cur = cur->next;
    }
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
            } else if (strcmp(COMMAND[0], "addBatch") == 0) { 
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
                } else {
                    printf("invalid command\n");
                }
            } else if (
            strcmp(COMMAND[0], "addEvent") == 0 ||
            strcmp(COMMAND[0], "addReservation") == 0 ||
            strcmp(COMMAND[0], "addParking") == 0 ||
            strcmp(COMMAND[0], "bookEssentials") == 0 ) {  
                CreateBookingFromCommand(fd[1]);
                freeBookings(); // Free bookings for child
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
                    int fA, pA, oA; // Use int to read values for bool with sscanf to avoid compile warnings
                    int i;
                    sscanf(buff + 5, "%d %d %d %d %s %d %d %d %d %d", // buff + 5 is to skip "done "
                        &newBooking->startTime, &newBooking->endTime, &newBooking->member,
                        &newBooking->priority, essentialsString, &fA, &pA, &oA, &newBooking->bookingID, &newBooking->num);
                    if (newBooking->num != 0) { // If there exists overwrittenIDs
                        newBooking->overwrittenIDs = malloc(newBooking->num * sizeof(int)); // Allocate memory for overwrittenIDs
                        char *token = strtok(buff, " "); // Split the buff by " "
                        for (i = 0; i < 10; i++) { // Skip the first 10 tokens
                            token = strtok(NULL, " ");
                        }
    
                        for (i = 0; i < newBooking->num; i++) {
                            token = strtok(NULL, " "); // Continue to get the remaining tokens
                            if (token != NULL) {
                                newBooking->overwrittenIDs[i] = atoi(token); // Convert the string to int and store in overwrittenIDs
                            }
                        }
                    }

                    newBooking->next = NULL;

                    for (i = 0; i < 7; i++) { // 7 is because we only need the first 7 char of essentialsString (excluding '\0')
                        if (essentialsString[i] == '1') {
                            newBooking->essentials[i] = true;
                        }
                        else {
                            newBooking->essentials[i] = false;
                        }
                    }

                    // Convert int to bool and store them in the newBooking
                    newBooking->fAccepted = (fA != 0);
                    newBooking->pAccepted = (pA != 0);
                    newBooking->oAccepted = (oA != 0);

                    insertIntoBookings(&head, newBooking);

                    if (newBooking->num != 0) {
                        cancelBookings(newBooking);
                    }

                    // DEBUG
                    printf("--------------------------\n");
                    printf("New Booking:\n");
                    printf("start: %d\n", newBooking->startTime);
                    printf("end: %d\n", newBooking->endTime);
                    printf("Member: %d\n", newBooking->member);
                    printf("Priority: %d\n", newBooking->priority);
                    printf("Essentials:\n");
                    for (i = 0; i < 6; i++) {
                        if (newBooking->essentials[i]) {
                            printf("\t%s\n", essentialsName[i]);
                        }
                    }
                    printf("fAccepted: %s\n", newBooking->fAccepted ? "True" : "False");
                    printf("pAccepted: %s\n", newBooking->pAccepted ? "True" : "False");
                    printf("oAccepted: %s\n", newBooking->oAccepted ? "True" : "False");
                    
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
    freeBookings();
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