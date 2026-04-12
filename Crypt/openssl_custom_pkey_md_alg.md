# Adding Custom Hash Algorithm to OpenSSL for Public Key Crypt

This will demonstrate how to add a custom hash algorithm to OpenSSL and then add a new EC Signature Algorithm which uses that custom hash algorithm  

I am using `openssl-1.1.1o` for this  

<br />

## Creating the Hash Algorithm

All of this code goes in (new file) `apps/custom_alg_register.c`  

```C
typedef struct {
    unsigned char accum[32];
    size_t total;
} MYHASH_CTX;

static int myhash_init(EVP_MD_CTX *ctx)
{
    MYHASH_CTX *c = EVP_MD_CTX_md_data(ctx);
    memset(c, 0, sizeof(*c));
    return 1;
}

static int myhash_update(EVP_MD_CTX *ctx, const void *data, size_t count)
{
    MYHASH_CTX *c = EVP_MD_CTX_md_data(ctx);
    const unsigned char *p = (const unsigned char *)data;
    size_t i;

    printf("myhash_update called\n");

    for (i = 0; i < count; i++) {
        c->accum[i % sizeof(c->accum)] ^= p[i];
    }
    c->total += count;
    return 1;
}

static int myhash_final(EVP_MD_CTX *ctx, unsigned char *md)
{
    MYHASH_CTX *c = EVP_MD_CTX_md_data(ctx);
    size_t i;

    printf("myhash_final called\n");

    for (i = 0; i < sizeof(c->accum); i++) {
        md[i] = c->accum[i] ^ (unsigned char)(c->total & 0xff);
    }
    return 1;
}

static int myhash_copy(EVP_MD_CTX *to, const EVP_MD_CTX *from)
{
    MYHASH_CTX *dst = EVP_MD_CTX_md_data(to);
    const MYHASH_CTX *src = EVP_MD_CTX_md_data(from);

    memcpy(dst, src, sizeof(*dst));
    return 1;
}

static int myhash_cleanup(EVP_MD_CTX *ctx)
{
    MYHASH_CTX *c = EVP_MD_CTX_md_data(ctx);
    OPENSSL_cleanse(c, sizeof(*c));
    return 1;
}

const EVP_MD *EVP_myhash(void)
{
    if (g_myhash_md != NULL)
        return g_myhash_md;

    if (g_myhash_nid == NID_undef)
        return NULL;

    g_myhash_md = EVP_MD_meth_new(g_myhash_nid, NID_undef);
    if (g_myhash_md == NULL)
        return NULL;

    if (!EVP_MD_meth_set_result_size(g_myhash_md, 32) ||
        !EVP_MD_meth_set_input_blocksize(g_myhash_md, 64) ||
        !EVP_MD_meth_set_app_datasize(g_myhash_md, sizeof(MYHASH_CTX)) ||
        !EVP_MD_meth_set_flags(g_myhash_md, 0) ||
        !EVP_MD_meth_set_init(g_myhash_md, myhash_init) ||
        !EVP_MD_meth_set_update(g_myhash_md, myhash_update) ||
        !EVP_MD_meth_set_final(g_myhash_md, myhash_final) ||
        !EVP_MD_meth_set_copy(g_myhash_md, myhash_copy) ||
        !EVP_MD_meth_set_cleanup(g_myhash_md, myhash_cleanup)) {
        EVP_MD_meth_free(g_myhash_md);
        g_myhash_md = NULL;
        return NULL;
    }

    return g_myhash_md;
}
```

<br />

## Registering the Types

All of this code also goes in `apps/custom_alg_register.c`  

```C
#define OID_MYHASH              "1.3.6.1.4.1.55555.1.1"
#define SN_MYHASH               "MYHASH"
#define LN_MYHASH               "my-custom-hash"

#define OID_ECDSA_WITH_MYHASH   "1.3.6.1.4.1.55555.1.2"
#define SN_ECDSA_WITH_MYHASH    "ecdsa-with-MYHASH"
#define LN_ECDSA_WITH_MYHASH    "ecdsa-with-my-custom-hash"

static int g_myhash_nid = NID_undef;
static int g_sig_nid = NID_undef;
static EVP_MD *g_myhash_md = NULL;

// ...
// Hash function setup here (see above)
// ...

int register_myhash_objects_and_sigalg(void)
{
    const EVP_MD *md;

    g_myhash_nid = OBJ_create(OID_MYHASH, SN_MYHASH, LN_MYHASH);
    if (g_myhash_nid == NID_undef)
        g_myhash_nid = OBJ_txt2nid(OID_MYHASH);
    if (g_myhash_nid == NID_undef)
        return 0;

    g_sig_nid = OBJ_create(OID_ECDSA_WITH_MYHASH,
                           SN_ECDSA_WITH_MYHASH,
                           LN_ECDSA_WITH_MYHASH);
    if (g_sig_nid == NID_undef)
        g_sig_nid = OBJ_txt2nid(OID_ECDSA_WITH_MYHASH);
    if (g_sig_nid == NID_undef)
        return 0;

    md = EVP_myhash();
    if (md == NULL)
        return 0;

    /*
     * Make digest discoverable by name for CLI / EVP lookup.
     */
    EVP_add_digest(md);

    /*
     * This is the important X.509 hook:
     * custom signature OID  ->  (custom digest, EC public key algorithm)
     */
    if (!OBJ_add_sigid(g_sig_nid, g_myhash_nid, EVP_PKEY_EC))
        return 0;

    return 1;
}
```

<br />

## Including it in the Build

Add a header file `apps/custom_alg_register.h` containing:
```C
#ifndef OSSL_APPS_CUSTOM_ALG_REGISTER_H
#define OSSL_APPS_CUSTOM_ALG_REGISTER_H

int register_myhash_objects_and_sigalg(void);

#endif
```

In the first part of `apps/build.info` add `custom_alg_register.c` so it looks like:
```pt
{- our @apps_openssl_src =
       qw(openssl.c custom_alg_register.c
          asn1pars.c ca.c ciphers.c ...
```

In `apps/openssl.c` add the include under `apps.h` include:
```C
#include "custom_alg_register.h"
```

In `apps/openssl.c` call the register function from `apps_startup`. Full function should look like this:
```C
static int apps_startup(void)
{
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    /* Set non-default library initialisation settings */
    if (!OPENSSL_init_ssl(OPENSSL_INIT_ENGINE_ALL_BUILTIN
                          | OPENSSL_INIT_LOAD_CONFIG, NULL))
        return 0;

    if (!register_myhash_objects_and_sigalg()) {
        BIO_printf(bio_err, "Failed to register custom digest/signature algorithm\n");
        ERR_print_errors(bio_err);
        return 0;
    }

    setup_ui_method();

    return 1;
}
```

<br />

## Add Hash Algorithm as Supported EC Hash Algo

In `crypto/ec/ec_pmeth.c` > `pkey_ec_ctrl` there is a `switch(type)`. Patch the `EVP_PKEY_CTRL_MD` to add the hash algorithm to the if statement. Full case should look like:
```C
case EVP_PKEY_CTRL_MD:
    int myhash_nid = OBJ_sn2nid("MYHASH"); 

    printf("myhash_nid: %d\n", myhash_nid);

    if (EVP_MD_type((const EVP_MD *)p2) != NID_sha1 &&
        EVP_MD_type((const EVP_MD *)p2) != myhash_nid &&
        EVP_MD_type((const EVP_MD *)p2) != NID_ecdsa_with_SHA1 &&
        EVP_MD_type((const EVP_MD *)p2) != NID_sha224 &&
        EVP_MD_type((const EVP_MD *)p2) != NID_sha256 &&
        EVP_MD_type((const EVP_MD *)p2) != NID_sha384 &&
        EVP_MD_type((const EVP_MD *)p2) != NID_sha512 &&
        EVP_MD_type((const EVP_MD *)p2) != NID_sha3_224 &&
        EVP_MD_type((const EVP_MD *)p2) != NID_sha3_256 &&
        EVP_MD_type((const EVP_MD *)p2) != NID_sha3_384 &&
        EVP_MD_type((const EVP_MD *)p2) != NID_sha3_512) {
        ECerr(EC_F_PKEY_EC_CTRL, EC_R_INVALID_DIGEST_TYPE);
        return 0;
    }
    dctx->md = p2;
    return 1;
```

<br />

## Building OpenSSL

```sh
./Configure linux-x86_64 \
    no-shared \
    no-dso \
    --prefix="$PWD/out" \
    --openssldir="$PWD/out/ssl" \
    -static

make
make install_sw

# For convenience, copy the default .cnf:
mkdir out/ssl
cp ./apps/openssl.cnf ./out/ssl/openssl.cnf
```

<br />

## Using Custom Signature Algorithm

```sh
export OPENSSL=/path/to/openssl-1.1.1o/out/bin/openssl

$OPENSSL list -digest-algorithms | grep -i myhash
    # MYHASH
    # MYHASH

$OPENSSL ecparam -name prime256v1 -genkey -noout -out ca.key.pem
$OPENSSL ecparam -name prime256v1 -genkey -noout -out leaf.key.pem

cat > ca.ext <<'EOF'
basicConstraints = critical, CA:true
keyUsage = critical, keyCertSign, cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer
EOF

$OPENSSL req -new -x509 \
  -key ca.key.pem \
  -days 3650 \
  -subj "/CN=Example EC Root CA" \
  -myhash \
  -out ca.cert.pem \
  -extensions v3_ca \
  -config <(cat /path/to/openssl-1.1.1o/apps/openssl.cnf <(printf '\n[v3_ca]\n'; cat ca.ext))

$OPENSSL x509 -in ca.cert.pem -text -noout
    # ...
    # Signature Algorithm: ecdsa-with-my-custom-hash
    # ...
    # Signature Algorithm: ecdsa-with-my-custom-hash
    # ...

$OPENSSL req -new \
  -key leaf.key.pem \
  -subj "/CN=example-leaf" \
  -myhash \
  -out leaf.csr.pem

cat > leaf.ext <<'EOF'
basicConstraints = critical, CA:false
keyUsage = critical, digitalSignature, keyAgreement
extendedKeyUsage = serverAuth, clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer
EOF

$OPENSSL x509 -req \
  -in leaf.csr.pem \
  -CA ca.cert.pem \
  -CAkey ca.key.pem \
  -CAcreateserial \
  -out leaf.cert.pem \
  -days 825 \
  -myhash \
  -extfile leaf.ext

$OPENSSL verify -CAfile ca.cert.pem leaf.cert.pem
    # myhash_nid: 1195
    # myhash_update called
    # myhash_final called
    # leaf.cert.pem: OK
```
