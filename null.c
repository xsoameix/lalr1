#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

#define DA_TAIL(base) ((da_int_t) ~(base))
#define DA_PREV(cell) ((cell).base)
#define DA_NEXT(cell) ((da_int_t) -(cell).check)
#define DA_FIRST(ary) DA_NEXT(ary[0])
#define DA_LAST(ary) DA_PREV((ary)[DA_FIRST(ary)])
#define DA_IS_BASE(base) ((base) >= 0)
#define DA_IS_TAIL(base) ((base) < 0)
#define DA_IS_USED(ary, i) (!(i) || (ary)[i].check >= 0)
#define DA_TAIL_SET(cell, v) ((cell).base = (da_int_t) ~(v))
#define DA_PREV_SET(cell, v) ((cell).base = (v))
#define DA_NEXT_SET(cell, v) ((cell).check = (da_int_t) -(v))
#define DA_FIRST_SET(ary, v) DA_NEXT_SET((ary)[0], v)
#define DA_CONNECT(ary, a, b) \
    (DA_NEXT_SET((ary)[a], b), DA_PREV_SET((ary)[b], a))
#define DA_ADD(ary, i, from) \
    (DA_CONNECT(ary, DA_PREV(ary[i]), DA_NEXT(ary[i])), \
     (void) ((DA_PREV(ary[i]) > i && \
              DA_FIRST_SET(ary, DA_NEXT(ary[i])) != 0) || \
             (DA_PREV(ary[i]) == i && \
              DA_FIRST_SET(ary, 0) != 0)), \
     ary[i].check = from)
#define DA_SMAXT(t) (((t)0x7<<(sizeof(t)*8-4))|(((t)0x1<<(sizeof(t)*8-4))-(t)1))
#define DA_UMAXT(t) (((t)0xf<<(sizeof(t)*8-4))|(((t)0x1<<(sizeof(t)*8-4))-(t)1))
#define DA_INT_MAX DA_SMAXT(da_int_t)
#define DA_KEY_MAX DA_UMAXT(da_key_t)
#define DA_OFF_MIN 0
#define DA_OFF_MAX(len, base) \
    (da_off_t) (DA_KEY_MAX < (len) - 1 - (da_uint_t) (base) ? \
                DA_KEY_MAX : (len) - 1 - (da_uint_t) (base))
#define DA_OFF_ERR(off, len, base) \
    ((off) >= (len) - (da_uint_t) (base))
#define DA_GOTO(base, c) ((da_int_t) ((base) + (c)))
#define DA_BACK(to, c)   ((da_int_t) ((to)   - (c)))
#define DA_OFFSET(a) (a)

#define DA_INT_T short // change this for your need

typedef   signed DA_INT_T da_int_t;
typedef unsigned DA_INT_T da_uint_t;
typedef unsigned char  da_key_t;
typedef unsigned char  da_off_t;
typedef unsigned short da_off_len_t;
typedef unsigned int   da_data_t; // change this for your need

typedef struct {
  da_int_t base;
  da_int_t check;
} da_cell;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820)
#endif

typedef struct {
  da_cell * ary;
  da_key_t * tail;
  da_uint_t len;
  da_uint_t tlen;
  da_uint_t tcapa;
} da_trie;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef enum {
  DA_LOOKUP_ERR = 1,
  DA_LEN_ERR,
  DA_ALLOC_ERR,
  DA_CELL_MAX_ERR,
  DA_TAIL_MAX_ERR
} DA_ERR;

int
da_init(da_trie * da, da_uint_t len) {
  if (len < 2) return DA_LEN_ERR;
  if (len - 1 > DA_INT_MAX) return DA_CELL_MAX_ERR;
  da_cell * ary;
  if ((ary = malloc(sizeof(* ary) * len)) == NULL) return DA_ALLOC_ERR;
  ary[0].base = 0, DA_FIRST_SET(ary, 1);
  da_int_t last = (da_int_t) (len - 1);
  for (da_int_t from = 1; from < last; from++) DA_CONNECT(ary, from, from+1);
  return DA_CONNECT(ary, last, 1), da->ary = ary, da->len = len, 0;
}

int
da_realloc(da_trie * da) {
  da_uint_t len = da->len;
  if (len - 1 > DA_INT_MAX - len) return DA_CELL_MAX_ERR;
  da_uint_t len_new = (da_uint_t) (len << 1);
  da_cell * ary = da->ary;
  if ((ary = realloc(ary, sizeof(* ary) * len_new)) == NULL)
    return DA_ALLOC_ERR;
  da_int_t last = (da_int_t) (len_new - 1);
  for (da_int_t from = (da_int_t) len; from < last; from++)
    DA_CONNECT(ary, from, from+1);
  if (DA_FIRST(ary)) DA_CONNECT(ary, DA_LAST(ary), len);
  else               DA_FIRST_SET(ary, len);
  DA_CONNECT(ary, last, DA_FIRST(ary));
  return da->ary = ary, da->len = len_new, 0;
}

int
da_base(da_trie * da, const da_off_t * off, da_off_len_t n, da_int_t * ret) {
  int err;
  for (da_int_t to = 0;;) {
    if (DA_NEXT(da->ary[to]) <= to &&
        (err = da_realloc(da)) != 0) return err;
    if ((to = DA_NEXT(da->ary[to])) < off[0]) continue;
    da_int_t base = DA_BACK(to, off[0]);
    da_off_len_t i;
    for (i = 1; i < n; i++) {
      while (DA_OFF_ERR(off[i], da->len, base))
        if ((err = da_realloc(da)) != 0) return err;
      if (DA_IS_USED(da->ary, DA_GOTO(base, off[i]))) break;
    }
    if (i == n) return * ret = base, 0;
  }
}

void
da_relocate(const da_trie * da, da_int_t from, da_int_t base_new,
            const da_off_t * off, da_off_len_t n) {
  da_cell * ary = da->ary;
  da_uint_t len = da->len;
  da_int_t base = ary[from].base;
  ary[from].base = base_new;
  for (da_off_len_t i = 0; i < n; i++) {
    da_int_t to = DA_GOTO(base, off[i]);
    da_int_t to_new = DA_GOTO(base_new, off[i]);
    da_int_t base_child = ary[to].base;
    if (DA_IS_BASE(base_child)) {
      da_off_t max = DA_OFF_MAX(len, base_child);
      for (da_off_len_t j = DA_OFF_MIN; j <= max; j++)
        if (ary[DA_GOTO(base_child, j)].check == to)
          ary[DA_GOTO(base_child, j)].check = to_new;
    }
    DA_ADD(ary, to_new, from);
    ary[to_new].base = ary[to].base;
  }
  da_int_t prev = 0;
  for (da_off_len_t i = 0; i < n; i++) {
    da_int_t to = DA_GOTO(base, off[i]);
    da_int_t next;
    while ((next = DA_NEXT(ary[prev])) > prev && next < to)
      prev = next;
    if (prev) {
      DA_CONNECT(ary, prev, to);
      DA_CONNECT(ary, to, next);
    } else if (DA_FIRST(ary)) {
      DA_CONNECT(ary, DA_LAST(ary), to);
      DA_CONNECT(ary, to, DA_FIRST(ary));
      DA_FIRST_SET(ary, to);
    } else {
      DA_CONNECT(ary, to, to);
      DA_FIRST_SET(ary, to);
    }
    prev = to;
  }
}

int
da_tail_init(da_trie * da, da_uint_t capa) {
  if (!capa) return DA_LEN_ERR;
  if (capa - 1 > DA_INT_MAX) return DA_TAIL_MAX_ERR;
  da_key_t * tail;
  if ((tail = calloc(capa, sizeof(* tail))) == NULL) return DA_ALLOC_ERR;
  return da->tail = tail, da->tlen = 0, da->tcapa = capa, 0;
}

int
da_tail_realloc(da_trie * da) {
  da_uint_t capa = da->tcapa;
  if (capa - 1 > DA_INT_MAX - capa) return DA_TAIL_MAX_ERR;
  da_uint_t capa_new = (da_uint_t) (capa << 1);
  da_key_t * tail = da->tail;
  if ((tail = realloc(tail, sizeof(* tail) * capa_new)) == NULL)
    return DA_ALLOC_ERR;
  for (da_uint_t i = capa; i < capa_new; i++) tail[i] = 0;
  return da->tail = tail, da->tcapa = capa_new, 0;
}

int
da_insert(da_trie * da, const da_key_t * a, da_data_t data) {
  size_t i = 0, n = strlen((const char *) a) + 1;
  int err;
  for (da_int_t from = 0;;) {
    da_int_t base = da->ary[from].base;
    if (DA_IS_TAIL(base)) {
      da_int_t pos = DA_TAIL(base);
      da_key_t * tail = &da->tail[pos];
      for (n -= i, a += i, i = 0;; i++)
        if (i == n)               return * (da_data_t *) &tail[i] = data, 0;
        else if (a[i] != tail[i]) break;
      da_int_t prefix = (da_int_t) i;
      size_t tlen = 0;
      while (a[(size_t) prefix + tlen]) tlen++;
      while (tlen > (da_uint_t) (da->tcapa - da->tlen) ||
             sizeof(data) > (da_uint_t) (da->tcapa - da->tlen) - tlen)
        if ((err = da_tail_realloc(da)) != 0) return err;
      tail = &da->tail[pos];
      for (da_int_t j = 0; j < prefix; j++) {
        da_off_t off = DA_OFFSET(a[j]);
        da_int_t base_new;
        if ((err = da_base(da, &off, 1, &base_new)) != 0) return err;
        da->ary[from].base = base_new;
        da_int_t to = DA_GOTO(base_new, off);
        DA_ADD(da->ary, to, from), from = to;
      }
      da_off_t off[2];
      off[0] = DA_OFFSET(tail[prefix]);
      off[1] = DA_OFFSET(a[prefix]);
      da_int_t base_new;
      if ((err = da_base(da, off, 2, &base_new)) != 0) return err;
      da->ary[from].base = base_new;
      da_int_t to1 = DA_GOTO(base_new, off[0]);
      da_int_t to2 = DA_GOTO(base_new, off[1]);
      DA_ADD(da->ary, to1, from);
      DA_ADD(da->ary, to2, from);
      DA_TAIL_SET(da->ary[to1], pos + prefix + 1);
      DA_TAIL_SET(da->ary[to2], da->tlen);
      for (size_t j = 0; j < tlen; j++)
        da->tail[da->tlen + j] = a[(size_t) prefix + 1 + j];
      * (da_data_t *) &da->tail[da->tlen + tlen] = data;
      return da->tlen = (da_uint_t) (da->tlen + tlen + sizeof(data)), 0;
    }
    da_off_t off = DA_OFFSET(a[i++]);
    while (DA_OFF_ERR(off, da->len, base))
      if ((err = da_realloc(da)) != 0) return err;
    da_int_t to = DA_GOTO(base, off);
    da_int_t from2 = da->ary[to].check;
    if (from2 != from) {
      a += i - 1;
      size_t tlen = 0;
      while (a[tlen]) tlen++;
      while (tlen > (da_uint_t) (da->tcapa - da->tlen) ||
             sizeof(data) > (da_uint_t) (da->tcapa - da->tlen) - tlen)
        if ((err = da_tail_realloc(da)) != 0) return err;
      if (DA_IS_USED(da->ary, to)) {
        da_int_t base2;
        da_off_t max1 = DA_OFF_MAX(da->len, base);
        da_off_t max2;
        da_off_t off1[DA_KEY_MAX + 1]; da_off_len_t n1 = 0;
        da_off_t off2[DA_KEY_MAX + 1]; da_off_len_t n2 = 0;
        for (da_off_len_t j = DA_OFF_MIN; j <= max1; j++)
          if (da->ary[DA_GOTO(base, j)].check == from || j == off)
            off1[n1++] = (da_off_t) j;
        if (to) {
          base2 = da->ary[from2].base;
          max2 = DA_OFF_MAX(da->len, base2);
          for (da_off_len_t j = DA_OFF_MIN; j <= max2; j++)
            if (da->ary[DA_GOTO(base2, j)].check == from2)
              off2[n2++] = (da_off_t) j;
        }
        if (!to || n1 <= n2) {
          da_int_t base_new;
          if ((err = da_base(da, off1, n1, &base_new)) != 0) return err;
          da_relocate(da, from, base_new, off1, --n1);
          to = DA_GOTO(base_new, off);
        } else {
          da_int_t base_new;
          if ((err = da_base(da, off2, n2, &base_new)) != 0) return err;
          da_relocate(da, from2, base_new, off2, n2);
          da_off_t sibling = off1[off1[0] == off];
          from = da->ary[DA_GOTO(base, sibling)].check;
        }
      }
      DA_ADD(da->ary, to, from);
      DA_TAIL_SET(da->ary[to], da->tlen);
      for (size_t j = 0; j < tlen; j++)
        da->tail[da->tlen + j] = a[(size_t) 1 + j];
      * (da_data_t *) &da->tail[da->tlen + tlen] = data;
      return da->tlen = (da_uint_t) (da->tlen + tlen + sizeof(data)), 0;
    }
    from = to;
  }
}

int
da_lookup(da_trie * da, const da_key_t * a, da_data_t * data) {
  da_cell * ary = da->ary;
  da_uint_t len = da->len;
  size_t i = 0, n = strlen((const char *) a) + 1;
  for (da_int_t from = 0;;) {
    da_int_t base = ary[from].base;
    if (DA_IS_TAIL(base)) {
      da_key_t * tail = &da->tail[DA_TAIL(base)];
      for (n -= i, a += i, i = 0;; i++)
        if (i == n)               return * data = * (da_data_t *) &tail[i], 0;
        else if (a[i] != tail[i]) return DA_LOOKUP_ERR;
    }
    da_off_t off = DA_OFFSET(a[i++]);
    if (DA_OFF_ERR(off, len, base)) return DA_CELL_MAX_ERR;
    da_int_t to = DA_GOTO(base, off);
    if (ary[to].check != from) return DA_LOOKUP_ERR;
    from = to;
  }
}

int
main(int argc, char ** argv) {
  int ret = 1;
  // overview
  //  1. check argv
  //  2. open file
  //  3. read text
  //  4. parse text
  //     read token
  //  5. compute null
  //  6. compute items
  //  7. compute DR
  //  8. compute read (use null)
  //  9. compute include & lookback (use *null, prods_len, *pterms, sprods)

  // check argv
  if (argc < 2) {
    char * self = argv[0];
    for (size_t i = 0; self[i]; i++)
      if (self[i] == '\n' || self[i] == '\\')
        self += i + 1, i = 0;
    fprintf(stderr, "Usage: %s <INPUT-FILE>\n", self);
    return ret;
  }
  // open file
  //  input_s       = input file name
  //  input         = input file
  //  *input_size_l = 0 ~ LONG_MAX,     text size without null byte
  //  *input_size   = 0 ~ SIZE_MAX - 1, text size without null byte
  //  * => could be null or 0
  char * input_s = argv[1];
  FILE * input;
  if ((input = fopen(input_s, "rb")) == NULL) {
    perror(input_s);
    return ret;
  }
  if (fseek(input, 0, SEEK_END)) {
    perror(input_s);
    goto close_input;
  }
  long input_size_l;
  if ((input_size_l = ftell(input)) == -1) {
    perror(input_s);
    goto close_input;
  }
  if ((unsigned long) input_size_l > SIZE_MAX - 1) { // text & null byte
    fprintf(stderr, "%s is large than %zu\n", input_s, SIZE_MAX - 1);
    goto close_input;
  }
  size_t input_size = (size_t) input_size_l;
  if (fseek(input, 0, SEEK_SET)) {
    perror(input_s);
    goto close_input;
  }
  // read text
  //  text = (text) + null byte
  char * text;
  if ((text = malloc(input_size + 1)) == NULL) {
    fprintf(stderr, "malloc %s: %s\n", input_s, strerror(errno));
    goto close_input;
  }
  if (input_size && fread(text, input_size, 1, input) != 1) {
    perror(input_s);
    goto free_text;
  }
  text[input_size] = '\0';
  // parse text
  //  da          =  dot            + other symbols
  //  tok         =  the token read from text + '\0'
  //  tok_size    =  size of tok
  //  toks        =  dot + '\0'     + other symbols
  //  toks_s      =  0 (dot)        + index of toks of other symbols
  //  toks_size   =  2 (dot + '\0') + total size of toks of other symbols
  //  syms        =  0 (dot)        + index of other symbols
  //                 >= 1 (rule index + 1) if symbol is nt
  //                 0                     if symbol is t or dot
  //              => nt index  if symbol is nt
  //                 t index   if symbol is t
  //                 0         if symbol is dot
  //  syms_size   =  1 (dot) + number of other symbols
  //  nts_p       =  dot            + probability of nt of other symbols
  //                 1  if symbol is nt
  //                 0  if symbol is t or dot
  //  *nts        =  nonterminals
  //  *nts_size   =  number of nts
  //  *ts         =  terminals
  //  *ts_size    =  number of ts
  //  *terms      =  symbols with dot in each prods
  //  *terms_size =  number of terms
  //  *prods      =  index of terms
  //  *prods_size =  number of prods
  //  rules       =  index of prods of nts + end index of prod of last nt
  //  *rules_size =  number of rules, same as nts_size
  //  * => could be null or 0
  da_trie da;
  if ((ret = da_init(&da, 2)) != 0) {
    fprintf(stderr, "da_init(da) error: %d\n", ret);
    goto free_text;
  }
  if ((ret = da_tail_init(&da, 1)) != 0) {
    fprintf(stderr, "da_tail_init(da) error: %d\n", ret);
    goto free_da_ary;
  }
  typedef unsigned char sym_t;
  assert(sizeof(sym_t) <= sizeof(da_data_t));
  char st = 0;
  char * tok = NULL;
  size_t tok_size = 0;
  char * toks = NULL;
  size_t toks_size = 0;
  size_t * toks_s = NULL;
  sym_t * terms = NULL;
  size_t terms_size = 0;
  size_t * prods = NULL;
  size_t prods_size = 0;
  size_t * rules;
  if ((rules = malloc(1 * sizeof(* rules))) == NULL) {
    perror("malloc rules");
    goto free_da_tail_ary;
  }
  sym_t rules_size = 0;
  sym_t * syms = NULL;
  sym_t syms_size = 0;
  sym_t * nts = NULL;
  sym_t nts_size = 0;
  char * nts_p = NULL;
  {
    const char * dot = ".";
    size_t tok_len = strlen(dot);
    sym_t sym = syms_size++;
    if ((ret = da_insert(&da, (da_key_t *) dot, sym)) != 0) {
      fprintf(stderr, "da_insert(\"%s\") error: %d\n", dot, ret);
      goto free_tok;
    }
    size_t req = toks_size + tok_len + 1;
    void * mem;
    if ((mem = realloc(toks, req)) == NULL) {
      perror("realloc toks");
      goto free_tok;
    }
    toks = mem, toks_size = req;
    memcpy(toks + toks_size - tok_len - 1, dot, tok_len);
    toks[toks_size - 1] = '\0';
    if ((mem = realloc(toks_s, syms_size * sizeof(* toks_s))) == NULL) {
      perror("realloc toks_s");
      goto free_tok;
    }
    toks_s = mem;
    toks_s[syms_size - 1] = toks_size - tok_len - 1;
    if ((mem = realloc(syms, syms_size * sizeof(* syms))) == NULL) {
      perror("realloc syms");
      goto free_tok;
    }
    syms = mem;
    syms[syms_size - 1] = 0;
    if ((mem = realloc(nts_p, syms_size * sizeof(* nts_p))) == NULL) {
      perror("realloc nts_p");
      goto free_tok;
    }
    nts_p = mem;
    nts_p[syms_size - 1] = 0;
  }
  for (size_t i = 0;;) {
    // read token
    //  st = state
    //       A : a a a | a a a | ; B : | b ; \0
    //       0 1 2 3 3 3 2 3 3 3 2 0 1 2 2 3  0
    while (text[i] == ' ' || text[i] == '\t' ||
           text[i] == '\r' || text[i] == '\n') i++;
    size_t j = i;
    while ((text[j] >= 'a' && text[j] <= 'z') ||
           (text[j] >= 'A' && text[j] <= 'Z') ||
           text[j] == '\'' ||
           text[j] == '+' ||
           text[j] == '-' ||
           text[j] == '*' ||
           text[j] == '(' ||
           text[j] == ')' ||
           text[j] == '$') j++;
    enum {COLON = 1, VBAR, SEMI, END};
    char c = 0;
    if (j == i) {
      if (text[j] == '\\' &&
          (text[j + 1] == ':' ||
           text[j + 1] == '|' ||
           text[j + 1] == ';')) i++, j += 2;
      else if (text[j] == ':')  c = COLON, j++;
      else if (text[j] == '|')  c = VBAR,  j++;
      else if (text[j] == ';')  c = SEMI,  j++;
      else if (text[j] == '\0') c = END;
      else {
        fprintf(stderr, "Unknown character: %c\n", text[j]);
        goto free_tok;
      }
    }
    size_t tok_len = j - i;
    size_t tok_req = tok_len + 1;
    if (tok_req > tok_size) {
      char * mem;
      if ((mem = realloc(tok, tok_req)) == NULL) {
        perror("realloc tok");
        goto free_tok;
      }
      tok = mem, tok_size = tok_req;
    }
    memcpy(tok, text + i, tok_len);
    tok[tok_len] = '\0';
    char st_next = st;
    if (st == 0) {
      if (c == COLON || c == VBAR || c == SEMI) {
        fprintf(stderr, "Invalid character: %c\n", c);
        goto free_tok;
      }
      if (c == END) {
        break;
      } else {
        st_next = 1;
      }
    } else if (st == 1) {
      if (c == END) {
        fprintf(stderr, "Incompleted syntax\n");
        goto free_tok;
      } else if (c != COLON) {
        fprintf(stderr, "Invalid token: %s\n", tok);
        goto free_tok;
      }
      st_next = 2;
    } else if (st == 2) {
      if (c == END) {
        fprintf(stderr, "Incompleted syntax\n");
        goto free_tok;
      } else if (c == COLON) {
        fprintf(stderr, "Invalid character: %c\n", c);
        goto free_tok;
      }
      if (c == VBAR) {
        st_next = 2;
      } else if (c == SEMI) {
        st_next = 0;
      } else {
        st_next = 3;
      }
    } else if (st == 3) {
      if (c == END) {
        fprintf(stderr, "Incompleted syntax\n");
        goto free_tok;
      } else if (c == COLON) {
        fprintf(stderr, "Invalid character: %c\n", c);
        goto free_tok;
      }
      if (c == VBAR) {
        st_next = 2;
      } else if (c == SEMI) {
        st_next = 0;
      } else {
        st_next = 3;
      }
    }
    sym_t sym = 0;
    if (c != COLON && c != VBAR && c != SEMI) {
      da_data_t da_sym;
      if (da_lookup(&da, (da_key_t *) tok, &da_sym)) {
        if (syms_size == DA_UMAXT(sym_t)) {
          fprintf(stderr, "the number of symbols is too large (limit = %d)\n",
                  syms_size);
          goto free_tok;
        }
        sym = syms_size++;
        if ((ret = da_insert(&da, (da_key_t *) tok, sym)) != 0) {
          fprintf(stderr, "da_insert(\"%s\") error: %d\n", tok, ret);
          goto free_tok;
        }
        size_t req = toks_size + tok_len + 1;
        void * mem;
        if ((mem = realloc(toks, req)) == NULL) {
          perror("realloc toks");
          goto free_tok;
        }
        toks = mem, toks_size = req;
        memcpy(toks + toks_size - tok_len - 1, tok, tok_len);
        toks[toks_size - 1] = '\0';
        if ((mem = realloc(toks_s, syms_size * sizeof(* toks_s))) == NULL) {
          perror("realloc toks_s");
          goto free_tok;
        }
        toks_s = mem;
        toks_s[syms_size - 1] = toks_size - tok_len - 1;
        if ((mem = realloc(syms, syms_size * sizeof(* syms))) == NULL) {
          perror("realloc syms");
          goto free_tok;
        }
        syms = mem;
        syms[syms_size - 1] = 0;
        if ((mem = realloc(nts_p, syms_size * sizeof(* nts_p))) == NULL) {
          perror("realloc nts_p");
          goto free_tok;
        }
        nts_p = mem;
        nts_p[syms_size - 1] = 0;
      } else {
        sym = (sym_t) da_sym;
      }
    }
    if (st == 2 || st == 3) {
      size_t req = terms_size + 1;
      void * mem;
      if ((mem = realloc(terms, req * sizeof(* terms))) == NULL) {
        perror("realloc terms");
        goto free_tok;
      }
      terms = mem, terms_size = req;
      terms[terms_size - 1] = sym;
    }
    if (st == 2) {
      size_t req = prods_size + 1;
      void * mem;
      if ((mem = realloc(prods, req * sizeof(* prods))) == NULL) {
        perror("realloc prods");
        goto free_tok;
      }
      prods = mem, prods_size = req;
      prods[prods_size - 1] = terms_size - 1;
    }
    if (st == 0) {
      if (syms[sym]) {
        fprintf(stderr, "Duplicated nonterminal: %s\n", toks + toks_s[sym]);
        goto free_tok;
      }
      sym_t req = (sym_t) (rules_size + 1);
      void * mem;
      if ((mem = realloc(rules, (sym_t) (req + 1) * sizeof(* rules))) == NULL) {
        perror("realloc rules");
        goto free_tok;
      }
      rules = mem, rules_size = req;
      rules[rules_size - 1] = prods_size;
      sym_t nt = nts_size++;
      if ((mem = realloc(nts, nts_size * sizeof(* nts))) == NULL) {
        perror("realloc nts");
        goto free_tok;
      }
      nts = mem;
      nts[nt] = sym;
      syms[sym] = nt + 1;
      nts_p[sym] = 1;
    }
    st = st_next;
    i = j;
  }
  rules[rules_size] = prods_size;
  sym_t ts_size = (sym_t) (syms_size - nts_size - 1); // dot is not terminal
  sym_t * ts = NULL;
  if (ts_size && (ts = malloc(ts_size * sizeof(* ts))) == NULL) {
    perror("malloc ts");
    goto free_tok;
  }
  for (sym_t i = 1, j = 0; i < syms_size; i++)
    if (syms[i]) {
      syms[i]--;
    } else {
      syms[i] = j;
      ts[j++] = i;
    }
  // prods
  if (argc > 2) {
    for (sym_t i = 0; i < syms_size; i++) {
      if (nts_p[i]) {
        printf("%s:\n", toks + toks_s[i]);
        for (size_t j = rules[syms[i]]; j < rules[syms[i] + 1]; j++) {
          printf("  ");
          size_t k = prods[j];
          do {
            printf(" %s", toks + toks_s[terms[k]]);
          } while (terms[k++]);
          printf("\n");
        }
      } else {
        printf("%s: T\n", toks + toks_s[i]);
      }
    }
    printf("\n");
  }
  // compute null
  //  null       = probability of nullable of symbols
  //  nadj       = first edge of symbols
  //  *nnext     = next edge of edges
  //  *nterms    = index of terms (grouped by symbol) of symbols
  //  *prods_len = number of terms of prods
  //  *rules_len = number of prods of rules
  //  *pterms    = index of prod of terms
  //  *sprods    = index of symbol of prods
  //  nqueue     = index of symbols, nullable of which is known
  char * null;
  if ((null = malloc(syms_size)) == NULL) {
    perror("malloc null");
    goto free_ts;
  }
  enum {NULL_UNK = 2};
  null[0] = 1;
  for (sym_t i = 1; i < syms_size; i++)
    if (nts_p[i]) {
      null[i] = NULL_UNK;
    } else {
      null[i] = 0;
    }
  size_t * nadj;
  if ((nadj = malloc(syms_size * sizeof(* nadj))) == NULL) {
    perror("malloc nadj");
    goto free_null;
  }
  size_t * nnext = NULL;
  if (terms_size && (nnext = malloc(terms_size * sizeof(* nnext))) == NULL) {
    perror("malloc nnext");
    goto free_nadj;
  }
  size_t * nterms = NULL;
  if (terms_size && (nterms = malloc(terms_size * sizeof(* nterms))) == NULL) {
    perror("malloc nterms");
    goto free_nnext;
  }
  size_t * prods_len = NULL;
  if (prods_size &&
      (prods_len = malloc(prods_size * sizeof(* prods_len))) == NULL) {
    perror("malloc prods_len");
    goto free_nterms;
  }
  size_t * rules_len = NULL;
  if (rules_size &&
      (rules_len = malloc(rules_size * sizeof(* rules_len))) == NULL) {
    perror("malloc rules_len");
    goto free_prods_len;
  }
  size_t * pterms = NULL;
  if (terms_size && (pterms = malloc(terms_size * sizeof(* pterms))) == NULL) {
    perror("malloc pterms");
    goto free_rules_len;
  }
  sym_t * sprods = NULL;
  if (prods_size && (sprods = malloc(prods_size * sizeof(* sprods))) == NULL) {
    perror("malloc sprods");
    goto free_pterms;
  }
  sym_t * nqueue;
  if ((nqueue = malloc(syms_size * sizeof(* nqueue))) == NULL) {
    perror("malloc nqueue");
    goto free_sprods;
  }
  size_t nedges = 0;
  for (sym_t i = 0; i < syms_size; i++)
    nadj[i] = 0;
  for (size_t i = 0; i < terms_size; i++) {
    size_t next = nadj[terms[i]];
    nnext[nedges] = next;
    nterms[nedges] = i;
    nadj[terms[i]] = nedges + 1;
    nedges++;
  }
  for (sym_t i = 0; i < nts_size; i++) {
    for (size_t j = rules[i]; j < rules[i + 1]; j++) {
      size_t k = prods[j];
      do {
        pterms[k] = j;
      } while (terms[k++]);
      prods_len[j] = k - prods[j];
      sprods[j] = nts[i];
    }
    rules_len[i] = rules[i + 1] - rules[i];
  }
  if (argc > 3) {
    for (sym_t i = 0; i < syms_size; i++) {
      printf("%s:", toks + toks_s[i]);
      for (size_t j = nadj[i]; j; j = nnext[j - 1]) {
        printf(" %zu", nterms[j - 1]);
      }
      printf("\n");
    }
    printf("\n");
    for (size_t i = 0; i < terms_size; i++) {
      printf(" %zu", pterms[i]);
    }
    printf("\n");
    for (size_t i = 0; i < prods_size; i++) {
      printf(" %d", sprods[i]);
    }
    printf("\n");
    printf("\n");
    for (size_t i = 0; i < prods_size; i++) {
      printf(" %zu", prods_len[i]);
    }
    printf("\n");
    for (sym_t i = 0; i < rules_size; i++) {
      printf(" %zu", rules_len[i]);
    }
    printf("\n");
    for (sym_t i = 0; i < syms_size; i++) {
      printf(" %d", null[i]);
    }
    printf("\n");
  }
  size_t nqueue_start = 0;
  size_t nqueue_end = 0;
  nqueue[nqueue_end++] = 0;
  for (sym_t i = 0; i < ts_size; i++)
    nqueue[nqueue_end++] = ts[i];
  while (nqueue_start < nqueue_end) {
    sym_t sym = nqueue[nqueue_start++];
    for (size_t i = nadj[sym]; i; i = nnext[i - 1]) {
      size_t term = nterms[i - 1];
      size_t prod = pterms[term];
      sym_t child = sprods[prod];
      sym_t nt = syms[child];
      if (null[child] == NULL_UNK) {
        if (null[terms[term]] == 1) {
          if (prods_len[prod]) {
            prods_len[prod]--;
            if (!prods_len[prod]) {
              rules_len[nt] = 0;
              null[child] = 1;
              nqueue[nqueue_end++] = child;
            }
          }
        } else if (null[terms[term]] == 0) {
          prods_len[prod] = 0;
          if (rules_len[nt]) {
            rules_len[nt]--;
            if (!rules_len[nt]) {
              null[child] = 0;
              nqueue[nqueue_end++] = child;
            }
          }
        }
      }
    }
    if (argc > 3) {
      printf("=== sym %s\n", toks + toks_s[sym]);
      for (size_t i = 0; i < prods_size; i++) {
        printf(" %zu", prods_len[i]);
      }
      printf("\n");
      for (sym_t i = 0; i < nts_size; i++) {
        printf(" %zu", rules_len[i]);
      }
      printf("\n");
      for (sym_t i = 0; i < syms_size; i++) {
        printf(" %d", null[i]);
      }
      printf("\n");
    }
  }
  if (argc > 2) {
    for (sym_t i = 0; i < syms_size; i++) {
      printf(" %d", null[i]);
    }
    printf("\n");
    printf("\n");
  }
  for (sym_t i = 0; i < syms_size; i++)
    if (null[i] == NULL_UNK) {
      fprintf(stderr, "Cannot compute the null(\"%s\")\n", toks + toks_s[i]);
      goto free_nqueue;
    }
  // compute items
  //  *iswork     = terms of item
  //                1 if term is in item
  //                0 if term is not in item
  //  *isadj_size = number of items
  //  *isadj      = first edge of items
  //  *isnext     = next edge of edges
  //  *isto       = index of terms of items
  //  *isedges    = number of edges
  //  isets       = index of terms of items, encoded string
  //  *close      = index of terms (which are searched) of item
  //              = index of terms of item in encoded string form
  //  close_bytes = number of bytes of a token in encoded string
  //  close_len   = number of terms of item in close
  //              = number of tokens of item in encoded string
  typedef unsigned int item_t;
  assert(sizeof(item_t) <= sizeof(da_data_t));
  char * iswork = NULL;
  if (terms_size && (iswork = malloc(terms_size)) == NULL) {
    perror("malloc iswork");
    goto free_nqueue;
  }
  item_t isadj_size = 0;
  size_t * isadj = NULL;
  size_t * isnext = NULL;
  size_t * isto = NULL;
  size_t isedges = 0;
  da_trie isets;
  if ((ret = da_init(&isets, 2)) != 0) {
    fprintf(stderr, "da_init(isets) error: %d\n", ret);
    goto free_iswork;
  }
  if ((ret = da_tail_init(&isets, 1)) != 0) {
    fprintf(stderr, "da_tail_init(isets) error: %d\n", ret);
    goto free_isets_ary;
  }
  size_t * close = NULL;
  if (terms_size &&
      (close = malloc(terms_size * sizeof(* close) + 1)) == NULL) {
    perror("malloc close");
    goto free_isets_tail;
  }
  size_t close_bytes = 1;
  if (terms_size)
    for (size_t i = terms_size - 1; i /= 255;)
      close_bytes++;
  size_t close_len = 0;
  if (nts_size) {
    size_t i = 0;
    for (size_t j = 0; j < terms_size; j++)
      iswork[j] = 0;
    for (size_t j = rules[i]; j < rules[i + 1]; j++)
      close[close_len++] = prods[j];
    while (close_len) {
      size_t term = close[--close_len];
      if (iswork[term]) continue;
      iswork[term] = 1;
      sym_t sym = terms[term];
      if (!nts_p[sym]) continue;
      for (size_t j = rules[syms[sym]]; j < rules[syms[sym] + 1]; j++)
        close[close_len++] = prods[j];
    }
    item_t item = isadj_size++;
    size_t close_size = 0;
    for (size_t j = 0; j < terms_size; j++)
      if (iswork[j]) {
        size_t term = j;
        for (size_t k = 0; k < close_bytes; k++) {
          ((da_key_t *) close)[close_size++] = (da_key_t) (term % 255 + 1);
          term /= 255;
        }
        close_len++;
      }
    ((da_key_t *) close)[close_size] = '\0';
    if ((ret = da_insert(&isets, (da_key_t *) close, item)) != 0) {
      fprintf(stderr, "da_insert(item %u) error: %d\n", isadj_size, ret);
      goto free_close;
    }
    void * mem;
    if ((mem = realloc(isadj, isadj_size * sizeof(* isadj))) == NULL) {
      perror("realloc isadj");
      goto free_close;
    }
    isadj = mem;
    isadj[item] = 0;
    size_t req = isedges + close_len;
    if ((mem = realloc(isnext, req * sizeof(* isnext))) == NULL) {
      perror("realloc isnext");
      goto free_close;
    }
    isnext = mem;
    if ((mem = realloc(isto, req * sizeof(* isto))) == NULL) {
      perror("realloc isto");
      goto free_close;
    }
    isto = mem;
    for (size_t j = terms_size; j; j--)
      if (iswork[j - 1]) {
        size_t next = isadj[item];
        isadj[item] = isedges + 1;
        isnext[isedges] = next;
        isto[isedges] = j - 1;
        isedges++;
      }
    close_len = 0;
  }
  //  *imat      =  index of edges (indexed by symbol) of items
  //  *imat_size =  number of items, same as isadj_size
  //  *iadj      =  first edge of items
  //  *inext     =  next edge of edges
  //  *ito       =  number of terms (grouped by symbols) of items
  //             => start index of terms (grouped by symbols) of items
  //             => end   index of terms (grouped by symbols) of items
  //             => index of next items of items
  //  *isym      =  index of next symbols of items
  //  *iedges    =  number of edges
  //  *iedge     =  index of core terms of next item
  //  stack      =  index of items (which are searched)
  //  *stack_len =  number of items in stack
  size_t * imat = NULL;
  size_t imat_size = 0;
  size_t * iadj = NULL;
  size_t * inext = NULL;
  item_t * ito = NULL;
  sym_t * isym = NULL;
  size_t iedges = 0;
  size_t * iedge = NULL;
  if (terms_size && (iedge = malloc(terms_size * sizeof(* iedge))) == NULL) {
    perror("malloc iedge");
    goto free_close;
  }
  item_t stack_size = 0;
  item_t stack_len = 0;
  item_t * stack = NULL;
  if (isadj_size) {
    if ((stack = malloc(1 * sizeof(* stack))) == NULL) {
      perror("malloc stack");
      goto free_iedge;
    }
    stack[stack_len++] = 0;
  }
  while (stack_len) {
    size_t item = stack[--stack_len];
    size_t req;
    void * mem;
    if (imat_size < isadj_size) {
      req = isadj_size * syms_size;
      if ((mem = realloc(imat, req * sizeof(* imat))) == NULL) {
        perror("realloc imat");
        goto free_stack;
      }
      imat = mem;
      if ((mem = realloc(iadj, isadj_size * sizeof(* iadj))) == NULL) {
        perror("realloc iadj");
        goto free_stack;
      }
      iadj = mem;
      imat_size = isadj_size;
    }
    size_t * mat = imat + item * syms_size;
    for (sym_t i = 0; i < syms_size; i++)
      mat[i] = 0;
    iadj[item] = 0;
    for (size_t i = isadj[item]; i; i = isnext[i - 1]) {
      size_t term = isto[i - 1];
      if (terms[term]) {
        sym_t sym = terms[term];
        if (!mat[sym]) {
          req = iedges + 1;
          if ((mem = realloc(inext, req * sizeof(* inext))) == NULL) {
            perror("realloc inext");
            goto free_stack;
          }
          inext = mem;
          if ((mem = realloc(ito, req * sizeof(* ito))) == NULL) {
            perror("realloc ito");
            goto free_stack;
          }
          ito = mem;
          if ((mem = realloc(isym, req * sizeof(* isym))) == NULL) {
            perror("realloc isym");
            goto free_stack;
          }
          isym = mem;
          size_t next = iadj[item];
          iadj[item] = iedges + 1;
          inext[iedges] = next;
          ito[iedges] = 0;
          isym[iedges] = sym;
          mat[sym] = iedges + 1;
          iedges++;
        }
        ito[mat[sym] - 1]++;
      }
    }
    item_t base = 0;
    for (size_t i = iadj[item]; i; i = inext[i - 1]) {
      item_t len = ito[i - 1];
      ito[i - 1] = base;
      base += len;
    }
    for (size_t i = isadj[item]; i; i = isnext[i - 1]) {
      size_t term = isto[i - 1];
      if (terms[term])
        iedge[ito[mat[terms[term]] - 1]++] = term + 1;
    }
    for (size_t i = iadj[item], j = 0; i; i = inext[i - 1]) {
      for (size_t k = 0; k < terms_size; k++)
        iswork[k] = 0;
      for (; j < ito[i - 1]; j++)
        close[close_len++] = iedge[j];
      while (close_len) {
        size_t term = close[--close_len];
        if (iswork[term]) continue;
        iswork[term] = 1;
        sym_t sym = terms[term];
        if (!nts_p[sym]) continue;
        for (size_t k = rules[syms[sym]]; k < rules[syms[sym] + 1]; k++)
          close[close_len++] = prods[k];
      }
      size_t close_size = 0;
      for (size_t k = 0; k < terms_size; k++)
        if (iswork[k]) {
          size_t term = k;
          for (size_t l = 0; l < close_bytes; l++) {
            ((da_key_t *) close)[close_size++] = (da_key_t) (term % 255 + 1);
            term /= 255;
          }
          close_len++;
        }
      ((da_key_t *) close)[close_size] = '\0';
      item_t to;
      if (da_lookup(&isets, (da_key_t *) close, &to)) {
        to = isadj_size++;
        if ((ret = da_insert(&isets, (da_key_t *) close, to)) != 0) {
          fprintf(stderr, "da_insert(item %u) error: %d\n", to, ret);
          goto free_stack;
        }
        if ((mem = realloc(isadj, isadj_size * sizeof(* isadj))) == NULL) {
          perror("realloc isadj");
          goto free_stack;
        }
        isadj = mem;
        isadj[to] = 0;
        req = isedges + close_len;
        if ((mem = realloc(isnext, req * sizeof(* isnext))) == NULL) {
          perror("realloc isnext");
          goto free_stack;
        }
        isnext = mem;
        if ((mem = realloc(isto, req * sizeof(* isto))) == NULL) {
          perror("realloc isto");
          goto free_stack;
        }
        isto = mem;
        for (size_t k = terms_size; k; k--)
          if (iswork[k - 1]) {
            size_t next = isadj[to];
            isadj[to] = isedges + 1;
            isnext[isedges] = next;
            isto[isedges] = k - 1;
            isedges++;
          }
        if (stack_len == stack_size) {
          item_t ireq = stack_size + 1;
          if ((mem = realloc(stack, ireq * sizeof(* stack))) == NULL) {
            perror("realloc stack");
            goto free_stack;
          }
          stack = mem, stack_size = ireq;
        }
        stack[stack_len++] = to;
      }
      ito[i - 1] = to;
      close_len = 0;
    }
  }
  if (argc > 2) {
    for (item_t i = 0; i < isadj_size; i++) {
      printf("%u: {", i);
      for (size_t j = isadj[i]; j; j = isnext[j - 1])
        printf(" %zu", isto[j - 1]);
      printf(" }\n");
      size_t * mat = imat + i * syms_size;
      for (sym_t j = 0; j < syms_size; j++)
        if (mat[j])
          printf(" %s %zu %u\n",
                 toks + toks_s[j], mat[j] - 1, ito[mat[j] - 1]);
    }
    printf("\n");
  }
  // compute DR
  //  *etrms = ts of edges of items, used in DR, Read, Follow, and LA
  //           1 if t is in edge
  //           0 if t is not in edge
  char * etrms = NULL;
  if (iedges && (etrms = malloc(iedges * ts_size)) == NULL) {
    perror("malloc etrms");
    goto free_stack;
  }
  for (size_t i = 0, len = iedges * ts_size; i < len; i++)
    etrms[i] = 0;
  for (size_t i = 0; i < iedges; i++)
    if (nts_p[isym[i]]) { // DR(p, A), A is nt
      item_t to = ito[i];
      char * etrm = etrms + i * ts_size;
      for (size_t j = iadj[to]; j; j = inext[j - 1]) {
        size_t sym = isym[j - 1];
        if (!nts_p[sym]) // DR(p, A) = {t of T | p -A-> r -t-> }, T is t
          etrm[syms[sym]] = 1;
      }
    }
  if (argc > 3) {
    for (size_t i = 0; i < iedges; i++) {
      char * etrm = etrms + i * ts_size;
      printf("%zu:", i);
      for (sym_t j = 0; j < ts_size; j++)
        if (etrm[j])
          printf(" %s", toks + toks_s[ts[j]]);
      printf("\n");
    }
    printf("\n");
  }
  // compute read
  if (argc > 3) {
    for (size_t i = 0; i < iedges; i++) {
      printf("%zu:", i);
      if (syms[isym[i]]) {
        item_t to = ito[i];
        for (size_t j = iadj[to]; j; j = inext[j - 1]) {
          size_t sym = isym[j - 1];
          if (null[sym])
            printf(" %zu", j - 1);
        }
      }
      printf("\n");
    }
    printf("\n");
  }
  size_t * stash = NULL;
  if (iedges && (stash = malloc(iedges * sizeof(* stash))) == NULL) {
    perror("malloc stash");
    goto free_etrms;
  }
  size_t stash_head = iedges;
  size_t * scc = NULL;
  if (iedges && (scc = malloc(iedges * sizeof(* scc))) == NULL) {
    perror("malloc scc");
    goto free_stash;
  }
  for (size_t i = 0; i < iedges; i++)
    scc[i] = iedges;
  size_t scc_len = 0;
  size_t * parent = NULL;
  size_t * child = NULL;
  size_t child_len = 0;
  size_t child_size = 0;
  for (size_t i = 0; i < iedges; i++)
    if (scc[i] == iedges) {
      size_t req = child_len + 1;
      if (req > child_size) {
        void * mem;
        if ((mem = realloc(parent, req * sizeof(* parent))) == NULL) {
          perror("realloc parent");
          goto free_scc;
        }
        parent = mem;
        if ((mem = realloc(child, req * sizeof(* child))) == NULL) {
          perror("realloc child");
          goto free_scc;
        }
        child = mem;
        child_size = req;
      }
      child[child_len] = i;
      parent[child_len++] = i;
      while (child_len) {
        size_t p = parent[--child_len];
        size_t n = child[child_len];
        if (scc[n] == iedges) { // tree edge
          scc[n] = --stash_head;
          stash[stash_head] = n;
          child_len++;
          if (nts_p[isym[n]]) { // Read(p, A), A is nt
            item_t to = ito[n];
            for (size_t j = iadj[to]; j; j = inext[j - 1])
              if (null[isym[j - 1]]) { // p -A-> r -C->, C is nullable
                if ((req = child_len + 1) > child_size) {
                  void * mem;
                  if ((mem = realloc(parent, req * sizeof(* parent))) == NULL) {
                    perror("realloc parent");
                    goto free_scc;
                  }
                  parent = mem;
                  if ((mem = realloc(child, req * sizeof(* child))) == NULL) {
                    perror("realloc child");
                    goto free_scc;
                  }
                  child = mem;
                  child_size = req;
                }
                child[child_len] = j - 1; // Read(r, C)
                parent[child_len++] = n;
              }
          }
        } else if (scc[n] >= scc[p] && n != i) { // back or tree edge in SCC
          scc[p] = scc[n];
        } else if (scc[n] >= scc_len) { // tree edge, goes back to parent node
          do {
            size_t w = stash[stash_head];
            scc[w] = scc_len;
          } while (stash[stash_head++] != n);
          scc_len++;
          if (n != i) {
            char * src = etrms + n * ts_size;
            char * dst = etrms + p * ts_size;
            for (sym_t j = 0; j < ts_size; j++)
              dst[j] = dst[j] || src[j]; // Read(p, A) contains Read(r, C)
          }
        } else { // cross or forward edge
          char * src = etrms + n * ts_size;
          char * dst = etrms + p * ts_size;
          for (sym_t j = 0; j < ts_size; j++)
            dst[j] = dst[j] || src[j]; // Read(p, A) contains Read(r, C)
        }
      }
    }
  char cycle = scc_len < iedges; // not LR(k) if cycle detected (nontrivial SCC)
  if (argc > 2)
    printf("read cycle %d\n\n", cycle);
  if (argc > 3) {
    for (size_t i = 0; i < iedges; i++) {
      char * etrm = etrms + i * ts_size;
      printf("%zu:", i);
      for (sym_t j = 0; j < ts_size; j++)
        if (etrm[j])
          printf(" %s", toks + toks_s[ts[j]]);
      printf("\n");
    }
    printf("\n");
  }
  // compute include & lookback
  //  *irmat      = reductions of items, indexed by prod
  //  *iradj_size = number of reductions of items
  //  *iradj      = first edge of items
  //  *irnext     = next edge of edges
  //  *irto       = index of edge of reductions of items
  //  *iredges    = number of edges of reductions of items
  //  *irtrms     = ts of reductions of items
  //  *itrms      = ts of items
  //  *pnull      = probability of nullable of terms
  //  *eadj       = first edge of edges
  //  *enext      = next edge of edges
  //  *eto        = index of edge of edges of items
  //  *eedges     = number of edges of edges of items
  size_t * irmat = NULL;
  if (isadj_size &&
      (irmat = malloc(isadj_size * prods_size * sizeof(* irmat))) == NULL) {
    perror("malloc irmat");
    goto free_scc;
  }
  size_t iradj_size = 0;
  for (item_t i = 0; i < isadj_size; i++)
    for (size_t j = isadj[i]; j; j = isnext[j - 1])
      if (!terms[isto[j - 1]]) // LA(q, A -> w .)
        iradj_size++;
  size_t * iradj = NULL;
  if (iradj_size && (iradj = malloc(iradj_size * sizeof(* iradj))) == NULL) {
    perror("malloc iradj");
    goto free_irmat;
  }
  for (size_t i = 0; i < iradj_size; i++)
    iradj[i] = 0;
  iradj_size = 0;
  for (item_t i = 0; i < isadj_size; i++) {
    size_t * row = irmat + i * prods_size;
    for (size_t j = isadj[i]; j; j = isnext[j - 1]) {
      size_t term = isto[j - 1];
      if (!terms[term]) // LA(q, A -> w .)
        row[pterms[term]] = iradj_size++;
    }
  }
  size_t * irnext = NULL;
  size_t * irto = NULL;
  size_t iredges = 0;
  char * irtrms = NULL;
  if (iradj_size && (irtrms = malloc(iradj_size * ts_size)) == NULL) {
    perror("malloc irtrms");
    goto free_iradj;
  }
  for (size_t i = 0, len = iradj_size * ts_size; i < len; i++)
    irtrms[i] = 0;
  size_t * itrms = NULL;
  if (ts_size &&
      (itrms = malloc(isadj_size * ts_size * sizeof(* itrms))) == NULL) {
    perror("malloc itrms");
    goto free_irtrms;
  }
  for (size_t i = 0, len = isadj_size * ts_size; i < len; i++)
    itrms[i] = 0;
  char * pnull = NULL;
  if (terms_size && (pnull = malloc(terms_size)) == NULL) {
    perror("malloc pnull");
    goto free_itrms;
  }
  char empty = 1;
  for (size_t i = terms_size; i; i--) {
    size_t sym = terms[i - 1];
    if (!null[sym])
      empty = 0;
    else if (!sym)
      empty = 1;
    pnull[i - 1] = empty; // B -> \B A . \r, \r is nullable
  }
  if (argc > 3) {
    for (size_t i = 0; i < terms_size; i++)
      printf(" %d", pnull[i]);
    printf("\n");
    printf("\n");
  }
  size_t * eadj = NULL;
  if (iedges && (eadj = malloc(iedges * sizeof(* eadj))) == NULL) {
    perror("malloc eadj");
    goto free_pnull;
  }
  for (size_t i = 0; i < iedges; i++)
    eadj[i] = 0;
  size_t * enext = NULL;
  size_t * eto = NULL;
  size_t eedges = 0;
  for (item_t i = 0; i < isadj_size; i++)
    for (size_t j = iadj[i]; j; j = inext[j - 1]) {
      size_t sym = isym[j - 1];
      if (nts_p[sym]) // Follow(p', B), B is nt
        for (size_t k = rules[syms[sym]]; k < rules[syms[sym] + 1]; k++) {
          item_t m = i;
          size_t l = prods[k];
          for (; terms[l]; m = ito[imat[m * syms_size + terms[l]] - 1], l++)
            if (nts_p[terms[l]] && pnull[l + 1]) { // Follow(p, A), A is nt
              size_t req = eedges + 1;
              void * mem;
              if ((mem = realloc(enext, req * sizeof(* enext))) == NULL) {
                perror("realloc enext");
                goto free_eadj;
              }
              enext = mem;
              if ((mem = realloc(eto, req * sizeof(* eto))) == NULL) {
                perror("realloc eto");
                goto free_eadj;
              }
              eto = mem;
              size_t to = imat[m * syms_size + terms[l]] - 1; // p -A->
              size_t next = eadj[to];
              eadj[to] = eedges + 1;
              enext[eedges] = next;
              eto[eedges] = j - 1; // Follow(p, A) contains Follow(p', B)
              eedges++;
            }
          size_t req = iredges + 1;
          void * mem;
          if ((mem = realloc(irnext, req * sizeof(* irnext))) == NULL) {
            perror("realloc irnext");
            goto free_eadj;
          }
          irnext = mem;
          if ((mem = realloc(irto, req * sizeof(* irto))) == NULL) {
            perror("realloc irto");
            goto free_eadj;
          }
          irto = mem;
          size_t to = irmat[m * prods_size + pterms[l]]; // LA(q, A -> w)
          size_t next = iradj[to];
          iradj[to] = iredges + 1;
          irnext[iredges] = next;
          irto[iredges] = j - 1; // LA(q, A -> w) contains Follow(p, A)
          iredges++;
        }
    }
  for (size_t i = 0; i < iedges; i++)
    scc[i] = iedges;
  scc_len = 0;
  cycle = 0;
  for (size_t i = 0; i < iedges; i++)
    if (scc[i] == iedges) {
      size_t req = child_len + 1;
      if (req > child_size) {
        void * mem;
        if ((mem = realloc(parent, req * sizeof(* parent))) == NULL) {
          perror("realloc parent");
          goto free_eadj;
        }
        parent = mem;
        if ((mem = realloc(child, req * sizeof(* child))) == NULL) {
          perror("realloc child");
          goto free_eadj;
        }
        child = mem;
        child_size = req;
      }
      child[child_len] = i;
      parent[child_len++] = i;
      while (child_len) { // tree edge
        size_t p = parent[--child_len];
        size_t n = child[child_len];
        if (scc[n] == iedges) {
          scc[n] = --stash_head;
          stash[stash_head] = n;
          child_len++;
          for (size_t j = eadj[n]; j; j = enext[j - 1]) {
            if ((req = child_len + 1) > child_size) {
              void * mem;
              if ((mem = realloc(parent, req * sizeof(* parent))) == NULL) {
                perror("realloc parent");
                goto free_eadj;
              }
              parent = mem;
              if ((mem = realloc(child, req * sizeof(* child))) == NULL) {
                perror("realloc child");
                goto free_eadj;
              }
              child = mem;
              child_size = req;
            }
            child[child_len] = eto[j - 1]; // Follow(p, A)
            parent[child_len++] = n;
          }
        } else if (scc[n] >= scc[p] && n != i) { // back or tree edge in SCC
          scc[p] = scc[n];
        } else if (scc[n] >= scc_len) { // tree edge, goes back to parent node
          char trivial = 1;
          do {
            size_t w = stash[stash_head];
            scc[w] = scc_len;
            if (trivial) {
              trivial = 0;
            } else { // nontrivial SCC
              char * etrm = etrms + w * ts_size;
              for (sym_t j = 0; j < ts_size; j++)
                if (etrm[j]) // not LR(k) if cycle is detected where
                  cycle = 1; //  Read set is not empty
            }
          } while (stash[stash_head++] != n);
          scc_len++;
          if (n != i) {
            char * src = etrms + n * ts_size;
            char * dst = etrms + p * ts_size;
            for (sym_t j = 0; j < ts_size; j++)
              dst[j] = dst[j] || src[j]; // Follow(p, A) contains Follow(p', B)
          }
        } else { // cross or forward edge
          char * src = etrms + n * ts_size;
          char * dst = etrms + p * ts_size;
          for (sym_t j = 0; j < ts_size; j++)
            dst[j] = dst[j] || src[j]; // Follow(p, A) contains Follow(p', B)
        }
      }
    }
  if (argc > 2)
    printf("follow cycle %d\n\n", cycle);
  if (argc > 3) {
    for (size_t i = 0; i < iedges; i++) {
      char * etrm = etrms + i * ts_size;
      printf("%zu:", i);
      for (sym_t j = 0; j < ts_size; j++)
        if (etrm[j])
          printf(" %s", toks + toks_s[ts[j]]);
      printf("\n");
    }
    printf("\n");
  }
  for (item_t i = 0; i < isadj_size; i++) {
    size_t * row = irmat + i * prods_size;
    for (size_t j = isadj[i]; j; j = isnext[j - 1]) {
      size_t term = isto[j - 1];
      if (!terms[term]) { // LA(q, A -> w .)
        size_t to = row[pterms[term]];
        char * dst = irtrms + to * ts_size;
        for (size_t k = iradj[to]; k; k = irnext[k - 1]) {
          char * src = etrms + irto[k - 1] * ts_size;
          for (sym_t l = 0; l < ts_size; l++)
            dst[l] = dst[l] || src[l]; // LA(q, A -> w) contains Follow(p, A)
        }
      }
    }
  }
  char conflict = 0;
  for (item_t i = 0; i < isadj_size; i++) {
    size_t * mat = imat + i * syms_size;
    size_t * itrm = itrms + i * ts_size;
    size_t * row = irmat + i * prods_size;
    for (size_t j = isadj[i]; j; j = isnext[j - 1]) {
      size_t term = isto[j - 1];
      if (terms[term]) { // shift
        size_t sym = terms[term];
        if (!nts_p[sym]) { // shift if t
          if (itrm[syms[sym]])
            conflict = 1;
          if (syms[sym]) // S -> E $, $ is first t, t index is 0
            itrm[syms[sym]] = ito[mat[sym] - 1] + 2; // shift if t
          else
            itrm[syms[sym]] = 1; // accept if $
        }
      } else { // reduction
        char * trm = irtrms + row[pterms[term]] * ts_size;
        for (sym_t k = 0; k < ts_size; k++)
          if (trm[k]) {
            if (itrm[k])
              conflict = 1;
            itrm[k] = pterms[term] + isadj_size + 2; // reduce A -> w
          }
      }
    }
  }
  for (sym_t i = 0; i < nts_size; i++)
    for (size_t j = rules[i]; j < rules[i + 1]; j++) {
      size_t k = prods[j];
      while (terms[k]) k++;
      prods_len[j] = k - prods[j];
    }
  printf("conflict %d\n\n", conflict);
  if (argc > 2) {
    for (item_t i = 0; i < isadj_size; i++) {
      size_t * row = irmat + i * prods_size;
      printf("%u:\n", i);
      for (size_t j = isadj[i]; j; j = isnext[j - 1]) {
        size_t term = isto[j - 1];
        if (terms[term]) {
          printf(" s %s", toks + toks_s[terms[term]]);
        } else {
          printf(" r");
          char * trm = irtrms + row[pterms[term]] * ts_size;
          for (sym_t k = 0; k < ts_size; k++)
            if (trm[k])
              printf(" %s", toks + toks_s[ts[k]]);
        }
        printf("\n");
      }
    }
    printf("\n");
  }
  if (argc > 3) {
    for (size_t i = 0; i < prods_size; i++) {
      printf(" %zu", prods_len[i]);
    }
    printf("\n");
    for (size_t i = 0; i < prods_size; i++) {
      printf(" %d", sprods[i]);
    }
    printf("\n");
    printf("\n");
  }
  printf("    ");
  for (sym_t j = 0; j < nts_size; j++)
    printf(" %3s", toks + toks_s[nts[j]]);
  printf("   ");
  for (sym_t j = 0; j < ts_size; j++)
    printf(" %3s", toks + toks_s[ts[j]]);
  printf("\n");
  for (item_t i = 0; i < isadj_size; i++) {
    printf(" T%-2u", i);
    //      "r" %zu \0
    char buf[1 + 2 + 1];
    int len;
    size_t * mat = imat + i * syms_size;
    for (sym_t j = 0; j < nts_size; j++) {
      if (mat[nts[j]])
        len = snprintf(buf, sizeof(buf), "s%u", ito[mat[nts[j]] - 1]);
      else
        len = snprintf(buf, sizeof(buf), ".");
      for (; len < sizeof(buf); len++)
        printf(" ");
      printf("%s", buf);
    }
    printf("  |");
    for (sym_t j = 0; j < ts_size; j++) {
      size_t itrm = itrms[i * ts_size + j];
      if (itrm >= isadj_size + 2)
        len = snprintf(buf, sizeof(buf), "r%zu", itrm - isadj_size - 2);
      else if (itrm >= 2)
        len = snprintf(buf, sizeof(buf), "s%zu", itrm - 2);
      else if (itrm == 1)
        len = snprintf(buf, sizeof(buf), "o");
      else
        len = snprintf(buf, sizeof(buf), ".");
      for (; len < sizeof(buf); len++)
        printf(" ");
      printf("%s", buf);
    }
    printf("\n");
  }
  printf("\n");
  {
    char * dot_s = "graph.dot";
    FILE * dot;
    if ((dot = fopen(dot_s, "wb")) == NULL)
      goto close_dot;
    fprintf(dot,
            "digraph R {\n"
            " rankdir=LR;\n"
            " layout=dot;\n"
            " overlap=false;\n"
            " splines=true;\n"
            " node [fontname=\"Consolas\", fontsize=10];\n"
            " edge [fontname=\"Consolas\", fontsize=11];\n");
    for (item_t i = 0; i < isadj_size; i++) {
      size_t * row = irmat + i * prods_size;
      fprintf(dot,
              " t%u [shape=none, margin=0, label=<"
              "<table border=\"0\" cellborder=\"0\""
              " cellspacing=\"0\" cellpadding=\"0\">"
              "<tr><td colspan=\"2\" border=\"1\""
              " cellpadding=\"0\">%u</td></tr>", i, i);
      for (size_t j = isadj[i]; j; j = isnext[j - 1]) {
        size_t term = isto[j - 1];
        fprintf(dot,
                "<tr><td cellpadding=\"3\" align=\"left\""
                " border=\"1\" sides=\"lrb\">");
        if (!terms[term])
          fprintf(dot, "<font color=\"darkviolet\"><b><i>");
        fprintf(dot, "%s", toks + toks_s[sprods[pterms[term]]]);
        if (!terms[term])
          fprintf(dot, "</i></b></font>");
        fprintf(dot, " :");
        size_t first = term;
        while (first && terms[first - 1]) first--;
        for (size_t k = first; k < term; k++)
          fprintf(dot, " <font color=\"navy\"><b><i>%s</i></b></font>",
                  toks + toks_s[terms[k]]);
        for (size_t k = term; terms[k]; k++)
          fprintf(dot, " %s",
                  toks + toks_s[terms[k]]);
        fprintf(dot, "</td><td cellpadding=\"3\" border=\"1\" sides=\"rb\">");
        if (terms[term]) {
          fprintf(dot, "%s", toks + toks_s[terms[term]]);
        } else {
          char * trm = irtrms + row[pterms[term]] * ts_size;
          char prev = 0;
          for (sym_t k = 0; k < ts_size; k++)
            if (trm[k]) {
              if (prev)
                fprintf(dot, ",");
              fprintf(dot, "%s", toks + toks_s[ts[k]]);
              prev = 1;
            }
        }
        fprintf(dot, "</td></tr>");
      }
      fprintf(dot, "</table>>];\n");
    }
    for (item_t i = 0; i < isadj_size; i++) {
      for (size_t j = iadj[i]; j; j = inext[j - 1]) {
        fprintf(dot, " t%u -> t%u [label=\"%s\"];\n",
                i, ito[j - 1], toks + toks_s[isym[j - 1]]);
      }
    }
    fprintf(dot, "}\n");
close_dot:
    fclose(dot);
  }
  // done
  printf("done\n");
  // next steps
  //  0. construct phi-inaccessible tables
  // *1. merge compatible tables (postponement)
  // ~2. merge columns (elimination of single productions)
  // *3. construct default action list (error or reduce of tables),
  //      remove default action entries (reduce),
  //     construct default goto list (next table of nts),
  //      remove default goto entries (next table), and
  //     compress tables (double displacement)

  //da_trie iacts;
  //if ((ret = da_init(&iacts, 2)) != 0) {
  //  fprintf(stderr, "da_init(iacts) error: %d\n", ret);
  //  goto free_eadj;
  //}
  //if ((ret = da_tail_init(&iacts, 1)) != 0) {
  //  fprintf(stderr, "da_tail_init(iacts) error: %d\n", ret);
  //  goto free_iacts_ary;
  //}
  //size_t * iacts_key;
  //if ((iacts_key = malloc(ts_size * sizeof(* iacts_key) + 1)) == NULL) {
  //  perror("malloc iacts_key");
  //  goto free_iacts_tail;
  //}
  //size_t iacts_bytes = 0;
  //for (size_t i = 2 + isadj_size + prods_size - 1; i; i /= 255)
  //  iacts_bytes++;
  //item_t * acts;
  //if ((acts = malloc(isadj_size * sizeof(* acts))) == NULL) {
  //  perror("malloc acts");
  //  goto free_iacts_key;
  //}
  //item_t * racts;
  //if ((racts = malloc(isadj_size * sizeof(* racts))) == NULL) {
  //  perror("malloc racts");
  //  goto free_acts;
  //}
  //item_t racts_size = 0;
  //for (item_t i = 0; i < isadj_size; i++) {
  //  size_t * itrm = itrms + i * ts_size;
  //  size_t iacts_len = 0;
  //  for (sym_t j = 0; j < ts_size; j++) {
  //    size_t act = itrm[j];
  //    for (size_t k = 0; k < iacts_bytes; k++) {
  //      ((da_key_t *) iacts_key)[iacts_len++] = (da_key_t) (act % 255 + 1);
  //      act /= 255;
  //    }
  //  }
  //  ((da_key_t *) iacts_key)[iacts_len] = '\0';
  //  da_data_t da_ract;
  //  item_t ract;
  //  if (da_lookup(&iacts, (da_key_t *) iacts_key, &da_ract)) {
  //    ract = racts_size++;
  //    if ((ret = da_insert(&iacts, (da_key_t *) iacts_key, ract)) != 0) {
  //      fprintf(stderr, "da_insert(item %u) error: %d\n", i, ret);
  //      goto free_racts;
  //    }
  //    racts[ract] = i;
  //  } else {
  //    ract = (item_t) da_ract;
  //  }
  //  acts[i] = ract;
  //}
  //for (item_t i = 0; i < isadj_size; i++)
  //  printf(" %2u", i);
  //printf("\n");
  //for (item_t i = 0; i < isadj_size; i++)
  //  printf(" %2u", acts[i]);
  //printf("\n");
  //for (item_t i = 0; i < racts_size; i++)
  //  printf(" %2u", racts[i]);
  //printf("\n");
  //printf("\n");
  //sym_t * flen;
  //if ((flen = malloc(racts_size * sizeof(* flen))) == NULL) {
  //  perror("malloc flen");
  //  goto free_racts;
  //}
  //for (item_t i = 0; i < racts_size; i++)
  //  flen[i] = 0;
  //size_t fcapa = racts_size;
  //size_t * fsize;
  //if ((fsize = malloc(syms_size * sizeof(* fsize))) == NULL) {
  //  perror("malloc fsize");
  //  goto free_flen;
  //}
  //size_t fall = iedges;
  //size_t fcnt = 0;
  //size_t * fcol;
  //if ((fcol = malloc(syms_size * sizeof(* fcol))) == NULL) {
  //  perror("malloc flen");
  //  goto free_fsize;
  //}
  //for (sym_t i = 0; i < ts_size; i++)
  //  fsize[i] = 0;
  //for (item_t i = 0; i < racts_size; i++)
  //  if (itrms[racts[i] * ts_size])
  //    flen[i]++, fsize[0]++, fcnt++;
  //fcol[0] = 0;
  //for (sym_t i = 1; i < ts_size; i++) {
  //  for (item_t j = 0; j < racts_size; j++)
  //    if (itrms[racts[j] * ts_size + i])
  //      fcnt++;
  //  size_t off = 0;
  //  for (;;) {
  //    for (item_t j = 0; j < racts_size; j++)
  //      if (itrms[racts[j] * ts_size + i])
  //        fsize[flen[off + j]]++;
  //    double allow = (double) fcnt;
  //    double base = pow(2.0, 2.0 - (double) fcnt / (double) fall);
  //    char valid = 1;
  //    for (sym_t j = 0; j < ts_size; j++) {
  //      if (fsize[j] > allow) { valid = 0; break; }
  //      allow /= base;
  //    }
  //    if (valid) break;
  //    for (item_t j = 0; j < racts_size; j++)
  //      if (itrms[racts[j] * ts_size + i])
  //        fsize[flen[off + j]]--;
  //    off++;
  //    if (off + racts_size > fcapa) {
  //      size_t req = fcapa + racts_size;
  //      void * mem;
  //      if ((mem = realloc(flen, req)) == NULL) {
  //        perror("realloc flen");
  //        goto free_fcol;
  //      }
  //      flen = mem;
  //      for (size_t j = fcapa; j < req; j++)
  //        flen[j] = 0;
  //      fcapa = req;
  //    }
  //  }
  //  for (item_t j = 0; j < racts_size; j++)
  //    if (itrms[racts[j] * ts_size + i])
  //      flen[off + j]++;
  //  fcol[i] = off;
  //}
  //size_t fchip = 0;
  //size_t fsort = 0;
  //for (sym_t i = 0; i < ts_size; i++)
  //  fsize[i] = 0;
  //for (size_t i = 0; i < fcapa; i++)
  //  if (flen[i])
  //    fsize[flen[i] - 1]++, fsort++, fchip = i + 1;
  //for (sym_t i = (sym_t) (ts_size - 1); i; i--)
  //  fsize[i - 1] += fsize[i];
  //size_t * forder;
  //if ((forder = malloc(fsort * sizeof(* forder))) == NULL) {
  //  perror("malloc fcapa");
  //  goto free_fcol;
  //}
  //for (size_t i = fchip; i; i--)
  //  if (flen[i - 1])
  //    forder[--fsize[flen[i - 1] - 1]] = i - 1;
  //size_t * frow;
  //if ((frow = malloc(fchip * sizeof(* frow))) == NULL) {
  //  perror("malloc frow");
  //  goto free_forder;
  //}
  //char * fuse;
  //if ((fuse = malloc(syms_size * sizeof(* fuse))) == NULL) {
  //  perror("malloc frow");
  //  goto free_frow;
  //}
  //for (size_t i = 0; i < fchip; i++)
  //  frow[i] = 0;
  //for (sym_t i = 0; i < syms_size; i++)
  //  fuse[i] = 0;
  //size_t freq = syms_size;
  //for (size_t i = 0; i < fsort; i++) {
  //  size_t row = forder[i];
  //  size_t off = 0;
  //  for (;;) {
  //    char valid = 1;
  //    for (sym_t j = 0; j < ts_size; j++)
  //      if (row >= fcol[j] &&
  //          row < fcol[j] + racts_size &&
  //          itrms[racts[row - fcol[j]] * ts_size + j] &&
  //          fuse[off + j]) {
  //        valid = 0;
  //        break;
  //      }
  //    if (valid) break;
  //    off++;
  //    if (off + ts_size > freq) {
  //      size_t req = freq + ts_size;
  //      void * mem;
  //      if ((mem = realloc(fuse, req * sizeof(* fuse))) == NULL) {
  //        perror("realloc fuse");
  //        goto free_fuse;
  //      }
  //      fuse = mem;
  //      for (size_t j = freq; j < req; j++)
  //        fuse[j] = 0;
  //      freq = req;
  //    }
  //  }
  //  frow[row] = off;
  //  for (sym_t j = 0; j < ts_size; j++)
  //    if (row >= fcol[j] &&
  //        row < fcol[j] + racts_size &&
  //        itrms[racts[row - fcol[j]] * ts_size + j])
  //      fuse[off + j] = 1;
  //}
  //size_t fslice = 0;
  //for (size_t i = 0; i < freq; i++)
  //  if (fuse[i])
  //    fslice = i + 1;
  //size_t * fchk;
  //if ((fchk = malloc(fslice * sizeof(* fchk))) == NULL) {
  //  perror("malloc fchk");
  //  goto free_fuse;
  //}
  //size_t * fval;
  //if ((fval = malloc(fslice * sizeof(* fval))) == NULL) {
  //  perror("malloc fval");
  //  goto free_fchk;
  //}
  //for (size_t i = 0; i < fslice; i++)
  //  fchk[i] = 0;
  //for (size_t i = 0; i < fslice; i++)
  //  fval[i] = 0;
  //for (size_t i = 0; i < fsort; i++) {
  //  size_t row = forder[i];
  //  for (sym_t j = 0; j < ts_size; j++) {
  //    size_t to;
  //    if (row >= fcol[j] &&
  //        row < fcol[j] + racts_size &&
  //        (to = itrms[racts[row - fcol[j]] * ts_size + j]) > 0) {
  //      size_t index = frow[row] + j;
  //      fchk[index] = row + 1;
  //      fval[index] = to;
  //    }
  //  }
  //}
  //if (argc > 3) {
  //  for (item_t i = 0; i < racts_size; i++) {
  //    for (sym_t j = 0; j < ts_size; j++)
  //      printf(" %2zu", itrms[racts[i] * ts_size + j]);
  //    printf("\n");
  //  }
  //  printf("\n");
  //  for (sym_t i = 0; i < ts_size; i++) {
  //    printf(" %2u", i);
  //  }
  //  printf("\n");
  //  for (sym_t i = 0; i < ts_size; i++) {
  //    printf(" %2zu", fcol[i]);
  //  }
  //  printf("\n");
  //  printf("\n");
  //  for (size_t j = 0; j < fchip; j++) {
  //    printf(" %2zu", j);
  //  }
  //  printf("\n");
  //  for (size_t j = 0; j < fchip; j++) {
  //    printf(" %2zu", frow[j]);
  //  }
  //  printf("\n");
  //  printf("\n");
  //  for (size_t j = 0; j < fslice; j++) {
  //    printf(" %2zu", j);
  //  }
  //  printf("\n");
  //  for (size_t j = 0; j < fslice; j++) {
  //    printf(" %2zu", fval[j]);
  //  }
  //  printf("\n");
  //}
//  const char * output_s = "table.c";
//  FILE * output;
//  if ((output = fopen(output_s, "w+")) == NULL) {
//    perror(output_s);
//    goto free_eadj;
//  }
//  const char * itype = "unsigned int";
//  if (fprintf(output, "%s next[%zu][%zu] = {\n", itype, isadj_size, id) < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  for (item_t i = 0; i < isadj_size; i++) {
//    size_t * mat = imat + i * id;
//    if (fprintf(output, "  {") < 0) {
//      perror(output_s);
//      goto close_output;
//    }
//    for (sym_t j = 0; j < id; j++) {
//      if (mat[j]) {
//        if (fprintf(output, " %2zu", ito[mat[j] - 1] + 1) < 0) {
//          perror(output_s);
//          goto close_output;
//        }
//      } else {
//        if (fprintf(output, " %2zu", 0) < 0) {
//          perror(output_s);
//          goto close_output;
//        }
//      }
//      if (j + 1 < id) {
//        if (fprintf(output, ",") < 0) {
//          perror(output_s);
//          goto close_output;
//        }
//      }
//    }
//    if (fprintf(output, "}") < 0) {
//      perror(output_s);
//      goto close_output;
//    }
//    if (i + 1 < isadj_size) {
//      if (fprintf(output, ",") < 0) {
//        perror(output_s);
//        goto close_output;
//      }
//    }
//    if (fprintf(output, "\n") < 0) {
//      perror(output_s);
//      goto close_output;
//    }
//  }
//  if (fprintf(output, "};\n\n") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  if (fprintf(output, "size_t action[%zu][%zu] = {\n",
//              isadj_size, trms_size) < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  for (item_t i = 0; i < isadj_size; i++) {
//    size_t * itrm = itrms + i * trms_size;
//    if (fprintf(output, "  {") < 0) {
//      perror(output_s);
//      goto close_output;
//    }
//    for (sym_t j = 0; j < trms_size; j++) {
//      if (fprintf(output, " %2zu", itrm[j]) < 0) {
//        perror(output_s);
//        goto close_output;
//      }
//      if (j + 1 < trms_size) {
//        if (fprintf(output, ",") < 0) {
//          perror(output_s);
//          goto close_output;
//        }
//      }
//    }
//    if (fprintf(output, "}") < 0) {
//      perror(output_s);
//      goto close_output;
//    }
//    if (i + 1 < isadj_size) {
//      if (fprintf(output, ",") < 0) {
//        perror(output_s);
//        goto close_output;
//      }
//    }
//    if (fprintf(output, "\n") < 0) {
//      perror(output_s);
//      goto close_output;
//    }
//  }
//  if (fprintf(output, "};\n\n") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  if (fprintf(output, "size_t prods_len[%zu] = {\n", prods_size) < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  if (fprintf(output, " ") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  for (size_t i = 0; i < prods_size; i++) {
//    if (fprintf(output, " %zu", prods_len[i]) < 0) {
//      perror(output_s);
//      goto close_output;
//    }
//    if (i + 1 < prods_size) {
//      if (fprintf(output, ",") < 0) {
//        perror(output_s);
//        goto close_output;
//      }
//    }
//  }
//  if (fprintf(output, "\n") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  if (fprintf(output, "};\n\n") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  const char * stype = "unsigned char";
//  if (fprintf(output, "%s sprods[%zu] = {\n", stype, prods_size) < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  if (fprintf(output, " ") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  for (size_t i = 0; i < prods_size; i++) {
//    if (fprintf(output, " %zu", sprods[i]) < 0) {
//      perror(output_s);
//      goto close_output;
//    }
//    if (i + 1 < prods_size) {
//      if (fprintf(output, ",") < 0) {
//        perror(output_s);
//        goto close_output;
//      }
//    }
//  }
//  if (fprintf(output, "\n") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  if (fprintf(output, "};\n\n") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  if (fprintf(output, "%s trms_r[%zu] = {\n", stype, trms_size) < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  if (fprintf(output, " ") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  for (sym_t i = 0; i < trms_size; i++) {
//    if (fprintf(output, " %d", trms_r[i]) < 0) {
//      perror(output_s);
//      goto close_output;
//    }
//    if (i + 1 < trms_size) {
//      if (fprintf(output, ",") < 0) {
//        perror(output_s);
//        goto close_output;
//      }
//    }
//  }
//  if (fprintf(output, "\n") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  if (fprintf(output, "};\n\n") < 0) {
//    perror(output_s);
//    goto close_output;
//  }
//  ret = 0;
//close_output:
//  fclose(output);

//  free(fval);
//free_fchk:
//  free(fchk);
//free_fuse:
//  free(fuse);
//free_frow:
//  free(frow);
//free_forder:
//  free(forder);
//free_fcol:
//  free(fcol);
//free_fsize:
//  free(fsize);
//free_flen:
//  free(flen);
//free_racts:
//  free(racts);
//free_acts:
//  free(acts);
//free_iacts_key:
//  free(iacts_key);
//free_iacts_tail:
//  free(iacts.tail);
//free_iacts_ary:
//  free(iacts.ary);
free_eadj:
  free(eto);
  free(enext);
  free(eadj);
free_pnull:
  free(pnull);
free_itrms:
  free(itrms);
free_irtrms:
  free(irtrms);
free_iradj:
  free(irto);
  free(irnext);
  free(iradj);
free_irmat:
  free(irmat);
free_scc:
  free(child);
  free(parent);
  free(scc);
free_stash:
  free(stash);
free_etrms:
  free(etrms);
free_stack:
  free(stack);
free_iedge:
  free(iedge);
  free(isym);
  free(ito);
  free(inext);
  free(iadj);
  free(imat);
free_close:
  free(close);
free_isets_tail:
  free(isets.tail);
free_isets_ary:
  free(isets.ary);
free_iswork:
  free(isto);
  free(isnext);
  free(isadj);
  free(iswork);
free_nqueue:
  free(nqueue);
free_sprods:
  free(sprods);
free_pterms:
  free(pterms);
free_rules_len:
  free(rules_len);
free_prods_len:
  free(prods_len);
free_nterms:
  free(nterms);
free_nnext:
  free(nnext);
free_nadj:
  free(nadj);
free_null:
  free(null);
free_ts:
  free(ts);
free_tok:
  free(nts_p);
  free(nts);
  free(syms);
  free(rules);
  free(prods);
  free(terms);
  free(toks_s);
  free(toks);
  free(tok);
free_da_tail_ary:
  free(da.tail);
free_da_ary:
  free(da.ary);
free_text:
  free(text);
close_input:
  fclose(input);
  return ret;
}
