#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct element element;
/*
Singly linked list node storing pointers to a directory entry
  and to next list node.
*/
struct element
{
  // Next list node.
  element *next;
  // Pointer to a directory entry (file or subdirectory).
  struct dirent *entry;
};

/*
Compares nodes of a singly linked list.
reads:
a - first node
b - second node
returns:
< 0 if a is before b in lexicographic order by directory entry name
0 if a and b have equal directory entry names
> 0 if a is after b in lexicographic order by directory entry name
*/
int cmp(element *a, element *b);

typedef struct list list;
/*
Singly linked list. In functions operating on a list,
  we assume that a valid pointer to it is given.
*/
struct list
{
  /* The first and last nodes. Save a pointer to the last node to add new nodes
  to the list in constant time. */
  element *first, *last;
  // Number of list nodes.
  unsigned int count;
};

/*
Initializes the singly linked list.
writes:
l - empty singly linked list intended for the first use
*/
void initialize(list *l);

/*
Adds a directory entry at the end of the list.
reads:
newEntry - directory entry to be added at the end of the list
writes:
l - singly linked list with added node containing directory entry newEntry
*/
int pushBack(list *l, struct dirent *newEntry);

/*
Clears the singly linked list.
writes:
l - empty singly linked list intended for reuse
*/
void clear(list *l);

/* https://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
 * This file is copyright 2001 Simon Tatham.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*
Sorts by merging the singly linked list. This function contains
  author's original comments.
writes:
l - singly linked list sorted using cmp function comparing nodes
*/
void listMergeSort(list *l);

#endif // LINKED_LIST_H
