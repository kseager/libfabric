/*
 * Copyright (c) 2016 Intel Corporation, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdlib.h>
#include <fi_enosys.h>
#include <fi_util.h>


/*make seperate copy of mr_attr and use context for prov_mr */
static inline util_mr_item_t * create_mr_attr_copy(
                        const struct fi_mr_attr *in_attr, void * prov_mr) 
{
    util_mr_item_t * item;
    struct iovec *mr_iov; 
    int i = 0;

    assert(prov_mr && in_attr);

    item = malloc(sizeof(util_mr_item_t));
    *item = *in_attr; 
    item->context = prov_mr;
    mr_iov = malloc(sizeof(mr_iov) * in_attr->iov_count);
    
    for(i = 0; i < in_attr->iov_count; i++) 
        mr_iov[i] = in_attr->mr_iov[i];
    
    item->mr_iov = mr_iov;

    return item; 
}

static inline uint64_t get_mr_key(struct util_mr mr_h)
{
    assert(mr_h.b_key != UINT64_MAX);
    uint64_t b_key = mr_h.b_key;
    mr_h.b_key++;
    return b_key;        
}

static inline int verify_addr(util_mr_item_t * item, uint64_t in_access, 
                                 uint64_t in_addr, ssize_t in_len)
{
    int i = 0;

    if(!in_addr) 
        return -FI_EINVAL;

    if((in_access & item->access) != in_access)
        return -FI_EACCES;

      for (i = 0; i < item->iov_count; i++) {
        if ((uint64_t)item->mr_iov[i].iov_base <= in_addr &&
            (uint64_t)item->mr_iov[i].iov_base + 
                    item->mr_iov[i].iov_len >= in_addr + in_len)
            return 0;
    }

    return -FI_EACCES;
}

/*PSM is assuming attr already has offset calculated in, socket opposite */
/* use requested key, need to offset entry */
int ofi_mr_insert(struct util_mr * in_mr_h, const struct fi_mr_attr *in_attr, 
                                uint64_t * out_key, void * in_prov_mr)
{
    util_mr_item_t * item; 
  
    if (!in_attr || in_attr->iov_count <= 0 || !in_prov_mr) {
        return -FI_EINVAL;
    }

    item = create_mr_attr_copy(in_attr, in_prov_mr);

    /* Scalable MR handling */
    if(in_mr_h->mr_type == FI_MR_SCALABLE) {
        item->offset = (uintptr_t) in_attr->mr_iov[0].iov_base + in_attr->offset;
        /* verify key doesn't already exist */
        if(util_ds_find(in_mr_h->ds_handle, (void *)item->requested_key))
                return -FI_EINVAL;
    } else {
        item->requested_key = get_mr_key((*in_mr_h));
        item->offset = (uintptr_t) in_attr->mr_iov[0].iov_base;
    }

    util_ds_insert(in_mr_h->ds_handle, (void *)item->requested_key, item);   
    *out_key = item->requested_key; 

    return 0;
}

int ofi_mr_retrieve(struct util_mr * in_mr_h, ssize_t in_len,
                                void * in_addr, uint64_t in_key, 
                                uint64_t in_access, void **out_prov_mr)
{
    /*grab info */
    util_mr_itr itr;
    util_mr_item_t * item;
    
    itr = util_ds_find(in_mr_h->ds_handle, (void *)in_key);

    if(!itr)
        return -FI_EINVAL; 

    util_ds_return_keyvalue(in_mr_h->ds_handle, itr, (void **)&in_key, 
                                (void **) &item);

    /*return providers MR struct */
    (*out_prov_mr) = item->context;

    /*offset for scalable */
    if(in_mr_h->mr_type == FI_MR_SCALABLE) 
        in_addr = (char *)in_addr + item->offset;

    return verify_addr(item, in_access, (uint64_t)in_addr, in_len);
}

int ofi_mr_erase(struct util_mr * in_mr_h, uint64_t in_key, void ** out_prov_mr)
{
    util_mr_itr itr;
    util_mr_item_t * item;
    
    itr = util_ds_find(in_mr_h->ds_handle, (void *)in_key);

    if(!itr)
        return -FI_EINVAL; 

    /*release memory */
    util_ds_return_keyvalue(in_mr_h->ds_handle, itr, (void **)&in_key, 
                                (void **) &item);

    *out_prov_mr = item->context; /* user should free this item */
    free((void *)item->mr_iov);
    free(item); 

    util_ds_erase(in_mr_h->ds_handle, itr);

    return 0;
}

/*assumes uint64_t keys */
static int compare_mr_keys(void *key1, void *key2)
{
    uint64_t k1 = *((uint64_t *) key1);
    uint64_t k2 = *((uint64_t *) key2);
    return (k1 < k2) ?  -1 : (k1 > k2);
}


struct util_mr * util_mr_init(enum fi_mr_mode mode)
{
    struct util_mr * new_mr = malloc(sizeof(struct util_mr)); /*dc*/

    /* FI_MR_SCALABLE vs FI_MR_BASIC */
    new_mr->mr_type = mode; 

    new_mr->ds_handle = util_ds_init(compare_mr_keys); 
    
    new_mr->b_key = 0;

    return new_mr;
}


int util_mr_close(struct util_mr ** in_mr_h)
{
    util_ds_delete_ds((*in_mr_h)->ds_handle);
    free(*in_mr_h);

    return 0;
}










