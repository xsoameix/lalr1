#include <stdio.h>

int
main_up(void) {
#define N 6
  int a[N][N];
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
      a[i][j] = 0;
  a[0][1] = 1;
  a[1][2] = 1;
  a[2][3] = 1;
  a[3][1] = 1;
  a[0][4] = 1;
  a[4][3] = 1;
  a[4][5] = 1;
  a[0][5] = 1;
  int scc[N];
  int scc_index = N;
  int index[N];
  for (int i = 0; i < N; i++)
    index[i] = 0;
  int levels = 0;
  int parent[N * 3];
  int stack[N * 3];
  int stack_len = 0;
  stack[stack_len] = 0;
  parent[stack_len] = 0;
  stack_len++;
  while (stack_len) {
    stack_len--;
    int p = parent[stack_len];
    int n = stack[stack_len];
    printf("%zu", n);
    if (index[n]) {
      if (index[n] > scc_index) {
        printf(" cf\n");
      } else if (index[n] <= index[p]) {
        printf(" bk index %d %d %d %d\n", n, p, index[n], index[p]);
        index[p] = index[n];
      } else { // if (scc[index[n] - 1] == n)
        printf(" ret %d index %d %d\n", p, index[n], index[p]);
        printf("  scc %d:", scc_index);
        do {
          int w = scc[--levels];
          printf(" %d", w);
          index[w] = scc_index;
        } while (scc[levels] != n);
        scc_index--;
        printf("\n");
      }
      continue;
    }
    index[n] = levels + 1;
    stack_len++;
    scc[levels++] = n;
    printf(":");
    for (int i = N; i; i--)
      if (a[n][i - 1]) {
        int j = i - 1;
        printf(" %d", j);
        stack[stack_len] = j;
        parent[stack_len] = n;
        stack_len++;
      }
    printf("\n");
    printf("stack len %d\n", stack_len);
  }
  return 0;
#undef N
}

int
main(void) {
#define N 6
  int a[N][N];
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
      a[i][j] = 0;
  a[0][1] = 1;
  a[1][2] = 1;
  a[2][3] = 1;
  a[3][1] = 1;
  a[0][4] = 1;
  a[4][3] = 1;
  a[4][5] = 1;
  a[0][5] = 1;
  int stash[N];
  int stash_head = N;
  int scc_len = 0;
  int scc[N];
  for (int i = 0; i < N; i++)
    scc[i] = N;
  int parent[N * 3];
  int stack[N * 3];
  int stack_len = 0;
  stack[stack_len] = 0;
  parent[stack_len] = 0;
  stack_len++;
  while (stack_len) {
    stack_len--;
    int p = parent[stack_len];
    int n = stack[stack_len];
    printf("%zu", n);
    if (scc[n] == N) {
      scc[n] = --stash_head;
      stash[stash_head] = n;
      stack_len++;
      printf(":");
      for (int i = N; i; i--)
        if (a[n][i - 1]) {
          int j = i - 1;
          printf(" %d", j);
          stack[stack_len] = j;
          parent[stack_len] = n;
          stack_len++;
        }
      printf("\n");
      printf("stack len %d\n", stack_len);
    } else if (scc[n] >= scc[p] && n) {
      printf(" bk scc %d %d %d %d\n", n, p, scc[n], scc[p]);
      scc[p] = scc[n];
    } else if (scc[n] >= scc_len) { // if (stash[scc[n]] == n)
      printf(" ret %d scc %d %d\n", p, scc[n], scc[p]);
      printf("  scc %d:", scc_len);
      do {
        int w = stash[stash_head];
        printf(" %d", w);
        scc[w] = scc_len;
      } while (stash[stash_head++] != n);
      scc_len++;
      printf("\n");
    } else {
      printf(" cf\n");
    }
  }
  return 0;
}

int
main_empty(void) {
#define N 6
  int a[N][N];
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
      a[i][j] = 0;
  a[0][1] = 1;
  a[1][2] = 1;
  a[2][3] = 1;
  a[3][1] = 1;
  a[0][4] = 1;
  a[4][3] = 1;
  a[4][5] = 1;
  a[0][5] = 1;
  int stash[N];
  int stash_head = N;
  int scc_len = 0;
  int scc[N];
  for (int i = 0; i < N; i++)
    scc[i] = N;
  int parent[N * 3];
  int stack[N * 3];
  int stack_len = 0;
  stack[stack_len] = 0;
  parent[stack_len] = 0;
  stack_len++;
  while (stack_len) {
    stack_len--;
    int p = parent[stack_len];
    int n = stack[stack_len];
    if (scc[n] == N) {
      scc[n] = --stash_head;
      stash[stash_head] = n;
      stack_len++;
      for (int i = N; i; i--)
        if (a[n][i - 1]) {
          stack[stack_len] = i - 1;
          parent[stack_len] = n;
          stack_len++;
        }
    } else if (scc[n] >= scc[p] && n) {
      scc[p] = scc[n];
    } else if (scc[n] >= scc_len) {
      do {
        int w = stash[stash_head];
        scc[w] = scc_len;
      } while (stash[stash_head++] != n);
      scc_len++;
    }
  }
  return 0;
}
