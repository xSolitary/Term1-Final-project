// unittestv2.c
// Minimal unit tests, no external framework.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <limits.h>

#define UNIT_TESTING          // <--- tells testV2.c to skip its main
#include "testV2.c"           // include implementation to access static helpers

// ---- tiny test harness ----
static int tests_run = 0, tests_failed = 0;

#define EXPECT_TRUE(msg, expr) do { \
    ++tests_run; \
    if (!(expr)) { ++tests_failed; \
        printf("[FAIL] %s (at %s:%d)\n", msg, __FILE__, __LINE__); \
    } \
} while(0)

#define EXPECT_EQ_INT(msg, exp, got) do { \
    ++tests_run; \
    if ((exp) != (got)) { ++tests_failed; \
        printf("[FAIL] %s: expected %d, got %d (at %s:%d)\n", msg, (int)(exp), (int)(got), __FILE__, __LINE__); \
    } \
} while(0)

#define EXPECT_EQ_STR(msg, exp, got) do { \
    ++tests_run; \
    if (strcmp((exp),(got)) != 0) { ++tests_failed; \
        printf("[FAIL] %s: expected \"%s\", got \"%s\" (at %s:%d)\n", msg, (exp), (got), __FILE__, __LINE__); \
    } \
} while(0)

static int float_equal(float a, float b) { return fabsf(a-b) < 1e-5f; }

#define EXPECT_EQ_FLOAT(msg, exp, got) do { \
    ++tests_run; \
    if (!float_equal((float)(exp),(float)(got))) { ++tests_failed; \
        printf("[FAIL] %s: expected %.6f, got %.6f (at %s:%d)\n", msg, (double)(exp), (double)(got), __FILE__, __LINE__); \
    } \
} while(0)

// ---- helpers for file-based tests ----
static void write_text_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    assert(f && "cannot open file for writing");
    fputs(content, f);
    fclose(f);
}

static void delete_file_if_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); remove(path); }
}

// ---- unit tests for pure/string helpers ----
static void test_chomp_() {
    char s1[32] = "hello\n";
    char s2[32] = "world\r\n";
    char s3[32] = "no newline";
    chomp(s1); chomp(s2); chomp(s3);
    EXPECT_EQ_STR("chomp removes \\n", "hello", s1);
    EXPECT_EQ_STR("chomp removes \\r\\n", "world", s2);
    EXPECT_EQ_STR("chomp keeps normal", "no newline", s3);
}

static void test_sanitize_commas_() {
    char s[64] = "a,b,c,,";
    sanitize_commas(s);
    EXPECT_EQ_STR("commas -> spaces", "a b c  ", s);
}

static void test_lowercase_() {
    char s[64] = "HeLLo W0RLD!";
    lowercase(s);
    EXPECT_EQ_STR("lowercase basic", "hello w0rld!", s);
}

static void test_line_starts_with_digit_() {
    EXPECT_TRUE("digit at start", line_starts_with_digit("123,abc"));
    EXPECT_TRUE("skip spaces then digit", line_starts_with_digit("   9,ok"));
    EXPECT_TRUE("tab then digit", line_starts_with_digit("\t7,ok"));
    EXPECT_TRUE("0 at start", line_starts_with_digit("0,zero"));
    EXPECT_TRUE("non-digit start -> false", !line_starts_with_digit("id,not"));
    EXPECT_TRUE("empty -> false", !line_starts_with_digit(""));
}

static void test_parse_csv_line_() {
    const char* line_ok = "  42 , John Doe , Widget X , 5 , 12.34 , 01-01-2024";
    int id, qty; float price; char cust[50], prod[50], date[20];
    int ok = parse_csv_line(line_ok, &id, cust, prod, &qty, &price, date);
    EXPECT_TRUE("parse valid CSV", ok);
    EXPECT_EQ_INT("id parsed", 42, id);
    EXPECT_EQ_STR("customer parsed", "John Doe", cust);
    EXPECT_EQ_STR("product parsed", "Widget X", prod);
    EXPECT_EQ_INT("qty parsed", 5, qty);
    EXPECT_EQ_FLOAT("price parsed", 12.34f, price);
    EXPECT_EQ_STR("date parsed", "01-01-2024", date);

    const char* bad = "x,not,int,here";
    ok = parse_csv_line(bad, &id, cust, prod, &qty, &price, date);
    EXPECT_TRUE("parse invalid CSV returns 0", !ok);
}

static void test_is_valid_date_str_() {
    EXPECT_TRUE("valid normal", is_valid_date_str("15-08-2024"));
    EXPECT_TRUE("leap day valid", is_valid_date_str("29-02-2024"));
    EXPECT_TRUE("non-leap Feb 29 invalid", !is_valid_date_str("29-02-2023"));
    EXPECT_TRUE("month out of range", !is_valid_date_str("10-13-2024"));
    EXPECT_TRUE("day out of range", !is_valid_date_str("32-01-2024"));
    EXPECT_TRUE("year too small", !is_valid_date_str("01-01-1998"));
    EXPECT_TRUE("year too big", !is_valid_date_str("01-01-2026"));
    EXPECT_TRUE("format must be DD-MM-YYYY", !is_valid_date_str("2024-01-01"));
}

static void test_try_parse_int_() {
    int v;
    EXPECT_TRUE("simple int", try_parse_int("123", &v) && v == 123);
    EXPECT_TRUE("negative int", try_parse_int("-45", &v) && v == -45);
    EXPECT_TRUE("reject spaces inside", !try_parse_int("12 3", &v));
    EXPECT_TRUE("reject alpha", !try_parse_int("12x", &v));
    EXPECT_TRUE("reject empty", !try_parse_int("", &v));
    // boundaries
    char buf[64];
    snprintf(buf, sizeof buf, "%d", INT_MAX);
    EXPECT_TRUE("INT_MAX ok", try_parse_int(buf, &v) && v == INT_MAX);

    long long too_big = (long long)INT_MAX + 1LL;
    snprintf(buf, sizeof buf, "%lld", too_big);
    EXPECT_TRUE("overflow rejected", !try_parse_int(buf, &v));
}

static void test_try_parse_float_() {
    float f;
    EXPECT_TRUE("simple float", try_parse_float("3.14", &f) && float_equal(f, 3.14f));
    EXPECT_TRUE("neg float", try_parse_float("-2.5", &f) && float_equal(f, -2.5f));
    EXPECT_TRUE("reject alpha", !try_parse_float("1.2.3", &f));
    EXPECT_TRUE("reject empty", !try_parse_float("", &f));
    EXPECT_TRUE("big guard reject", !try_parse_float("1e100", &f));
}

// ---- light CSV file tests ----
static void test_ensure_csv_header_and_orderIDExists_() {
    // Clean slate
    delete_file_if_exists(CSV_FILE);

    // ensure creates header
    ensure_csv_header();
    FILE* f = fopen(CSV_FILE, "r");
    EXPECT_TRUE("orders.csv created", f != NULL);
    char head[256] = {0};
    if (f) { fgets(head, sizeof head, f); fclose(f); }
    EXPECT_TRUE("header present",
        strstr(head, "orderid,customername,productname,quantity,price,orderdate") != NULL);

    // Write sample rows and test lookup
    write_text_file(CSV_FILE,
        "orderid,customername,productname,quantity,price,orderdate\n"
        "1,Alice,Widget,2,9.99,10-10-2024\n"
        "2,Bob,Gadget,1,5.50,11-10-2024\n");

    EXPECT_TRUE("orderIDExists(1) true", orderIDExists(1));
    EXPECT_TRUE("orderIDExists(2) true", orderIDExists(2));
    EXPECT_TRUE("orderIDExists(3) false", !orderIDExists(3));

    // cleanup
    delete_file_if_exists(CSV_FILE);
}

int main() {
    printf("Running tests...\n");

    // string/pure helpers
    test_chomp_();
    test_sanitize_commas_();
    test_lowercase_();
    test_line_starts_with_digit_();
    test_parse_csv_line_();
    test_is_valid_date_str_();
    test_try_parse_int_();
    test_try_parse_float_();

    // light filesystem tests
    test_ensure_csv_header_and_orderIDExists_();

    printf("\nTests run: %d, failed: %d\n", tests_run, tests_failed);
    if (tests_failed == 0) {
        printf("[OK] All tests passed.\n");
        return 0;
    } else {
        printf("[X] Some tests failed.\n");
        return 1;
    }
}
