/* SPDX-License-Identifier: GPL-2.0 */
/*
 * fscrypt_private.h
 *
 * Copyright (C) 2015, Google, Inc.
 *
 * This contains encryption key functions.
 *
 * Written by Michael Halcrow, Ildar Muslukhov, and Uday Savagaonkar, 2015.
 */

#ifndef _FSCRYPT_PRIVATE_H
#define _FSCRYPT_PRIVATE_H

#ifndef __FS_HAS_ENCRYPTION
#define __FS_HAS_ENCRYPTION 1
#endif
#include <linux/fscrypt.h>
#include <crypto/hash.h>
#include <linux/pfk.h>

<<<<<<< HEAD
/* Encryption parameters */
#define FS_KEY_DERIVATION_NONCE_SIZE	16

/**
 * Encryption context for inode
 *
 * Protector format:
 *  1 byte: Protector format (1 = this version)
 *  1 byte: File contents encryption mode
 *  1 byte: File names encryption mode
 *  1 byte: Flags
 *  8 bytes: Master Key descriptor
 *  16 bytes: Encryption Key derivation nonce
 */
struct fscrypt_context {
	u8 format;
	u8 contents_encryption_mode;
	u8 filenames_encryption_mode;
	u8 flags;
	u8 master_key_descriptor[FS_KEY_DESCRIPTOR_SIZE];
	u8 nonce[FS_KEY_DERIVATION_NONCE_SIZE];
} __packed;

#define FS_ENCRYPTION_CONTEXT_FORMAT_V1		1
=======
#define FSCRYPT_MIN_KEY_SIZE		16
#define FSCRYPT_MAX_HW_WRAPPED_KEY_SIZE	128

/*
 * Return the size expected for the given fscrypt_context based on its version
 * number, or 0 if the context version is unrecognized.
 */
static inline int fscrypt_context_size(const union fscrypt_context *ctx)
{
	switch (ctx->version) {
	case FSCRYPT_CONTEXT_V1:
		BUILD_BUG_ON(sizeof(ctx->v1) != 28);
		return sizeof(ctx->v1);
	case FSCRYPT_CONTEXT_V2:
		BUILD_BUG_ON(sizeof(ctx->v2) != 40);
		return sizeof(ctx->v2);
	}
	return 0;
}

/* Check whether an fscrypt_context has a recognized version number and size */
static inline bool fscrypt_context_is_valid(const union fscrypt_context *ctx,
					    int ctx_size)
{
	return ctx_size >= 1 && ctx_size == fscrypt_context_size(ctx);
}

/* Retrieve the context's nonce, assuming the context was already validated */
static inline const u8 *fscrypt_context_nonce(const union fscrypt_context *ctx)
{
	switch (ctx->version) {
	case FSCRYPT_CONTEXT_V1:
		return ctx->v1.nonce;
	case FSCRYPT_CONTEXT_V2:
		return ctx->v2.nonce;
	}
	WARN_ON(1);
	return NULL;
}

#undef fscrypt_policy
union fscrypt_policy {
	u8 version;
	struct fscrypt_policy_v1 v1;
	struct fscrypt_policy_v2 v2;
};

/*
 * Return the size expected for the given fscrypt_policy based on its version
 * number, or 0 if the policy version is unrecognized.
 */
static inline int fscrypt_policy_size(const union fscrypt_policy *policy)
{
	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		return sizeof(policy->v1);
	case FSCRYPT_POLICY_V2:
		return sizeof(policy->v2);
	}
	return 0;
}

/* Return the contents encryption mode of a valid encryption policy */
static inline u8
fscrypt_policy_contents_mode(const union fscrypt_policy *policy)
{
	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		return policy->v1.contents_encryption_mode;
	case FSCRYPT_POLICY_V2:
		return policy->v2.contents_encryption_mode;
	}
	BUG();
}

/* Return the filenames encryption mode of a valid encryption policy */
static inline u8
fscrypt_policy_fnames_mode(const union fscrypt_policy *policy)
{
	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		return policy->v1.filenames_encryption_mode;
	case FSCRYPT_POLICY_V2:
		return policy->v2.filenames_encryption_mode;
	}
	BUG();
}

/* Return the flags (FSCRYPT_POLICY_FLAG*) of a valid encryption policy */
static inline u8
fscrypt_policy_flags(const union fscrypt_policy *policy)
{
	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		return policy->v1.flags;
	case FSCRYPT_POLICY_V2:
		return policy->v2.flags;
	}
	BUG();
}
>>>>>>> 40b55d19f57f... Kernel: Xiaomi kernel changes for Xiaomi Pad 5 Android R

/**
 * For encrypted symlinks, the ciphertext length is stored at the beginning
 * of the string in little-endian format.
 */
struct fscrypt_symlink_data {
	__le16 len;
	char encrypted_path[1];
} __packed;

/*
 * fscrypt_info - the "encryption key" for an inode
 *
 * When an encrypted file's key is made available, an instance of this struct is
 * allocated and stored in ->i_crypt_info.  Once created, it remains until the
 * inode is evicted.
 */
struct fscrypt_info {

	/* The actual crypto transform used for encryption and decryption */
	struct crypto_skcipher *ci_ctfm;

	/*
	 * Cipher for ESSIV IV generation.  Only set for CBC contents
	 * encryption, otherwise is NULL.
	 */
	struct crypto_cipher *ci_essiv_tfm;

	/*
	 * Encryption mode used for this inode.  It corresponds to either
	 * ci_data_mode or ci_filename_mode, depending on the inode type.
	 */
	struct fscrypt_mode *ci_mode;

	/*
	 * If non-NULL, then this inode uses a master key directly rather than a
	 * derived key, and ci_ctfm will equal ci_master_key->mk_ctfm.
	 * Otherwise, this inode uses a derived key.
	 */
	struct fscrypt_master_key *ci_master_key;

	/* fields from the fscrypt_context */
	u8 ci_data_mode;
	u8 ci_filename_mode;
	u8 ci_flags;
	u8 ci_master_key_descriptor[FS_KEY_DESCRIPTOR_SIZE];
	u8 ci_nonce[FS_KEY_DERIVATION_NONCE_SIZE];
	u8 ci_raw_key[FS_MAX_KEY_SIZE];
};

typedef enum {
	FS_DECRYPT = 0,
	FS_ENCRYPT,
} fscrypt_direction_t;

#define FS_CTX_REQUIRES_FREE_ENCRYPT_FL		0x00000001
#define FS_CTX_HAS_BOUNCE_BUFFER_FL		0x00000002

static inline bool fscrypt_valid_enc_modes(u32 contents_mode,
					   u32 filenames_mode)
{
	if (contents_mode == FS_ENCRYPTION_MODE_AES_128_CBC &&
	    filenames_mode == FS_ENCRYPTION_MODE_AES_128_CTS)
		return true;

	if (contents_mode == FS_ENCRYPTION_MODE_AES_256_XTS &&
	    filenames_mode == FS_ENCRYPTION_MODE_AES_256_CTS)
		return true;

	if (contents_mode == FS_ENCRYPTION_MODE_PRIVATE &&
	    filenames_mode == FS_ENCRYPTION_MODE_AES_256_CTS)
		return true;

	if (contents_mode == FS_ENCRYPTION_MODE_ADIANTUM &&
	    filenames_mode == FS_ENCRYPTION_MODE_ADIANTUM)
		return true;

	return false;
}

static inline bool is_private_data_mode(const struct fscrypt_context *ctx)
{
	return ctx->contents_encryption_mode == FS_ENCRYPTION_MODE_PRIVATE;
}

/* crypto.c */
extern struct kmem_cache *fscrypt_info_cachep;
extern int fscrypt_initialize(unsigned int cop_flags);
extern int fscrypt_do_page_crypto(const struct inode *inode,
				  fscrypt_direction_t rw, u64 lblk_num,
				  struct page *src_page,
				  struct page *dest_page,
				  unsigned int len, unsigned int offs,
				  gfp_t gfp_flags);
extern struct page *fscrypt_alloc_bounce_page(struct fscrypt_ctx *ctx,
					      gfp_t gfp_flags);
extern const struct dentry_operations fscrypt_d_ops;

extern void __printf(3, 4) __cold
fscrypt_msg(struct super_block *sb, const char *level, const char *fmt, ...);

#define fscrypt_warn(sb, fmt, ...)		\
	fscrypt_msg(sb, KERN_WARNING, fmt, ##__VA_ARGS__)
#define fscrypt_err(sb, fmt, ...)		\
	fscrypt_msg(sb, KERN_ERR, fmt, ##__VA_ARGS__)

#define FSCRYPT_MAX_IV_SIZE	32

union fscrypt_iv {
	struct {
		/* logical block number within the file */
		__le64 lblk_num;

		/* per-file nonce; only set in DIRECT_KEY mode */
		u8 nonce[FS_KEY_DERIVATION_NONCE_SIZE];
	};
	u8 raw[FSCRYPT_MAX_IV_SIZE];
};

void fscrypt_generate_iv(union fscrypt_iv *iv, u64 lblk_num,
			 const struct fscrypt_info *ci);

/* fname.c */
extern int fname_encrypt(struct inode *inode, const struct qstr *iname,
			 u8 *out, unsigned int olen);
extern bool fscrypt_fname_encrypted_size(const struct inode *inode,
					 u32 orig_len, u32 max_len,
					 u32 *encrypted_len_ret);

/* keyinfo.c */

struct fscrypt_mode {
	const char *friendly_name;
	const char *cipher_str;
	int keysize;
	int ivsize;
	bool logged_impl_name;
	bool needs_essiv;
};

<<<<<<< HEAD
extern void __exit fscrypt_essiv_cleanup(void);
=======
extern struct fscrypt_mode fscrypt_modes[];

extern int fscrypt_prepare_key(struct fscrypt_prepared_key *prep_key,
			       const u8 *raw_key, unsigned int raw_key_size,
			       bool is_hw_wrapped,
			       const struct fscrypt_info *ci);

extern void fscrypt_destroy_prepared_key(struct fscrypt_prepared_key *prep_key);

extern int fscrypt_set_per_file_enc_key(struct fscrypt_info *ci,
					const u8 *raw_key);

extern int fscrypt_derive_dirhash_key(struct fscrypt_info *ci,
				      const struct fscrypt_master_key *mk);

/* keysetup_v1.c */

extern void fscrypt_put_direct_key(struct fscrypt_direct_key *dk);

extern int fscrypt_setup_v1_file_key(struct fscrypt_info *ci,
				     const u8 *raw_master_key);

extern int fscrypt_setup_v1_file_key_via_subscribed_keyrings(
					struct fscrypt_info *ci);
/* policy.c */

bool fscrypt_policies_equal(const union fscrypt_policy *policy1,
			    const union fscrypt_policy *policy2);
bool fscrypt_supported_policy(const union fscrypt_policy *policy_u,
			      const struct inode *inode);
int fscrypt_policy_from_context(union fscrypt_policy *policy_u,
				const union fscrypt_context *ctx_u,
				int ctx_size);
extern void put_crypt_info(struct fscrypt_info *ci);
extern struct fscrypt_mode * select_encryption_mode(const union fscrypt_policy *policy, const struct inode *inode);
extern int setup_file_encryption_key(struct fscrypt_info *ci, struct key **master_key_ret);
>>>>>>> 40b55d19f57f... Kernel: Xiaomi kernel changes for Xiaomi Pad 5 Android R

#endif /* _FSCRYPT_PRIVATE_H */
