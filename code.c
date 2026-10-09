#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define OPEN_MIN 480       // 08:00 น.
#define CLOSE_MIN 930      // 15:30 น.
#define DURATION_MIN 120   // 2 ชั่วโมง
#define MAX_ROOMS 6
#define MAX_BOOKINGS 100005
#define MAX_WAITLIST 500
#define REPEAT 1000

// ==========================================
// 1. DATA STRUCTURES
// ==========================================
typedef struct { char id[10], name[50], kind[20]; int seats; } Room;

Room ROOMS[MAX_ROOMS] = {
    {"movie",  "ห้องดูหนัง",      "Movie",   20},
    {"study1", "ห้องศึกษากลุ่ม 1",  "Study",   10},
    {"study2", "ห้องศึกษากลุ่ม 2",  "Study",   10},
    {"study3", "ห้องศึกษากลุ่ม 3",  "Study",   10},
    {"study4", "ห้องศึกษากลุ่ม 4",  "Study",   10},
    {"large",  "ห้องประชุมใหญ่",    "Meeting", 15}
};

typedef struct {
    int id, roomIdx, startMin, endMin, headcount;
    char date[11], name[60];
    long joinedAt;
} Booking;

Booking bookings[MAX_BOOKINGS], waitlist[MAX_WAITLIST];
int bookingCount = 0, waitlistCount = 0;

typedef struct IntervalNode {
    int low, high, max, bookingId, roomIdx;
    char date[11];
    struct IntervalNode *left, *right;
} IntervalNode;

IntervalNode *intervalTreeRoot = NULL;
typedef struct { int roomIdx, leftoverSeats; } Recommendation;

// ==========================================
// 2. TIME & INPUT UTILITIES
// ==========================================
void minToTimeStr(int min, char *buf) { sprintf(buf, "%02d:%02d", min / 60, min % 60); }

void readLine(const char *prompt, char *buf, int size) {
    printf("%s", prompt); fflush(stdout);
    if (!fgets(buf, size, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\r\n")] = '\0';
    char *p = buf; while (*p == ' ' || *p == '\t') p++;
    if (p != buf) memmove(buf, p, strlen(p) + 1);
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t')) buf[--len] = '\0';
}

bool readInt(const char *prompt, int *out) {
    char line[64]; readLine(prompt, line, sizeof(line));
    char *end; long v = strtol(line, &end, 10);
    if (end == line) return false;
    *out = (int)v; return true;
}

int parseTime(const char *s) {
    int h = -1, m = -1;
    if (sscanf(s, "%d%*[:.]%d", &h, &m) == 2);
    else if (sscanf(s, "%d", &h) == 1) {
        m = (h >= 100) ? h % 100 : 0;
        if (h >= 100) h /= 100;
    } else return -1;
    return (h >= 0 && h <= 23 && m >= 0 && m <= 59) ? h * 60 + m : -1;
}

bool validateStartTime(int startMin) {
    int lastStart = CLOSE_MIN - DURATION_MIN;
    if (startMin < OPEN_MIN || startMin > lastStart) {
        char a[10], b[10]; minToTimeStr(OPEN_MIN, a); minToTimeStr(lastStart, b);
        printf("เวลาเริ่มต้องอยู่ระหว่าง %s - %s น. (จองครั้งละ 2 ชม. ต้องจบไม่เกิน 15:30 น.)\n", a, b);
        return false;
    }
    return true;
}

bool readDate(const char *prompt, char *date) {
    readLine(prompt, date, 11);
    if (strlen(date) != 10 || date[4] != '-' || date[7] != '-') {
        printf("รูปแบบวันที่ไม่ถูกต้อง (ต้องเป็น YYYY-MM-DD)\n"); return false;
    }
    return true;
}

// ==========================================
// 3. INTERVAL TREE & HEAP QUEUE
// ==========================================
IntervalNode* insertInterval(IntervalNode *root, int low, int high, int id, int rIdx, const char *date) {
    if (!root) {
        IntervalNode *n = (IntervalNode*)malloc(sizeof(IntervalNode));
        *n = (IntervalNode){low, high, high, id, rIdx, "", NULL, NULL};
        strcpy(n->date, date); return n;
    }
    if (low < root->low) root->left = insertInterval(root->left, low, high, id, rIdx, date);
    else root->right = insertInterval(root->right, low, high, id, rIdx, date);
    if (root->max < high) root->max = high;
    return root;
}

void freeIntervalTree(IntervalNode *root) {
    if (!root) return;
    freeIntervalTree(root->left); freeIntervalTree(root->right); free(root);
}

bool hasOverlap(IntervalNode *root, int low, int high, int rIdx, const char *date) {
    if (!root) return false;
    if (root->roomIdx == rIdx && strcmp(root->date, date) == 0 && root->low < high && low < root->high) return true;
    if (root->left && root->left->max > low && hasOverlap(root->left, low, high, rIdx, date)) return true;
    return hasOverlap(root->right, low, high, rIdx, date);
}

void rebuildIntervalTree() {
    freeIntervalTree(intervalTreeRoot); intervalTreeRoot = NULL;
    for (int i = 0; i < bookingCount; i++)
        intervalTreeRoot = insertInterval(intervalTreeRoot, bookings[i].startMin, bookings[i].endMin, bookings[i].id, bookings[i].roomIdx, bookings[i].date);
}

void pushWaitlist(Booking entry) {
    if (waitlistCount >= MAX_WAITLIST) return;
    int i = waitlistCount++;
    while (i > 0 && entry.joinedAt < waitlist[(i - 1) / 2].joinedAt) {
        waitlist[i] = waitlist[(i - 1) / 2]; i = (i - 1) / 2;
    }
    waitlist[i] = entry;
}

bool popWaitlist(Booking *res) {
    if (waitlistCount <= 0) return false;
    *res = waitlist[0];
    Booking last = waitlist[--waitlistCount];
    int i = 0, child;
    while ((child = 2 * i + 1) < waitlistCount) {
        if (child + 1 < waitlistCount && waitlist[child + 1].joinedAt < waitlist[child].joinedAt) child++;
        if (last.joinedAt <= waitlist[child].joinedAt) break;
        waitlist[i] = waitlist[child]; i = child;
    }
    waitlist[i] = last; return true;
}

// ==========================================
// 4. SEARCH & BENCHMARK
// ==========================================
int Search(int headcount, const char *date, int startMin, Recommendation cand[]) {
    int endMin = startMin + DURATION_MIN, count = 0;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (headcount <= ROOMS[i].seats && !hasOverlap(intervalTreeRoot, startMin, endMin, i, date))
            cand[count++] = (Recommendation){i, ROOMS[i].seats - headcount};
    }
    for (int i = 1; i < count; i++) {
        Recommendation key = cand[i]; int j = i - 1;
        while (j >= 0 && cand[j].leftoverSeats > key.leftoverSeats) { cand[j + 1] = cand[j]; j--; }
        cand[j + 1] = key;
    }
    return count;
}

void generateMockBookings(int n) {
    bookingCount = (n > MAX_BOOKINGS) ? MAX_BOOKINGS : n;
    for (int i = 0; i < bookingCount; i++) {
        int start = OPEN_MIN + (rand() % (CLOSE_MIN - OPEN_MIN - 60));
        bookings[i] = (Booking){i + 1, rand() % MAX_ROOMS, start, start + 60 + (rand() % 60), 1 + rand() % 10, "2026-10-05", "", 0};
        sprintf(bookings[i].name, "User_%d", i + 1);
    }
    rebuildIntervalTree();
}

void runBigOBenchmark() {
    int sizes[3] = {1000, 10000, 100000}; double avg[3]; Recommendation cand[MAX_ROOMS];
    printf("\n=========================================\n   เริ่มการทดสอบประสิทธิภาพและเทียบ Big-O\n=========================================\n");
    int old_count = bookingCount;
    Booking *old_b = old_count ? (Booking*)malloc(sizeof(Booking) * old_count) : NULL;
    if (old_b) memcpy(old_b, bookings, sizeof(Booking) * old_count);

    for (int s = 0; s < 3; s++) {
        generateMockBookings(sizes[s]);
        clock_t start = clock(); int res = 0;
        for (int i = 0; i < REPEAT; i++) res = Search(5, "2026-10-05", 540, cand);
        avg[s] = ((double)(clock() - start) / CLOCKS_PER_SEC * 1000.0) / REPEAT;
        printf("\nn = %d | เวลาเฉลี่ย: %.6f ms | ผลการค้นหา: %d ห้อง\n", sizes[s], avg[s], res);
    }

    double r1 = avg[1] / (avg[0] > 0 ? avg[0] : 1e-6), r2 = avg[2] / (avg[1] > 0 ? avg[1] : 1e-6);
    double k1 = log10(r1), k2 = log10(r2), avg_k = (k1 + k2) / 2.0;
    printf("\n== วิเคราะห์เทียบ Big-O ==\n1K->10K: %.2fx (k=%.2f) | 10K->100K: %.2fx (k=%.2f)\n", r1, k1, r2, k2);
    printf("ผลประเมิน: %s\n=========================================\n",
           avg_k < 0.35 ? "O(log n) หรือ O(1)" : (avg_k <= 1.3 ? "O(n)" : "O(n log n) หรือ O(n^2)"));

    if (old_b) { memcpy(bookings, old_b, sizeof(Booking) * old_count); bookingCount = old_count; free(old_b); }
    else bookingCount = 0;
    rebuildIntervalTree();
}

// ==========================================
// 5. DASHBOARD & UI
// ==========================================
void printTree2D(IntervalNode *root, int space) {
    if (!root) return;
     void printTree(struct Node* root);