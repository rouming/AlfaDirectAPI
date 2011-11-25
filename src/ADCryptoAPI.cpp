#ifdef _WIN_
  #include <windows.h>
#elif defined(_CRYPTOAPI_)
  #include <stdint.h>
  #include <Crypt.h>
#endif

#include <malloc.h>
#include <stdio.h>

#include <QByteArray>

#include "ADCryptoAPI.h"

/******************************************************************************/

// GOST
#define HASH_ALGORITHM "1.2.643.2.2.9"

/******************************************************************************/

bool ADCryptoAPI::loadCertificate ( const char* certData,
                                    int certDataSize,
                                    void** certContext )
{
#ifdef _CRYPTOAPI_
    PCCERT_CONTEXT userCert = 0;

    if ( certData == 0 || certContext == 0 )
        return false;

    // From hex
    QByteArray ba = QByteArray::fromHex(
        QByteArray::fromRawData(certData, certDataSize));

    userCert = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                            (const BYTE*)ba.data(), ba.size());
    if ( ! userCert ) {
        printf("Error: can't create certificate context from encoded data, "
               "err=%x\n", GetLastError());
        return false;
    }

    *((PCCERT_CONTEXT*)certContext) = userCert;
    return true;
#else
    (void)certData;
    (void)certDataSize;
    return false;
#endif
}

bool ADCryptoAPI::unloadCertificate ( const void* certContext )
{
#ifdef _CRYPTOAPI_
    if ( certContext == 0 )
        return false;

    return !!CertFreeCertificateContext((PCCERT_CONTEXT)certContext);
#else
    (void)certContext;
    return false;
#endif
}

bool ADCryptoAPI::loadContext ( const void* certContext_,
                                void** provContext )
{
#ifdef _CRYPTOAPI_
    HANDLE           certStore = 0;
    CRYPT_KEY_PROV_INFO* cryptKeyProvInfo = 0;
    CRYPT_HASH_BLOB  blob;
    BOOL             ret = 0;
    BYTE             data[128];
    DWORD            len = 0;
    HCRYPTPROV       cryptProv = 0;
    PCCERT_CONTEXT   certContext = (PCCERT_CONTEXT)certContext_;

    if ( certContext == 0 || provContext == 0 )
        return false;

    // Open cert system store
    certStore = CertOpenSystemStore(0, "MY");
    if ( !certStore ) {
        printf("Error: can't open store, err=%d\n",
               GetLastError());
        goto err;
    }

    // Get size of key identifier property
    len = sizeof(data);
    ret = CertGetCertificateContextProperty(certContext,
                                            CERT_KEY_IDENTIFIER_PROP_ID,
                                            (void*)data,
                                            &len);

    if ( !ret ) {
        printf("Error: can't get certificate property, err=%d\n",
               GetLastError());
        goto err;
    }

    // Init blob with key identifier
    blob.cbData = len;
    blob.pbData = data;

    // Find certificate in store with the same key identifier
    certContext = CertFindCertificateInStore(certStore,
                                             X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                             0,
                                             CERT_FIND_KEY_IDENTIFIER,
                                             &blob,
                                             0);
    if ( certContext == 0 ) {
        printf("Error: can't find certificate by key identifier, err=%d\n",
               GetLastError());
        goto err;
    }

    // Get size of provider info structure from certificate
    ret  = CertGetCertificateContextProperty(certContext,
                                             CERT_KEY_PROV_INFO_PROP_ID,
                                             0,
                                             &len);
    if ( !ret ) {
        printf("Error: can't get size of provider info, err=%d\n",
               GetLastError());
        goto err;
    }

    // Allocate size for provider info structure
    cryptKeyProvInfo = (CRYPT_KEY_PROV_INFO*)malloc(len);
    if ( !cryptKeyProvInfo ) {
        printf("Error: memory problems!\n");
        goto err;
    }

    // Get provider info structure from certificate
    ret = CertGetCertificateContextProperty(certContext,
                                            CERT_KEY_PROV_INFO_PROP_ID,
                                            cryptKeyProvInfo,
                                            &len);
    if ( !ret ) {
        printf("Error: can't get provider info property, err=%x\n",
               GetLastError());
        goto err;
    }

    // Open provider
    ret = CryptAcquireContextW(&cryptProv,
                               cryptKeyProvInfo->pwszContainerName,
                               cryptKeyProvInfo->pwszProvName,
                               cryptKeyProvInfo->dwProvType,
                               0);
    if ( !ret ) {
        printf("Error: can't get open provider, err=%x\n",
               GetLastError());
        goto err;
    }

    *((HCRYPTPROV*)provContext) = cryptProv;

err:
    if ( cryptKeyProvInfo ) {
        free(cryptKeyProvInfo);
        cryptKeyProvInfo = 0;
    }
    if ( certContext ) {
        CertFreeCertificateContext(certContext);
        certContext = 0;
    }
    if ( certStore ) {
        CertCloseStore(certStore, 0);
        certStore = 0;
    }

    return !!ret;
#else
    (void)certContext_;
    (void)provContext;
    return false;
#endif
}

bool ADCryptoAPI::unloadContext ( const void* provContext )
{
#ifdef _CRYPTOAPI_
    if ( provContext == 0 )
        return false;

    return !!CryptReleaseContext((HCRYPTPROV)provContext, 0);
#else
    (void)provContext;
    return false;
#endif
}

bool ADCryptoAPI::makeSignature ( const void* provContext,
                                  const void* certContext,
                                  const char* data, unsigned int dataSize,
                                  char** resultData, unsigned int* resultSize )
{
#ifdef _CRYPTOAPI_
    DWORD encBlobSz;
    BYTE* encBlob = 0;
    QByteArray ba;

    CRYPT_ALGORITHM_IDENTIFIER hashAlgorithm;
    DWORD                      hashAlgSize;
    CMSG_SIGNER_ENCODE_INFO    signerEncodeInfo;
    CMSG_SIGNER_ENCODE_INFO    signerEncodeInfoArray[1];
    CMSG_SIGNED_ENCODE_INFO    signedMsgEncodeInfo;
    DWORD                      flags = 0;
    HCRYPTMSG                  msg = 0;
    BOOL                       ret = 0;

    if ( provContext == 0 || certContext == 0 ||
         data == 0 || dataSize == 0 ||
         resultData == 0 || resultSize == 0 )
        return false;

    // Init hash algorithm
    hashAlgSize = sizeof(hashAlgorithm);
    memset(&hashAlgorithm, 0, hashAlgSize);
    hashAlgorithm.pszObjId = (char*)HASH_ALGORITHM;

    // Init signer encode structure
    memset(&signerEncodeInfo, 0, sizeof(CMSG_SIGNER_ENCODE_INFO));
    signerEncodeInfo.cbSize = sizeof(CMSG_SIGNER_ENCODE_INFO);
    signerEncodeInfo.pCertInfo = ((PCCERT_CONTEXT)certContext)->pCertInfo;
    signerEncodeInfo.hCryptProv = (HCRYPTPROV)provContext;
    signerEncodeInfo.dwKeySpec = AT_SIGNATURE;
    signerEncodeInfo.HashAlgorithm = hashAlgorithm;

    // Init signers encode array
    signerEncodeInfoArray[0] = signerEncodeInfo;

    // Init signed msg
    memset(&signedMsgEncodeInfo, 0, sizeof(signedMsgEncodeInfo));
    signedMsgEncodeInfo.cbSize = sizeof(signedMsgEncodeInfo);
    signedMsgEncodeInfo.cSigners = 1;
    signedMsgEncodeInfo.rgSigners = signerEncodeInfoArray;

    // Get length of signed message
    encBlobSz = CryptMsgCalculateEncodedLength(
                                       X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                       flags,
                                       CMSG_SIGNED,
                                       &signedMsgEncodeInfo,
                                       0,
                                       (DWORD)dataSize);
    if ( !encBlobSz ) {
        printf("Error: can't calculate size for signed msg, err=%x\n",
               GetLastError());
        goto err;
    }

    // Allocate size for msg
    encBlob = (BYTE*)malloc(encBlobSz);
    if ( !encBlob ) {
        printf("Error: memory allocation problems\n");
        goto err;
    }

    // Create msg
    msg = CryptMsgOpenToEncode(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                               flags,
                               CMSG_SIGNED,
                               &signedMsgEncodeInfo,
                               0,
                               0);
    if ( !msg ) {
        printf("Error: can't open msg to encode, err=%x\n",
               GetLastError());
        goto err;
    }

    // Add content to msg
    ret = CryptMsgUpdate(msg, (const BYTE*)data, dataSize, TRUE);
    if ( !ret ) {
        printf("Error: can't add content to msg, err=%x\n",
               GetLastError());
        goto err;
    }

    // Get encoded content from signed message
    ret = CryptMsgGetParam(msg,
                           CMSG_CONTENT_PARAM,
                           0,
                           encBlob,
                           &encBlobSz);
    if ( !ret ) {
        printf("Error: can't get encoded content from msg, err=%x\n",
               GetLastError());
        goto err;

    }

    // To hex
    ba = QByteArray::fromRawData((char*)encBlob, encBlobSz).toHex();
    *resultSize = ba.size() + 2;
    *resultData = (char*)malloc(*resultSize);

    if ( !*resultData ) {
        printf("Error: memory allocation problems\n");
        goto err;
    }

    // Required '0x' prefix
    (*resultData)[0] = '0';
    (*resultData)[1] = 'x';

    // Copy
    memcpy(*resultData + 2, ba.data(), ba.size());

err:

    if ( msg ) {
        CryptMsgClose(msg);
        msg = 0;
    }
    if ( encBlob ) {
        free(encBlob);
        encBlob = 0;
    }

    return !!ret;

#else
    (void)provContext;
    (void)certContext;
    (void)data;
    (void)dataSize;
    (void)resultData;
    (void)resultSize;
    return false;
#endif
}

/******************************************************************************/
