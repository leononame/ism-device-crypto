//
// Created by leo on 11/22/16.
//

#include "global.h"
#include <unistd.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

/// Decrypts AES key from file to memory.
/// \param key Buffer in which key is written. Memory allocated by function.
/// \param length Length of key. Is set by function.
/// \return integer
/// \retval 0 for success
/// \retval 1 for failure
int UnbindAESKey(BYTE** key, int* length);
/// \param ciphertext Ciphertext to be decrypted.
/// \param ciphertext_len Length of ciphertext.
/// \param key Key used for decryption.
/// \param iv Initialization Vector.
/// \param plaintext Resulting plaintext.
/// \return Length of plaintext.
int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
            unsigned char *iv, unsigned char *plaintext);


int main(int argc, char** argv) {
    if (!fileExists(KEYPATH)) {
        printf("Error. Couldn't find key file. Please make sure your encryption key is present.\n");
        exit(EXIT_FAILURE);
    }
    if (!fileExists(FILEPATH)) {
        printf("Error. Couldn't find the encrypted file. Please make sure your encrypted data is present.\n");
        exit(EXIT_FAILURE);
    }
    // Command line switches
    int opt;
    // Check switches
    while(((opt = getopt(argc, argv, "hv")) != -1)) {
        switch (opt) {
            // Switch -h for help
            case 'h':
                printf("\n\nThis program decrypts data using AES-256 in CBC mode. The encryption key is the key created using create_key. "\
                       "It will be unbound by the TPM for the time of the decryption.\n\n" \
                       "Usage:\n\tdecrypt_data -v data\n" \
                       "\t\t-h\tHelp. Displays this help.\n" \
                       "\t\t-v\tVerbose. Displays information during the process.\n");
                exit(EXIT_SUCCESS);
                // Switch -v for verbose
            case 'v':
                verbose = 1;
                break;
            default: break;
        }
    }

    // Init openSSL library
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    OPENSSL_config(NULL);

    // Initialize TPM context
    if (TPM_InitContext())
        ExitFailure();

    // Get data
    unsigned char iv[16];
    int data_len;
    char* data;
    // Read IV and encrypted data from file
    FILE* fin = fopen(FILEPATH, "r");
    fseek(fin, 0, SEEK_END);
    data_len = (int)ftell(fin) - sizeof iv;
    fseek(fin, 0, SEEK_SET);
    data = malloc(data_len * sizeof(char));
    if (data) {
        fread (iv, 1, sizeof iv, fin);
        fread (data, 1, data_len, fin);
    }
    else {
        EVP_cleanup();
        ERR_free_strings();
        free(data);
        ExitFailure();
    }
    fclose(fin);

    // GET AES key
    BYTE* key;
    int key_length;
    if(UnbindAESKey(&key, &key_length))
        ExitFailure();
    // Decrypt data
    unsigned char plaintext[data_len];
    int plaintext_len = decrypt((unsigned char*)data, data_len, key, iv, plaintext);
    // Override key
    memset(key, 0, sizeof key);

    plaintext[plaintext_len] = '\0';
    printf("%s\n", plaintext);


    // Close everything
    TPM_CloseContext();
    EVP_cleanup();
    ERR_free_strings();
//    free(key);
    exit(EXIT_SUCCESS);
}


int UnbindAESKey(BYTE** key, int* length) {
    // TPM variables
    TSS_HKEY hBindKey = 0;
    TSS_UUID BindKey_UUID = KEY_UUID;
    TSS_HOBJECT hEncData = 0;
    TSS_HPOLICY hBindKeyPolicy = 0;
    // Encrypted data
    FILE *fin;
    UINT32 encKeyLength;
    BYTE *encKey;
    // Unencrypted key
    UINT32 keyLength;

    print_info("Reading AES key from file... ");
    fflush(stdout);
    // Read encrypted data from file
    fin = fopen(KEYPATH, "r");
    fseek(fin, 0, SEEK_END);
    encKeyLength = (uint32_t)ftell(fin);
    fseek(fin, 0, SEEK_SET);
    encKey = (BYTE*)malloc(encKeyLength*sizeof(BYTE));
    if (encKey) {
        fread (encKey, 1, encKeyLength, fin);
    }
    fclose(fin);
    print_info("Success.\nUnbinding AES key... ");
    fflush(stdout);

    // Get key
    result = Tspi_Context_GetKeyByUUID(hContext, TSS_PS_TYPE_SYSTEM, BindKey_UUID, &hBindKey);
    if(result != TSS_SUCCESS) {
        TPM_CloseContext();
        printf("Error during data unbinding: Get key by UUID. Error 0x%08x:%s\n", result, Trspi_Error_String(result));
        return 1;
    }
    // Load the key with wrapping key
    result = Tspi_Key_LoadKey(hBindKey, hSRK);
    if(result != TSS_SUCCESS) {
        TPM_CloseContext();
        printf("Error during data unbinding: Load key. Error 0x%08x:%s\n", result, Trspi_Error_String(result));
        return 1;
    }
    // Get policy
    result = Tspi_GetPolicyObject(hBindKey, TSS_POLICY_USAGE, &hBindKeyPolicy);
    if(result != TSS_SUCCESS) {
        TPM_CloseContext();
        printf("Error during data unbinding: Get key policy. Error 0x%08x:%s\n", result, Trspi_Error_String(result));
        return 1;
    }
    // Set policy secret
    Tspi_Policy_SetSecret( hBindKeyPolicy, TSS_SECRET_MODE_PLAIN, 20, wks  );
    if(result != TSS_SUCCESS) {
        TPM_CloseContext();
        printf("Error during data unbinding: Set key secret. Error 0x%08x:%s\n", result, Trspi_Error_String(result));
        return 1;
    }
    // Create data object.
    result = Tspi_Context_CreateObject(hContext, TSS_OBJECT_TYPE_ENCDATA, TSS_ENCDATA_BIND, &hEncData);
    if(result != TSS_SUCCESS) {
        TPM_CloseContext();
        printf("Error during data unbinding: Create data object. Error 0x%08x:%s\n", result, Trspi_Error_String(result));
        return 1;
    }

    // Feed encrypted data into data object.
    result = Tspi_SetAttribData(hEncData, TSS_TSPATTRIB_ENCDATA_BLOB, TSS_TSPATTRIB_ENCDATABLOB_BLOB, encKeyLength, encKey);
    if(result != TSS_SUCCESS) {
        TPM_CloseContext();
        printf("Error during data unbinding: Feed encrypted data into object. Error 0x%08x:%s\n", result, Trspi_Error_String(result));
        return 1;
    }

    // Unbind data
    result = Tspi_Data_Unbind(hEncData, hBindKey, &keyLength, key);
    if(result != TSS_SUCCESS) {
        TPM_CloseContext();
        printf("Error during data unbinding: Unbind data. Error 0x%08x:%s\n", result, Trspi_Error_String(result));
        return 1;
    }

    print_info("Success.\n");
    free(encKey);
    *length = keyLength;
    // Close handles
    Tspi_Key_UnloadKey(hBindKey);
    Tspi_Context_CloseObject(hContext, hBindKeyPolicy);
    Tspi_Context_CloseObject(hContext, hEncData);
    Tspi_Context_CloseObject(hContext, hBindKey);
    return 0;
}

int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key, unsigned char *iv, unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) handleErrors();

    /* Initialise the encryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * Here, key: 256 bits, IV: 128 bits */
    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handleErrors();

    /* Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
        handleErrors();
    plaintext_len = len;

    /* Finalise the decryption. Further plaintext bytes may be written at
     * this stage.
     */
    if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len)) handleErrors();
    plaintext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;
}