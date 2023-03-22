#include "linked_list.h"

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

int cmp(element *a, element *b)
{
  // Function comparing list nodes (lexicographic order).
  return strcmp(a->entry->d_name, b->entry->d_name);
}

void initialize(list *l)
{
  // Set pointers to the first and last nodes to NULL.
  l->first = l->last = NULL;
  // Set the number of nodes to 0.
  l->count = 0;
}

int pushBack(list *l, struct dirent *newEntry)
{
  element *new = NULL;
  // Reserve memory for a new list node. If an error occured
  if ((new = malloc(sizeof(element))) == NULL)
    // Return an error code.
    return -1;
  // Store the directory entry pointer in the list node 
  new->entry = newEntry;
  // Set the pointer to the next list node to NULL.
  new->next = NULL;
  // If the list is empty, thus first == NULL i last == NULL
  if (l->first == NULL)
  {
    // Set the new node as the first one
    l->first = new;
    // and the last one.
    l->last = new;
  }
  // If the list is not empty
  else
  {
    // Set the new node as the next after current last one.
    l->last->next = new;
    // Move the last node pointer to the newly added node.
    l->last = new;
  }
  // Increment the number of nodes.
  ++l->count;
  // Return the correct ending code.
  return 0;
}

void clear(list *l)
{
  // Save the pointer to the first node.
  element *cur = l->first, *next;
  while (cur != NULL)
  {
    // Save the pointer to the next node.
    next = cur->next;
    // Release the node's memory.
    free(cur);
    // Move the pointer to the next node.
    cur = next;
  }
  // Zero out the list's fields.
  initialize(l);
}

void listMergeSort(list *l)
{
  element *p, *q, *e, *tail, *list = l->first;
  int insize, nmerges, psize, qsize, i;
  if (list == NULL) // Silly special case: if `list' was passed in as NULL, return immediately.
    return;
  insize = 1;
  while (1)
  {
    p = list;
    list = NULL;
    tail = NULL;
    nmerges = 0; // count number of merges we do in this pass
    while (p)
    {
      nmerges++; // there exists a merge to be done
      // step `insize' places along from p
      q = p;
      psize = 0;
      for (i = 0; i < insize; i++)
      {
        psize++;
        q = q->next;
        if (!q)
          break;
      }
      // if q hasn't fallen off end, we have two lists to merge
      qsize = insize;
      // now we have two lists; merge them
      while (psize > 0 || (qsize > 0 && q))
      {
        // decide whether next element of merge comes from p or q
        if (psize == 0)
        {
          // p is empty; e must come from q.
          e = q;
          q = q->next;
          qsize--;
        }
        else if (qsize == 0 || !q)
        {
          // q is empty; e must come from p.
          e = p;
          p = p->next;
          psize--;
        }
        else if (cmp(p, q) <= 0)
        {
          // First element of p is lower (or same); e must come from p.
          e = p;
          p = p->next;
          psize--;
        }
        else
        {
          // First element of q is lower; e must come from q.
          e = q;
          q = q->next;
          qsize--;
        }
        // add the next element to the merged list
        if (tail)
        {
          tail->next = e;
        }
        else
        {
          list = e;
        }
        tail = e;
      }
      // now p has stepped `insize' places along, and q has too
      p = q;
    }
    tail->next = NULL;
    // If we have done only one merge, we're finished.
    if (nmerges <= 1) // allow for nmerges==0, the empty list case
    {
      l->last = tail;
      l->first = list;
      return;
    }
    // Otherwise repeat, merging lists twice the size
    insize *= 2;
  }
}
