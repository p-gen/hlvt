/* ****************************************************************** */
/* Primitive and incomplete headless emulation base on VTParse by     */
/* Paul Williams' DEC                                                 */
/* compatible state machine parser (public domain) improved to manage */
/* UTF-8 sequences.                                                   */
/* Pierre Gentile                                                     */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

/* TODO: manage attributes */

#include "vtparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

typedef struct ll_node_s     ll_node_t;
typedef struct ll_s          ll_t;
typedef struct screen_s      screen_t;
typedef struct line_s        line_t;
typedef struct attrs_bytes_s attrs_bytes_t;

static void *
xmalloc(size_t size);

static void *
xcalloc(size_t num, size_t size);

static void *
xrealloc(void * ptr, size_t size);

char *
xstrdup(const char * p);

static int
ll_append(ll_t * const list, void * const data);

/* here for coherency but not used. */
static int
ll_prepend(ll_t * const list, void * const data);

/* Not used here */
#if 0
static void
ll_insert_before(ll_t * const list, ll_node_t * node, void * const data);

static void
ll_insert_after(ll_t * const list, ll_node_t * node, void * const data);

static int
ll_delete(ll_t * const list, ll_node_t * node);

static ll_node_t *
ll_find(ll_t * const, void * const, int (*)(const void *, const void *));
#endif

static void
ll_init(ll_t * list);

static ll_node_t *
ll_new_node(void);

static ll_t *
ll_new(void);

void
usage(char * prog);

static line_t *
line_new();

static void
display(screen_t * screen, unsigned height);

static line_t *
line_new();

void
screen_init(screen_t * s, unsigned height_opt);

static int
compar(const void * a, const void * b);

/* ******************************* */
/* Linked list specific structures */
/* ******************************* */

/* Linked list node structure */
/* """""""""""""""""""""""""" */
struct ll_node_s
{
  void *             data;
  struct ll_node_s * next;
  struct ll_node_s * prev;
};

/* Linked List structure */
/* """"""""""""""""""""" */
struct ll_s
{
  ll_node_t * head;
  ll_node_t * tail;
  size_t      len;
};

struct line_s
{
  unsigned         allocated; /* allocated memory                  */
  unsigned         length;    /* number of multiobytes in the line */
  unsigned         bytes;     /* nb of bytes in the line           */
  unsigned char *  string;    /* line content                      */
  attrs_bytes_t ** attrs;     /* array of attributes affected to    *
                                        each column of the line           */
};

struct attrs_bytes_s
{
  size_t          len;   /* number of bytes forming the attributes */
  unsigned char * bytes; /* attributes                             */
};

attrs_bytes_t curr_attrs_bytes;

struct screen_s
{
  ll_node_t * top;
  ll_node_t * bottom;
  ll_node_t * current;
  unsigned    saved_row;
  unsigned    column;
  unsigned    saved_column;
  ll_t *      lines;
};

screen_t screen;

char * my_optarg;     /* Global argument pointer. */
int    my_optind = 0; /* Global argv index. */
int    my_opterr = 1; /* for compatibility, should error be printed? */
int    my_optopt;     /* for compatibility, option character checked */

static const char * prog = "hlvt";
static char *       scan = NULL; /* Private scan pointer. */
static unsigned     no_attr;

/* ====================================================== */
/* Like strspn but based on length and not on a delimiter */
/* ====================================================== */
size_t
memspn(const char * data, size_t len, const char * accept, size_t accept_len)
{
  size_t i, j;
  for (i = 0; i < len; i++)
  {
    for (j = 0; j < accept_len; j++)
      if (accept[j] == data[i])
        goto cont_outer;
    break;
  cont_outer:;
  }
  return i;
}

/* ========================= */
/* qsort comparison function */
/* ========================= */
static int
compar(const void * a, const void * b)
{
  return *(unsigned char *)a - *(unsigned char *)b;
}

/* ===================================================================t */
/* Print the column and the attributes associated with the character at */
/* column n of the current line.                                        */
/* ===================================================================t */
static int
attrs_print(attrs_bytes_t ** attrs, size_t n)
{
  size_t          i, offset;
  attrs_bytes_t * v = attrs[n];

  if ((offset = memspn(v->bytes, v->len, "\0", 1)) == v->len)
    return 0;

  printf("%d:", n);
  for (i = offset; i < v->len; i++)
    printf("%02x", v->bytes[i]);
  fputs(" ", stdout);

  return 0;
}

/* ======================================================= */
/* Merge two sorted arrays, first_a with a integers and    */
/* second_a with b integers, into a sorted array result_a. */
/* Duplicated values are removed.                          */
/* ======================================================= */
int
attrs_merge(unsigned char * first_a, int a, unsigned char * second_a, int b,
            unsigned char * result_a)
{
  int           i, j, k;
  unsigned char old;

  i   = 0;
  j   = 0;
  k   = 0;
  old = '\0';

  while (i < a && j < b)
  {
    if (first_a[i] <= second_a[j])
    {
      /* copy first_a[i] to result_a[k] and move */
      /* the pointer i and k forward             */
      /* """"""""""""""""""""""""""""""""""""""" */
      if (first_a[i] != old)
        old = result_a[k++] = first_a[i];
      i++;
    }
    else
    {
      /* copy second_a[j] to result_a[k] and move */
      /* the pointer j and k forward              */
      /* """""""""""""""""""""""""""""""""""""""" */
      if (second_a[j] != old)
        old = result_a[k++] = second_a[j];
      j++;
    }
  }
  /* move the remaining elements in first_a into result_a */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  while (i < a)
  {
    if (first_a[i] != old)
      old = result_a[k++] = first_a[i];
    i++;
  }
  /* move the remaining elements in second_a into result_a */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
  while (j < b)
  {
    if (second_a[j] != old)
      old = result_a[k++] = second_a[j];
    j++;
  }

  return k;
}

/* ********************* */
/* Linked List functions */
/* ********************* */

/* ======================== */
/* Create a new linked list */
/* ======================== */
ll_t *
ll_new(void)
{
  ll_t * ret = xmalloc(sizeof(ll_t));
  ll_init(ret);

  return ret;
}

/* ======================== */
/* Initialize a linked list */
/* ======================== */
void
ll_init(ll_t * list)
{
  list->head = NULL;
  list->tail = NULL;
  list->len  = 0;
}

/* ==================================================== */
/* Allocate the space for a new node in the linked list */
/* ==================================================== */
ll_node_t *
ll_new_node(void)
{
  ll_node_t * ret = xmalloc(sizeof(ll_node_t));

  if (ret == NULL)
    errno = ENOMEM;

  return ret;
}

/* ==================================================================== */
/* Append a new node filled with its data at the end of the linked list */
/* The user is responsible for the memory management of the data        */
/* ==================================================================== */
int
ll_append(ll_t * const list, void * const data)
{
  int         ret = 1;
  ll_node_t * node;

  if (list)
  {
    node = ll_new_node();
    if (node)
    {
      node->data = data;
      node->next = NULL;

      node->prev = list->tail;
      if (list->tail)
        list->tail->next = node;
      else
        list->head = node;

      list->tail = node;

      ++list->len;
      ret = 0;
    }
  }

  return ret;
}

/* =================================================================== */
/* Put a new node filled with its data at the beginning of the linked  */
/* list. The user is responsible for the memory management of the data */
/* =================================================================== */
int
ll_prepend(ll_t * const list, void * const data)
{
  int         ret = 1;
  ll_node_t * node;

  if (list)
  {
    node = ll_new_node();
    if (node)
    {
      node->data = data;
      node->prev = NULL;

      node->next = list->head;
      if (list->head)
        list->head->prev = node;
      else
        list->tail = node;

      list->head = node;

      ++list->len;
      ret = 0;
    }
  }

  return ret;
}

/* ======================================================= */
/* Insert a new node before the specified node in the list */
/* TODO test it                                           */
/* ======================================================= */
void
ll_insert_before(ll_t * const list, ll_node_t * node, void * const data)
{
  ll_node_t * new_node;

  if (list)
  {
    if (node->prev == NULL)
      ll_prepend(list, data);
    else
    {
      new_node = ll_new_node();
      if (new_node)
      {
        new_node->data   = data;
        new_node->next   = node;
        new_node->prev   = node->prev;
        node->prev->next = new_node;

        ++list->len;
      }
    }
  }
}

/* ====================================================== */
/* Insert a new node after the specified node in the list */
/* TODO test it                                           */
/* ====================================================== */
void
ll_insert_after(ll_t * const list, ll_node_t * node, void * const data)
{
  ll_node_t * new_node;

  if (list)
  {
    if (node->next == NULL)
      ll_append(list, data);
    else
    {
      new_node = ll_new_node();
      if (new_node)
      {
        new_node->data   = data;
        new_node->prev   = node;
        new_node->next   = node->next;
        node->next->prev = new_node;

        ++list->len;
      }
    }
  }
}

/* ================================ */
/* Remove a node from a linked list */
/* ================================ */
int
ll_delete(ll_t * const list, ll_node_t * node)
{
  if (list->head == list->tail)
  {
    if (list->head != NULL)
      list->head = list->tail = NULL;
    else
      return 0;
  }
  else if (node->prev == NULL)
  {
    list->head       = node->next;
    list->head->prev = NULL;
  }
  else if (node->next == NULL)
  {
    list->tail       = node->prev;
    list->tail->next = NULL;
  }
  else
  {
    node->next->prev = node->prev;
    node->prev->next = node->next;
  }

  --list->len;

  return 1;
}

/* *************************** */
/* Memory allocation functions */
/* *************************** */

/* Created by Kevin Locke (from numerous canonical examples)         */
/*                                                                   */
/* I hereby place this file in the public domain.  It may be freely  */
/* reproduced, distributed, used, modified, built upon, or otherwise */
/* employed by anyone for any purpose without restriction.           */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

/* ================= */
/* Customized malloc */
/* ================= */
void *
xmalloc(size_t size)
{
  void * allocated;
  size_t real_size;

  real_size = (size > 0) ? size : 1;
  allocated = malloc(real_size);
  if (allocated == NULL)
  {
    fprintf(stderr,
            "Error: Insufficient memory (attempt to malloc %lu bytes)\n",
            (unsigned long int)size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* ================= */
/* Customized calloc */
/* ================= */
void *
xcalloc(size_t n, size_t size)
{
  void * allocated;

  n         = (n > 0) ? n : 1;
  size      = (size > 0) ? size : 1;
  allocated = calloc(n, size);
  if (allocated == NULL)
  {
    fprintf(stderr,
            "Error: Insufficient memory (attempt to calloc %lu bytes)\n",
            (unsigned long int)size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* ================== */
/* Customized realloc */
/* ================== */
void *
xrealloc(void * p, size_t size)
{
  void * allocated;

  allocated = realloc(p, size);
  if (allocated == NULL && size > 0)
  {
    fprintf(stderr,
            "Error: Insufficient memory (attempt to xrealloc %lu bytes)\n",
            (unsigned long int)size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* =================================== */
/* strdup implementation using xmalloc */
/* =================================== */
char *
xstrdup(const char * p)
{
  char * allocated;

  allocated = xmalloc(strlen(p) + 1);
  strcpy(allocated, p);

  return allocated;
}

/* ================================= */
/* Print message about a bad option. */
/* ================================= */
static int
badopt(const char * mess, int ch)
{
  if (my_opterr)
  {
    fputs(prog, stderr);
    fputs(mess, stderr);
    (void)putc(ch, stderr);
    (void)putc('\n', stderr);
  }
  return ('?');
}

/* ========================================================================== */
/* my_getopt - get option letter from argv                                    */
/*                                                                            */
/* This is a version of the public domain getopt() implementation by          */
/* Henry Spencer, changed for 4.3BSD compatibility (in addition to System V). */
/* It allows rescanning of an option list by setting optind to 0 before       */
/* calling, which is why we use it even if the system has its own (in fact,   */
/* this one has a unique name so as not to conflict with the system's).       */
/* Thanks to Dennis Ferguson for the appropriate modifications.               */
/*                                                                            */
/* This file is in the Public Domain.                                         */
/* ========================================================================== */
static int
my_getopt(int argc, char * argv[], const char * optstring)
{
  register char         c;
  register const char * place;

  my_optarg = NULL;

  if (my_optind == 0)
  {
    scan = NULL;
    my_optind++;
  }

  if (scan == NULL || *scan == '\0')
  {
    if (my_optind >= argc || argv[my_optind][0] != '-'
        || argv[my_optind][1] == '\0')
    {
      return (EOF);
    }
    if (argv[my_optind][1] == '-' && argv[my_optind][2] == '\0')
    {
      my_optind++;
      return (EOF);
    }

    scan = argv[my_optind++] + 1;
  }

  c         = *scan++;
  my_optopt = c & 0377;
  for (place = optstring; place != NULL && *place != '\0'; ++place)
    if (*place == c)
      break;

  if (place == NULL || *place == '\0' || c == ':' || c == '?')
  {
    return (badopt(": unknown option -", c));
  }

  place++;
  if (*place == ':')
  {
    if (*scan != '\0')
    {
      my_optarg = scan;
      scan      = NULL;
    }
    else if (my_optind >= argc)
    {
      return (badopt(": option requires argument -", c));
    }
    else
    {
      my_optarg = argv[my_optind++];
    }
  }

  return (c & 0377);
}

/* ============================================== */
/* Allocate and initialize a new line_t structure */
/* ============================================== */
line_t *
line_new()
{
  line_t * line = xmalloc(sizeof(line_t));

  line->allocated = 128;
  line->length    = 0;
  line->bytes     = 0;
  line->string    = xmalloc(line->allocated);
  *(line->string) = '\0';

  if (!no_attr)
    line->attrs = xmalloc(sizeof(attrs_bytes_t *) * 128);

  return line;
}

/* ================================================ */
/* Allocate and initialize a new screen_t structure */
/* ================================================ */
void
screen_init(screen_t * s, unsigned height_opt)
{
  unsigned i;
  line_t * line;

  s->column = 0;
  s->lines  = ll_new();

  for (i = 0; i < height_opt; i++)
  {
    line = line_new();
    ll_append(s->lines, line);
  }

  s->top          = s->lines->head;
  s->bottom       = s->lines->tail;
  s->current      = s->top;
  s->saved_row    = 0;
  s->saved_column = 0;
}

/* ============================================================ */
/* Counts the number of UTF-8 character in a byte stream before */
/* reaching a given column * (included).                        */
/* OUT: the byte position correspnding to the given column      */
/* ============================================================ */
unsigned
find_nth_char(unsigned char * str, unsigned len, unsigned column,
              unsigned * pos)
{
  unsigned n = 0;
  unsigned p;

  for (p = 0; p < len; p++)
  {
    if (n == column)
      break;

    if (str[p] <= 0x7f)
      goto next;
    if (str[p] >= 0xc2 && str[p] <= 0xdf)
      p++;
    else if (str[p] >= 0xe0 && str[p] <= 0xef)
      p += 2;
    else if (str[p] >= 0xf0 && str[p] <= 0xf4)
      p += 3;
    else
      continue;

  next:
    n++;
  }

  *pos = p;

  return n;
}

/* ===================================================== */
/* Callback function called for each DEC decoded element */
/* ===================================================== */
void
parser_callback(vtparse_t * parser, vtparse_action_t action, unsigned char ch)
{
  line_t *        cl;
  ll_node_t *     node;
  unsigned        i;
  static unsigned ch_bytes = 1;
  static unsigned rem_bytes;

  /* Lengh in bytes of the utf-8 character introduced by ch */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (ch <= 0x7f)
  {
    ch_bytes  = 1;
    rem_bytes = 0;
  }
  else if (rem_bytes == 0 && ch >= 0xc2 && ch <= 0xdf)
  {
    ch_bytes  = 2;
    rem_bytes = 1;
  }
  else if (rem_bytes == 0 && ch >= 0xe0 && ch <= 0xef)
  {
    ch_bytes  = 3;
    rem_bytes = 2;
  }
  else if (rem_bytes == 0 && ch >= 0xf0 && ch <= 0xf4)
  {
    ch_bytes  = 4;
    rem_bytes = 3;
  }
  else if (rem_bytes > 0 && ch >= 0x80 && ch <= 0xbf)
    rem_bytes--;
  else /* invalid */
  {
    ch_bytes = rem_bytes = 1;
    rem_bytes            = 0;
    ch                   = '?';
  }

  switch (action)
  {
    attrs_bytes_t * attrs;

    case VTPARSE_ACTION_PRINT:
      /* TODO: manage 0x7f (DEL) */
      /* """"""""""""""""""""""" */
      cl = (line_t *)screen.current->data;

      if (cl->bytes == cl->allocated - 1)
      {
        cl->allocated += 64;
        cl->string = xrealloc(cl->string, cl->allocated);
      }

      /* We need to add spaces at the end of the line if the column to */
      /* write is after it.                                            */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (screen.column > cl->length)
      {
        if (!no_attr)
          cl->attrs = xrealloc(cl->attrs, 64 * sizeof(attrs_bytes_t *)
                                            * (screen.column / 64 + 1));

        for (i = 0; i < screen.column - cl->length; i++)
        {
          if (cl->bytes + i == cl->allocated - 1)
          {
            cl->allocated += 64;
            cl->string = xrealloc(cl->string, cl->allocated);
          }
          *(cl->string + cl->bytes + i) = ' ';

          if (!no_attr)
          {
            attrs                     = xmalloc(sizeof(attrs_bytes_t));
            attrs->len                = 1;
            attrs->bytes              = xmalloc(1);
            *attrs->bytes             = '\0';
            cl->attrs[cl->length + i] = attrs;
          }
        }

        cl->length += i;
        cl->bytes += i;
      }

      if (!no_attr)
      {
        attrs        = xmalloc(sizeof(attrs_bytes_t));
        attrs->len   = curr_attrs_bytes.len;
        attrs->bytes = xmalloc(attrs->len);

        for (i = 0; i < attrs->len; i++)
          attrs->bytes[i] = curr_attrs_bytes.bytes[i];
      }

      /* if we are at the end of the current line, increase its size */
      /* by 1 and if the UTF-8 character is complete, add 1 to the   */
      /* logical length of the line and the column.                  */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (screen.column == cl->length)
      {
        *(cl->string + cl->bytes) = ch;
        cl->bytes++;
        *(cl->string + cl->bytes) = '\0';

        if (rem_bytes == 0) /* A complete UTF-8 sequence has been read */
        {
          if (!no_attr && attrs->len > 0)
            cl->attrs[screen.column] = attrs;

          cl->length++;
          screen.column++;
        }
      }

      /* We are in the middle of an existing line, we need to find the */
      /* offset of the possible UTF-8 character at the current column, */
      /* before writing it.                                            */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      else
      {
        unsigned pos;

        (void)find_nth_char(cl->string, cl->bytes, screen.column, &pos);
        *(cl->string + pos + ch_bytes - rem_bytes - 1) = ch;

        if (rem_bytes == 0)
        {
          if (!no_attr && attrs->len > 0)
          {
            attrs_bytes_t * old_attrs;

            old_attrs = cl->attrs[screen.column];
            free(old_attrs->bytes);
            free(old_attrs);

            cl->attrs[screen.column] = attrs;
          }

          screen.column++;
          if (pos + ch_bytes - rem_bytes - 1 > cl->bytes)
            cl->bytes = pos + ch_bytes - rem_bytes;
        }
      }
      break;

    case VTPARSE_ACTION_ESC_DISPATCH:
      switch (ch)
      {
        unsigned n;

        case '7': /* Save cursor position and attributes DECSC */
          screen.saved_row = 0;
          node             = screen.current;
          while (node != screen.top)
          {
            screen.saved_row++;
            node = node->prev;
          }
          screen.saved_column = screen.column;
          break;

        case '8': /* Restore cursor position and attributes DECSC */
          n              = screen.saved_row;
          screen.current = screen.top;
          while (n > 0)
          {
            screen.current = screen.current->next;
            n--;
          }
          screen.column = screen.saved_column;
          break;

        case 'E':
          if (screen.current == screen.lines->tail)
          {
            ll_append(screen.lines, line_new());
            screen.current = screen.lines->tail;
            screen.top     = screen.top->next;
            screen.bottom  = screen.current;
          }
          else
          {
            if (screen.current == screen.bottom)
            {
              screen.top    = screen.top->next;
              screen.bottom = screen.bottom->next;
            }
            screen.current = screen.current->next;
          }
          screen.column = 0;
          break;

        case 'D':
          if (screen.current < screen.bottom)
            screen.current = screen.current->next;
          /* TODO: scroll up when the bottom of region is reached */
          break;

        case 'M':
          if (screen.current > screen.top)
            screen.current = screen.current->prev;
          /* TODO: scroll down when the top of region is reached */
          break;

        default:
          break;
      }
      break;

    case VTPARSE_ACTION_CSI_DISPATCH:
      switch (ch)
      {
        case 'A': /* Move cursor up n lines CUU */
          if (parser->num_params == 0)
          {
            if (screen.current > screen.top)
              screen.current = screen.current->prev;
          }
          else
            for (i = 0; i < parser->params[0]; i++)
            {
              if (screen.current > screen.top)
                screen.current = screen.current->prev;
              else
                break;
            }
          break;

        case 'B': /* Move cursor down n lines CUD */
          if (parser->num_params == 0)
          {
            if (screen.current < screen.bottom)
              screen.current = screen.current->next;
          }
          else
            for (i = 0; i < parser->params[0]; i++)
            {
              if (screen.current < screen.bottom)
                screen.current = screen.current->next;
              else
                break;
            }
          break;

        case 'C': /* Move cursor right n lines CUF */
          if (parser->num_params == 0)
            screen.column++;
          else
            for (i = 0; i < parser->params[0]; i++)
              screen.column++;
          break;

        case 'D': /* Move cursor left n lines CUB */
          if (parser->num_params == 0)
          {
            if (screen.column > 0)
              screen.column--;
          }
          else
            for (i = 0; i < parser->params[0]; i++)
            {
              if (screen.column > 0)
                screen.column--;
              else
                break;
            }
          break;

        case 'G': /* Move cursor - hpa */
          if (parser->num_params != 1)
            goto error;

          if (parser->params[0] == 0)
            screen.column = 0;
          else
            screen.column = parser->params[0] - 1;
          break;

        case 'f': /* Move cursor */
        case 'H': /* Move cursor */
          if (parser->num_params == 0)
          {
            /* To upper left corner - cursorhome */
            screen.current = screen.top;
            screen.column  = 0;
          }
          else /* Move cursor to screen location y,x - CUP */
          {
            unsigned x, y;

            if (parser->num_params != 2)
              goto error;

            y = parser->params[0];
            x = parser->params[1];

            /* x and y are 1 based byt treats 0 the same as 1 */
            /* """""""""""""""""""""""""""""""""""""""""""""" */
            if (x > 0)
              x--;
            if (y > 0)
              y--;

            if (y > 23)
              y = 23;

            /* go down y lines from th first line */
            /* """""""""""""""""""""""""""""""""" */
            node = screen.top;
            for (i = 0; i < y; i++)
              node = node->next;

            screen.current = node;
            screen.column  = x;
          }
          break;

        case 'J':
        {
          ll_node_t * start_node = screen.top;
          ll_node_t * stop_node  = screen.bottom;

          if (parser->num_params == 0
              || (parser->num_params == 1 && parser->params[0] == 0))
          {
            start_node = screen.current;
            stop_node  = screen.bottom;
          }
          else if (parser->num_params == 1)
          {
            switch (parser->params[0])
            {
              case 1:
                start_node = screen.top;
                stop_node  = screen.current;
                break;

              case 2:
                start_node = screen.top;
                stop_node  = screen.bottom;
                break;
            }
          }

          node = start_node;
          do
          {
            cl        = (line_t *)node->data;
            cl->bytes = cl->length = 0;
            cl->string[0]          = '\0';
            node                   = node->next;
          } while (node != NULL && node != stop_node);
        }
        break;

        case 'K': /* Clear line */
        {
          unsigned n;
          unsigned pos;

          cl = (line_t *)screen.current->data;
          n  = find_nth_char(cl->string, cl->bytes, screen.column, &pos);
          if (parser->num_params == 0
              || (parser->num_params == 1 && parser->params[0] == 0))
          {
            cl->bytes       = pos;
            cl->length      = n;
            cl->string[pos] = '\0';
          }
          else
            switch (parser->params[0])
            {
              case 1:
                /* from cursor left EL1 */
                for (i = 0; i < pos; i++)
                  cl->string[i] = ' ';
                break;
              case 2:
                /* from entire line EL2 */
                for (i = 0; i < cl->bytes; i++)
                  cl->string[i] = ' ';
                cl->bytes = cl->length = 0;
                cl->string[0]          = '\0';

                break;
            }
          break;
        }

        case 'm':
          if (!no_attr)
          {
            if (parser->num_params == 0
                || (parser->num_params == 1 && parser->params[0] == '\0'))
            {
              curr_attrs_bytes.len      = 1;
              curr_attrs_bytes.bytes[0] = '\0';
            }
            else
            {
              /* The current attribute structure is empty */
              /* """""""""""""""""""""""""""""""""""""""" */
              if (curr_attrs_bytes.bytes[0] == '\0')
              {
                curr_attrs_bytes.len = parser->num_params;
                curr_attrs_bytes.bytes =
                  xrealloc(curr_attrs_bytes.bytes, curr_attrs_bytes.len);

                for (i = 0; i < parser->num_params; i++)
                  curr_attrs_bytes.bytes[i] = parser->params[i];
                qsort(curr_attrs_bytes.bytes, parser->num_params, 1, compar);
              }
              else
              {
                /* The current attribute structure already contains */
                /* some attributes, we need to merge the new ones   */
                /* """""""""""""""""""""""""""""""""""""""""""""""" */
                unsigned char * bytes;
                int             len;

                bytes = xmalloc(curr_attrs_bytes.len + parser->num_params);
                qsort(parser->params, parser->num_params, 1, compar);
                len = attrs_merge(curr_attrs_bytes.bytes, curr_attrs_bytes.len,
                                  parser->params, parser->num_params, bytes);
                bytes = xrealloc(bytes, len);
                free(curr_attrs_bytes.bytes);

                curr_attrs_bytes.len   = len;
                curr_attrs_bytes.bytes = bytes;
              }
            }
          }
          break;

        default:
          break;
      }
      break;

    case VTPARSE_ACTION_EXECUTE:
      switch (ch)
      {
        case 0x0d: /* Carriage return */
          screen.column = 0;
          break;

        case 0x08: /* Backspace */
          if (screen.column > 0)
            screen.column--;
          break;

        case 0x09: /* Tab */
          /* FIXME: for now assume a hard coded tab each 8 characters */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          screen.column = (screen.column + 8) / 8 * 8;
          break;

        case 0x0a: /* Line feed */
          if (screen.current == screen.lines->tail)
          {
            ll_append(screen.lines, line_new());
            screen.current = screen.lines->tail;
            screen.top     = screen.top->next;
            screen.bottom  = screen.current;
          }
          else
          {
            if (screen.current == screen.bottom)
            {
              screen.top    = screen.top->next;
              screen.bottom = screen.bottom->next;
            }
            screen.current = screen.current->next;
          }
          screen.column = 0;
          break;
      }
      break;

    case VTPARSE_ACTION_CLEAR:
      break;

    case VTPARSE_ACTION_COLLECT:
      break;

    case VTPARSE_ACTION_HOOK:
      break;

    case VTPARSE_ACTION_UNHOOK:
      break;

    case VTPARSE_ACTION_OSC_END:
      break;

    case VTPARSE_ACTION_OSC_PUT:
      break;

    case VTPARSE_ACTION_OSC_START:
      break;

    case VTPARSE_ACTION_PARAM:
      break;

    case VTPARSE_ACTION_PUT:
      break;

    case VTPARSE_ACTION_IGNORE:
      break;

    case VTPARSE_ACTION_ERROR:
    error:
      fprintf(stderr, "ch: %c\n", ch);
      exit(1);
      break;
  }
#if 0
    default:
      printf("Received action %s %d\n", ACTION_NAMES[action], action);
      if (ch != 0)
        printf("Char: 0x%02x ('%c')\n", ch, ch);
      if (parser->num_intermediate_chars > 0)
      {
        printf("%d Intermediate chars:\n", parser->num_intermediate_chars);
        for (i = 0; i < parser->num_intermediate_chars; i++)
          printf("  0x%02x ('%c')\n", parser->intermediate_chars[i],
                 parser->intermediate_chars[i]);
      }
      if (parser->num_params > 0)
      {
        printf("%d Parameters:\n", parser->num_params);
        for (i = 0; i < parser->num_params; i++)
          printf("\t%d\n", parser->params[i]);
      }

      printf("\n");
      break;
#endif
}

/* ================================================================ */
/* Screen display, only the first non-empty lines will be displayes */
/*                                                                  */
/* screen    (IN): screen to display                                */
/* frame_opt (IN): 1: display some screen meta-data, 0 omit them    */
/* ================================================================ */
void
display(screen_t * screen, unsigned frame_opt)
{
  line_t *    line;
  ll_node_t * node;
  ll_node_t * last;
  unsigned    n = 1;

  if (frame_opt)
    puts("--- virtual display top ---");

  /* Find the latest non-empty line in the virtual screen */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  last = screen->bottom;
  while (last)
  {
    line = (line_t *)(last->data);
    if (*(line->string) != '\0')
      break;
    last = last->prev;
  }

  /* Displays the used lines from the top of the virtual screen */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  node = screen->top;
  while (node)
  {
    line = (line_t *)(node->data);
    if (frame_opt)
    {
      if (node == screen->current)
        printf("%3d:-%s|\n", n, line->string); /* Current line */
      else
        printf("%3d:|%s|\n", n, line->string);
    }
    else
      puts((char *)line->string);

    if (!no_attr)
    {
      size_t i;

      for (i = 0; i < line->length; i++)
        attrs_print(line->attrs, i);
      puts("");
    }

    if (node == last)
      break;

    n++;
    node = node->next;
  }
  if (frame_opt)
    puts("--- virtual display bottom ---");
}

/* ===================================== */
/* Usage display in case of syntax error */
/* ===================================== */
void
usage(char * prog)
{
  printf("usage: %s [-l screen_lines] [-f] [-n]\n", prog);
  exit(EXIT_FAILURE);
}

/* ============= */
/* Program entry */
/* ============= */
int
main(int argc, char ** argv)
{
  unsigned char buf[1024];
  int           bytes;
  int           n;
  int           end;
  int           opt;
  unsigned      height_opt;
  unsigned      frame_opt;
  vtparse_t     parser;

  height_opt = 24; /* Defaults to 24 lines             */
  frame_opt  = 0;  /* Displays the window's frame      */
  no_attr    = 0;  /* Enables DEC attributes reporting */

  while ((opt = my_getopt(argc, argv, "l:fn")) != -1)
  {
    switch (opt)
    {
      case 'l':
        n = sscanf(my_optarg, "%u%n", &height_opt, &end);
        if (n != 1 || my_optarg[end] != '\0')
          usage((char *)prog);
        if (height_opt == 0)
          usage((char *)prog);
        break;

      case 'f':
        frame_opt = 1;
        break;

      case 'n':
        no_attr = 1;
        break;

      default:
        usage(argv[0]);
        break;
    }
  }

  if (my_optind < argc)
  {
    fputs("arguments are not allowed.\n", stderr);
    exit(EXIT_FAILURE);
  }

  /* Various initializations */
  /* """"""""""""""""""""""" */
  vtparse_init(&parser, parser_callback);
  screen_init(&screen, height_opt);

  if (!no_attr)
  {
    /* Initialization of the current attributes to 'no attribute' */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    curr_attrs_bytes.len      = 1;
    curr_attrs_bytes.bytes    = xmalloc(1);
    curr_attrs_bytes.bytes[0] = '\0';
  }

  /* Parsing */
  /* """"""" */
  do
  {
    bytes = read(STDIN_FILENO, buf, 1024);
    vtparse(&parser, buf, bytes);
  } while (bytes > 0);

  /* Final screen display with attributes */
  /* """""""""""""""""""""""""""""""""""" */
  display(&screen, frame_opt);

  return 0;
}
