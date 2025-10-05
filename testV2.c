#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define CSV_FILE "orders.csv"


static void chomp(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

static void sanitize_commas(char *s) {
    for (char *p = s; *p; ++p) if (*p == ',') *p = ' ';  // keep CSV simple
}

static void lowercase(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

static int line_starts_with_digit(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return *s && isdigit((unsigned char)*s);
}


//CSV parse id,customer,product,qty,price,date
static int parse_csv_line(const char *line,
                          int *orderid, char *customer, char *product,
                          int *qty, float *price, char *date) {
    return sscanf(line, " %d , %49[^,] , %49[^,] , %d , %f , %19[^\n]",
                  orderid, customer, product, qty, price, date) == 6;
}

// Basic DD-MM-YYYY validation
static int is_valid_date_str(const char *s) {
    int d, m, y;
    if (sscanf(s, "%2d-%2d-%4d", &d, &m, &y) != 3) return 0;
    if (y < 1999 || y > 2025) return 0;
    if (m < 1 || m > 12) return 0;
    int mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = ( (y%4==0 && y%100!=0) || (y%400==0) );
    if (leap) mdays[2] = 29;
    if (d < 1 || d > mdays[m]) return 0;
    return 1;
}

/* Safe input (loops until valid)  */

static void read_line(const char *prompt, char *buf, size_t cap) {
    for (;;) {
        if (prompt && *prompt) printf("%s", prompt);
        if (!fgets(buf, cap, stdin)) { clearerr(stdin); continue; }
        chomp(buf);
        return;
    }
}

static int try_parse_int(const char *s, int *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    if (v < INT_MIN || v > INT_MAX) return 0;
    *out = (int)v;
    return 1;
}

static int try_parse_float(const char *s, float *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return 0;
    if (v < -1e30 || v > 1e30) return 0; // arbitrary big guard
    *out = (float)v;
    return 1;
}

/* loops until a valid integer (with optional min constraint) */
static int read_int_loop(const char *prompt, int *out, int enforce_min, int minval) {
    char buf[128];
    for (;;) {
        read_line(prompt, buf, sizeof buf);
        if (try_parse_int(buf, out) && (!enforce_min || *out >= minval)) return 1;
        if (enforce_min)
            printf("Invalid input. Please enter an integer >= %d.\n", minval);
        else
            printf("Invalid input. Please enter an integer.\n");
    }
}

/* loops until a valid float (with optional min constraint) */
static int read_float_loop(const char *prompt, float *out, int enforce_min, float minval) {
    char buf[128];
    for (;;) {
        read_line(prompt, buf, sizeof buf);
        if (try_parse_float(buf, out) && (!enforce_min || *out >= minval)) return 1;
        if (enforce_min)
            printf("Invalid input. Please enter a number >= %.2f.\n", minval);
        else
            printf("Invalid input. Please enter a number.\n");
    }
}

/* loops until non-empty text; disallows commas */
static void read_text_loop(const char *prompt, char *out, size_t cap) {
    for (;;) {
        read_line(prompt, out, cap);
        sanitize_commas(out);
        if (out[0] != '\0') return;
        printf("Input cannot be empty.\n");
    }
}

//optional versions for update (blank = keep)
static int read_optional_int(const char *prompt, int *out_value) {
    char buf[128];
    read_line(prompt, buf, sizeof buf);
    if (buf[0] == '\0') return 0; // keep
    int v;
    if (!try_parse_int(buf, &v)) { printf("Not a valid integer. Keeping old value.\n"); return 0; }
    *out_value = v; return 1;
}

static int read_optional_float(const char *prompt, float *out_value) {
    char buf[128];
    read_line(prompt, buf, sizeof buf);
    if (buf[0] == '\0') return 0;
    float v;
    if (!try_parse_float(buf, &v)) { printf("Not a valid number. Keeping old value.\n"); return 0; }
    *out_value = v; return 1;
}

static int read_optional_text(const char *prompt, char *dst, size_t cap) {
    char buf[256];
    read_line(prompt, buf, sizeof buf);
    if (buf[0] == '\0') return 0;
    sanitize_commas(buf);
    strncpy(dst, buf, cap - 1); dst[cap - 1] = '\0';
    return 1;
}

//date with loop
static void read_date_loop(const char *prompt, char *dst, size_t cap) {
    for (;;) {
        read_line(prompt, dst, cap);
        if (is_valid_date_str(dst)) return;
        printf("Invalid date. Use DD-MM-YYYY , 0x not allow use x instead and a real calendar date.\n");
    }
}

static int read_optional_date(const char *prompt, char *dst, size_t cap) {
    char buf[64];
    read_line(prompt, buf, sizeof buf);
    if (buf[0] == '\0') return 0;
    if (!is_valid_date_str(buf)) { printf("Invalid date. Keeping old value.\n"); return 0; }
    strncpy(dst, buf, cap - 1); dst[cap - 1] = '\0';
    return 1;
}

/* ======================= CSV file helpers ======================= */

static void ensure_csv_header(void) {
    FILE *f = fopen(CSV_FILE, "r");
    if (!f) {
        FILE *w = fopen(CSV_FILE, "w");
        if (w) {
            fputs("orderid,customername,productname,quantity,price,orderdate\n", w);
            fclose(w);
        }
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    if (size == 0) {
        FILE *w = fopen(CSV_FILE, "w");
        if (w) {
            fputs("orderid,customername,productname,quantity,price,orderdate\n", w);
            fclose(w);
        }
    }
}

static int orderIDExists(int target) {
    FILE *f = fopen(CSV_FILE, "r");
    if (!f) return 0;

    char line[512];
    long pos = ftell(f);
    if (fgets(line, sizeof line, f)) {
        if (line_starts_with_digit(line)) fseek(f, pos, SEEK_SET);
    } else { fclose(f); return 0; }

    while (fgets(line, sizeof line, f)) {
        int id, qty; float price;
        char customer[50], product[50], date[20];
        if (!parse_csv_line(line, &id, customer, product, &qty, &price, date)) continue;
        if (id == target) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

/* ======================= Features ======================= */

static void Addcsv(void) {
    ensure_csv_header();

    int id, qty;
    float price;
    char customer[50], product[50], date[20];

    // unique id
    for (;;) {
        read_int_loop("Enter Order ID: ", &id, 0, 0);
        if (!orderIDExists(id)) break;
        printf("Order ID %d already exists. Try another.\n", id);
    }

    //valid data type
    read_text_loop("Customer name: ", customer, sizeof customer);
    read_text_loop("Product name: ",  product,  sizeof product);
    read_int_loop ("Quantity (>=0): ", &qty, 1, 0);
    read_float_loop("Price (>=0): ", &price, 1, 0.0f);
    read_date_loop ("Order date (DD-MM-YY): ", date, sizeof date);

    FILE *f = fopen(CSV_FILE, "a");
    if (!f) { perror(CSV_FILE); return; }
    fprintf(f, "%d,%s,%s,%d,%.2f,%s\n", id, customer, product, qty, price, date);
    fclose(f);
    printf("Added: %d,%s,%s,%d,%.2f,%s\n", id, customer, product, qty, price, date);
}

static void searchByOrderID(void) {
    FILE *file = fopen(CSV_FILE, "r");
    if (!file) { perror(CSV_FILE); return; }

    int id;
    read_int_loop("Enter Order ID to search: ", &id, 0, 0);

    char line[512];
    int found = 0;

    long pos = ftell(file);
    if (fgets(line, sizeof line, file)) {
        if (line_starts_with_digit(line)) fseek(file, pos, SEEK_SET);
    } else { fclose(file); printf("No data.\n"); return; }

    while (fgets(line, sizeof line, file)) {
        int orderid, qty; float price;
        char customer[50], product[50], date[20];
        if (!parse_csv_line(line, &orderid, customer, product, &qty, &price, date)) continue;

        if (orderid == id) {
            printf("Found: %d, %s, %s, %d, %.2f, %s\n",
                   orderid, customer, product, qty, price, date);
            found = 1;
            break;
        }
    }
    if (!found) printf("OrderID %d not found.\n", id);
    fclose(file);
}

static void searchByProductName(void) {
    FILE *file = fopen(CSV_FILE, "r");
    if (!file) { perror(CSV_FILE); return; }

    char needle[64];
    read_text_loop("Enter product name (substring, case-insensitive): ", needle, sizeof needle);

    char needle_lc[64];
    strncpy(needle_lc, needle, sizeof needle_lc - 1);
    needle_lc[sizeof needle_lc - 1] = '\0';
    lowercase(needle_lc);

    char line[512];
    int printed_header = 0, matches = 0;

    long pos = ftell(file);
    if (fgets(line, sizeof line, file)) {
        if (line_starts_with_digit(line)) fseek(file, pos, SEEK_SET);
    } else { fclose(file); printf("No data.\n"); return; }

    while (fgets(line, sizeof line, file)) {
        int orderid, qty; float price;
        char customer[50], product[50], date[20];
        if (!parse_csv_line(line, &orderid, customer, product, &qty, &price, date)) continue;

        char product_lc[50];
        strncpy(product_lc, product, sizeof product_lc - 1);
        product_lc[sizeof product_lc - 1] = '\0';
        lowercase(product_lc);

        if (strstr(product_lc, needle_lc)) {
            if (!printed_header) {
                printf("Matches for \"%s\":\n", needle);
                printed_header = 1;
            }
            printf("%d, %s, %s, %d, %.2f, %s\n",
                   orderid, customer, product, qty, price, date);
            matches++;
        }
    }

    if (!matches) printf("No orders found for product containing \"%s\".\n", needle);
    fclose(file);
}

static void updateOrderByID(void) {
    FILE *in = fopen(CSV_FILE, "r");
    if (!in) { perror(CSV_FILE); return; }

    FILE *out = fopen("orders.tmp", "w");
    if (!out) { perror("orders.tmp"); fclose(in); return; }

    int target;
    read_int_loop("Enter Order ID to update: ", &target, 0, 0);

    char line[512];
    int found = 0;

    long pos = ftell(in);
    if (fgets(line, sizeof line, in)) {
        if (!line_starts_with_digit(line)) {
            fputs(line, out); /* copy header */
        } else {
            fseek(in, pos, SEEK_SET);
        }
    } else { printf("File is empty.\n"); fclose(in); fclose(out); remove("orders.tmp"); return; }

    while (fgets(line, sizeof line, in)) {
        int orderid, qty; float price;
        char customer[50], product[50], date[20];

        if (!parse_csv_line(line, &orderid, customer, product, &qty, &price, date)) {
            fputs(line, out); /* preserve unknown lines */
            continue;
        }

        if (orderid == target) {
            found = 1;
            printf("Current: %d, %s, %s, %d, %.2f, %s\n",
                   orderid, customer, product, qty, price, date);

            //optional edits
            if (read_optional_text ("New customer name (leave blank to keep): ", customer, sizeof customer)) { /* ok */ }
            if (read_optional_text ("New product name  (leave blank to keep): ", product,  sizeof product))  { /* ok */ }

            int new_qty;
            if (read_optional_int("New quantity (leave blank to keep): ", &new_qty)) {
                if (new_qty < 0) printf("Quantity must be >= 0. Keeping old value.\n");
                else qty = new_qty;
            }

            float new_price;
            if (read_optional_float("New price (leave blank to keep): ", &new_price)) {
                if (new_price < 0.f) printf("Price must be >= 0. Keeping old value.\n");
                else price = new_price;
            }

            if (read_optional_date("New order date DD-MM-YYYY (leave blank to keep): ", date, sizeof date)) { /* ok */ }

            fprintf(out, "%d,%s,%s,%d,%.2f,%s\n",
                    orderid, customer, product, qty, price, date);
        } else {
            fputs(line, out);
        }
    }

    fclose(in);
    if (fclose(out) != 0) { perror("close tmp"); remove("orders.tmp"); return; }

    if (!found) {
        printf("OrderID %d not found. No changes made.\n", target);
        remove("orders.tmp");
        return;
    }

    if (remove(CSV_FILE) != 0) { perror("remove original"); remove("orders.tmp"); return; }
    if (rename("orders.tmp", CSV_FILE) != 0) { perror("rename tmp->csv"); return; }

    printf("Order %d updated successfully.\n", target);
}

/* ======================= Delete by OrderID (choose line + confirm) ======================= */
static void deleteByOrderID(void) {
    FILE *in = fopen(CSV_FILE, "r");
    if (!in) { perror(CSV_FILE); return; }

    int target;
    read_int_loop("Enter Order ID to delete: ", &target, 0, 0);

    /* First pass: collect matches so user can choose which one to delete */
    char line[512];
    int has_header = 0;
    int matches = 0;

    /* Peek header */
    long pos = ftell(in);
    if (fgets(line, sizeof line, in)) {
        if (!line_starts_with_digit(line)) has_header = 1;
        else fseek(in, pos, SEEK_SET);
    } else { fclose(in); printf("No data.\n"); return; }

    /* We’ll store a small snapshot of matches for display */
    typedef struct {
        int orderid, qty;
        float price;
        char customer[50], product[50], date[20];
    } Row;
    Row found[1024];

    while (fgets(line, sizeof line, in)) {
        int orderid, qty; float price;
        char customer[50], product[50], date[20];
        if (!parse_csv_line(line, &orderid, customer, product, &qty, &price, date)) continue;
        if (orderid == target) {
            if (matches < (int)(sizeof found / sizeof found[0])) {
                found[matches].orderid = orderid;
                found[matches].qty = qty;
                found[matches].price = price;
                strncpy(found[matches].customer, customer, sizeof found[matches].customer - 1);
                found[matches].customer[sizeof found[matches].customer - 1] = '\0';
                strncpy(found[matches].product, product, sizeof found[matches].product - 1);
                found[matches].product[sizeof found[matches].product - 1] = '\0';
                strncpy(found[matches].date, date, sizeof found[matches].date - 1);
                found[matches].date[sizeof found[matches].date - 1] = '\0';
            }
            matches++;
        }
    }

    if (matches == 0) {
        fclose(in);
        printf("OrderID %d not found. Nothing to delete.\n", target);
        return;
    }

    printf("\nFound %d record(s) with OrderID %d:\n", matches, target);
    for (int i = 0; i < matches && i < 1024; ++i) {
        printf("  [%d] %d, %s, %s, %d, %.2f, %s\n",
               i + 1,
               found[i].orderid, found[i].customer, found[i].product,
               found[i].qty, found[i].price, found[i].date);
    }
    if (matches > 1024) {
        printf("  ...and %d more (only first 1024 shown)\n", matches - 1024);
    }

    /* Choose which matching line to delete (1..matches) */
    int choice_index = 1;
    if (matches > 1) {
        for (;;) {
            if (read_int_loop("Choose which one to delete [1..N]: ", &choice_index, 1, 1)) {
                if (choice_index >= 1 && choice_index <= matches) break;
            }
            printf("Please choose a number between 1 and %d.\n", matches);
        }
    }

    /* Confirm deletion */
    char confirm[16];
    read_line("Confirm delete? (Y/N): ", confirm, sizeof confirm);
    if (!(confirm[0] == 'Y' || confirm[0] == 'y')) {
        fclose(in);
        printf("Canceled. No changes made.\n");
        return;
    }

    /* Second pass: write everything except the selected occurrence of that OrderID */
    FILE *out = fopen("orders.tmp", "w");
    if (!out) { perror("orders.tmp"); fclose(in); return; }

    rewind(in);
    int current_match_idx = 0;

    /* Copy header if present */
    if (has_header && fgets(line, sizeof line, in)) {
        fputs(line, out);
    } else {
        /* If no header was present, ensure we start at beginning */
        if (!has_header) rewind(in);
    }

    while (fgets(line, sizeof line, in)) {
        int orderid, qty; float price;
        char customer[50], product[50], date[20];

        if (!parse_csv_line(line, &orderid, customer, product, &qty, &price, date)) {
            fputs(line, out);  /* keep unparsable lines */
            continue;
        }

        if (orderid == target) {
            current_match_idx++;
            if (current_match_idx == choice_index) {
                /* Skip writing this one = delete */
                continue;
            }
        }
        fputs(line, out);
    }

    fclose(in);
    if (fclose(out) != 0) { perror("close tmp"); remove("orders.tmp"); return; }

    if (remove(CSV_FILE) != 0) { perror("remove original"); remove("orders.tmp"); return; }
    if (rename("orders.tmp", CSV_FILE) != 0) { perror("rename tmp->csv"); return; }

    printf("Deleted record [%d] for OrderID %d successfully.\n", choice_index, target);
}


/* ======================= Menu ======================= */

static int read_menu_choice(int minc, int maxc) {
    char buf[64];
    int c;
    for (;;) {
        read_line("Choose: ", buf, sizeof buf);
        if (try_parse_int(buf, &c) && c >= minc && c <= maxc) return c;
        printf("Please enter a number between %d and %d.\n", minc, maxc);
    }
}

static void searchMenu(void) {
    for (;;) {
        printf("\n-- Search Menu --\n");
        printf("[1] By Order ID\n");
        printf("[2] By Product Name\n");
        printf("[3] Back\n");
        int choice = read_menu_choice(1, 3);
        if (choice == 1)      searchByOrderID();
        else if (choice == 2) searchByProductName();
        else break;
    }
}

//main

int main(void) {
    ensure_csv_header();

    for (;;) {
        printf("\n==== Orders CSV App (safe input) ====\n");
        printf("[1] Add order\n");
        printf("[2] Search\n");
        printf("[3] Update by ID\n");
        printf("[4] Delete by ID\n");
        printf("[5] Exit\n");
        int choice = read_menu_choice(1, 4);

        switch (choice) {
            case 1: Addcsv(); break;
            case 2: searchMenu(); break;
            case 3: updateOrderByID(); break;
            case 4: deleteByOrderID(); break;
            case 5: printf("End of program\n"); return 0;
        }
    }
}
