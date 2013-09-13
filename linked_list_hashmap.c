/*
 
Copyright (c) 2011, Willem-Hendrik Thiart
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * The names of its contributors may not be used to endorse or promote
      products derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL WILLEM-HENDRIK THIART BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <assert.h>

#include "linked_list_hashmap.h"

/* when we call for more capacity */
#define SPACERATIO 0.5

typedef struct hash_node_s hash_node_t;

struct hash_node_s
{
    hash_entry_t ety;
    hash_node_t *next;
};

static void __ensurecapacity(
    hashmap_t * h
);

/**
 * Allocate memory for nodes. Used for chained nodes. */
static hash_node_t *__allocnodes(
    unsigned int count
)
{
    // FIXME: make a chain node reservoir
    return calloc(count, sizeof(hash_node_t));
}

hashmap_t *hashmap_new(
    func_longhash_f hash,
    func_longcmp_f cmp,
    unsigned int initial_capacity
)
{
    hashmap_t *h;

    h = calloc(1, sizeof(hashmap_t));
    h->arraySize = initial_capacity;
    h->array = __allocnodes(h->arraySize);
    h->hash = hash;
    h->compare = cmp;
    return h;
}

/**
 * @return number of items within hash */
int hashmap_count(const hashmap_t * h)
{
    return h->count;
}

/**
 * @return size of the array used within hash */
int hashmap_size(
    hashmap_t * h
)
{
    return h->arraySize;
}

/**
 * free all the nodes in a chain, recursively. */
static void __node_empty(
    hashmap_t * h,
    hash_node_t * node
)
{
    if (NULL == node)
    {
        return;
    }
    else
    {
        __node_empty(h, node->next);
        free(node);
        h->count--;
    }
}

/**
 * Empty this hash. */
void hashmap_clear(
    hashmap_t * h
)
{
    int ii;

    for (ii = 0; ii < h->arraySize; ii++)
    {
        hash_node_t *node;

        node = &((hash_node_t *) h->array)[ii];

        if (NULL == node->ety.key)
            continue;

        /* normal actions will overwrite the value */
        node->ety.key = NULL;

        /* empty and free this chain */
        __node_empty(h, node->next);
        node->next = NULL;

        h->count--;
        assert(0 <= h->count);
    }

    assert(0 == hashmap_count(h));
}

/**
 * Free all the memory related to this hash. */
void hashmap_free(
    hashmap_t * h
)
{
    assert(h);
    hashmap_clear(h);
}

/**
 * Free all the memory related to this hash.
 * This includes the actual h itself. */
void hashmap_freeall(
    hashmap_t * h
)
{
    assert(h);
    hashmap_free(h);
    free(h);
}

inline static unsigned int __doProbe(
    hashmap_t * h,
    const void *key
)
{
    return h->hash(key) % h->arraySize;
}

/**
 * Get this key's value.
 * @return key's item, otherwise NULL */
void *hashmap_get(
    hashmap_t * h,
    const void *key
)
{
    unsigned int probe;

    hash_node_t *node;

    if (0 == hashmap_count(h) || !key)
        return NULL;

    probe = __doProbe(h, key);
    node = &((hash_node_t *) h->array)[probe];
    
    if (NULL == node->ety.key)
    {
        return NULL; /* we don't have this item */
    }
    else
    {
        /* iterate down the node's linked list chain */
        do
        {
            if (0 == h->compare(key, node->ety.key))
            {
                return (void *) node->ety.val;
            }
        }
        while ((node = node->next)); 
    }

    return NULL;
}

/**
 * Is this key inside this map?
 * @return 1 if key is in hash, otherwise 0 */
int hashmap_contains_key(
    hashmap_t * h,
    const void *key
)
{
    return (NULL != hashmap_get(h, key));
}

/**
 * Remove the value refrenced by this key from the hash. */
void hashmap_remove_entry(
    hashmap_t * h,
    hash_entry_t * entry,
    const void *key
)
{
    hash_node_t *n, *n_parent;

    n = &((hash_node_t *) h->array)[__doProbe(h, key)];

    if (!n->ety.key)
        goto notfound;

    n_parent = NULL;

    do {
        if (0 != h->compare(key, n->ety.key))
        {
            /* does not match, lets traverse the chain.. */
            n_parent = n;
            n = n->next;
            continue;
        }

        memcpy(entry, &n->ety, sizeof(hash_entry_t));

        /* I am a root node on the array */
        if (!n_parent)
        {
            /* I have a node on my chain. This node will replace me */
            if (n->next)
            {
                hash_node_t *tmp;
                
                tmp = n->next;
                memcpy(&n->ety, &tmp->ety, sizeof(hash_entry_t));
                /* Replace me with my next on chain */
                n->next = tmp->next;
                free(tmp);
            }
            else
            {
                /* un-assign */
                n->ety.key = NULL;
            }
        }
        else
        {
            /* Replace me with my next on chain */
            n_parent->next = n->next;
            free(n);
        }

        h->count--;
        return;

    } while (n);
   
notfound:
    entry->key = NULL;
    entry->val = NULL;
}

/**
 * Remove this key and value from the map.
 * @return value of key, or NULL on failure */
void *hashmap_remove(
    hashmap_t * h,
    const void *key
)
{
    hash_entry_t entry;

    hashmap_remove_entry(h, &entry, key);
    return (void *) entry.val;
}

inline static void __nodeassign(
    hashmap_t * h,
    hash_node_t * node,
    void *key,
    void *val
)
{
    /* Don't double increment if we are replacing an item */
    if (!node->ety.key)
        h->count++;

    assert(h->count < 32768);
    node->ety.key = key;
    node->ety.val = val;
}

/**
 * Associate key with val.
 * Does not insert key if an equal key exists.
 * @return previous associated val; otherwise NULL */
void *hashmap_put(
    hashmap_t * h,
    void *key,
    void *val_new
)
{
    hash_node_t *node;

    if (!key || !val_new)
        return NULL;

    assert(key);
    assert(val_new);
    assert(h->array);

    __ensurecapacity(h);

    node = &((hash_node_t *) h->array)[__doProbe(h, key)];

    assert(node);

    /* this one wasn't assigned */
    if (NULL == node->ety.key)
    {
        __nodeassign(h, node, key, val_new);
    }
    else
    {
        /* check the linked list */
        do
        {
            /* if same key, then we are just replacing val */
            if (0 == h->compare(key, node->ety.key))
            {
                void *val_prev;
                
                val_prev = node->ety.val;
                node->ety.val = val_new;
                return val_prev;
            }
        }
        while (node->next && (node = node->next));

        node->next = __allocnodes(1);
        __nodeassign(h, node->next, key, val_new);
    }

    return NULL;
}

/**
 * Put this key/value entry into the hash */
void hashmap_put_entry(
    hashmap_t * h,
    hash_entry_t * entry
)
{
    hashmap_put(h, entry->key, entry->val);
}

/**
 * Increase hash capacity.
 * @param factor : increase by this factor */
void hashmap_increase_capacity(
    hashmap_t * h,
    unsigned int factor)
{
    hash_node_t *array_old;
    int ii, asize_old;

    /*  stored old array */
    array_old = h->array;
    asize_old = h->arraySize;

    /*  double array capacity */
    h->arraySize *= factor;
    h->array = __allocnodes(h->arraySize);
    h->count = 0;

    for (ii = 0; ii < asize_old; ii++)
    {
        hash_node_t *node;
        
        node = &((hash_node_t *) array_old)[ii];

        /*  if key is null */
        if (NULL == node->ety.key)
            continue;

        hashmap_put(h, node->ety.key, node->ety.val);

        /* re-add chained hash nodes */
        node = node->next;

        while (node)
        {
            hash_node_t *next;
            
            next = node->next;
            hashmap_put(h, node->ety.key, node->ety.val);
            assert(NULL != node->ety.key);
            free(node);
            node = next;
        }
    }

    free(array_old);
}

static void __ensurecapacity(
    hashmap_t * h
)
{
    if ((float) h->count / h->arraySize < SPACERATIO)
    {
        return;
    }
    else
    {
        hashmap_increase_capacity(h,2);
    }
}

void* hashmap_iterator_peek(
    hashmap_t * h,
    hashmap_iterator_t * iter
)
{
    if (NULL == iter->cur_linked)
    {
        for (; iter->cur < h->arraySize; iter->cur++)
        {
            hash_node_t *node;

            node = &((hash_node_t *) h->array)[iter->cur];

            if (node->ety.key)
                return node->ety.key;
        }

        return NULL;
    }
    else
    {
        hash_node_t *node;

        node = iter->cur_linked;
        return node->ety.key;
    }
}

void* hashmap_iterator_peek_value(
    hashmap_t * h,
    hashmap_iterator_t * iter)
{
    return hashmap_get(h,hashmap_iterator_peek(h,iter));
}

int hashmap_iterator_has_next(
    hashmap_t * h,
    hashmap_iterator_t * iter
)
{
    return NULL != hashmap_iterator_peek(h,iter);
}

/**
 * Iterate to the next item on a hash iterator
 * @return next item value from iterator */
void *hashmap_iterator_next_value(
    hashmap_t * h,
    hashmap_iterator_t * iter
)
{
    void* k;
    
    k = hashmap_iterator_next(h,iter);
    if (!k) return NULL;
    return hashmap_get(h,k);
}

/**
 * Iterate to the next item on a hash iterator
 * @return next item key from iterator */
void *hashmap_iterator_next(
    hashmap_t * h,
    hashmap_iterator_t * iter
)
{
    hash_node_t *n;

    assert(iter);

    /* if we have a node ready to look at on the chain.. */
    if ((n = iter->cur_linked))
    {
        hash_node_t *n_parent;

        n_parent = &((hash_node_t *) h->array)[iter->cur];

        /* check that we aren't following a dangling pointer.
         * There is a chance that cur_linked is now on the array. */
        if (!n_parent->next)
        {
            n = n_parent;
            iter->cur_linked = NULL;
            iter->cur++;
        }
        else
        {
            /*  it's safe to increment */
            if (NULL == n->next)
                iter->cur++;
            iter->cur_linked = n->next;
        }
        return n->ety.key;
    }
    /*  otherwise check if we have a node to look at */
    else
    {
        for (; iter->cur < h->arraySize; iter->cur++)
        {
            n = &((hash_node_t *) h->array)[iter->cur];

            if (n->ety.key)
                break;
        }

        /*  exit if we are at the end */
        if (h->arraySize == iter->cur)
        {
            return NULL;
        }

        n = &((hash_node_t *) h->array)[iter->cur];

        if (n->next)
        {
            iter->cur_linked = n->next;
        }
        else
        {
            /*  Only increment if we aren't following the chain.
             *  This is so that if the user is removing while iterating
             *  we don't follow the dangling iter->cur_linked pointer
             *  if the node got placed on the array. */
            iter->cur += 1;
        }

        return n->ety.key;
    }
}

/**
 * Initialise a new hash iterator over this hash
 * It is safe to remove items while iterating.
 */
void hashmap_iterator(
    hashmap_t * h __attribute__((__unused__)),
    hashmap_iterator_t * iter
)
{
    iter->cur = 0;
    iter->cur_linked = NULL;
}

/*--------------------------------------------------------------79-characters-*/
