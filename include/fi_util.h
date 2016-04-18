/*
 * Copyright (c) 2015-2016 Intel Corporation, Inc.  All rights reserved.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <pthread.h>

#include <rdma/fabric.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_prov.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_trigger.h>

#include <fi.h>
#include <fi_list.h>
#include <fi_mem.h>
#include <fi_rbuf.h>
#include <fi_signal.h>
#include <fi_enosys.h>
#include <rbtree.h>
#ifndef _FI_UTIL_H_
#define _FI_UTIL_H_


#define UTIL_FLAG_ERROR	(1ULL << 60)


/*
 * Provider details
 * TODO: Determine if having this structure (with expanded fields)
 * would help support fi_getinfo.  If so, fill out and update
 * implementation
struct util_prov {
	const struct fi_provider *prov;
	const struct fi_info	*info;  -- list of provider info's
	provider specific handlers, e.g. resolve addressing
};
 */


/*
 * Fabric
 */
struct util_fabric {
	struct fid_fabric	fabric_fid;
	struct dlist_entry	list_entry;
	fastlock_t		lock;
	atomic_t		ref;
	const char		*name;
	const struct fi_provider *prov;

	struct dlist_entry	domain_list;
};

int fi_fabric_init(const struct fi_provider *prov,
		   struct fi_fabric_attr *prov_attr,
		   struct fi_fabric_attr *user_attr,
		   struct util_fabric *fabric, void *context);
int util_fabric_close(struct util_fabric *fabric);
int util_trywait(struct fid_fabric *fabric, struct fid **fids, int count);

/*
 * Domain
 */
struct util_domain {
	struct fid_domain	domain_fid;
	struct dlist_entry	list_entry;
	struct util_fabric	*fabric;
	fastlock_t		lock;
	atomic_t		ref;
	const struct fi_provider *prov;

	const char		*name;
	uint64_t		caps;
	uint64_t		mode;
	uint32_t		addr_format;
	enum fi_av_type		av_type;
};

int fi_domain_init(struct fid_fabric *fabric_fid, const struct fi_info *info,
		     struct util_domain *domain, void *context);
int util_domain_close(struct util_domain *domain);


struct util_ep;
typedef void (*fi_ep_progress_func)(struct util_ep *util_ep);

struct util_ep {
	struct fid_ep		ep_fid;
	struct util_domain	*domain;
	struct util_av		*av;
	struct util_cq		*rx_cq;
	struct util_cq		*tx_cq;
	uint64_t		caps;
	uint64_t		flags;
	fi_ep_progress_func	progress;
};


/*
 * Completion queue
 *
 * Utility provider derived CQs that require manual progress must
 * progress the CQ when fi_cq_read is called with a count = 0.
 * In such cases, fi_cq_read will return 0 if there are available
 * entries on the CQ.  This allows poll sets to drive progress
 * without introducing private interfaces to the CQ.
 */
#define FI_DEFAULT_CQ_SIZE	1024

typedef void (*fi_cq_read_func)(void **dst, void *src);

struct util_cq_err_entry {
	struct fi_cq_err_entry	err_entry;
	struct slist_entry	list_entry;
};

DECLARE_CIRQUE(struct fi_cq_data_entry, util_comp_cirq);

struct util_cq {
	struct fid_cq		cq_fid;
	struct util_domain	*domain;
	struct util_wait	*wait;
	atomic_t		ref;
	struct dlist_entry	list;
	fastlock_t		list_lock;
	fastlock_t		cq_lock;

	struct util_comp_cirq	*cirq;
	fi_addr_t		*src;

	struct slist		err_list;
	fi_cq_read_func		read_entry;
	int			internal_wait;
};

int util_cq_open(const struct fi_provider *prov,
		 struct fid_domain *domain, struct fi_cq_attr *attr,
		 struct fid_cq **cq_fid, void *context);


/*
 * Counter
 */
struct util_cntr {
	struct fid_cntr		cntr_fid;
	struct util_domain	*domain;
	atomic_t		ref;
};


/*
 * AV / addressing
 */
struct util_av_hash_entry {
	int			index;
	int			next;
};

struct util_av_hash {
	struct util_av_hash_entry *table;
	int			free_list;
	int			slots;
	int			total_count;
};

struct util_av {
	struct fid_av		av_fid;
	struct util_domain	*domain;
	struct util_eq		*eq;
	atomic_t		ref;
	fastlock_t		lock;
	const struct fi_provider *prov;

	void			*context;
	uint64_t		flags;
	size_t			count;
	size_t			addrlen;
	ssize_t			free_list;
	struct util_av_hash	hash;
	void			*data;
};

struct util_av_attr {
	size_t			addrlen;
	size_t			overhead;
	uint64_t		flags;
};

int fi_av_create(struct util_domain *domain,
		 const struct fi_av_attr *attr, const struct util_av_attr *util_attr,
		 struct fid_av **av, void *context);
int ip_av_create(struct fid_domain *domain_fid, struct fi_av_attr *attr,
		 struct fid_av **av, void *context);

void *fi_av_get_addr(struct util_av *av, int index);
#define ip_av_get_addr fi_av_get_addr
int ip_av_get_index(struct util_av *av, const void *addr);

int fi_get_addr(uint32_t addr_format, uint64_t flags,
		const char *node, const char *service,
		void **addr, size_t *addrlen);
int fi_get_src_addr(uint32_t addr_format,
		    const void *dest_addr, size_t dest_addrlen,
		    void **src_addr, size_t *src_addrlen);


/*
 * Poll set
 */
struct util_poll {
	struct fid_poll		poll_fid;
	struct util_domain	*domain;
	struct dlist_entry	fid_list;
	fastlock_t		lock;
	atomic_t		ref;
	const struct fi_provider *prov;
};

int fi_poll_create_(const struct fi_provider *prov, struct fid_domain *domain,
		    struct fi_poll_attr *attr, struct fid_poll **pollset);
int fi_poll_create(struct fid_domain *domain, struct fi_poll_attr *attr,
		   struct fid_poll **pollset);


/*
 * Wait set
 */
struct util_wait;
typedef void (*fi_wait_signal_func)(struct util_wait *wait);
typedef int (*fi_wait_try_func)(struct util_wait *wait);

struct util_wait {
	struct fid_wait		wait_fid;
	struct util_fabric	*fabric;
	struct util_poll	*pollset;
	atomic_t		ref;
	const struct fi_provider *prov;

	enum fi_wait_obj	wait_obj;
	fi_wait_signal_func	signal;
	fi_wait_try_func	try;
};

int fi_wait_init(struct util_fabric *fabric, struct fi_wait_attr *attr,
		 struct util_wait *wait);
int fi_wait_cleanup(struct util_wait *wait);

struct util_wait_fd {
	struct util_wait	util_wait;
	struct fd_signal	signal;
	fi_epoll_t		epoll_fd;
};

int fi_wait_fd_open(struct fid_fabric *fabric, struct fi_wait_attr *attr,
		struct fid_wait **waitset);


/*
 * EQ
 */
struct util_eq {
	struct fid_eq		eq_fid;
	struct util_fabric	*fabric;
	struct util_wait	*wait;
	fastlock_t		lock;
	atomic_t		ref;
	const struct fi_provider *prov;

	struct slist		list;
	int			internal_wait;
};

struct util_event {
	struct slist_entry	entry;
	int			size;
	int			event;
	int			err;
	uint8_t			data[0];
};

int fi_eq_create(struct fid_fabric *fabric, struct fi_eq_attr *attr,
		 struct fid_eq **eq_fid, void *context);

/*
 * MR
 */

/* Data structure abstraction -- always going to need map like DS */

/*need better solution... */
typedef RbtStatus ds_status;
typedef void *util_mr_itr;
typedef struct fi_mr_attr util_mr_item_t; /*hide addr related info & store prov_mr ptr */

struct fi_ops_util_ds {
    void (*return_keyvalue) (void *ds_handle, void *ds_itr, void **key, 
                            void **value);
    void * (*find) (void * ds_handle, void *key);
    ds_status (*insert) (void *ds_handle, void *key, void *value);
    ds_status (*erase) (void * ds_handle, void * ds_itr);
    void (*delete_ds) (void * ds_handle);
};

/* USER UTIL MR API CALLS */

struct util_mr {
    void *ds_handle;
    struct fi_ops_util_ds * ds_ops; /* rbt_ops? */
    uint64_t b_key; /* track available key (BASIC usage) */
    enum fi_mr_mode mr_type;
};

extern struct util_mr * util_mr_init(enum fi_mr_mode mode);
extern int util_mr_insert(struct util_mr in_mr_h, 
                                const struct fi_mr_attr *in_attr, 
                                uint64_t * out_key, void * in_prov_mr);
extern int util_mr_retrieve(struct util_mr in_mr_h, ssize_t in_len,
                                void * in_addr, uint64_t in_key, 
                                uint64_t in_access, void **out_prov_mr);
extern int util_mr_erase(struct util_mr in_mr_h, uint64_t in_key, 
                            void ** out_prov_mr);
extern int util_mr_close(struct util_mr * in_mr_h);



/*
 * Attributes and capabilities
 */
#define FI_PRIMARY_CAPS	(FI_MSG | FI_RMA | FI_TAGGED | FI_ATOMICS | \
			 FI_NAMED_RX_CTX | FI_DIRECTED_RECV | \
			 FI_READ | FI_WRITE | FI_RECV | FI_SEND | \
			 FI_REMOTE_READ | FI_REMOTE_WRITE)

#define FI_SECONDARY_CAPS (FI_MULTI_RECV | FI_SOURCE | FI_RMA_EVENT | \
			   FI_TRIGGER | FI_FENCE)

int fi_check_fabric_attr(const struct fi_provider *prov,
			 const struct fi_fabric_attr *prov_attr,
			 const struct fi_fabric_attr *user_attr);
int fi_check_wait_attr(const struct fi_provider *prov,
		       const struct fi_wait_attr *attr);
int fi_check_domain_attr(const struct fi_provider *prov,
			 const struct fi_domain_attr *prov_attr,
			 const struct fi_domain_attr *user_attr);
int fi_check_ep_attr(const struct fi_provider *prov,
		     const struct fi_ep_attr *prov_attr,
		     const struct fi_ep_attr *user_attr);
int fi_check_cq_attr(const struct fi_provider *prov,
		     const struct fi_cq_attr *attr);
int fi_check_rx_attr(const struct fi_provider *prov,
		     const struct fi_rx_attr *prov_attr,
		     const struct fi_rx_attr *user_attr);
int fi_check_tx_attr(const struct fi_provider *prov,
		     const struct fi_tx_attr *prov_attr,
		     const struct fi_tx_attr *user_attr);
int fi_check_info(const struct fi_provider *prov,
		  const struct fi_info *prov_info,
		  const struct fi_info *user_info);
void fi_alter_info(struct fi_info *info,
		   const struct fi_info *hints);

int util_getinfo(const struct fi_provider *prov, uint32_t version,
		 const char *node, const char *service, uint64_t flags,
		 const struct fi_info *prov_info, struct fi_info *hints,
		 struct fi_info **info);


struct fid_list_entry {
	struct dlist_entry	entry;
	struct fid		*fid;

	uint64_t		last_cntr_val;
};

int fid_list_insert(struct dlist_entry *fid_list, fastlock_t *lock,
		    struct fid *fid);
void fid_list_remove(struct dlist_entry *fid_list, fastlock_t *lock,
		     struct fid *fid);

void fi_fabric_insert(struct util_fabric *fabric);
struct util_fabric *fi_fabric_find(const char *name);
void fi_fabric_remove(struct util_fabric *fabric);


#endif
