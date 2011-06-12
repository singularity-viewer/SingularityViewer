// Copyright (c)2009 Thomas Shikami
// 
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
// 
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "llviewerprecompiledheaders.h"
#include "emerald.h"
#include <sstream>
#include <cstring>

#include <openssl/evp.h>
#include <openssl/dh.h>
#include <openssl/dsa.h>

//-- Ascii85 encoder and decoder

typedef unsigned int U32;

static void encodeU32(unsigned int in, char *out)
{
	out[4] = char(in % 85) + char(33);
	in /= 85;
	out[3] = char(in % 85) + char(33);
	in /= 85;
	out[2] = char(in % 85) + char(33);
	in /= 85;
	out[1] = char(in % 85) + char(33);
	in /= 85;
	out[0] = char(in % 85) + char(33);
}

static unsigned int decodeU32(const char *in)
{
	U32 out;

	out = U32(in[0] - 33);
	out *= 85;
	out += U32(in[1] - 33);
	out *= 85;
	out += U32(in[2] - 33);
	out *= 85;
	out += U32(in[3] - 33);
	out *= 85;
	out += U32(in[4] - 33);

	return out;
}

// static
std::string EAscii85::encode(const std::vector<unsigned char> &in)
{
	std::ostringstream out;
	U32 tuple;
	int count;
	char block[6];
	block[5] = '\0';

	out << "<~";

	count = 0;
	tuple = 0;
	for(size_t i = 0; i < in.size(); i++)
	{
		tuple <<= 8;
		tuple += in[i];
		if(++count == 4)
		{
			if(tuple == 0)
			{
				out << "z";
			}
			else
			{
				encodeU32(tuple, block);
				out << block;
			}
			count = 0;
		}
	}

	switch(count)
	{
	case 1:
		tuple <<= 8;
		tuple += 255;
		// pass through
	case 2:
		tuple <<= 8;
		tuple += 255;
		// pass through
	case 3:
		tuple <<= 8;
		tuple += 255;
	}
	
	encodeU32(tuple, block);

	switch(count)
	{
	case 1:
		block[2] = '\0';
		break;
	case 2:
		block[3] = '\0';
		break;
	case 3:
		block[4] = '\0';
		break;
	}

	if(count > 0)
		out << block;

	out << "~>";

	return out.str();
}

// static
std::vector<unsigned char> EAscii85::decode(const std::string &in)
{
	std::vector<unsigned char> out;
	size_t len;
	int count = 0;
	char block[6];
	block[5] = '\0';
	U32 tuple;

	// approximate length
	len = in.length() / 5 * 4;

	out.clear();

	if(in.length() < 4) return out;

	std::string::const_iterator i = in.begin();

	if(*i != '<') return out;
	i++;
	if(*i != '~') return out;
	i++;

	out.reserve(len);
	
	for(; i != in.end(); i++)
	{
		char c = *i;

		if(c >= '!' && c < 'v')
		{
			block[count++] = c;
		}

		switch(c)
		{
		case 'z':
			if(count == 1)
			{
				for(count = 0; count < 4; count++)
					out.push_back(0);
				count = 0;
			}
			break;
		case '~':
			if(count > 1)
			{
				switch(count)
				{
				case 2:
					block[2] = 'u';
				case 3:
					block[3] = 'u';
				case 4:
					block[4] = 'u';
				}
				tuple = decodeU32(block);
				for(;count > 1; count--)
				{
					out.push_back(char(tuple >> 24));
					tuple <<= 8;
				}
				count = 0;
			}
		}

		if(count == 5)
		{
			tuple = decodeU32(block);
			for(count = 0; count < 4; count++)
			{
				out.push_back(char(tuple >> 24));
				tuple <<= 8;
			}
			count = 0;
		}
	}

	return out;
}

//-- AES wrapper

class EAESEncrypt::EncryptImpl {
public:
	EncryptImpl(const unsigned char *key, const unsigned char *iv);
	~EncryptImpl();

	std::vector<unsigned char> encrypt(const std::string &in);

	EVP_CIPHER_CTX ctx;
};

EAESEncrypt::EAESEncrypt(const unsigned char *key, const unsigned char *iv)
{
	mEncryptImpl = new EAESEncrypt::EncryptImpl(key, iv);
}

EAESEncrypt::EAESEncrypt(const std::vector<unsigned char> &key, const std::vector<unsigned char> &iv)
{
	mEncryptImpl = new EAESEncrypt::EncryptImpl(&key[0], &iv[0]);
}

EAESEncrypt::~EAESEncrypt()
{
	delete mEncryptImpl;
}

std::vector<unsigned char> EAESEncrypt::encrypt(const std::string &in)
{
	return mEncryptImpl->encrypt(in);
}

EAESEncrypt::EncryptImpl::EncryptImpl(const unsigned char *key, const unsigned char *iv)
{
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL, key, iv);
}

EAESEncrypt::EncryptImpl::~EncryptImpl()
{
	EVP_CIPHER_CTX_cleanup(&ctx);
}

std::vector<unsigned char> EAESEncrypt::EncryptImpl::encrypt(const std::string &in)
{
	std::vector<unsigned char> out;
	int outlen;
	int tmplen;

	out.resize(in.length() + 32);

	EVP_EncryptUpdate(&ctx, &out[0], &outlen, reinterpret_cast<const unsigned char *>(in.c_str()), in.length());
	EVP_EncryptFinal_ex(&ctx, &out[outlen], &tmplen);

	out.resize(outlen + tmplen);

	return out;
}

class EAESDecrypt::DecryptImpl {
public:
	DecryptImpl(const unsigned char *key, const unsigned char *iv);
	~DecryptImpl();

	std::string decrypt(const std::vector<unsigned char> &in);

	EVP_CIPHER_CTX ctx;
};

EAESDecrypt::EAESDecrypt(const unsigned char *key, const unsigned char *iv)
{
	mDecryptImpl = new EAESDecrypt::DecryptImpl(key, iv);
}

EAESDecrypt::EAESDecrypt(const std::vector<unsigned char> &key, const std::vector<unsigned char> &iv)
{
	mDecryptImpl = new EAESDecrypt::DecryptImpl(&key[0], &iv[0]);
}

EAESDecrypt::~EAESDecrypt()
{
	delete mDecryptImpl;
}

EAESDecrypt::DecryptImpl::DecryptImpl(const unsigned char *key, const unsigned char *iv)
{
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL, key, iv);
}

EAESDecrypt::DecryptImpl::~DecryptImpl()
{
	EVP_CIPHER_CTX_cleanup(&ctx);
}

std::string EAESDecrypt::decrypt(const std::vector<unsigned char> &in)
{
	return mDecryptImpl->decrypt(in);
}

std::string EAESDecrypt::DecryptImpl::decrypt(const std::vector<unsigned char> &in)
{
	std::vector<unsigned char> out;
	int outlen;
	int tmplen;

	if(in.size() == 0) return "";

	out.resize(in.size() + 32);

	EVP_DecryptUpdate(&ctx, &out[0], &outlen, &in[0], in.size());
	EVP_DecryptFinal_ex(&ctx, &out[outlen], &tmplen);

	out.resize(outlen + tmplen);

	if(out.empty())
		return "";

	return std::string(reinterpret_cast<const char *>(&out[0]), out.size());
}

EGenKey::EGenKey(const std::string &password, const unsigned char *salt)
{
	EVP_BytesToKey(EVP_aes_128_cbc(), EVP_sha1(), salt,
		reinterpret_cast<const unsigned char *>(password.c_str()), password.length(),
		1000, mKey, mIv);
}

EGenKey::~EGenKey()
{
	memset(mKey, 0, 16);
	memset(mIv, 0, 16);
}

DH *get_dh2048();

class BigNum
{
public:
	BigNum(const std::vector<unsigned char> &bin)
	{
		mBN = BN_bin2bn(&(bin[0]), bin.size(), NULL);
	}

	BigNum()
	{
		mBN = BN_new();
	}

	BigNum(const BigNum &other)
	{
		mBN = BN_dup(*other);
	}

	BigNum(BIGNUM *bn)
	{
		mBN = BN_dup(bn);
	}

	~BigNum()
	{
		BN_clear_free(mBN);
		mBN = NULL;
	}

	friend BIGNUM* operator*(const BigNum&);

	BigNum& operator=(const std::vector<unsigned char> &bin)
	{
		mBN = BN_bin2bn(&(bin[0]), bin.size(), mBN);
		return *this;
	}

	BigNum& operator=(const BigNum& other)
	{
		BN_copy(mBN, *other);
		return *this;
	}

	void to(std::vector<unsigned char> &dest)
	{
		dest.resize(BN_num_bytes(mBN));
		BN_bn2bin(mBN, &(dest[0]));
	}

	BIGNUM* to(BIGNUM *ret)
	{
		return BN_copy(ret, mBN);
	}

	BIGNUM* dup()
	{
		return BN_dup(mBN);
	}

	static BIGNUM* dup(const std::vector<unsigned char> &bin)
	{
		return BN_bin2bn(&(bin[0]), bin.size(), NULL);
	}

private:
	BIGNUM *mBN;
};

BIGNUM* operator*(const BigNum& o) { return o.mBN; }

class EDH::DHImpl
{
public:
	DH *mDH;
};

EDH::EDH(const std::vector<unsigned char> &priv_key)
{
	mDHImpl = new DHImpl();

	mDHImpl->mDH = get_dh2048();
	mDHImpl->mDH->priv_key = BigNum::dup(priv_key);

	DH_generate_key(mDHImpl->mDH);
}

EDH::EDH()
{
	mDHImpl = new DHImpl();

	mDHImpl->mDH = get_dh2048();

	DH_generate_key(mDHImpl->mDH);
}

EDH::~EDH()
{
	if(mDHImpl)
	{
		if(mDHImpl->mDH)
		{
			mDHImpl->mDH->priv_key = NULL;
			mDHImpl->mDH->pub_key = NULL;
			DH_free(mDHImpl->mDH);
			mDHImpl->mDH = NULL;
		}
		delete mDHImpl;
		mDHImpl = NULL;
	}
}

void EDH::secretTo(std::vector<unsigned char> &secret)
{
	BigNum(mDHImpl->mDH->priv_key).to(secret);
}

void EDH::publicTo(std::vector<unsigned char> &pub)
{
	BigNum(mDHImpl->mDH->pub_key).to(pub);
}

std::vector<unsigned char> EDH::computeKey(const std::vector<unsigned char> &other_pub)
{
	std::vector<unsigned char> temp;
	BigNum pub(other_pub);
	temp.resize(DH_size(mDHImpl->mDH));

	DH_compute_key(&(temp[0]), *pub, mDHImpl->mDH);

	return temp;
}

DSA *get_dsa2048();

class EDSA::DSAImpl
{
public:
	DSA *mDSA;
};

EDSA::EDSA()
{
	mDSAImpl = new DSAImpl();
	mDSAImpl->mDSA = get_dsa2048();
}

EDSA::EDSA(const std::vector<unsigned char> &secret)
{
	mDSAImpl = new DSAImpl();

	const unsigned char *buf = &(secret[0]);

	mDSAImpl->mDSA = d2i_DSAPrivateKey(NULL, &buf, secret.size());
}

std::vector<unsigned char> EDSA::generateSecret()
{
	DSA_generate_key(mDSAImpl->mDSA);

	std::vector<unsigned char> temp;

	int size = i2d_DSAPrivateKey(mDSAImpl->mDSA, NULL);

	temp.resize(size);

	unsigned char *buf = &(temp[0]);

	i2d_DSAPrivateKey(mDSAImpl->mDSA, &buf);

	return temp;
}

std::vector<unsigned char> EDSA::getPublic()
{
	std::vector<unsigned char> temp;

	mDSAImpl->mDSA->write_params = 0;

	int size = i2d_DSAPublicKey(mDSAImpl->mDSA, NULL);

	temp.resize(size);

	unsigned char *buf = &(temp[0]);

	i2d_DSAPublicKey(mDSAImpl->mDSA, &buf);

	return temp;
}

void EDSA::setPublic(const std::vector<unsigned char> &pub)
{
	const unsigned char *buf = &(pub[0]);

	d2i_DSAPublicKey(&mDSAImpl->mDSA, &buf, pub.size());
}

bool EDSA::verify(const std::vector<unsigned char> &dgst, const std::vector<unsigned char> &sig)
{
	bool result = false;

	if(DSA_verify(0, &(dgst[0]), dgst.size(), &(sig[0]), sig.size(), mDSAImpl->mDSA) == 1)
	{
		result = true;
	}

	return result;
}

// static
bool EDSA::verify(const std::vector<unsigned char> &dgst, const std::vector<unsigned char> &sig, const std::vector <unsigned char> &pub)
{
	const unsigned char *buf = &(pub[0]);
	DSA *dsa;
	bool result = false;

	dsa = get_dsa2048();

	d2i_DSAPublicKey(&dsa, &buf, pub.size());

	if(DSA_verify(0, &(dgst[0]), dgst.size(), &(sig[0]), sig.size(), dsa) == 1)
	{
		result = true;
	}

	DSA_free(dsa);

	return result;
}

std::vector<unsigned char> EDSA::sign(const std::vector<unsigned char> &dgst)
{
	std::vector<unsigned char> sig;
	unsigned int size = DSA_size(mDSAImpl->mDSA);

	sig.resize(size);

	DSA_sign(0, &(dgst[0]), dgst.size(), &(sig[0]), &size, mDSAImpl->mDSA);

	sig.resize(size);

	return sig;
}

EDSA::~EDSA()
{
	delete mDSAImpl;
	mDSAImpl = NULL;
}
