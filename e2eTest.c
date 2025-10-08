#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #define APP_EXE   ".\\orders_app.exe"
  #define BUILD_CMD "cmd /c \"gcc -std=c99 -O2 -DCSV_FILE=\\\"Unittestorders.csv\\\" Ordermanager.c -o orders_app.exe\""
  // quote exe + redirect because folder name may have spaces
  #define RUN_FMT   "cmd /c \"\"%s\" < \"e2e_in.txt\" > \"e2e_out.txt\"\""
#else
  #define APP_EXE   "./orders_app"
  #define BUILD_CMD "gcc -std=c99 -O2 -DCSV_FILE=\\\"Unittestorders.csv\\\" Ordermanager.c -o orders_app"
  #define RUN_FMT   "%s < e2e_in.txt > e2e_out.txt"
#endif

static void write_text_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    fputs(content, f);
    fclose(f);
}
static void delete_if_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); remove(path); }
}
static int file_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}
static int out_contains(const char* needle) {
    FILE* f = fopen("e2e_out.txt", "r");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(n + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, n, f);
    buf[n] = '\0';
    fclose(f);
    int ok = strstr(buf, needle) != NULL;
    free(buf);
    return ok;
}

int main(void) {
    // Clean slate
    delete_if_exists("Unittestorders.csv");
    delete_if_exists("e2e_in.txt");
    delete_if_exists("e2e_out.txt");

    // Build app if missing
    if (!file_exists(APP_EXE)) {
        int brc = system(BUILD_CMD);
        if (brc != 0 || !file_exists(APP_EXE)) {
            printf("[E2E] FAIL: build failed (rc=%d). Is gcc in PATH?\n", brc);
            return 1;
        }
    }

    // Script:
    // 1 Add 9001,Zed,Bolt,2,3.50,01-01-2024
    // 2 Search -> [1] by ID 9001 -> back
    // 3 Search -> [2] by product "Bolt" (should find one)
    // 4 Update 9001 -> change product to "BoltX"
    // 5 Search -> [2] by product "boltx" (case-insensitive, should find one)
    // 6 Delete 9001
    // 7 Exit
    const char* script =
        "1\n"
        "9001\n"
        "Zed\n"
        "Bolt\n"
        "2\n"
        "3.50\n"
        "01-01-2024\n"
        "2\n"      // Search
        "1\n"      // by Order ID
        "9001\n"
        "3\n"      // Back to main
        "2\n"      // Search again
        "2\n"      // by Product Name
        "Bolt\n"   // product substring (before update)
        "3\n"      // Back to main
        "3\n"      // Update by ID
        "9001\n"
        "\n"       // keep customer
        "BoltX\n"  // change product
        "\n"       // keep qty
        "\n"       // keep price
        "\n"       // keep date
        "2\n"      // Search again
        "2\n"      // by Product Name
        "boltx\n"  // lowercased search after update
        "3\n"      // Back to main
        "4\n"      // Delete
        "9001\n"
        "Y\n"
        "5\n";     // Exit

    write_text_file("e2e_in.txt", script);

    // Run app
    char run_cmd[512];
    snprintf(run_cmd, sizeof run_cmd, RUN_FMT, APP_EXE);
    int rc = system(run_cmd);
    if (rc != 0) {
        printf("[E2E] FAIL: app did not run successfully (rc=%d)\n", rc);
        return 1;
    }

    // Assertions
    int fails = 0;

    // After Add
    if (!out_contains("Added: 9001,Zed,Bolt,2,3.50,01-01-2024")) {
        printf("[E2E] FAIL: 'Added' line not found.\n"); fails++;
    }
    // Search by ID
    if (!out_contains("Found: 9001, Zed, Bolt, 2, 3.50, 01-01-2024")) {
        printf("[E2E] FAIL: 'Found by ID' line not found.\n"); fails++;
    }
    // Search by product BEFORE update (should show product "Bolt")
    if (!out_contains("Matches for \"Bolt\":")) {
        printf("[E2E] FAIL: 'Matches for \"Bolt\":' header not found.\n"); fails++;
    }
    if (!out_contains("9001, Zed, Bolt, 2, 3.50, 01-01-2024")) {
        printf("[E2E] FAIL: 'product search before update' row not found.\n"); fails++;
    }
    // Update done
    if (!out_contains("Order 9001 updated successfully.")) {
        printf("[E2E] FAIL: 'updated successfully' line not found.\n"); fails++;
    }
    // Search by product AFTER update (should show product "BoltX", case-insensitive search text)
    if (!out_contains("Matches for \"boltx\":")) {
        printf("[E2E] FAIL: 'Matches for \"boltx\":' header not found.\n"); fails++;
    }
    if (!out_contains("9001, Zed, BoltX, 2, 3.50, 01-01-2024")) {
        printf("[E2E] FAIL: 'product search after update' row not found.\n"); fails++;
    }
    // Delete done
    if (!out_contains("Deleted record [1] for OrderID 9001 successfully.")) {
        printf("[E2E] FAIL: 'deleted successfully' line not found.\n"); fails++;
    }
    // Exit confirmed
    if (!out_contains("End of program")) {
        printf("[E2E] FAIL: 'End of program' line not found.\n"); fails++;
    }

    if (fails == 0) {
        printf("[E2E] PASS: full flow OK.\n");
        return 0;
    } else {
        printf("[E2E] DONE with %d failure(s). See e2e_out.txt for details.\n", fails);
        return 1;
    }
}
