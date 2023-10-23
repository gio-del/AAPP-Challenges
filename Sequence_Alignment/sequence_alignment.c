#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

#define MAX_LENGTH 100
#define UNREACHABLE -1

#define MATCH_COST 0
#define GAP_COST 2
#define MISMATCH_COST 10 // Because it is 5 for both sequences, so the total cost is 10

/* Prototypes */
void print_table(int **table, int n, int m, char *x, char *y);
int **allocate_table(int n, int m);
void initialize_table_minimum_cost(int **table, int n, int m, char *x, char *y);
void initialize_table_minimum_length(int **table, int n, int m, char *x, char *y);
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

    int **C_minimum_cost = allocate_table(n+1, m+1);

    initialize_table_minimum_cost(C_minimum_cost, n, m, x, y);

    printf("The dp table for the minimum cost alignment is:\n");
    print_table(C_minimum_cost, n, m, x, y);

    char x_align_minimum_cost[MAX_LENGTH];
    char y_align_minimum_cost[MAX_LENGTH];

    solve(C_minimum_cost, n, m, x, y, x_align_minimum_cost, y_align_minimum_cost);

    printf("The minimum cost alignment is:\n");
    printf("%s\n", x_align_minimum_cost);
    printf("%s\n", y_align_minimum_cost);


    printf("=====================================\n");

    int **C_minimum_length = allocate_table(n+1, m+1);

    initialize_table_minimum_length(C_minimum_length, n, m, x, y);

    printf("The dp table for the minimum length alignment is:\n");
    print_table(C_minimum_length, n, m, x, y);

    char x_align_minimum_length[MAX_LENGTH];
    char y_align_minimum_length[MAX_LENGTH];

    solve(C_minimum_length, n, m, x, y, x_align_minimum_length, y_align_minimum_length);

    printf("The minimum length alignment is:\n");
    printf("%s\n", x_align_minimum_length);
    printf("%s\n", y_align_minimum_length);
}

void initialize_table_minimum_length(int **table, int n, int m, char *x, char *y) {
    // What is the idea of the minimum length?? At each cell of the matrix we have ALSO the number of gaps inserted to arrive at that cell.

    int max_gaps = max(n, m) - min(n, m);

    // The idea is that we can have at most max_gaps gaps in the shortest sequence, so we can have at most max_gaps gaps in the alignment.
    // In this case we look for minimization of cumulated gaps instead of minimization of cumulated cost.
    // Only if we have the same cum_gaps coming from two different cells, we look at the cost to decide which one to choose.

    // Implementation:
    // Remember cum_gaps are in cum_gaps matrix, while costs are in table matrix.
    int **cum_gaps = allocate_table(n+1, m+1);

    table[0][0] = 0;
    cum_gaps[0][0] = 0;

    for(int i=1; i<=max_gaps; i++) { // First row: we can only have gaps, specifically max_gaps gaps
        table[i][0] = table[i-1][0] + GAP_COST;
        cum_gaps[i][0] = cum_gaps[i-1][0] + 1;
    }

    for(int i=max_gaps+1; i<=n; i++) { // First row: the rest is unreachable
        table[i][0] = UNREACHABLE;
        // cum_gaps here is not important, we will never use it
    }

    for(int j=1; j<=max_gaps; j++) { // First column: we can only have gaps, specifically max_gaps gaps
        table[0][j] = table[0][j-1] + GAP_COST;
        cum_gaps[0][j] = cum_gaps[0][j-1] + 1;
    }

    for(int j=max_gaps+1; j<=m; j++) { // First column: the rest is unreachable
        table[0][j] = UNREACHABLE;
        // cum_gaps here is not important, we will never use it
    }

    for(int i=1; i<=n; i++) {
        for(int j=1; j<=m; j++) {
            // Check cum_gaps:
                // Find the minimum cum_gaps. If we don't come from the diagonal, we have to add 1 to the minimum cum_gaps because we are adding a gap.
                // If we come from the diagonal, we don't have to add anything.
                // If the new possible cum_gaps is equal to the minimum cum_gaps, we have to check the cost to decide which one to choose.
            table[i][j] = UNREACHABLE;
            if(table[i-1][j-1] != UNREACHABLE) {
                int min_cum_gaps = cum_gaps[i-1][j-1];
                table[i][j] = table[i-1][j-1] + match_or_mismatch(j-1, i-1, x, y); // Assume we come from the diagonal

                // now check if there is a smaller cum_gaps from the other two cells (in this case we have to add 1 to the minimum cum_gaps)

                if(table[i-1][j] != UNREACHABLE) {
                        if(cum_gaps[i-1][j] + 1 < min_cum_gaps && cum_gaps[i-1][j] + 1 <= max_gaps) {
                            min_cum_gaps = cum_gaps[i-1][j] + 1;
                            table[i][j] = table[i-1][j] + GAP_COST;
                        } else if(cum_gaps[i-1][j] + 1 == min_cum_gaps && cum_gaps[i-1][j] + 1 <= max_gaps) {
                            if(table[i-1][j] + GAP_COST < table[i][j]) {
                                table[i][j] = table[i-1][j] + GAP_COST;
                            }
                        }
                    }
                if(table[i][j-1] != UNREACHABLE) {
                    if(cum_gaps[i][j-1] + 1 < min_cum_gaps && cum_gaps[i][j-1] + 1 <= max_gaps) {
                        min_cum_gaps = cum_gaps[i][j-1] + 1;
                        table[i][j] = table[i][j-1] + GAP_COST;
                    } else if(cum_gaps[i][j-1] + 1 == min_cum_gaps && cum_gaps[i][j-1] + 1 <= max_gaps) {
                        if(table[i][j-1] + GAP_COST < table[i][j]) {
                            table[i][j] = table[i][j-1] + GAP_COST;
                        }
                    }
                }
                cum_gaps[i][j] = min_cum_gaps;
            }
        }
    }
    // Print cum_gaps for debugging
    printf("The cum_gaps table is:\n");
    print_table(cum_gaps, n, m, x, y);
}





int **allocate_table(int n, int m) {
    int **table = malloc(sizeof(int *) * n);
    for(int i = 0; i<n; i++) {
        table[i] = malloc(sizeof(int) * m);
    }
    return table;
}

void initialize_table_minimum_cost(int **table, int n, int m, char *x, char *y) {
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
        return MATCH_COST;
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