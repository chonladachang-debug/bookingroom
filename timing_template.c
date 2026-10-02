/* =====================================================================
   timing_template.c — โปรแกรมต้นแบบสำหรับวัดเวลาการค้นหา
   รายวิชา 01204212 แบบชนิดข้อมูลนามธรรมและการแก้ปัญหา
   โครงงาน: ระบบจองห้องสมุด (Library Room Booking System)

   วิธีคอมไพล์ : gcc timing_template.c -o timing
   วิธีรัน      : ./timing data_1000.txt targets_1000.txt
                 (บน Windows ใช้ timing.exe data_1000.txt targets_1000.txt)
   ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
//#include <windows.h> /* เพิ่มเพื่อใช้คำสั่งเปลี่ยน Encoding ของ Windows Console */

#define MAXN   100000
#define REPEAT 1000      /* จำนวนรอบที่วนค้นหาซ้ำ ใช้ค่าเดียวกันทุกการทดลอง */

int data[MAXN];        /* ข้อมูลตามลำดับในไฟล์ (ยังไม่เรียง) */
int sorted_data[MAXN]; /* สำเนาที่เรียงแล้ว สำหรับ Binary Search */
int n;

/* ---------- อ่านไฟล์ข้อมูล: บรรทัดแรกคือ n จากนั้นตามด้วยค่า n ค่า ---------- */
int LoadData(const char *filename, int arr[])
{
    FILE *fp = fopen(filename, "r");
    int  count, i;

    if (fp == NULL) {
        printf("เปิดไฟล์ %s ไม่ได้\n", filename);
        exit(1);
    }
    fscanf(fp, "%d", &count);
    for (i = 0; i < count; i++)
        fscanf(fp, "%d", &arr[i]);
    fclose(fp);
    return count;
}

/* ---------- ใช้เรียงข้อมูลก่อนทำ Binary Search (ไม่นับเวลาส่วนนี้) ---------- */
int CompareInt(const void *a, const void *b)
{
    return (*(const int *)a) - (*(const int *)b);
}

/* ---------- อัลกอริทึมที่ 1: Sequential Search ---------- */
int SequentialSearch(int arr[], int size, int target)
{
    int i;
    for (i = 0; i < size; i++)
        if (arr[i] == target)
            return i;            /* คืนตำแหน่งที่พบ */
    return -1;                   /* ไม่พบ */
}

/* ---------- อัลกอริทึมที่ 2: Binary Search (ข้อมูลต้องเรียงแล้ว) ---------- */
int BinarySearch(int arr[], int size, int target)
{
    int first = 0, last = size - 1, mid;

    while (first <= last) {
        mid = (first + last) / 2;
        if (target > arr[mid])      first = mid + 1;
        else if (target < arr[mid]) last  = mid - 1;
        else                        return mid;
    }
    return -1;
}

/* =====================================================================
   จุดที่แก้ไขตามคอมเมนต์: แทนที่ด้วย Interval Tree ของกลุ่ม
   (โครงสร้าง Interval Tree สำหรับตรวจเวลาการจองซ้อนทับ O(log N))
   ===================================================================== */

typedef struct IntervalNode {
    int low;                   /* เวลาเริ่มต้น (Start Time) */
    int high;                  /* เวลาสิ้นสุด (End Time) */
    int max;                   /* ค่า high ที่มากที่สุดใน Subtree */
    struct IntervalNode *left;
    struct IntervalNode *right;
} IntervalNode;

IntervalNode *tree_root = NULL;

/* ฟังก์ชันสร้าง Node ใหม่สำหรับ Interval Tree */
IntervalNode* CreateNode(int low, int high) {
    IntervalNode *node = (IntervalNode*)malloc(sizeof(IntervalNode));
    node->low = low;
    node->high = high;
    node->max = high;
    node->left = node->right = NULL;
    return node;
}

/* ฟังก์ชันเพิ่มช่วงเวลาเข้าสู่ Interval Tree */
IntervalNode* InsertInterval(IntervalNode *root, int low, int high) {
    if (root == NULL) return CreateNode(low, high);

    if (low < root->low)
        root->left = InsertInterval(root->left, low, high);
    else
        root->right = InsertInterval(root->right, low, high);

    if (root->max < high)
        root->max = high;

    return root;
}

/* สร้าง Interval Tree จากข้อมูลในไฟล์ก่อนเริ่มจับเวลา (ไม่ถูกนับเวลาการวัดผล) */
void BuildMyTree(int arr[], int size) {
    int i;
    for (i = 0; i < size; i++) {
        /* สมมุติระยะเวลาจองช่วงละ 120 นาที (2 ชั่วโมง) */
        tree_root = InsertInterval(tree_root, arr[i], arr[i] + 120);
    }
}

/* ฟังก์ชันค้นหาช่วงเวลาที่ซ้อนทับกัน (Interval Search) */
int SearchInterval(IntervalNode *root, int target_low, int target_high) {
    if (root == NULL) return -1;

    /* ตรวจสอบว่าช่วงเวลาทับซ้อนกันหรือไม่ */
    if (target_low < root->high && root->low < target_high)
        return 1; /* พบช่วงเวลาที่ซ้อนทับ (BUSY) */

    if (root->left != NULL && root->left->max > target_low)
        return SearchInterval(root->left, target_low, target_high);

    return SearchInterval(root->right, target_low, target_high);
}

/* ฟังก์ชัน MySearch ของกลุ่ม (แทนที่จุดที่คอมเมนต์แจ้งให้แก้ไข) */
int MySearch(int arr[], int size, int target)
{
    (void)arr; (void)size;
    /* ตรวจสอบว่าช่วงเวลา target ซ้อนทับกับรายการใน Interval Tree หรือไม่ */
    return SearchInterval(tree_root, target, target + 120);
}

/* ---------- วัดเวลาเฉลี่ยต่อการค้นหาหนึ่งครั้ง หน่วยเป็นมิลลิวินาที ---------- */
double MeasureMillisec(int (*SearchFunc)(int[], int, int),
                       int arr[], int size, int targets[], int tcount)
{
    clock_t start, end;
    double  total_sec;
    int     r, i, result = 0;

    start = clock();                                  /* เริ่มจับเวลา */
    for (r = 0; r < REPEAT; r++)
        for (i = 0; i < tcount; i++)
            result += SearchFunc(arr, size, targets[i]);
    end = clock();                                    /* หยุดจับเวลา */

    if (result == -99999999) printf(" ");  /* กันคอมไพเลอร์ตัดโค้ดทิ้ง */

    total_sec = (double)(end - start) / CLOCKS_PER_SEC;
    return total_sec * 1000.0 / (REPEAT * tcount);    /* เฉลี่ยต่อหนึ่งครั้ง */
}

int main(int argc, char *argv[])
{
    /* บังคับให้ Console ของ Windows แสดงผลด้วย UTF-8 (แก้ภาษาเอเลี่ยน) */
    //SetConsoleOutputCP(65001);

    static int targets[MAXN]; /* แก้ไขขนาดให้รองรับข้อมูลได้สูงสุด MAXN */
    int tcount, i;
    double ms_seq, ms_bin, ms_my;

    if (argc < 3) {
        printf("วิธีใช้: %s <ไฟล์ข้อมูล> <ไฟล์ค่าที่ค้นหา>\n", argv[0]);
        printf("ตัวอย่าง: %s data_1000.txt targets_1000.txt\n", argv[0]);
        return 1;
    }

    /* ---- เตรียมข้อมูล (ไม่จับเวลาส่วนนี้) ---- */
    n      = LoadData(argv[1], data);
    tcount = LoadData(argv[2], targets);

    for (i = 0; i < n; i++) sorted_data[i] = data[i];
    qsort(sorted_data, n, sizeof(int), CompareInt);

    /* สร้าง Interval Tree จากข้อมูลเดิมก่อนจับเวลา */
    BuildMyTree(data, n);

    printf("=====================================================\n");
    printf(" ไฟล์ข้อมูล       : %s\n", argv[1]);
    printf(" จำนวนข้อมูล n    : %d\n", n);
    printf(" จำนวนค่าที่ค้นหา : %d\n", tcount);
    printf(" จำนวนรอบที่วัด   : %d รอบต่อค่า\n", REPEAT);
    printf("=====================================================\n");

    /* ---- ตรวจความถูกต้องก่อนวัดเวลา ---- */
    printf("\n[ ตรวจความถูกต้องของผลการค้นหา ]\n");
    printf(" %-12s %-10s %-10s %-10s\n", "ค่าที่ค้นหา", "Sequential", "Binary", "MySearch");
    for (i = 0; i < tcount; i++) {
        int a = SequentialSearch(data, n, targets[i]);
        int b = BinarySearch(sorted_data, n, targets[i]);
        int c = MySearch(data, n, targets[i]);
        printf(" %-12d %-10s %-10s %-10s\n", targets[i],
               (a >= 0) ? "พบ" : "ไม่พบ",
               (b >= 0) ? "พบ" : "ไม่พบ",
               (c >= 0) ? "พบ" : "ไม่พบ");
    }

    /* ---- วัดเวลา ---- */
    ms_seq = MeasureMillisec(SequentialSearch, data,        n, targets, tcount);
    ms_bin = MeasureMillisec(BinarySearch,     sorted_data, n, targets, tcount);
    ms_my  = MeasureMillisec(MySearch,         data,        n, targets, tcount);

    printf("\n[ ผลการวัดเวลา เฉลี่ยต่อการค้นหาหนึ่งครั้ง ]\n");
    printf(" Sequential Search : %.6f ms\n", ms_seq);
    printf(" Binary Search     : %.6f ms\n", ms_bin);
    printf(" MySearch (ของกลุ่ม): %.6f ms\n", ms_my);
    printf("\nนำค่าที่ได้ไปกรอกในตารางบันทึกผล แล้วรันซ้ำจนครบ 5 ครั้ง\n");

    return 0;
}