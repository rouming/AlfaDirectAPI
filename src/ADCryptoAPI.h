#ifndef ADCRYPTOAPI_H
#define ADCRYPTOAPI_H

namespace ADCryptoAPI
{
    bool loadCertificate ( const char* certData,
                           int certDataSize,
                           void** certContext );

    bool unloadCertificate ( const void* certContext );

    bool loadContext ( const void* certContext, void** provContext );

    bool unloadContext ( const void* provContext );

    bool makeSignature ( const void* provContext,
                         const void* certContext,
                         const char* szData, unsigned int dataSize,
                         char** resultData, unsigned int* resultSize );

};

#endif //ADCRYPTOAPI_H
