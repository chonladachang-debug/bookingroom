/* =====================================================================
   timing_template.c  —  โปรแกรมต้นแบบสำหรับวัดเวลาการค้นหา
   รายวิชา 01204212 แบบชนิดข้อมูลนามธรรมและการแก้ปัญหา
   ใช้ประกอบกิจกรรมนำเสนอโครงงาน วันที่ 5 ตุลาคม 2569

   วิธีคอมไพล์ :  gcc timing_template.c -o timing
   วิธีรัน      :  ./timing data_1000.txt targets_1000.txt
                  (บน Windows ใช้  timing.exe data_1000.txt targets_1000.txt)

   ให้แต่ละกลุ่มแทนที่ฟังก์ชัน MySearch ด้วยอัลกอริทึมการค้นหาของกลุ่มตนเอง
   ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h> /* เพิ่มเพื่อใช้คำสั่งเปลี่ยน Encoding ของ Windows Console */

#define MAXN   100000
#define REPEAT 1000      /* จำนวนรอบที่วนค้นหาซ้ำ ใช้ค่าเดียวกันทุกการทดลอง */

int data[MAXN];          /* ข้อมูลตามลำดับในไฟล์ (ยังไม่เรียง) */
int sorted_data[MAXN];   /* สำเนาที่เรียงแล้ว สำหรับ Binary Search */
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
   จุดที่แต่ละกลุ่มต้องแก้ไข
   แทนที่เนื้อในของฟังก์ชันนี้ด้วยอัลกอริทึมการค้นหาของกลุ่มตนเอง
   เช่น การค้นหาใน BST, AVL Tree, Hash Table หรือวิธีอื่นที่ออกแบบไว้
   ===================================================================== */
int MySearch(int arr[], int size, int target)
{
    return SequentialSearch(arr, size, target);   /* <-- เปลี่ยนตรงนี้ */
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
    SetConsoleOutputCP(65001);

    int    targets[100], tcount, i;
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

    printf("=====================================================\n");
    printf(" ไฟล์ข้อมูล      : %s\n", argv[1]);
    printf(" จำนวนข้อมูล n   : %d\n", n);
    printf(" จำนวนค่าที่ค้นหา : %d\n", tcount);
    printf(" จำนวนรอบที่วัด  : %d รอบต่อค่า\n", REPEAT);
    printf("=====================================================\n");

    /* ---- ตรวจความถูกต้องก่อนวัดเวลา ---- */
    printf("\n[ ตรวจความถูกต้องของผลการค้นหา ]\n");
    printf(" %-12s %-10s %-10s\n", "ค่าที่ค้นหา", "Sequential", "Binary");
    for (i = 0; i < tcount; i++) {
        int a = SequentialSearch(data, n, targets[i]);
        int b = BinarySearch(sorted_data, n, targets[i]);
        printf(" %-12d %-10s %-10s\n", targets[i],
               (a >= 0) ? "พบ" : "ไม่พบ",
               (b >= 0) ? "พบ" : "ไม่พบ");
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