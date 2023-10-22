#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(x, y) (((x) < (y)) ? (x) : (y))

#define MAX_LENGTH 100

#define GAP_COST 2
#define MISMATCH_COST 10 // Because it is 5 for both sequences, so the total cost is 10

/* Prototypes */
void print_table(int **table, int n, int m, char *x, char *y);
int **allocate_table(int n, int m);
void initialize_table(int **table, int n, int m, char *x, char *y);
int match_or_mismatch(int i, int j, char *x, char *y);
void solve(int **table, int n, int m, char *x, char *y, char *x_align, char *y_align);

int main(int argc, char* argv[]) {
    FILE * fp = fopen("input.txt", "r");
    if (fp == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    char x[MAX_LENGTH];
    char y[MAX_LENGTH];

    fscanf(fp, "%s", x);
    fscanf(fp, "%s", y);

    int n = strlen(y);
    int m = strlen(x);

    printf("x: %s, len(x): %d\n", x, m);
    printf("y: %s, len(y): %d\n", y, n);

    int **C = allocate_table(n+1, m+1);

    initialize_table(C, n, m, x, y);

    printf("The dp table is:\n");
    print_table(C, n, m, x, y);

    char x_align[MAX_LENGTH];
    char y_align[MAX_LENGTH];

    solve(C, n, m, x, y, x_align, y_align);

    printf("The alignment is:\n");
    printf("%s\n", x_align);
    printf("%s\n", y_align);
}

void print_table(int **table, int n, int m, char *x, char *y) {
    printf("       |");
    for(int i = 0; i<m; i++) {
        printf("%3c|", x[i]);
    }
    printf("\n");
    for(int i = 0; i<=m; i++) {
        printf("-----");
    }
    printf("\n");
    for(int i = 0; i<=n; i++) {
        if (i == 0) {
            printf("   |");
        } else {
            printf(" %c |", y[i-1]);
        }
        for(int j = 0; j<=m; j++) {
            printf("%3d|", table[i][j]);
        }
        printf("\n");
    }
}

int **allocate_table(int n, int m) {
    int **table = malloc(sizeof(int *) * n);
    for(int i = 0; i<n; i++) {
        table[i] = malloc(sizeof(int) * m);
    }
    return table;
}

void initialize_table(int **table, int n, int m, char *x, char *y) {
    table[0][0] = 0;

    for(int i = 1; i <= n; i++) {
        table[i][0] = table[i-1][0] + GAP_COST;
    }

    for(int j = 1; j <= m; j++) {
        table[0][j] = table[0][j-1] + GAP_COST;
    }

    // C[i][j]=min{C[i-1][j]+GAP_COST, C[i][j-1]+GAP_COST, C[i-1][j-1]+MISMATCH_COST if x[i]!=y[j], C[i-1][j-1] if x[i]==y[j]}
    for(int i = 1; i<=n; i++) {
        for(int j = 1; j<=m; j++) {
            table[i][j] = min(
                min(
                    table[i-1][j] + GAP_COST,
                    table[i][j-1] + GAP_COST
                    ),
                table[i-1][j-1] + match_or_mismatch(j-1, i-1, x, y));
        }
    }
}

int match_or_mismatch(int i, int j, char *x, char *y) {
    if (x[i] == y[j]) {
        return 0;
    } else {
        return MISMATCH_COST;
    }
}

void solve(int **table, int n, int m, char *x, char *y, char *x_align, char *y_align) {
    int i = n;
    int j = m;
    int k = 0;
    int num_mismatches = 0;
    int num_gaps = 0;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && table[i][j] == table[i-1][j-1] + match_or_mismatch(j-1, i-1, x, y)) {
            if(x[j-1] == y[i-1]) {
                // Match
                x_align[k] = x[j-1];
                y_align[k] = y[i-1];
            } else {
                // Mismatch
                x_align[k] = '*';
                y_align[k] = '*';
                num_mismatches++;
            }
            i--;
            j--;
        } else if (i > 0 && table[i][j] == table[i-1][j] + GAP_COST) {
            // Gap in x
            x_align[k] = '-';
            y_align[k] = y[i-1];
            num_gaps++;
            i--;
        } else {
            // Gap in y
            x_align[k] = x[j-1];
            y_align[k] = '-';
            num_gaps++;
            j--;
        }
        k++;
    }

    for(int i = 0; i<k/2; i++) {
        char temp = x_align[i];
        x_align[i] = x_align[k-i-1];
        x_align[k-i-1] = temp;
    }
    for(int i = 0; i<k/2; i++) {
        char temp = y_align[i];
        y_align[i] = y_align[k-i-1];
        y_align[k-i-1] = temp;
    }
    x_align[k] = '\0';
    y_align[k] = '\0';

    printf("Number of mismatches: %d\n", num_mismatches);
    printf("Number of gaps: %d\n", num_gaps);
    printf("Total cost: %d\n", table[n][m]); // Or num_mismatches * MISMATCH_COST + num_gaps * GAP_COST
}