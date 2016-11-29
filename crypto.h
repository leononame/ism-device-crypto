//
// Created by leo on 11/29/16.
//


#ifndef ENC_CRYPTO_H_H
#define ENC_CRYPTO_H_H



void encrypt_dat(char *name, unsigned char *data);
void decrypt_dat(char *name, unsigned char** plaintext, int* plaintext_length);
void renew_key(char *name);
void create_key(char *name);
#endif //ENC_CRYPTO_H_H
