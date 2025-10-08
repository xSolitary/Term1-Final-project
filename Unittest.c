#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#define UNIT_TESTING
#include "OrderManagerForUnittest.c"   // include implementation so we can hit static helpers

// ------------------- mini harness (prints only on FAIL) -------------------
static int tests_run = 0, tests_failed = 0;

#define CHECK_TRUE(msg, expr) do { \
    ++tests_run; \
    if (!(expr)) { ++tests_failed; \
        printf("[FAIL] %s (at %s:%d)\n", msg, __FILE__, __LINE__); \
    } \
} while (0)

#define CHECK_EQ_INT(msg, exp, got) do { \
    ++tests_run; \
    if ((int)(exp) != (int)(got)) { ++tests_failed; \
        printf("[FAIL] %s: expected %d, got %d (at %s:%d)\n", msg, (int)(exp), (int)(got), __FILE__, __LINE__); \
    } \
} while (0)

#define CHECK_EQ_STR(msg, exp, got) do { \
    ++tests_run; \
    if (strcmp((exp),(got)) != 0) { ++tests_failed; \
        printf("[FAIL] %s: expected \"%s\", got \"%s\" (at %s:%d)\n", msg, (exp), (got), __FILE__, __LINE__); \
    } \
} while (0)

// ------------------- utilities: files, stdin, mute output -----------------
static void delete_file_if_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); remove(path); }
}

static void write_text_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    assert(f && "cannot open file for writing");
    fputs(content, f);
    fclose(f);
}

static char* read_whole_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(n + 1);
    fread(buf, 1, n, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

// feed next test input via a temp file
static void set_stdin_from_string(const char* s) {
    const char* in = "tmp_stdin.txt";
    write_text_file(in, s);
    freopen(in, "r", stdin);
}

#ifdef _WIN32
  #define DEV_NULL    "NUL"
  #define CONSOLE_OUT "CON"
#else
  #define DEV_NULL    "/dev/null"
  #define CONSOLE_OUT "/dev/tty"
#endif

// Mute/unmute app outputs; keep harness output visible.
static void mute_outputs_begin(void)  { freopen(DEV_NULL, "w", stdout); freopen(DEV_NULL, "w", stderr); }
static void mute_outputs_end(void)    { freopen(CONSOLE_OUT, "w", stdout); freopen(CONSOLE_OUT, "w", stderr); }

// Run a statement with app output muted (do NOT wrap CHECK_* inside)
#define RUN_SILENT(stmt) do { mute_outputs_begin(); stmt; mute_outputs_end(); } while (0)

// ------------------- tests for each function ------------------------------

// chomp
static void t_chomp(void) {
    char a[32]="hi\n", b[32]="x\r\n", c[32]="nope";
    chomp(a); chomp(b); chomp(c);
    CHECK_EQ_STR("chomp \\n", "hi", a);
    CHECK_EQ_STR("chomp \\r\\n", "x", b);
    CHECK_EQ_STR("chomp none", "nope", c);
}

// sanitize_commas
static void t_sanitize_commas(void) {
    char s[32] = "a,b,,c";
    sanitize_commas(s);
    CHECK_EQ_STR("commas->spaces", "a b  c", s);
}

// lowercase
static void t_lowercase(void) {
    char s[32] = "HeLLo-123";
    lowercase(s);
    CHECK_EQ_STR("lowercase", "hello-123", s);
}

// line_starts_with_digit
static void t_line_starts_with_digit(void) {
    CHECK_TRUE("digit", line_starts_with_digit("1,abc"));
    CHECK_TRUE("spaces+digit", line_starts_with_digit("   9,x"));
    CHECK_TRUE("no", !line_starts_with_digit("id,1"));
    CHECK_TRUE("empty", !line_starts_with_digit(""));
}

// parse_csv_line
static void t_parse_csv_line(void) {
    const char* line = "42,John Doe,Widget X,5,12.34,01-01-2024";
    int id,qty; float price; char customer[50],product[50],date[20];
    int ok = parse_csv_line(line, &id, customer, product, &qty, &price, date);
    CHECK_TRUE("parse ok", ok==1);
    CHECK_EQ_INT("id", 42, id);
    CHECK_EQ_INT("qty", 5, qty);
    CHECK_TRUE("price", fabsf(price-12.34f) < 1e-5f);
    CHECK_EQ_STR("customer", "John Doe", customer);
    CHECK_EQ_STR("product",  "Widget X", product);
    CHECK_EQ_STR("date",     "01-01-2024", date);

    const char* bad = "x,y,z";
    ok = parse_csv_line(bad, &id, customer, product, &qty, &price, date);
    CHECK_TRUE("parse bad -> 0", ok==0);
}

// is_valid_date_str
static void t_is_valid_date_str(void) {
    CHECK_TRUE("ok",  is_valid_date_str("15-08-2024"));
    CHECK_TRUE("leap",is_valid_date_str("29-02-2024"));
    CHECK_TRUE("no 29-02 in 2023", !is_valid_date_str("29-02-2023"));
    CHECK_TRUE("bad month", !is_valid_date_str("01-13-2024"));
    CHECK_TRUE("bad day",   !is_valid_date_str("32-01-2024"));
    CHECK_TRUE("format",    !is_valid_date_str("2024-01-01"));
}

// read_line (smoke)
static void t_read_line(void) {
    char buf[64];
    set_stdin_from_string("hello world\n");
    RUN_SILENT(read_line("prompt: ", buf, sizeof buf));
    CHECK_EQ_STR("read_line", "hello world", buf);
}

// try_parse_int
static void t_try_parse_int(void) {
    int v;
    CHECK_TRUE("int", try_parse_int("123", &v) && v==123);
    CHECK_TRUE("neg", try_parse_int("-7", &v)  && v==-7);
    CHECK_TRUE("bad", !try_parse_int("1x", &v));
    CHECK_TRUE("empty", !try_parse_int("", &v));
    // overflow guard
    char big[64];
    long long too_big = (long long)INT_MAX + 1LL;
    snprintf(big, sizeof big, "%lld", too_big);
    CHECK_TRUE("overflow rejected", !try_parse_int(big, &v));
}

// try_parse_float
static void t_try_parse_float(void) {
    float f;
    CHECK_TRUE("ok", try_parse_float("3.14", &f) && fabsf(f-3.14f)<1e-5f);
    CHECK_TRUE("neg", try_parse_float("-2.5", &f) && fabsf(f+2.5f)<1e-5f);
    CHECK_TRUE("bad", !try_parse_float("1.2.3", &f));
    CHECK_TRUE("empty", !try_parse_float("", &f));
}

// read_int_loop (invalid then valid)
static void t_read_int_loop(void) {
    int v, rc;
    set_stdin_from_string("abc\n5\n");
    RUN_SILENT(rc = read_int_loop("int:", &v, 0, 0));
    CHECK_TRUE("read_int_loop", rc==1 && v==5);
}

// read_float_loop (invalid then valid)
static void t_read_float_loop(void) {
    float v; int rc;
    set_stdin_from_string("x\n2.50\n");
    RUN_SILENT(rc = read_float_loop("flt:", &v, 1, 0.0f));
    CHECK_TRUE("read_float_loop", rc==1 && fabsf(v-2.5f)<1e-5f);
}

// read_text_loop (commas sanitized)
static void t_read_text_loop(void) {
    char out[32];
    set_stdin_from_string("A,b,c\n");
    RUN_SILENT(read_text_loop("txt:", out, sizeof out));
    CHECK_EQ_STR("sanitized", "A b c", out);
}

// read_optional_int
static void t_read_optional_int(void) {
    int out=99; int rc;
    set_stdin_from_string("\n");           // blank -> keep
    RUN_SILENT(rc = read_optional_int("opt-int:", &out));
    CHECK_TRUE("blank -> 0", rc==0 && out==99);

    set_stdin_from_string("123\n");
    RUN_SILENT(rc = read_optional_int("opt-int:", &out));
    CHECK_TRUE("set 123", rc==1 && out==123);
}

// read_optional_float
static void t_read_optional_float(void) {
    float out=1.0f; int rc;
    set_stdin_from_string("\n");
    RUN_SILENT(rc = read_optional_float("opt-f:", &out));
    CHECK_TRUE("blank -> 0", rc==0 && fabsf(out-1.0f)<1e-5f);

    set_stdin_from_string("4.75\n");
    RUN_SILENT(rc = read_optional_float("opt-f:", &out));
    CHECK_TRUE("set 4.75", rc==1 && fabsf(out-4.75f)<1e-5f);
}

// read_optional_text
static void t_read_optional_text(void) {
    char out[32]="keep"; int rc;
    set_stdin_from_string("\n");
    RUN_SILENT(rc = read_optional_text("opt-t:", out, sizeof out));
    CHECK_TRUE("blank -> 0", rc==0 && strcmp(out,"keep")==0);

    set_stdin_from_string("x,y\n");
    RUN_SILENT(rc = read_optional_text("opt-t:", out, sizeof out));
    CHECK_TRUE("set", rc==1 && strcmp(out,"x y")==0);
}

// read_date_loop (invalid then valid)
static void t_read_date_loop(void) {
    char out[32];
    set_stdin_from_string("2024-01-01\n01-01-2024\n");
    RUN_SILENT(read_date_loop("date:", out, sizeof out));
    CHECK_EQ_STR("date ok", "01-01-2024", out);
}

// read_optional_date
static void t_read_optional_date(void) {
    char out[32]="keep"; int rc;
    set_stdin_from_string("\n");
    RUN_SILENT(rc = read_optional_date("opt-d:", out, sizeof out));
    CHECK_TRUE("blank->0", rc==0 && strcmp(out,"keep")==0);

    set_stdin_from_string("15-08-2024\n");
    RUN_SILENT(rc = read_optional_date("opt-d:", out, sizeof out));
    CHECK_TRUE("set date", rc==1 && strcmp(out,"15-08-2024")==0);
}

// read_menu_choice (invalid inputs then valid)
static void t_read_menu_choice(void) {
    int c;
    set_stdin_from_string("0\nx\n6\n2\n");
    RUN_SILENT(c = read_menu_choice(1,5));
    CHECK_EQ_INT("menu=2", 2, c);
}

// ensure_csv_header + orderIDExists
static void t_ensure_and_exists(void) {
    delete_file_if_exists(CSV_FILE);
    RUN_SILENT(ensure_csv_header());
    char* s = read_whole_file(CSV_FILE);
    CHECK_TRUE("file exists", s!=NULL);
    if (s) {
        CHECK_TRUE("has header", strstr(s,"orderid,customername,productname,quantity,price,orderdate")!=NULL);
        free(s);
    }
    CHECK_TRUE("id 999 not exist", !orderIDExists(999));
}

// Addcsv (append a new row)
static void t_Addcsv(void) {
    write_text_file(CSV_FILE, "orderid,customername,productname,quantity,price,orderdate\n");
    set_stdin_from_string("101\nAlice\nWidget\n2\n9.99\n01-01-2024\n");
    RUN_SILENT(Addcsv());
    char* s = read_whole_file(CSV_FILE);
    CHECK_TRUE("row added", s && strstr(s, "101,Alice,Widget,2,9.99,01-01-2024")!=NULL);
    if (s) free(s);
    CHECK_TRUE("exists(101)", orderIDExists(101));
}

// searchByOrderID (smoke)
static void t_searchByOrderID(void) {
    write_text_file(CSV_FILE,
        "orderid,customername,productname,quantity,price,orderdate\n"
        "200,Bob,Gadget,1,5.50,02-01-2024\n"
        "201,Cara,Tool,3,7.00,03-01-2024\n");
    set_stdin_from_string("200\n");
    RUN_SILENT(searchByOrderID());
}

// searchByProductName (smoke)
static void t_searchByProductName(void) {
    write_text_file(CSV_FILE,
        "orderid,customername,productname,quantity,price,orderdate\n"
        "300,Dan,WiDGet,1,8.00,04-01-2024\n"
        "301,Eve,Thing,2,6.00,05-01-2024\n");
    set_stdin_from_string("widget\n");
    RUN_SILENT(searchByProductName());
}

// searchMenu (go in and immediately back out)
static void t_searchMenu(void) {
    set_stdin_from_string("3\n");
    RUN_SILENT(searchMenu());
}

// updateOrderByID (change just product; others blank)
static void t_updateOrderByID(void) {
    write_text_file(CSV_FILE,
        "orderid,customername,productname,quantity,price,orderdate\n"
        "400,Fay,Item,5,10.00,06-01-2024\n");
    set_stdin_from_string(
        "400\n"     // id to update
        "\n"        // keep customer
        "NewItem\n" // new product
        "\n"        // keep qty
        "\n"        // keep price
        "\n"        // keep date
    );
    RUN_SILENT(updateOrderByID());
    char* s = read_whole_file(CSV_FILE);
    CHECK_TRUE("product changed", s && strstr(s, "400,Fay,NewItem,5,10.00,06-01-2024")!=NULL);
    if (s) free(s);
}

// deleteByOrderID (two matches; choose 1; confirm Y)
static void t_deleteByOrderID(void) {
    write_text_file(CSV_FILE,
        "orderid,customername,productname,quantity,price,orderdate\n"
        "500,Ann,AAA,1,1.00,01-02-2024\n"
        "500,Ben,BBB,2,2.00,02-02-2024\n"
        "501,Cid,CCC,3,3.00,03-02-2024\n");
    set_stdin_from_string("500\n1\nY\n");
    RUN_SILENT(deleteByOrderID());
    char* s = read_whole_file(CSV_FILE);
    CHECK_TRUE("deleted one of 500", s && strstr(s,"500,Ann,AAA,1,1.00,01-02-2024")==NULL);
    CHECK_TRUE("still has other 500", s && strstr(s,"500,Ben,BBB,2,2.00,02-02-2024")!=NULL);
    CHECK_TRUE("kept 501", s && strstr(s,"501,Cid,CCC,3,3.00,03-02-2024")!=NULL);
    if (s) free(s);
}

// ------------------- runner ----------------------------------------------
int main(void) {
    // string & parsing
    t_chomp();
    t_sanitize_commas();
    t_lowercase();
    t_line_starts_with_digit();
    t_parse_csv_line();
    t_is_valid_date_str();

    // input helpers
    t_read_line();
    t_try_parse_int();
    t_try_parse_float();
    t_read_int_loop();
    t_read_float_loop();
    t_read_text_loop();
    t_read_optional_int();
    t_read_optional_float();
    t_read_optional_text();
    t_read_date_loop();
    t_read_optional_date();
    t_read_menu_choice();

    // CSV + features
    t_ensure_and_exists();
    t_Addcsv();
    t_searchByOrderID();
    t_searchByProductName();
    t_searchMenu();
    t_updateOrderByID();
    t_deleteByOrderID();

    printf("\nTests run: %d, failed: %d\n", tests_run, tests_failed);
    if (tests_failed == 0) {
        printf("[OK] All tests passed.\n");
        return 0;
    } else {
        printf("[X] Some tests failed.\n");
        return 1;
    }
}
