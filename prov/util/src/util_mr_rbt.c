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

/* RBT Data structure interface hooks */

void * util_ds_init(int(*Compare)(void *a, void *b))
{

    return rbtNew(Compare);

}

void util_ds_return_keyvalue(void *ds_handle, void *ds_itr, void **key, 
                            void **value) 
{

    rbtKeyValue(ds_handle, ds_itr, key, value);

}

void * util_ds_find(void * ds_handle, void *key)
{

    return rbtFind(ds_handle, key);

}

int util_ds_insert(void *ds_handle, void *key, void *value)
{

    return rbtInsert(ds_handle, key, value);

}

int util_ds_erase(void * ds_handle, void * ds_itr)
{

    return rbtErase(ds_handle, ds_itr);

}

void util_ds_delete_ds(void * ds_handle)
{

    rbtDelete(ds_handle);

}




