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
void free_table(int **table, int n);
int find_alignment_simple(char *x, char *y, char *x_align, char *y_align, int gap_cost, int mismatch_cost);
int find_alignment_minimum_length(char *x, char *y, char *x_align, char *y_align, int gap_cost, int mismatch_cost);
int match_or_mismatch(int i, int j, char *x, char *y, int mismatch_cost);
int solve(int **table, int n, int m, char *x, char *y, char *x_align, char *y_align, int gap_cost, int mismatch_cost);
void generate_maximum_cost_strings(int n, int m, char *x, char *y, int gap_cost, int mismatch_cost);

int main(int argc, char* argv[]) {
    if(argc == 1) {
        printf("Usage: \n\t%s -simple <x> <y>\n\t%s -min <x> <y>\n\t%s -max <n> <m> <gap_cost> <mismatch_cost>\n", argv[0], argv[0], argv[0]);
    }
    else {
        char x_align[MAX_LENGTH];
        char y_align[MAX_LENGTH];
        switch(argv[1][3]) {
            case 'm': // siMple alignment, i.e. no constraint on the number of gaps
                find_alignment_simple(argv[2], argv[3], x_align, y_align, GAP_COST, MISMATCH_COST);

                printf("The minimum cost alignment is:\n");
                printf("%s\n", x_align);
                printf("%s\n", y_align);
                break;
            case 'n': // miNimum length alignment, i.e. the number of gaps is minimized
                find_alignment_minimum_length(argv[2], argv[3], x_align, y_align, GAP_COST, MISMATCH_COST);

                printf("The minimum length alignment is:\n");
                printf("%s\n", x_align);
                printf("%s\n", y_align);
                break;
            case 'x': // maXimum cost strings, i.e. the strings with maximum cost alignment
                int n = atoi(argv[2]);
                int m = atoi(argv[3]);

                char *x = malloc(sizeof(char) * m);
                char *y = malloc(sizeof(char) * n);

                int gap_cost = atoi(argv[4]);
                int mismatch_cost = atoi(argv[5]);

                generate_maximum_cost_strings(n, m, x, y, gap_cost, mismatch_cost);

                printf("\nThe strings with maximum cost alignment are:\n");

                printf("\t%s\n", x);
                printf("\t%s\n", y);

                free(x);
                free(y);
                break;
            default:
                printf("Invalid option\n");
                break;
        }
    }
}

int find_alignment_simple(char *x, char *y, char *x_align, char *y_align, int gap_cost, int mismatch_cost) {
    int m = strlen(x);
    int n = strlen(y);
    int **table = allocate_table(n+1, m+1);

    table[0][0] = 0;

    for(int i = 1; i <= n; i++) {
        table[i][0] = table[i-1][0] + gap_cost;
    }

    for(int j = 1; j <= m; j++) {
        table[0][j] = table[0][j-1] + gap_cost;
    }

    // C[i][j]=min{C[i-1][j]+GAP_COST, C[i][j-1]+GAP_COST, C[i-1][j-1]+MISMATCH_COST if x[i]!=y[j], C[i-1][j-1] if x[i]==y[j]}
    for(int i = 1; i<=n; i++) {
        for(int j = 1; j<=m; j++) {
            table[i][j] = min(
                min(
                    table[i-1][j] + gap_cost,
                    table[i][j-1] + gap_cost
                    ),
                table[i-1][j-1] + match_or_mismatch(j-1, i-1, x, y, mismatch_cost));
        }
    }

    printf("The dp table for the simple alignment is:\n");
    print_table(table, n, m, x, y);

    int cost = solve(table, n, m, x, y, x_align, y_align, gap_cost, mismatch_cost);

    free_table(table, n+1);

    return cost;
}

int find_alignment_minimum_length(char *x, char *y, char *x_align, char *y_align, int gap_cost, int mismatch_cost) {
    int m = strlen(x);
    int n = strlen(y);

    int max_gaps = max(n, m) - min(n, m);

    int **table = allocate_table(n+1, m+1);
    int **cum_gaps = allocate_table(n+1, m+1);

    table[0][0] = 0;
    cum_gaps[0][0] = 0;

    for(int i=1; i<=max_gaps && i<=n; i++) {
        table[i][0] = table[i-1][0] + gap_cost;
        cum_gaps[i][0] = cum_gaps[i-1][0] + 1;
    }

    for(int i=max_gaps+1; i<=n; i++) {
        table[i][0] = UNREACHABLE;
    }

    for(int j=1; j<=max_gaps && j<=m; j++) {
        table[0][j] = table[0][j-1] + gap_cost;
        cum_gaps[0][j] = cum_gaps[0][j-1] + 1;
    }

    for(int j=max_gaps+1; j<=m; j++) {
        table[0][j] = UNREACHABLE;
        // cum_gaps here is not important
    }

    for(int i=1; i<=n; i++) {
        for(int j=1; j<=m; j++) {
            table[i][j] = UNREACHABLE;
            if(table[i-1][j-1] != UNREACHABLE) {
                int min_cum_gaps = cum_gaps[i-1][j-1];
                table[i][j] = table[i-1][j-1] + match_or_mismatch(j-1, i-1, x, y, mismatch_cost);

                if(table[i-1][j] != UNREACHABLE) {
                        if(cum_gaps[i-1][j] + 1 < min_cum_gaps && cum_gaps[i-1][j] + 1 <= max_gaps) {
                            min_cum_gaps = cum_gaps[i-1][j] + 1;
                            table[i][j] = table[i-1][j] + gap_cost;
                        } else if(cum_gaps[i-1][j] + 1 == min_cum_gaps && cum_gaps[i-1][j] + 1 <= max_gaps) {
                            if(table[i-1][j] + gap_cost < table[i][j]) {
                                table[i][j] = table[i-1][j] + gap_cost;
                            }
                        }
                    }
                if(table[i][j-1] != UNREACHABLE) {
                    if(cum_gaps[i][j-1] + 1 < min_cum_gaps && cum_gaps[i][j-1] + 1 <= max_gaps) {
                        min_cum_gaps = cum_gaps[i][j-1] + 1;
                        table[i][j] = table[i][j-1] + gap_cost;
                    } else if(cum_gaps[i][j-1] + 1 == min_cum_gaps && cum_gaps[i][j-1] + 1 <= max_gaps) {
                        if(table[i][j-1] + gap_cost < table[i][j]) {
                            table[i][j] = table[i][j-1] + gap_cost;
                        }
                    }
                }
                cum_gaps[i][j] = min_cum_gaps;
            }
        }
    }

    // printf("The cum_gaps table is:\n"); // DEBUGGING PRINT
    // print_table(cum_gaps, n, m, x, y);

    printf("The dp table for the minimum length alignment is:\n");
    print_table(table, n, m, x, y);

    int cost = solve(table, n, m, x, y, x_align, y_align, gap_cost, mismatch_cost);

    free_table(table, n+1);

    return cost;
}

int **allocate_table(int n, int m) {
    int **table = malloc(sizeof(int *) * n);
    for(int i = 0; i<n; i++) {
        table[i] = malloc(sizeof(int) * m);
    }
    return table;
}

void free_table(int **table, int n) {
    for(int i = 0; i<n; i++) {
        free(table[i]);
        table[i] = NULL;
    }
    free(table);
    table = NULL;
}

int match_or_mismatch(int i, int j, char *x, char *y, int mismatch_cost) {
    if (x[i] == y[j]) {
        return 0;
    } else {
        return mismatch_cost;
    }
}

int solve(int **table, int n, int m, char *x, char *y, char *x_align, char *y_align, int gap_cost, int mismatch_cost) {
    int i = n;
    int j = m;
    int k = 0;
    int num_mismatches = 0;
    int num_gaps = 0;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && table[i][j] == table[i-1][j-1] + match_or_mismatch(j-1, i-1, x, y, mismatch_cost)) {
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
        } else if (i > 0 && table[i][j] == table[i-1][j] + gap_cost) {
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

    printf("Mismatches: %d, Gaps: %d, Cost: %d\n", num_mismatches, num_gaps, table[n][m]);

    return table[n][m];
}

void generate_maximum_cost_strings(int n, int m, char *x, char *y, int gap_cost, int mismatch_cost) {
    if(n < m) {
        int temp = n;
        n = m;
        m = temp;
    }

    x[0] = 'G';
    x[1] = 'T';
    x[2] = 'A';
    x[3] = 'C';

    y[0] = 'T';
    y[1] = 'A';
    y[2] = 'C';
    y[3] = 'G';

    for(int i = 4; i<m; i++) {
        x[i] = 'C';
        y[i] = 'G';
    }

    for(int j = m; j<n; j++) {
        y[j] = 'G';
    }

    char x_align[MAX_LENGTH];
    char y_align[MAX_LENGTH];

    int cost = find_alignment_minimum_length(x, y, x_align, y_align, gap_cost, mismatch_cost);

    printf("The maximum cost is: %d\n", cost);

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