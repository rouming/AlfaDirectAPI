/*******************************************************************************
#
# How to use CryptoPRO and AlfaDirect on Linux
#

# I'am using Arch linux and before installing crypto pro linux rpms you should do
# some preparations. Basic faq for linux you can find here: http://cryptopro.ru/faq

#0. FYI base cprocsp folders
/etc/cron.daily/cprocsp
/etc/opt/cprocsp
/etc/init.d/cprocsp
/opt/cprocsp
/var/opt/cprocsp

#1. install fake Linux Standard Base runtime
yaourt -S ld-lsb

#2. install rpm-org (others didn't work for me)
yaourt -S rpm-org

#3. create lsb ELF interpreter if your arch is x86-64
sudo ln -s /lib64/ld-linux-x86-64.so.2 /lib64/ld-lsb-x86-64.so.3

# Then install crypto pro rpms
#

#1. firstly install base
sudo rpm -i --nodeps lsb-cprocsp-base-3.6.1-4.noarch.rpm

#2. install others rpms
sudo rpm -i  --force --nodeps *.rpm

#3. start server. all errors are in /var/errors.log
sudo /opt/cprocsp/sbin/<arch>/cryptsrv


#
# Install keys and certificates
#

#
# Some important steps on Windows
#

#1. do copy of private key container and name it as 'alfadirect':
      Win Settings/КриптоПро CSP/Сервис/Скопировать...

#2. export certificate based on private key container in base64 and
    name it e.g. 'cert.cer'. do not include private key into this certificate:
      Win Settings/КриптоПро CSP/Сервис/Просмотреть сертификаты в контейнере...
    choose 'alfadirect' key container
    choose Свойства/Состав/Копировать в файл
      - do not export private key
      - choose base64

#3. copy new private key container (folder) 'alfadirect' and cert.cer on linux host

#
# Steps on Linux
#

#0. start server (or make sure it is already running)
sudo /opt/cprocsp/sbin/<arch>/cryptsrv

#1. copy private key folder to
/var/opt/cprocsp/keys/<username>/alfadirect
where <username> - your system username

#2. get list of keys containers and check, that newly copied container is there
/opt/cprocsp/bin/<arch>/csptestf -keyset -enum_cont -verifycontext -fqcn

#3. check key container, no errors should be
/opt/cprocsp/bin/<arch>/csptest -keyset -check -cont '\\.\HDIMAGE\alfadirect'

#4. install certificate

# with pin
/opt/cprocsp/bin/<arch>/certmgr -inst -at_signature -store uMy -pin 1q2w3e -file cert.cer -cont '\\.\HDIMAGE\alfadirect'

# or without pin
/opt/cprocsp/bin/<arch>/certmgr -inst -at_signature -store uMy -file cert.cer -cont '\\.\HDIMAGE\alfadirect'

#5. get list of containers, check, that newly installed container is in list
#   and this container linked with private key, i.e. this line exists:
#  PrivateKey Link: Yes. Container: HDIMAGE\\alfadirect\2E20
/opt/cprocsp/bin/<arch>/certmgr -list

# man is working too
man certmgr
*******************************************************************************/

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
    (void)certContext;
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
