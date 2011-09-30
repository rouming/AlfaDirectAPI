#ifndef CRYPTOPRO_H
#define CRYPTOPRO_H

#ifdef _WIN_

#include <windows.h>


BOOL WINAPI CryptAcquireProvider (char *pszStoreName, PCCERT_CONTEXT cert, HCRYPTPROV *phCryptProv, DWORD *keytype, BOOL *release);

LPWSTR WINAPI
AtlA2WHelper(LPWSTR lpw, LPCSTR lpa, int nChars, UINT acp)
{
    /* verify that no illegal character present*/
    /* since lpw was allocated based on the size of lpa*/
    /* don't worry about the number of chars*/
    lpw[0] = '\0';
    MultiByteToWideChar(acp, 0, lpa, -1, lpw, nChars);
    return lpw;
}
LPSTR WINAPI
AtlW2AHelper(LPSTR lpa, LPCWSTR lpw, int nChars, UINT acp)
{
    /* verify that no illegal character present*/
    /* since lpa was allocated based on the size of lpw*/
    /* don't worry about the number of chars*/
    lpa[0] = '\0';
    WideCharToMultiByte(acp, 0, lpw, -1, lpa, nChars, NULL, NULL);
    return lpa;
}


#define USES_CONVERSION \
    int _convert = 0; unsigned int _acp = CP_ACP; \
    LPCWSTR _lpw = NULL; LPCSTR _lpa = NULL
#define ATLA2WHELPER AtlA2WHelper
#define A2W(lpa) (((_lpa = lpa) == NULL) ? NULL : ( \
    _convert = (lstrlenA(_lpa)+1), \
    ATLA2WHELPER((LPWSTR) _alloca(_convert*2), _lpa, _convert, _acp)))
#define ATLW2AHELPER AtlW2AHelper
#define W2A(lpw) (((_lpw = lpw) == NULL) ? NULL : ( \
    _convert = (lstrlenW(_lpw)+1)*2, \
    ATLW2AHELPER((LPSTR) _alloca(_convert), _lpw, _convert, _acp)))




/*--------------------------------------------------------------------*/
/* Программа по заданному сертификату определяет наличие секретного ключа*/
/* и загружает требуемый провайдер.*/
/* Для определения провайдера используется функция CryptAcquireCertificatePrivateKey, */
/* если она присутствует в crypt32.dll. Иначе производистя поиск ключа по сертификату в справочнике.*/
/* Функция должна быть только*/
/*  Windows NT/2000: Requires Windows NT 4.0 SP3 or later (or Windows NT 4.0 with Internet Explorer 3.02 or later).*/
/*  Windows 95/98: Unsupported.*/

typedef BOOL (WINAPI *CPCryptAcquireCertificatePrivateKey)(
  PCCERT_CONTEXT pCert,
  DWORD dwFlags,
  void *pvReserved,
  HCRYPTPROV *phCryptProv,
  DWORD *pdwKeySpec,
  BOOL *pfCallerFreeProv
);

static CPCryptAcquireCertificatePrivateKey WantContext;


#define TYPE_DER (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)


#define OID_HashVerbaO "1.2.643.2.2.30.1"	/* ГОСТ Р 34.11-94, параметры по умолчанию */



/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/
/**/
/* Пример создания PKCS#7 Signed*/
/**/
/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/
static int do_sign ( const QByteArray& msg, const QByteArray& cert, QByteArray& sign, int detached, char *OID)
{
    PCCERT_CONTEXT  pUserCert = NULL;		/* User certificate to be used*/
    int         ret = 0;
    BYTE        *mem_tbs = NULL;
    size_t      mem_len = 0;

    CRYPT_SIGN_MESSAGE_PARA param;
    DWORD		MessageSizeArray[1];
    const BYTE		*MessageArray[1];
    DWORD		signed_len = 0;
    BYTE		*signed_mem = NULL;
    CRYPT_KEY_PROV_INFO *pCryptKeyProvInfo = NULL;
    DWORD		cbData = 0;

    /*--------------------------------------------------------------------*/
    /*  Переменные для определения системного времени и */
    /* записи его в формат подписанного сообщения*/

    CRYPT_ATTR_BLOB	cablob[1];
    CRYPT_ATTRIBUTE	ca[1];
    LPBYTE		pbAuth = NULL;
    DWORD		cbAuth = 0;
    FILETIME		fileTime;
    SYSTEMTIME		systemTime;




    pUserCert = CertCreateCertificateContext( X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                            (const BYTE*)cert.data(), cert.size());


    if (!pUserCert) {
    printf ("Cannot find User certificate\n" );
    goto err;
    }

    /*--------------------------------------------------------------------*/
    /* Для того чтобы функция CryptAcquireContext не загружала постоянно*/
    /* провайдер и ключ можно использовать флаг CERT_SET_KEY_CONTEXT_PROP_ID или*/
    /* CERT_SET_KEY_PROV_HANDLE_PROP_ID в значении флага структуры CRYPT_KEY_PROV_INFO.*/
    /**/
    /* Для этого определим наличие этого свойства и перезапишем флаг*/
    ret = CertGetCertificateContextProperty(pUserCert,
    CERT_KEY_PROV_INFO_PROP_ID, NULL, &cbData);
    if (ret) {
    pCryptKeyProvInfo = (CRYPT_KEY_PROV_INFO *)malloc(cbData);
    if(!pCryptKeyProvInfo)
        qWarning("Error in allocation of memory.");

    ret = CertGetCertificateContextProperty(pUserCert,
        CERT_KEY_PROV_INFO_PROP_ID, pCryptKeyProvInfo,&cbData);
    if (ret)
    {
        /* Установим флаг кеширования провайдера*/
        pCryptKeyProvInfo->dwFlags = CERT_SET_KEY_CONTEXT_PROP_ID;
        /* Установим свойства в контексте сертификата*/
        ret = CertSetCertificateContextProperty(pUserCert, CERT_KEY_PROV_INFO_PROP_ID,
        CERT_STORE_NO_CRYPT_RELEASE_FLAG, pCryptKeyProvInfo);
        free(pCryptKeyProvInfo);
    }
    else
        qWarning("The property was not retrieved.");
    }
    else {
    printf ("Cannot retrive certificate property.\n");
    }


    /*--------------------------------------------------------------------*/
    /* Откроем файл который будем подписывать*/
    mem_tbs = (BYTE*)msg.data();
    mem_len = msg.size();

    /*--------------------------------------------------------------------*/
    /* Установим параметры*/

    /* Обязательно ныжно обнулить все поля структуры. */
    /* Иначе это может привести к access violation в функциях CryptoAPI*/
    /* В примере из MSDN это отсутствует*/
    memset(&param, 0, sizeof(CRYPT_SIGN_MESSAGE_PARA));
    param.cbSize = sizeof(CRYPT_SIGN_MESSAGE_PARA);
    param.dwMsgEncodingType = TYPE_DER;
    param.pSigningCert = pUserCert;

    param.HashAlgorithm.pszObjId = OID;
    param.HashAlgorithm.Parameters.cbData = 0;
    param.HashAlgorithm.Parameters.pbData = NULL;
    param.pvHashAuxInfo = NULL;	/* не используется*/
    param.cMsgCert = 0;		/* не вклачаем сертификат отправителя*/
    param.rgpMsgCert = NULL;
    param.cAuthAttr = 0;
    param.dwInnerContentType = 0;
    param.cMsgCrl = 0;
    param.cUnauthAttr = 0;


   /*---------------------------------------------------------------------------------
    Определим системное время и добавим его в список аутентифицируемых (подписанных)
    атрибутов PKCS#7 сообщения с идентификатором szOID_RSA_signingTime.
    ---------------------------------------------------------------------------------*/
    GetSystemTime(&systemTime);
    SystemTimeToFileTime(&systemTime, &fileTime);

    /* Определим требуемую длину для хранения времени*/
    ret = CryptEncodeObject(TYPE_DER,
    szOID_RSA_signingTime,
    (LPVOID)&fileTime,
    NULL,
    &cbAuth);
    if (!ret)
    qWarning("Cannot encode object");

    pbAuth = (BYTE*) malloc (cbAuth);
    if (!pbAuth)
        qWarning("Memory allocation error");

    /* Кодирование времени в атрибут типа szOID_RSA_signingTime */
    ret = CryptEncodeObject(TYPE_DER,
    szOID_RSA_signingTime,
    (LPVOID)&fileTime,
    pbAuth,
    &cbAuth);
    if (!ret)
    qWarning("Cannot encode object");

    cablob[0].cbData = cbAuth;
    cablob[0].pbData = pbAuth;

    ca[0].pszObjId = szOID_RSA_signingTime;
    ca[0].cValue = 1;
    ca[0].rgValue = cablob;

    param.cAuthAttr = 1;
    param.rgAuthAttr = ca;

   /*---------------------------------------------------------------------------------
    dwFlags
    Normally zero. If the encoded output is to be a CMSG_SIGNED inner content
    of an outer cryptographic message such as a CMSG_ENVELOPED message,
    the CRYPT_MESSAGE_BARE_CONTENT_OUT_FLAG must be set.
    If it is not set, the message will be encoded as an inner content type of CMSG_DATA.
    With Windows 2000, CRYPT_MESSAGE_ENCAPSULATED_CONTENT_OUT_FLAG can be set
    to encapsulate non-data inner content into an OCTET STRING.
    Also, CRYPT_MESSAGE_KEYID_SINGER_FLAG can be set to identify signers
    by their Key Identifier and not their Issuer and Serial Number.
    ---------------------------------------------------------------------------------*/
    param.dwFlags = 0;

    MessageArray[0] = mem_tbs;
    MessageSizeArray[0] = mem_len;

    printf ("Source message length: %lu\n", mem_len);

    /* Возможен вариант использования функции CryptSignMessage в двухпроходной схеме*/
    /* с передачей вместо исходных данных нуля для определения длины подписанных данных*/
    /* (см. раздел Возвращение данных неопределенной длины в Руководстве программиста).*/
    /* В этом случае функция CryptSignMessage производит инициализацию провайдера, соответствующего сертификату и */
    /* подпись данных для определения длины, что приводит к необходимости двойной загрузки ключа.*/
    /**/
    /* Для того чтобы этого избежать приложение может заранее зарезервировать определенное */
    /* количество памяти, достаточное для создания подписанного сообщения.*/

    /*--------------------------------------------------------------------*/
    /* Определение длины подписанного сообщения*/
    ret = CryptSignMessage(
        &param,
        detached,
        1,
        MessageArray,
        MessageSizeArray,
        NULL,
        &signed_len);

    if (ret) {
        printf("Calculated signature (or signed message) length: %lu\n", signed_len);
        signed_mem = (BYTE*) malloc (signed_len);
        if (!signed_mem)
        goto err;
    }
    else
    {
        qWarning("Signature creation error");
    }
    /*--------------------------------------------------------------------*/
    /* Формирование подписанного сообщения*/
    ret = CryptSignMessage(
        &param,
        detached,
        1,
        MessageArray,
        MessageSizeArray,
        signed_mem,
        &signed_len);
    if (ret) {
        printf("Signature was done. Signature (or signed message) length: %lu\n", signed_len);
    }
    else
    {
        qWarning("Signature creation error");
    }


    /* Запись в файл*/
    sign = QByteArray((char*)signed_mem, signed_len);

err:
    if (signed_mem) free (signed_mem);
    return ret;
}

int do_low_sign (  const QByteArray& msg, const QByteArray& cert, QByteArray& sign, char *OID, int include, int detached, int Cert_LM)
{
(void)Cert_LM;
    /*--------------------------------------------------------------------*/
    /*  Переменные для определения системного времени и */
    /* записи его в формат подписанного сообщения*/

    CRYPT_ATTR_BLOB	cablob[1];
    CRYPT_ATTRIBUTE	ca[1];
    LPBYTE		pbAuth = NULL;
    DWORD		cbAuth = 0;
    FILETIME		fileTime;
    SYSTEMTIME		systemTime;

    HCRYPTPROV      hCryptProv = 0;     /* Дескриптор провайдера*/
    PCCERT_CONTEXT  pUserCert = NULL;       /* Сертификат, используемый для формирования ЭЦП*/

    DWORD       keytype = 0;        /* Тип ключа (возвращается)*/
    BOOL        should_release_ctx = FALSE;
    int         ret = 0;
    BYTE        *mem_tbs = NULL;        /* Исходные данные*/
    size_t      mem_len = 0;        /* Длина данных*/

    HCRYPTMSG       hMsg = 0;           /* Дескриптор сообщения*/

    CRYPT_ALGORITHM_IDENTIFIER	HashAlgorithm;	/* Идентификатор алгоритма хэширования*/
    DWORD			HashAlgSize;
    CMSG_SIGNER_ENCODE_INFO	SignerEncodeInfo;   /* Структура, описывающая отправителя*/
    CMSG_SIGNER_ENCODE_INFO	SignerEncodeInfoArray[1]; /* Массив структур, описывающих отправителя*/
    CERT_BLOB			SignerCertBlob;
    CERT_BLOB			SignerCertBlobArray[1];
    DWORD			cbEncodedBlob;
    BYTE			*pbEncodedBlob = NULL;
    CMSG_SIGNED_ENCODE_INFO	SignedMsgEncodeInfo;	/* Структура, описывающая подписанное сообщение*/
    DWORD			flags = 0;

    pUserCert = CertCreateCertificateContext( X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                            (const BYTE*)cert.data(), cert.size());


    /*--------------------------------------------------------------------*/
    /* Программа по заданному сертификату определяет наличие секретного ключа*/
    /* и загружает требуемый провайдер.*/
    /* Для определения провайдера используется функция CryptAcquireCertificatePrivateKey, */
    /* если она присутствует в crypt32.dll. Иначе производистя поиск ключа по сертификату в справочнике.*/
    ret = CryptAcquireProvider ("my", pUserCert, &hCryptProv, &keytype, &should_release_ctx);
    if (ret) {
    printf("A CSP has been acquired. \n");
    }
    else {
    printf("Cryptographic context could not be acquired.");
    }

    /*--------------------------------------------------------------------*/
    /* Прочитаем файл, который будем подписывать.*/
    mem_tbs = (BYTE*)msg.data();
    mem_len = msg.size();

   /*--------------------------------------------------------------------*/
    /* Инициализируем структуру алгоритма*/

    HashAlgSize = sizeof(HashAlgorithm);
    memset(&HashAlgorithm, 0, HashAlgSize);
    HashAlgorithm.pszObjId = OID;       /* Идентификатор алгоритма хэша*/

    /*--------------------------------------------------------------------*/
    /* Инициализируем структуру CMSG_SIGNER_ENCODE_INFO*/

    memset(&SignerEncodeInfo, 0, sizeof(CMSG_SIGNER_ENCODE_INFO));
    SignerEncodeInfo.cbSize = sizeof(CMSG_SIGNER_ENCODE_INFO);
    SignerEncodeInfo.pCertInfo = pUserCert->pCertInfo;
    SignerEncodeInfo.hCryptProv = hCryptProv;
    SignerEncodeInfo.dwKeySpec = keytype;
    SignerEncodeInfo.HashAlgorithm = HashAlgorithm;
    SignerEncodeInfo.pvHashAuxInfo = NULL;

   /*---------------------------------------------------------------------------------
    Определим системное время и добавим его в список аутентифицируемых (подписанных)
    атрибутов PKCS#7 сообщения с идентификатором szOID_RSA_signingTime.
    ---------------------------------------------------------------------------------*/
    GetSystemTime(&systemTime);
    SystemTimeToFileTime(&systemTime, &fileTime);

    /* Определим требуемую длину для хранения времени*/
    ret = CryptEncodeObject(TYPE_DER,
    szOID_RSA_signingTime,
    (LPVOID)&fileTime,
    NULL,
    &cbAuth);
    if (!ret)
    qWarning("Cannot encode object");

    pbAuth = (BYTE*) malloc (cbAuth);
    if (!pbAuth)
        qWarning("Memory allocation error");

    /* Кодирование времени в атрибут типа szOID_RSA_signingTime */
    ret = CryptEncodeObject(TYPE_DER,
    szOID_RSA_signingTime,
    (LPVOID)&fileTime,
    pbAuth,
    &cbAuth);
    if (!ret)
    qWarning("Cannot encode object");

    cablob[0].cbData = cbAuth;
    cablob[0].pbData = pbAuth;

    ca[0].pszObjId = szOID_RSA_signingTime;
    ca[0].cValue = 1;
    ca[0].rgValue = cablob;

    SignerEncodeInfo.cAuthAttr = 1;
    SignerEncodeInfo.rgAuthAttr = ca;


    /*--------------------------------------------------------------------*/
    /* Создадим массив отправителей. Сейчас только из одного.*/

    SignerEncodeInfoArray[0] = SignerEncodeInfo;

    /*--------------------------------------------------------------------*/
    /* Инициализируем структуру CMSG_SIGNED_ENCODE_INFO*/

    SignerCertBlob.cbData = pUserCert->cbCertEncoded;
    SignerCertBlob.pbData = pUserCert->pbCertEncoded;

    /*--------------------------------------------------------------------*/
    /* Инициализируем структуру массив структур CertBlob.*/

    SignerCertBlobArray[0] = SignerCertBlob;
    memset(&SignedMsgEncodeInfo, 0, sizeof(CMSG_SIGNED_ENCODE_INFO));
    SignedMsgEncodeInfo.cbSize = sizeof(CMSG_SIGNED_ENCODE_INFO);
    SignedMsgEncodeInfo.cSigners = 1;
    SignedMsgEncodeInfo.rgSigners = SignerEncodeInfoArray;
    SignedMsgEncodeInfo.cCertEncoded = include;
    /* Если задан флаг добавления сертификата отправителя*/
    if (include)
    SignedMsgEncodeInfo.rgCertEncoded = SignerCertBlobArray;
    else
    SignedMsgEncodeInfo.rgCertEncoded = NULL;

    SignedMsgEncodeInfo.rgCrlEncoded = NULL;
    if (detached)
    flags = CMSG_DETACHED_FLAG;

    /*--------------------------------------------------------------------*/
    /* Определим длину подписанного сообщения*/

    cbEncodedBlob = CryptMsgCalculateEncodedLength(
    TYPE_DER,		/* Message encoding type*/
    flags,                  /* Flags*/
    CMSG_SIGNED,            /* Message type*/
    &SignedMsgEncodeInfo,   /* Pointer to structure*/
    NULL,                   /* Inner content object ID*/
    mem_len);		/* Size of content*/
    if(cbEncodedBlob)
    {
    printf("The length of the data has been calculated. \n");
    }
    else
    {
    printf("Getting cbEncodedBlob length failed");
    }
    /*--------------------------------------------------------------------*/
    /* Резервируем память, требуемой длины*/

    pbEncodedBlob = (BYTE *) malloc(cbEncodedBlob);
    if (!pbEncodedBlob)
    printf("Memory allocation failed");
    /*--------------------------------------------------------------------*/
    /* Создадим дескриптор сообщения*/
    hMsg = CryptMsgOpenToEncode(
    TYPE_DER,		/* Encoding type*/
    flags,                  /* Flags (CMSG_DETACHED_FLAG )*/
    CMSG_SIGNED,            /* Message type*/
    &SignedMsgEncodeInfo,   /* Pointer to structure*/
    NULL,                   /* Inner content object ID*/
    NULL);                  /* Stream information (not used)*/
    if(hMsg) {
    printf("The message to be encoded has been opened. \n");
    }
    else
    {
    printf("OpenToEncode failed");
    }
    /*--------------------------------------------------------------------*/
    /* Поместим в сообщение подписываемые данные*/

    if(CryptMsgUpdate(
        hMsg,           /* Handle to the message*/
        mem_tbs,        /* Pointer to the content*/
        mem_len,        /* Size of the content*/
        TRUE))          /* Last call*/
    {
    printf("Content has been added to the encoded message. \n");
    }
    else
    {
    printf("MsgUpdate failed");
    }
    /*--------------------------------------------------------------------*/
    /* Вернем подписанное сообщение или только значение ЭЦП, если установлен признак detached*/

    if(CryptMsgGetParam(
    hMsg,                      /* Handle to the message*/
    CMSG_CONTENT_PARAM,        /* Parameter type*/
    0,                         /* Index*/
    pbEncodedBlob,             /* Pointer to the blob*/
    &cbEncodedBlob))           /* Size of the blob*/
    {
    printf("Message encoded successfully. \n");
    }
    else
    {
    printf("MsgGetParam failed");
    }
    /*--------------------------------------------------------------------*/
    /* Если задано, запишем подписанные данные в файл*/
    sign = QByteArray((char*)pbEncodedBlob, cbEncodedBlob);

    /*--------------------------------------------------------------------*/
    /* Очистка памяти*/
    if(hMsg)
    CryptMsgClose(hMsg);
    if(hCryptProv)
    CryptReleaseContext(hCryptProv,0);

    return 1;
} /*  End of main*/

BOOL WINAPI CryptAcquireProvider (char *pszStoreName, PCCERT_CONTEXT cert, HCRYPTPROV *phCryptProv, DWORD *keytype, BOOL *release)
{
   /*--------------------------------------------------------------------*/

    HANDLE           hCertStore = 0;
    PCCERT_CONTEXT   pCertContext=NULL;
    CRYPT_KEY_PROV_INFO *pCryptKeyProvInfo;
    /*char pszNameString[256];*/
    void*            pvData;
    DWORD            cbData;
    DWORD            dwPropId = 0;   /* 0 must be used on the first*/
    CRYPT_HASH_BLOB  blob;
    BOOL        ret = FALSE;
    DWORD       len = 0;
    BYTE        data[128];
    USES_CONVERSION;
    HMODULE     crypt32;
    wchar_t     wszStoreName[80];
    _lpw, _lpa;
    /*--------------------------------------------------------------------*/
    /* Попробуем определеить наличие функции CryptAcquireCertificatePrivateKey*/

    crypt32 = GetModuleHandleA ("crypt32.dll");
    if (crypt32) {
        WantContext = (CPCryptAcquireCertificatePrivateKey) GetProcAddress (crypt32,"CryptAcquireCertificatePrivateKey");
    if (WantContext)
        return WantContext(cert, 0, NULL, phCryptProv,keytype,release);
    else
        printf ("CRYPT32.DLL not supported CryptAcquireCertificatePrivateKey function\n");
    }

    /*--------------------------------------------------------------------*/
    /* Open the named system certificate store. */
    /*    if (!( hCertStore = CertOpenSystemStore(*/
    /*	0,*/
    /*	pszStoreName))) {*/
    if (MultiByteToWideChar(CP_OEMCP, MB_ERR_INVALID_CHARS,
                pszStoreName, -1,
                wszStoreName, sizeof(wszStoreName)/sizeof(wszStoreName[0])) <= 0) {
    printf("MultiByteToWideChar");
    goto err;
    }
    hCertStore = CertOpenStore(
                CERT_STORE_PROV_SYSTEM, /* LPCSTR lpszStoreProvider*/
                0,              /* DWORD dwMsgAndCertEncodingType*/
                0,              /* HCRYPTPROV hCryptProv*/
                CERT_STORE_OPEN_EXISTING_FLAG|CERT_STORE_READONLY_FLAG|
                CERT_SYSTEM_STORE_CURRENT_USER, /* DWORD dwFlags*/
                wszStoreName            /* const void *pvPara*/
                );
    if (!hCertStore) {
    printf("CertOpenStore");
    goto err;
    }

    /* Определим значение дополненя subjectKeyIdentifier*/
    /* If nonexistent, searches for the szOID_SUBJECT_KEY_IDENTIFIER extension. */
    /* If that fails, a SHA1 hash is done on the certificate's SubjectPublicKeyInfo */
    /* to produce the identifier values*/
    len = sizeof(data);
    ret = CertGetCertificateContextProperty(cert,
    CERT_KEY_IDENTIFIER_PROP_ID,
    (void*) data,
    &len);

    if (! ret) {
    printf("CertGetCertificateContextProperty");
    goto err;
    }
    blob.cbData = len;
    blob.pbData = data;

    /* Найдем сертификат с соответствующим значением*/
    pCertContext = CertFindCertificateInStore (hCertStore,
    TYPE_DER,
    0,
    CERT_FIND_KEY_IDENTIFIER,
    &blob,
    NULL);

    if (! pCertContext ) goto err;
        /*--------------------------------------------------------------------*/
    /* In a loop, find all of the property IDs for the given certificate.*/
    /* The loop continues until the CertEnumCertificateContextProperties */
    /* returns 0.*/

    while((dwPropId = CertEnumCertificateContextProperties(
        pCertContext, /* the context whose properties are to be listed.*/
        dwPropId)) != 0)    /* number of the last property found. Must be*/
        /* 0 to find the first property ID.*/
    {
        /*--------------------------------------------------------------------*/
        /* Each time through the loop, a property ID has been found.*/
        /* Print the property number and information about the property.*/

        printf("Property # %d found->", dwPropId);
        switch(dwPropId)
        {
        case CERT_FRIENDLY_NAME_PROP_ID:
        {
            /*--------------------------------------------------------------------*/
            /*  Retrieve the actual friendly name certificate property.*/
            /*  First, get the length of the property setting the*/
            /*  pvData parameter to NULL to get a value for cbData*/
            /*  to be used to allocate memory for the pvData buffer.*/
            printf("FRIENDLY_NAME_PROP_ID ");
            if(!(CertGetCertificateContextProperty(
            pCertContext,
            dwPropId,
            NULL,
            &cbData)))
            goto err;
            /*--------------------------------------------------------------------*/
            /* The call succeeded. Use the size to allocate memory for the */
            /* property.*/
            pvData = (void*)malloc(cbData);
            if(!pvData) {
            printf("Memory allocation failed.");
            }
            /*--------------------------------------------------------------------*/
            /* Allocation succeeded. Retrieve the property data.*/
            if(!(CertGetCertificateContextProperty(
            pCertContext,
            dwPropId,
            pvData,
            &cbData)))
            {
            printf("Call #2 getting the data failed.");
            }
            else
            {
            printf("\n  The friendly name is -> %s.", pvData);
            free(pvData);
            }
            break;
        }
        case CERT_SIGNATURE_HASH_PROP_ID:
        {
            printf("Signature hash ID. ");
            break;
        }
        case CERT_KEY_PROV_HANDLE_PROP_ID:
        {
            printf("KEY PROVIDER HANDLE.");
            break;
        }
        case CERT_KEY_PROV_INFO_PROP_ID:
        {
            printf("KEY PROV INFO PROP ID.");
            if(!(CertGetCertificateContextProperty(
            pCertContext,  /* A pointer to the certificate*/
            /* where the property will be set.*/
            dwPropId,      /* An identifier of the property to get. */
            /* In this case,*/
            /* CERT_KEY_PROV_INFO_PROP_ID*/
            NULL,          /* NULL on the first call to get the*/
            /* length.*/
            &cbData)))     /* The number of bytes that must be*/
            /* allocated for the structure.*/
            goto err;
            pCryptKeyProvInfo =
            (CRYPT_KEY_PROV_INFO *)malloc(cbData);
            if(!pCryptKeyProvInfo)
            {
            printf("Error in allocation of memory.");
            }
            if(CertGetCertificateContextProperty(
            pCertContext,
            dwPropId,
            pCryptKeyProvInfo,
            &cbData))
            {
            printf("\nThe current key container is %S\n",
                pCryptKeyProvInfo->pwszContainerName);
            printf("\nThe provider name is:%S",
                pCryptKeyProvInfo->pwszProvName);
            *keytype = pCryptKeyProvInfo->dwKeySpec;
            *release = TRUE;
            /* Откроем провайдер*/
            ret = CryptAcquireContextA (phCryptProv,
                W2A(pCryptKeyProvInfo->pwszContainerName),
                W2A(pCryptKeyProvInfo->pwszProvName),
                pCryptKeyProvInfo->dwProvType,
                0);
                free(pCryptKeyProvInfo);
            if (ret)
                break;
            }
            else
            {
            free(pCryptKeyProvInfo);
            printf("The property was not retrieved.");
            }
            break;
        }
        case CERT_SHA1_HASH_PROP_ID:
        {
            printf("SHA1 HASH id.");
            break;
        }
        case CERT_MD5_HASH_PROP_ID:
        {
            printf("md5 hash id. ");
            break;
        }
        case CERT_KEY_CONTEXT_PROP_ID:
        {
            printf("KEY CONTEXT PROP id.");
            break;
        }
        case CERT_KEY_SPEC_PROP_ID:
        {
            printf("KEY SPEC PROP id.");
            break;
        }
        case CERT_ENHKEY_USAGE_PROP_ID:
        {
            printf("ENHKEY USAGE PROP id.");
            break;
        }
        case CERT_NEXT_UPDATE_LOCATION_PROP_ID:
        {
            printf("NEXT UPDATE LOCATION PROP id.");
            break;
        }
        case CERT_PVK_FILE_PROP_ID:
        {
            printf("PVK FILE PROP id. ");
            break;
        }
        case CERT_DESCRIPTION_PROP_ID:
        {
            printf("DESCRIPTION PROP id. ");
            break;
        }
        case CERT_ACCESS_STATE_PROP_ID:
        {
            printf("ACCESS STATE PROP id. ");
            break;
        }
        case CERT_SMART_CARD_DATA_PROP_ID:
        {
            printf("SMART_CARD DATA PROP id. ");
            break;
        }
        case CERT_EFS_PROP_ID:
        {
            printf("EFS PROP id. ");
            break;
        }
        case CERT_FORTEZZA_DATA_PROP_ID:
        {
            printf("FORTEZZA DATA PROP id.");
            break;
        }
        case CERT_ARCHIVED_PROP_ID:
        {
            printf("ARCHIVED PROP id.");
            break;
        }
        case CERT_KEY_IDENTIFIER_PROP_ID:
        {
            printf("KEY IDENTIFIER PROP id. ");
            break;
        }
        case CERT_AUTO_ENROLL_PROP_ID:
        {
            printf("AUTO ENROLL id. ");
            break;
        }
        }  /* end switch*/
        printf("\n");
      } /* end the inner while loop. This is the end of the display of*/
      /* a single property of a single certificate.*/
/*--------------------------------------------------------------------*/
/* Free Memory and close the open store.*/
    if(pCertContext)
    {
    CertFreeCertificateContext(pCertContext);
    }
    if(hCertStore)
    CertCloseStore(hCertStore, CERT_CLOSE_STORE_FORCE_FLAG);

err:
    return ret;
}

#else
#error Unsupported platform

#endif

#endif //CRYPTOPRO_H
