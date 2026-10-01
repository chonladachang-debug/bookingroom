#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

// รองรับการตั้งค่า Encoding บนระบบปฏิบัติการ Windows
#ifdef _WIN32
#include <windows.h>
#endif

#define OPEN_MIN 480       // 08:00 น.
#define CLOSE_MIN 930      // 15:30 น.
#define DURATION_MIN 120   // 2 ชั่วโมง
#define MAX_ROOMS 6
#define MAX_BOOKINGS 100005 // ขยายขนาดให้รองรับ n = 100,000
#define MAX_WAITLIST 500
#define REPEAT 1000        /* วนค้นหาซ้ำ เพื่อให้เวลามากพอที่จะวัดได้ */

// ==========================================
// 0. MATH UTILITY (แก้ปัญหา Linker log10)
// ==========================================

// ฟังก์ชันคำนวณ log10 โดยไม่พึ่งพา libm (-lm)
double my_log10(double x) {
    if (x <= 0) return 0;
    double y = (x - 1.0) / (x + 1.0);
    double y2 = y * y;
    double sum = 0.0;
    double term = y;
    for (int i = 1; i <= 25; i += 2) {
        sum += term / i;
        term *= y2;
    }
    return (2.0 * sum) / 2.302585092994046; // ln(x) / ln(10)
}

// ==========================================
// 1. DATA STRUCTURES
// ==========================================

typedef struct {
    char id[10];
    char name[50];
    int seats;
    char kind[20];
} Room;

Room ROOMS[MAX_ROOMS] = {
    {"movie",  "ห้องดูหนัง",        20, "Movie"},
    {"study1", "ห้องศึกษากลุ่ม 1",  10, "Study"},
    {"study2", "ห้องศึกษากลุ่ม 2",  10, "Study"},
    {"study3", "ห้องศึกษากลุ่ม 3",  10, "Study"},
    {"study4", "ห้องศึกษากลุ่ม 4",  10, "Study"},
    {"large",  "ห้องประชุมใหญ่",    15, "Meeting"}
};

typedef struct {
    int id;
    int roomIdx;
    char date[11]; // YYYY-MM-DD
    int startMin;
    int endMin;
    char name[60];
} Booking;

Booking bookings[MAX_BOOKINGS];
int bookingCount = 0;

// Priority Queue Entry for Waitlist
typedef struct {
    int id;
    int roomIdx;
    char date[11];
    int startMin;
    int endMin;
    char name[60];
    long joinedAt; // Unix timestamp
} WaitlistEntry;

WaitlistEntry waitlist[MAX_WAITLIST];
int waitlistCount = 0;

// Interval Tree Node
typedef struct IntervalNode {
    int low;
    int high;
    int max;
    int bookingId;
    int roomIdx;
    char date[11];
    struct IntervalNode *left;
    struct IntervalNode *right;
} IntervalNode;

IntervalNode *intervalTreeRoot = NULL;

typedef struct {
    int roomIdx;
    int leftoverSeats;
} Recommendation;

// ==========================================
// 2. TIME UTILITIES
// ==========================================

void minToTimeStr(int min, char *buffer) {
    sprintf(buffer, "%02d:%02d", min / 60, min % 60);
}

int timeStrToMin(const char *timeStr) {
    int h, m;
    if (sscanf(timeStr, "%d:%d", &h, &m) == 2) {
        return h * 60 + m;
    }
    return -1;
}

// ==========================================
// 3. INTERVAL TREE FUNCTIONS (Overlap Check)
// ==========================================

IntervalNode* createIntervalNode(int low, int high, int bookingId, int roomIdx, const char *date) {
    IntervalNode *node = (IntervalNode*)malloc(sizeof(IntervalNode));
    node->low = low;
    node->high = high;
    node->max = high;
    node->bookingId = bookingId;
    node->roomIdx = roomIdx;
    strcpy(node->date, date);
    node->left = node->right = NULL;
    return node;
}

IntervalNode* insertInterval(IntervalNode *root, int low, int high, int bookingId, int roomIdx, const char *date) {
    if (!root) return createIntervalNode(low, high, bookingId, roomIdx, date);

    if (low < root->low)
        root->left = insertInterval(root->left, low, high, bookingId, roomIdx, date);
    else
        root->right = insertInterval(root->right, low, high, bookingId, roomIdx, date);

    if (root->max < high) root->max = high;
    return root;
}

void freeIntervalTree(IntervalNode *root) {
    if (!root) return;
    freeIntervalTree(root->left);
    freeIntervalTree(root->right);
    free(root);
}

bool hasOverlap(IntervalNode *root, int low, int high, int roomIdx, const char *date) {
    if (!root) return false;

    if (root->roomIdx == roomIdx && strcmp(root->date, date) == 0) {
        if (root->low < high && low < root->high) return true;
    }

    if (root->left && root->left->max > low) {
        if (hasOverlap(root->left, low, high, roomIdx, date)) return true;
    }

    return hasOverlap(root->right, low, high, roomIdx, date);
}

void rebuildIntervalTree() {
    freeIntervalTree(intervalTreeRoot);
    intervalTreeRoot = NULL;
    for (int i = 0; i < bookingCount; i++) {
        intervalTreeRoot = insertInterval(
            intervalTreeRoot,
            bookings[i].startMin,
            bookings[i].endMin,
            bookings[i].id,
            bookings[i].roomIdx,
            bookings[i].date
        );
    }
}

// ==========================================
// 4. DASHBOARD & TREE VISUALIZATION UTILITIES
// ==========================================

// ฟังก์ชันวาดแผนผัง Interval Tree ในคอนโซลแบบ 2D (หมุนข้าง 90 องศา)
void printTree2D(IntervalNode *root, int space) {
    if (root == NULL) return;

    space += 8;

    // แสดง Subtree ฝั่งขวาก่อน
    printTree2D(root->right, space);

    // แสดงโหนดปัจจุบัน [เวลาเริ่ม-จบ | Room ID | Max]
    printf("\n");
    for (int i = 8; i < space; i++) printf(" ");

    char sStr[10], eStr[10];
    minToTimeStr(root->low, sStr);
    minToTimeStr(root->high, eStr);

    printf("[%s-%s|%s|Max:%d]\n", sStr, eStr, ROOMS[root->roomIdx].id, root->max);

    // แสดง Subtree ฝั่งซ้าย
    printTree2D(root->left, space);
}

// ฟังก์ชันแสดง Dashboard Snapshot สรุปภาพรวม Tree Map + Booking List (ตามภาพแบบร่าง)
void displayDashboardSnapshot(const char *title) {
    printf("\n==================================================\n");
    printf("  [ DASHBOARD ] %s\n", title);
    printf("==================================================\n");

    // 1 & 2. แสดง Tree Map
    printf("\n>> 1 & 2. โครงสร้าง Interval Tree Map <<\n");
    if (intervalTreeRoot == NULL) {
        printf("   (ต้นไม้ว่างเปล่า - ยังไม่มีข้อมูลการจองในระบบ)\n");
    } else {
        printTree2D(intervalTreeRoot, 0);
    }

    // 3. แสดง List รายการการจองจริง
    printf("\n>> 3. รายการการจองจริงในระบบ (List/Grid) [ทั้งหมด %d รายการ] <<\n", bookingCount);
    if (bookingCount == 0) {
        printf("   (ไม่มีรายการจองในระบบ)\n");
    } else {
        for (int i = 0; i < bookingCount; i++) {
            char sStr[10], eStr[10];
            minToTimeStr(bookings[i].startMin, sStr);
            minToTimeStr(bookings[i].endMin, eStr);
            printf("   [Book #%d] %-12s | %-16s | %s (%s - %s น.)\n",
                   i + 1,
                   bookings[i].name,
                   ROOMS[bookings[i].roomIdx].name,
                   bookings[i].date, sStr, eStr);
        }
    }
    printf("==================================================\n");
}

// ==========================================
// 5. PRIORITY QUEUE (Min-Heap for Waitlist)
// ==========================================

void heapifyUp(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (waitlist[index].joinedAt < waitlist[parent].joinedAt) {
            WaitlistEntry temp = waitlist[index];
            waitlist[index] = waitlist[parent];
            waitlist[parent] = temp;
            index = parent;
        } else break;
    }
}

void heapifyDown(int index) {
    int smallest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < waitlistCount && waitlist[left].joinedAt < waitlist[smallest].joinedAt)
        smallest = left;
    if (right < waitlistCount && waitlist[right].joinedAt < waitlist[smallest].joinedAt)
        smallest = right;

    if (smallest != index) {
        WaitlistEntry temp = waitlist[index];
        waitlist[index] = waitlist[smallest];
        waitlist[smallest] = temp;
        heapifyDown(smallest);
    }
}

void pushWaitlist(WaitlistEntry entry) {
    if (waitlistCount >= MAX_WAITLIST) return;
    waitlist[waitlistCount] = entry;
    heapifyUp(waitlistCount);
    waitlistCount++;
}

bool popWaitlist(WaitlistEntry *result) {
    if (waitlistCount <= 0) return false;
    *result = waitlist[0];
    waitlist[0] = waitlist[waitlistCount - 1];
    waitlistCount--;
    heapifyDown(0);
    return true;
}

// ==========================================
// 6. SEARCH ALGORITHM & BENCHMARKING
// ==========================================

// อัลกอริทึมค้นหาของกลุ่ม (Greedy Search + Insertion Sort)
int Search(int headcount, const char *date, int startMin, Recommendation candidates[]) {
    int endMin = startMin + DURATION_MIN;
    int candidateCount = 0;

    // 1. ค้นหาห้องที่ไม่ซ้อนทับและรองรับจำนวนคนได้
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (ROOMS[i].seats >= headcount) {
            if (!hasOverlap(intervalTreeRoot, startMin, endMin, i, date)) {
                candidates[candidateCount].roomIdx = i;
                candidates[candidateCount].leftoverSeats = ROOMS[i].seats - headcount;
                candidateCount++;
            }
        }
    }

    // 2. จัดอันดับด้วย Insertion Sort ตามที่นั่งเหลือจากน้อยไปมาก (Greedy Best-Fit)
    for (int i = 1; i < candidateCount; i++) {
        Recommendation key = candidates[i];
        int j = i - 1;
        while (j >= 0 && candidates[j].leftoverSeats > key.leftoverSeats) {
            candidates[j + 1] = candidates[j];
            j--;
        }
        candidates[j + 1] = key;
    }

    return candidateCount;
}

// ฟังก์ชันสุ่มสร้างข้อมูลการจองจำลองสำหรับทดสอบตามขนาด n
void generateMockBookings(int n) {
    bookingCount = 0;
    if (n > MAX_BOOKINGS) n = MAX_BOOKINGS;

    for (int i = 0; i < n; i++) {
        bookings[i].id = i + 1;
        bookings[i].roomIdx = rand() % MAX_ROOMS;
        strcpy(bookings[i].date, "2026-10-05");
        int start = OPEN_MIN + (rand() % (CLOSE_MIN - OPEN_MIN - 60));
        bookings[i].startMin = start;
        bookings[i].endMin = start + 60 + (rand() % 60);
        sprintf(bookings[i].name, "User_%d", i + 1);
    }
    bookingCount = n;
    rebuildIntervalTree();
}

// ฟังก์ชันวัดเวลาการประมวลผล และคำนวณวิเคราะห์เทียบ Big-O อัตโนมัติ
void runBigOBenchmark() {
    int test_sizes[3] = {1000, 10000, 100000};
    double avg_times[3];
    Recommendation candidates[MAX_ROOMS];

    printf("\n=========================================\n");
    printf("   เริ่มการทดสอบประสิทธิภาพและเทียบ Big-O\n");
    printf("=========================================\n");

    // สำรองข้อมูลเดิมในระบบไว้ก่อนทดสอบ
    int old_count = bookingCount;
    Booking *old_bookings = NULL;
    if (old_count > 0) {
        old_bookings = (Booking*)malloc(sizeof(Booking) * old_count);
        if (old_bookings) memcpy(old_bookings, bookings, sizeof(Booking) * old_count);
    }

    for (int s = 0; s < 3; s++) {
        int n = test_sizes[s];
        printf("\nกำลังสร้างข้อมูลจำลองการจอง n = %d รายการ...", n);
        generateMockBookings(n);

        clock_t start = clock();
        int result = 0;
        for (int i = 0; i < REPEAT; i++) {
            result = Search(5, "2026-10-05", 540, candidates); // ค้นหาห้องสำหรับ 5 คน เวลา 09:00 น.
        }
        clock_t end = clock();

        double total_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double avg_msec = (total_sec * 1000.0) / REPEAT;
        avg_times[s] = avg_msec;

        printf("\nn = %d\n", n);
        printf("จำนวนรอบที่วัด   : %d รอบ\n", REPEAT);
        printf("เวลารวม          : %.6f วินาที\n", total_sec);
        printf("เวลาเฉลี่ยต่อครั้ง : %.6f มิลลิวินาที\n", avg_msec);
        printf("ผลการค้นหา       : %d ห้อง\n", result);
    }

    // คำนวณอัตราส่วนและค่า k = log10(time_ratio) ด้วย my_log10
    double r1 = avg_times[1] / (avg_times[0] > 0 ? avg_times[0] : 0.000001);
    double r2 = avg_times[2] / (avg_times[1] > 0 ? avg_times[1] : 0.000001);
    double k1 = my_log10(r1);
    double k2 = my_log10(r2);

    printf("\n== วิเคราะห์เทียบ Big-O ==\n");
    printf("1,000  -> 10,000  : อัตราส่วนเวลา %.2f เท่า, k = %.2f\n", r1, k1);
    printf("10,000 -> 100,000 : อัตราส่วนเวลา %.2f เท่า, k = %.2f\n", r2, k2);

    double avg_k = (k1 + k2) / 2.0;
    printf("ผลประเมิน Big-O จากเวลาจริง : ");
    if (avg_k < 0.35) {
        printf("O(log n) หรือ O(1)\n");
    } else if (avg_k >= 0.7 && avg_k <= 1.3) {
        printf("O(n)\n");
    } else {
        printf("O(n log n) หรือ O(n^2)\n");
    }
    printf("=========================================\n");

    // คืนค่าข้อมูลรายการจองเดิมของผู้ใช้
    if (old_bookings) {
        memcpy(bookings, old_bookings, sizeof(Booking) * old_count);
        bookingCount = old_count;
        free(old_bookings);
    } else {
        bookingCount = 0;
    }
    rebuildIntervalTree();
}

void recommendRooms(int headcount, const char *date, int startMin) {
    Recommendation candidates[MAX_ROOMS];
    clock_t start, end;
    double total_sec, avg_msec;
    int i, result;
    int n = bookingCount;

    start = clock();
    for (i = 0; i < REPEAT; i++) {
        result = Search(headcount, date, startMin, candidates);
    }
    end = clock();

    total_sec = (double)(end - start) / CLOCKS_PER_SEC;
    avg_msec  = total_sec * 1000.0 / REPEAT;

    printf("\n=== ผลการแนะนำห้อง (Greedy Best-Fit) ===\n");
    if (result == 0) {
        printf("ไม่พบห้องว่างที่รองรับจำนวน %d คนในช่วงเวลานี้\n", headcount);
    } else {
        for (int k = 0; k < result; k++) {
            int idx = candidates[k].roomIdx;
            printf("#%d %s (รองรับ %d คน | เหลือว่าง %d ที่นั่ง)\n",
                   k + 1, ROOMS[idx].name, ROOMS[idx].seats, candidates[k].leftoverSeats);
        }
    }

    printf("\n----------------------------------------\n");
    printf("จำนวนรายการจองในระบบ n = %d\n", n);
    printf("จำนวนรอบที่วัด   : %d รอบ\n", REPEAT);
    printf("เวลารวม          : %.6f วินาที\n", total_sec);
    printf("เวลาเฉลี่ยต่อครั้ง : %.6f มิลลิวินาที\n", avg_msec);
    printf("ผลการค้นหา       : %d ห้อง\n", result);
    printf("----------------------------------------\n");
}

// ==========================================
// 7. CORE LOGIC & MENU SYSTEM
// ==========================================

void checkAndPromoteWaitlist(int roomIdx, const char *date) {
    if (waitlistCount == 0) return;

    WaitlistEntry tempHeap[MAX_WAITLIST];
    int tempCount = 0;

    WaitlistEntry current;
    while (popWaitlist(&current)) {
        if (current.roomIdx == roomIdx && strcmp(current.date, date) == 0) {
            if (!hasOverlap(intervalTreeRoot, current.startMin, current.endMin, current.roomIdx, current.date)) {
                bookings[bookingCount].id = (int)time(NULL);
                bookings[bookingCount].roomIdx = current.roomIdx;
                strcpy(bookings[bookingCount].date, current.date);
                bookings[bookingCount].startMin = current.startMin;
                bookings[bookingCount].endMin = current.endMin;
                strcpy(bookings[bookingCount].name, current.name);
                bookingCount++;

                rebuildIntervalTree();
                printf("\n[ระบบอัตโนมัติ] เลื่อนคุณ %s จากคิวรอเข้าจองห้อง %s เรียบร้อย!\n",
                       current.name, ROOMS[current.roomIdx].name);
                continue;
            }
        }
        tempHeap[tempCount++] = current;
    }

    for (int i = 0; i < tempCount; i++) {
        pushWaitlist(tempHeap[i]);
    }
}

void bookRoomUI() {
    // 1. แสดง Dashboard ก่อนทำ Transaction
    displayDashboardSnapshot("ก่อนทำการจองห้อง (Before Transaction)");

    int roomIdx, startMin;
    char date[11], name[60], timeStr[10];

    printf("\n--- ดำเนินการจองห้อง ---\n");
    for (int i = 0; i < MAX_ROOMS; i++) {
        printf("%d. %s (%d ที่นั่ง)\n", i + 1, ROOMS[i].name, ROOMS[i].seats);
    }
    printf("เลือกหมายเลขห้อง (1-%d): ", MAX_ROOMS);
    if (scanf("%d", &roomIdx) != 1) return;
    roomIdx--;

    if (roomIdx < 0 || roomIdx >= MAX_ROOMS) {
        printf("หมายเลขห้องไม่ถูกต้อง!\n"); return;
    }

    printf("ใส่วันที่ (YYYY-MM-DD) เช่น 2026-09-29: ");
    scanf("%s", date);

    printf("เวลาที่เริ่ม (เช่น 08:00, 10:00, 13:00): ");
    scanf("%s", timeStr);
    startMin = timeStrToMin(timeStr);
    int endMin = startMin + DURATION_MIN;

    if (startMin < OPEN_MIN || endMin > CLOSE_MIN) {
        printf("ไม่อยู่ในเวลาทำการ (08:00 - 15:30 น.)\n"); return;
    }

    printf("ชื่อผู้จอง: ");
    scanf(" %[^\n]", name);

    if (hasOverlap(intervalTreeRoot, startMin, endMin, roomIdx, date)) {
        printf("\n[!] ห้องไม่ว่างในช่วงเวลานี้!\n");
        printf("ต้องการเข้าคิวรอ (Waiting List) หรือไม่? (1: เข้าคิว / 0: ยกเลิก): ");
        int choice; scanf("%d", &choice);
        if (choice == 1) {
            WaitlistEntry entry;
            entry.id = (int)time(NULL);
            entry.roomIdx = roomIdx;
            strcpy(entry.date, date);
            entry.startMin = startMin;
            entry.endMin = endMin;
            strcpy(entry.name, name);
            entry.joinedAt = time(NULL);
            pushWaitlist(entry);
            printf("เพิ่มคุณ %s เข้าคิวรอเรียบร้อยแล้ว\n", name);
        }
        return;
    }

    bookings[bookingCount].id = (int)time(NULL);
    bookings[bookingCount].roomIdx = roomIdx;
    strcpy(bookings[bookingCount].date, date);
    bookings[bookingCount].startMin = startMin;
    bookings[bookingCount].endMin = endMin;
    strcpy(bookings[bookingCount].name, name);
    bookingCount++;

    rebuildIntervalTree();
    printf("\n>>> จองห้อง %s สำเร็จ! <<<\n", ROOMS[roomIdx].name);

    // 2. แสดง Dashboard หลังทำ Transaction
    displayDashboardSnapshot("หลังทำการจองห้อง (After Transaction)");
}

void displayGridUI() {
    char date[11];
    printf("\nระบุวันที่ต้องการดูตาราง (YYYY-MM-DD): ");
    scanf("%s", date);

    printf("\n================ ตารางแสดงสถานะห้อง (%s) ================\n", date);
    printf("%-20s", "ห้อง / เวลา");
    for (int m = OPEN_MIN; m + DURATION_MIN <= CLOSE_MIN; m += 30) {
        char tStr[10]; minToTimeStr(m, tStr);
        printf("| %-5s ", tStr);
    }
    printf("|\n-------------------------------------------------------------------\n");

    for (int r = 0; r < MAX_ROOMS; r++) {
        printf("%-20s", ROOMS[r].name);
        for (int m = OPEN_MIN; m + DURATION_MIN <= CLOSE_MIN; m += 30) {
            if (hasOverlap(intervalTreeRoot, m, m + 30, r, date)) {
                printf("|  BUSY ");
            } else {
                printf("|  FREE ");
            }
        }
        printf("|\n");
    }
}

void cancelBookingUI() {
    if (bookingCount == 0) {
        printf("\nไม่มีรายการจองในระบบ\n"); return;
    }

    // 1. แสดง Dashboard ก่อนยกเลิก Transaction (Before)
    displayDashboardSnapshot("ก่อนทำการยกเลิกการจอง (Before Transaction)");

    printf("เลือกหมายเลขรายการที่ต้องการยกเลิก (1-%d / 0: ยกเลิกคำสั่ง): ", bookingCount);
    int choice; scanf("%d", &choice);
    if (choice < 1 || choice > bookingCount) return;

    int idx = choice - 1;
    int freedRoomIdx = bookings[idx].roomIdx;
    char freedDate[11];
    strcpy(freedDate, bookings[idx].date);

    for (int i = idx; i < bookingCount - 1; i++) {
        bookings[i] = bookings[i + 1];
    }
    bookingCount--;

    rebuildIntervalTree();
    printf("\n>>> ยกเลิกการจองเรียบร้อยแล้ว <<<\n");

    checkAndPromoteWaitlist(freedRoomIdx, freedDate);

    // 2. แสดง Dashboard หลังยกเลิก Transaction (After)
    displayDashboardSnapshot("หลังทำการยกเลิกการจอง (After Transaction)");
}

void recommendUI() {
    int cap; char date[11], timeStr[10];
    printf("\n--- ค้นหาห้องแนะนำ (Greedy Best-Fit) ---\n");
    printf("จำนวนผู้ใช้งาน (คน): "); scanf("%d", &cap);
    printf("วันที่ (YYYY-MM-DD): "); scanf("%s", date);
    printf("เวลาเริ่ม (เช่น 08:00, 10:00): "); scanf("%s", timeStr);

    int startMin = timeStrToMin(timeStr);
    if (startMin < OPEN_MIN || startMin + DURATION_MIN > CLOSE_MIN) {
        printf("เวลานี้อยู่นอกเวลาทำการ!\n"); return;
    }

    recommendRooms(cap, date, startMin);
}

int main() {
    srand((unsigned int)time(NULL));

#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    int choice;
    while (1) {
        printf("\n=========================================\n");
        printf("   ระบบจองห้องประชุมและห้องดูหนัง (ภาษา C)\n");
        printf("=========================================\n");
        printf("1. จองห้อง (แสดง Dashboard ก่อน-หลัง)\n");
        printf("2. ค้นหาห้องแนะนำ (Greedy Best-Fit & Single Benchmark)\n");
        printf("3. ทดสอบประสิทธิภาพเต็มรูปแบบ & วิเคราะห์ Big-O (n=1K, 10K, 100K)\n");
        printf("4. ดูตารางห้องว่าง / ไม่ว่าง\n");
        printf("5. ยกเลิกการจอง (แสดง Dashboard ก่อน-หลัง)\n");
        printf("6. แสดง Dashboard ปัจจุบัน (Tree Map & List)\n");
        printf("7. ออกจากระบบ\n");
        printf("เลือกเมนู (1-7): ");
        if (scanf("%d", &choice) != 1) break;

        switch (choice) {
            case 1: bookRoomUI(); break;
            case 2: recommendUI(); break;
            case 3: runBigOBenchmark(); break;
            case 4: displayGridUI(); break;
            case 5: cancelBookingUI(); break;
            case 6: displayDashboardSnapshot("สถานะปัจจุบัน (Current State)"); break;
            case 7:
                freeIntervalTree(intervalTreeRoot);
                printf("ออกจากระบบสำเร็จ\n");
                return 0;
            default:
                printf("กรุณาเลือกเมนู 1-7\n");
        }
    }
    return 0;
}