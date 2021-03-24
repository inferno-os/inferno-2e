/*
 * Authors: Jack Lacy and Michael Reiter, AT&T Bell Laboratories
 *
 * Copyright (c) 1994 AT&T Bell Laboratories
 *
 * This file should not be distributed without the permission of
 * the authors.
 */

typedef uchar byte;

#define MAX_NAME_LEN         64

#define MAX_DSS_MOD_LEN      128
#define MAX_DSS_PUB_KEY_LEN  (20 + 3 * MAX_DSS_MOD_LEN)
#define MAX_DSS_SIG_LEN      40

#define MAX(a,b) (a > b ? a : b)

/* Algorithms
 */
#define RSA_ALG                  ((byte) (1 << 0))
#define DSA_ALG                  ((byte) (1 << 1))
#define MD5_ALG                  ((byte) (1 << 2))
#define SHS_ALG                  ((byte) (1 << 3))

void putBigInt(BigInt big, Biobuf *stream);
BigInt getBigInt(Biobuf *stream);
void putTable(Table *table, Biobuf *stream);
Table * getTable(Biobuf *stream);

void putEGParams(EGParams *params, Biobuf *stream);
EGParams * getEGParams(Biobuf *stream);
void putEGPublicKey(EGPublicKey *key, Biobuf *stream);
EGPublicKey * getEGPublicKey(Biobuf *stream);
void putEGPrivateKey(EGPrivateKey *key, Biobuf *stream);
EGPrivateKey * getEGPrivateKey(Biobuf *stream);
void putEGSignature(EGSignature *sig, Biobuf *stream);
EGSignature * getEGSignature(Biobuf *stream);

void putDSSSignature(DSSSignature *sig, Biobuf *stream);
DSSSignature * getDSSSignature(Biobuf *stream);
#define putEGPublicKey putDSSPublicKey
#define getEGPublicKey getDSSPublicKey
#define putEGPrivateKey putDSSPrivateKey
#define getEGPrivateKey getDSSPrivateKey

void putCertificate(cert_info_t *cert, Biobuf *stream);
cert_info_t * getCertificate(Biobuf *stream);
